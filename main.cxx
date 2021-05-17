/*
 * main.cxx
 * 
*/

#include "config.h"
#include "custom.h"
#include "main.h"
#include "html.h"

VideoMixConfigurator OpalMCUEIE::vmcfg;
PCREATE_PROCESS(OpalMCUEIE);

OpalMCUEIE::OpalMCUEIE()
  : PHTTPServiceProcess(ProductInfo)
  , m_manager(NULL)
{
  m_currentLogLevel   = -1;
  m_currentTraceLevel = -1;
  m_traceFileRotated  = false;
  
}

OpalMCUEIE::~OpalMCUEIE()
{
  delete m_manager;
}

void OpalMCUEIE::Main()
{
  Suspend();
}

void OpalMCUEIE::OnControl()
{
}

void OpalMCUEIE::OnConfigChanged()
{
}

void OpalMCUEIE::OnStop()
{
  return PHTTPServiceProcess::OnStop();
}

PBoolean OpalMCUEIE::OnStart()
{   
  m_manager = new Manager();
#if OPAL_HAS_PCSS
  new PCSSEndPoint(*m_manager);
#endif // OPAL_HAS_PCSS

#if OPAL_HAS_MIXER
  m_manager->Initialise(GetArguments(), false, OPAL_PREFIX_MIXER":<du>");
#else
  m_manager->Initialise(GetArguments(), false);
#endif // OPAL_HAS_MIXER

  return PHTTPServiceProcess::OnStart();
}

PBoolean OpalMCUEIE::Initialise(const char * initMsg)
{
  PSYSTEMLOG(Warning, "Service " << GetName() << ' ' << initMsg);
  
  PConfig cfg;
  if (!m_manager->Configure(cfg))
    return false;
  
  vmcfg.go(vmcfg.bfw,vmcfg.bfh);

#if PTRACING
  int TraceLevel=cfg.GetInteger("Generals","TraceLevelKey", 0);
  int LogLevel=cfg.GetInteger("Generals","LogLevelKey", 0);
  if(m_currentLogLevel != LogLevel)
  {
    SetLogLevel((PSystemLog::Level)LogLevel);
    m_currentLogLevel = LogLevel;
  }
  PINDEX rotationLevel = cfg.GetInteger("Generals","TraceRotateKey", 0);
  if(m_currentTraceLevel != TraceLevel)
  {
    if(!m_traceFileRotated)
    {
      if(rotationLevel != 0)
      {
        PString
          pfx = PString(SERVER_LOGS) + PATH_SEPARATOR + "trace",
          sfx = ".txt";
        PFile::Remove(pfx + PString(rotationLevel-1) + sfx, true);
        for (PINDEX i=rotationLevel-1; i>0; i--) 
          PFile::Move(pfx + PString(i-1) + sfx, pfx + PString(i) + sfx, true,false);
        PFile::Move(pfx + sfx, pfx + "0" + sfx, true,false);
      }
      m_traceFileRotated = true;
    }

    PTrace::Initialise(TraceLevel, PString(SERVER_LOGS) + PATH_SEPARATOR + "trace.txt");
    PTrace::SetOptions(PTrace::FileAndLine);
    m_currentTraceLevel = TraceLevel;
  }

#endif //if PTRACING

// default log file name
#ifdef SERVER_LOGS
  {
    PString lfn = cfg.GetString("Generals","CallLogFilenameKey", "mcu_log.txt");
    if(lfn.Find(PATH_SEPARATOR) == P_MAX_INDEX) m_logFilename = PString(SERVER_LOGS)+PATH_SEPARATOR+lfn;
    else m_logFilename = lfn;
  }
#else
  m_logFilename = cfg.GetString("Generals","CallLogFilenameKey", "mcu_log.txt");
#endif

  m_httpBuffer=100;//cfg.GetInteger("Generals","HttpLinkEventBufferKey", 100);
  m_httpBufferedEvents.SetSize(m_httpBuffer);
  m_httpBufferIndex=0; m_httpBufferComplete=0;


  // get conference time limit
  m_roomTimeLimit = cfg.GetInteger("Generals","DefaultRoomTimeLimitKey", 0);

  // allow/disallow self-invite:
  m_allowLoopbackCalls = cfg.GetBoolean("Generals","AllowLoopbackCallsKey", false);

  // get default "room" (conference) name
  m_defaultRoomName = cfg.GetString("Generals","DefaultRoomKey", "OPALSERVER");
  m_autoDeleteRoom = cfg.GetBoolean("Generals","AutoDeleteRoomKey", false);
  m_copyWebLogToLog = false;//cfg.GetBoolean("Copy web log to call log", FALSE);
  
  CreateHTTPResource("HomePage");
  CreateHTTPResource("Control");
  CreateHTTPResource("Settings");
  CreateHTTPResource("H323");
  CreateHTTPResource("Conference");
  CreateHTTPResource("Comm");
  CreateHTTPResource("Jpeg");
  CreateHTTPResource("Invite");
  CreateHTTPResource("CallDetailRecord");
  CreateHTTPResource("CallDetailRecords");
  CreateHTTPResource("CallStatus");
  
  
  
  
#ifdef SYS_RESOURCE_DIR
#  define WEBSERVER_LINK(r1)              m_httpNameSpace.AddResource(new PHTTPFile(r1, PString(SYS_RESOURCE_DIR) + PATH_SEPARATOR + r1), PHTTPSpace::Overwrite)
#  define WEBSERVER_LINK_MIME(mt1,r1)     m_httpNameSpace.AddResource(new PHTTPFile(r1, PString(SYS_RESOURCE_DIR) + PATH_SEPARATOR + r1, mt1), PHTTPSpace::Overwrite)
#  define WEBSERVER_LINK_MIME_CFG(mt1,r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, PString(SYS_CONFIG_DIR) + PATH_SEPARATOR + r1, mt1), PHTTPSpace::Overwrite)
#else
#  define WEBSERVER_LINK(r1)              m_httpNameSpace.AddResource(new PHTTPFile(r1), PHTTPSpace::Overwrite)
#  define WEBSERVER_LINK_MIME(mt1,r1)     m_httpNameSpace.AddResource(new PHTTPFile(r1, r1, mt1), PHTTPSpace::Overwrite)
#  define WEBSERVER_LINK_MIME_CFG(mt1,r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, r1, mt1), PHTTPSpace::Overwrite)
#endif
  //WEBSERVER_LINK_MIME("text/css",  "bootstrap.css");
  WEBSERVER_LINK_MIME("text/javascript", "control1.js");
  WEBSERVER_LINK_MIME("text/javascript", "locale_en.js");
  WEBSERVER_LINK_MIME("text/css",  "main.css");
  WEBSERVER_LINK_MIME("image/png", "mylogo_text.png");
  WEBSERVER_LINK_MIME("image/png"                , "s15_ch.png");
  WEBSERVER_LINK_MIME("image/gif"                , "i15_getNoVideo.gif");
  WEBSERVER_LINK_MIME("image/gif"                , "openmcu.ru_vad_vad.gif");
  WEBSERVER_LINK_MIME("image/gif"                , "openmcu.ru_vad_disable.gif");
  WEBSERVER_LINK_MIME("image/gif"                , "openmcu.ru_vad_chosenvan.gif");
  WEBSERVER_LINK_MIME("image/gif"                , "i15_inv.gif");
  WEBSERVER_LINK_MIME("image/gif"                , "openmcu.ru_launched_Ypf.gif");
  WEBSERVER_LINK_MIME("image/gif"                , "i20_close.gif");
  WEBSERVER_LINK_MIME("image/gif"                , "i20_vad.gif");
  WEBSERVER_LINK_MIME("image/gif"                , "i20_vad2.gif");
  WEBSERVER_LINK_MIME("image/gif"                , "i20_static.gif");
  WEBSERVER_LINK_MIME("image/gif"                , "i20_plus.gif");
  WEBSERVER_LINK_MIME("image/gif"                , "i24_shuff.gif");
  WEBSERVER_LINK_MIME("image/gif"                , "i24_left.gif");
  WEBSERVER_LINK_MIME("image/gif"                , "i24_right.gif");
  WEBSERVER_LINK_MIME("image/gif"                , "i24_mix.gif");
  WEBSERVER_LINK_MIME("image/gif"                , "i24_clr.gif");
  WEBSERVER_LINK_MIME("image/gif"                , "i24_revert.gif");
  WEBSERVER_LINK_MIME("image/vnd.microsoft.icon" , "openmcu.ico");
  WEBSERVER_LINK_MIME("image/png"                , "openmcu.ru_logo_text.png");
  WEBSERVER_LINK_MIME("image/png"                , "menu_left.png");
  WEBSERVER_LINK_MIME("image/png"                , "i16_close_gray.png");
  WEBSERVER_LINK_MIME("image/png"                , "i16_close_red.png");
  WEBSERVER_LINK_MIME("image/png"                , "i32_lock.png");
  WEBSERVER_LINK_MIME("image/png"                , "i32_lockopen.png");
  
  if (ListenForHTTP((WORD)cfg.GetInteger("Generals","HttpPortKey", DefaultHTTPPort)))
    PSYSTEMLOG(Info, "Opened master socket for HTTP: " << m_httpListeningSockets.front().GetPort());
  else {
    PSYSTEMLOG(Fatal, "Cannot run without HTTP port.");
    return false;
  }
  
  // refresh Address Book
  
  PSYSTEMLOG(Info, "Completed configuring service " << GetName() << " (" << initMsg << ')');

  return true;
}

PString OpalMCUEIE::GetHtmlCopyright()
{
  PHTML html(PHTML::InBody);
  html << "Copyright &copy;"
       << m_compilationDate.AsString("yyyy") << " by "
       << PHTML::HotLink(m_copyrightHomePage + "\" target=\"_blank\"")
       << m_copyrightHolder;
  return html;
}

void OpalMCUEIE::CreateHTTPResource(const PString & name)
{
  PHTTPMultiSimpAuth authConference(GetName());

  if (name == "HomePage")
    m_httpNameSpace.AddResource(new HomePage(*this, authConference), PHTTPSpace::Overwrite);
  else if (name == "Control")
    m_httpNameSpace.AddResource(new ControlRoomPage(*this, authConference), PHTTPSpace::Overwrite);
  else if (name == "Settings")
    m_httpNameSpace.AddResource(new GeneralPConfigPage(*this, "Parameters", "Generals", authConference), PHTTPSpace::Overwrite);
  else if (name == "H323")
    m_httpNameSpace.AddResource(new H323PConfigPage(*this, "H323Parameters", "H323Parameters", authConference), PHTTPSpace::Overwrite);
  else if (name == "Conference")
    m_httpNameSpace.AddResource(new ConferencePConfigPage(*this, "ConferenceParameters", "Conference", authConference), PHTTPSpace::Overwrite);
  else if (name == "CallDetailRecord")
    m_httpNameSpace.AddResource(new CDRPage(GetManager(), authConference), PHTTPSpace::Overwrite);
  else if (name == "CallDetailRecords")
    m_httpNameSpace.AddResource(new CDRListPage(GetManager(), authConference), PHTTPSpace::Overwrite);
  else if (name == "CallStatus")
    m_httpNameSpace.AddResource(new CallStatusPage(GetManager(), authConference), PHTTPSpace::Overwrite);
  else if (name == "Invite")
    m_httpNameSpace.AddResource(new InvitePage(*this, authConference), PHTTPSpace::Overwrite);

  else if (name == "Comm")
    m_httpNameSpace.AddResource(new InteractiveHTTP(*this, authConference), PHTTPSpace::Overwrite);
  else if (name == "Jpeg")
    m_httpNameSpace.AddResource(new JpegFrameHTTP(*this, authConference), PHTTPSpace::Overwrite);
  
}

void OpalMCUEIE::LogMessage(const PString & str)
{
  static PMutex logMutex;
  static PTextFile logFile;

  PTime now;
  PString msg = now.AsString("dd/MM/yyyy") & str;
  logMutex.Wait();

  if (!logFile.IsOpen()) {
    if(!logFile.Open(m_logFilename, PFile::ReadWrite))
    {
      PTRACE(1,"MyProcess\tCan not open log file: " << m_logFilename << "\n" << msg << flush);
      logMutex.Signal();
      return;
    }
    if(!logFile.SetPosition(0, PFile::End))
    {
      PTRACE(1,"MyProcess\tCan not change log position, log file name: " << m_logFilename << "\n" << msg << flush);
      logFile.Close();
      logMutex.Signal();
      return;
    }
  }

  if(!logFile.WriteLine(msg))
  {
    PTRACE(1,"MyProcess\tCan not write to log file: " << m_logFilename << "\n" << msg << flush);
  }
  logFile.Close();
  logMutex.Signal();
}

void OpalMCUEIE::LogMessageHTML(PString str)
{ 
  PString str2, roomName;
  PINDEX tabPos=str.Find("\t");
  if(tabPos!=P_MAX_INDEX)
  {
    roomName=str.Left(tabPos);
    str=str.Mid(tabPos+1,P_MAX_INDEX);
  }
  if(str.GetLength()>8)
  {
    if(str[1]==':') str=PString("0")+str;
    if(!roomName.IsEmpty()) str=str.Left(8)+" "+roomName+str.Mid(9,P_MAX_INDEX);
  }
  bool tag=false;
  for (PINDEX i=0; i< str.GetLength(); i++)
  if(str[i]=='<') tag=TRUE;
  else if(str[i]=='>') tag=FALSE;
  else if(!tag) str2+=str[i];
  LogMessage(str2);
}

void OpalMCUEIE::HttpWrite_(PString evt) 
{
  m_httpBufferMutex.Wait();
  m_httpBufferedEvents[m_httpBufferIndex]=evt; 
  m_httpBufferIndex++;
  if(m_httpBufferIndex>=m_httpBuffer){ 
    m_httpBufferIndex=0; 
    m_httpBufferComplete=1; 
  }
  m_httpBufferMutex.Signal();
}
void OpalMCUEIE::HttpWriteEvent(PString evt) 
{
  PString evt0; PTime now;
  evt0 += now.AsString("h:mm:ss. ", PTime::Local) + evt;
  HttpWrite_(evt0+"<br>\n");
  if(m_copyWebLogToLog) LogMessageHTML(evt0);
    
}
void OpalMCUEIE::HttpWriteEventRoom(PString evt, PString room)
{
  PString evt0; PTime now;
  evt0 += room + "\t" + now.AsString("h:mm:ss. ", PTime::Local) + evt;
  HttpWrite_(evt0+"<br>\n");
  if(m_copyWebLogToLog) LogMessageHTML(evt0);
}
void OpalMCUEIE::HttpWriteCmdRoom(PString evt, PString room)
{
  PStringStream evt0;
  evt0 << room << "\t<script>p." << evt << "</script>\n";
  HttpWrite_(evt0);
}
void OpalMCUEIE::HttpWriteCmd(PString evt)
{
  PStringStream evt0;
  evt0 << "<script>p." << evt << "</script>\n";
  HttpWrite_(evt0);
}
PString OpalMCUEIE::HttpGetEvents(int &idx, PString room)
{
  PStringStream result;
  while (idx!=m_httpBufferIndex){
    PINDEX pos=m_httpBufferedEvents[idx].Find("\t",0);
    if(pos==P_MAX_INDEX)
      result << m_httpBufferedEvents[idx];
    else if(room=="")
      result << m_httpBufferedEvents[idx].Mid(pos+1,P_MAX_INDEX);
    else if(m_httpBufferedEvents[idx].Left(pos)==room) 
      result << m_httpBufferedEvents[idx].Mid(pos+1,P_MAX_INDEX);
    idx++;
    if(idx>=m_httpBuffer)
      idx=0;      
  }  
  return result;
}
    
PString OpalMCUEIE::HttpStartEventReading(int &idx, PString room)
{
  PStringStream result;
  if(m_httpBufferComplete){
    idx=m_httpBufferIndex+1; 
	if(idx>m_httpBuffer)
	  idx=0;
  } 
  else idx=0;
      
  while (idx!=m_httpBufferIndex){
    if(m_httpBufferedEvents[idx].Find("<script>",0)==P_MAX_INDEX){
      PINDEX pos=m_httpBufferedEvents[idx].Find("\t",0);
      if(pos==P_MAX_INDEX)
        result << m_httpBufferedEvents[idx];
      else if(room=="")
        result << m_httpBufferedEvents[idx].Mid(pos+1,P_MAX_INDEX);
      else if(m_httpBufferedEvents[idx].Left(pos)==room) 
        result << m_httpBufferedEvents[idx].Mid(pos+1,P_MAX_INDEX);
    }
    idx++;
    if(idx>=m_httpBuffer)
      idx=0;
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OPALMCUURL::OPALMCUURL(PString str)
{
  PINDEX delim1 = str.FindLast("[");
  PINDEX delim2 = str.FindLast("]");
  PINDEX delim3 = str.FindLast("<sip:");
  PINDEX delim4 = str.FindLast(">");

  if(delim3 != P_MAX_INDEX && delim4 != P_MAX_INDEX)
  {
    m_displayName = str.Left(delim3-1);
    m_displayName.Replace("\"","",TRUE,0);
    m_partyUrl = str.Mid(delim3+1, delim4-delim3-1);
  }
  else if(delim1 != P_MAX_INDEX && delim2 != P_MAX_INDEX)
  {
    m_displayName = str.Left(delim1-1);
    m_partyUrl = str.Mid(delim1+1, delim2-delim1-1);
  } 
  else {
    m_partyUrl = str;
    
  }
  
  cout << m_partyUrl << endl;
  if(m_partyUrl.Left(4) == "sip:") m_URLScheme = "sip";
  //else if(m_partyUrl.Left(3) == "pc:") m_URLScheme = "pc";
  //else if(m_partyUrl.Left(4) == "mcu:") m_URLScheme = "mcu";
  else if(m_partyUrl.Left(5) == "h323:") m_URLScheme = "h323";
  else { m_partyUrl = "h323:"+m_partyUrl; m_URLScheme = "h323"; }

  Parse((const char *)m_partyUrl, m_URLScheme);
  // parse old H.323 scheme
  if(m_URLScheme == "h323" && m_partyUrl.Left(5) != "h323" && m_partyUrl.Find("@") == P_MAX_INDEX &&
     m_hostname == "" && m_username != "")
  {
    m_hostname = m_username;
    m_username = "";
  }

  m_memberName = m_displayName+" ["+m_partyUrl+"]";
  cout << m_memberName << endl;
  
  m_urlId = m_URLScheme+":";
  if(m_username != "") m_urlId += m_username;
  else               m_urlId += m_displayName+"@"+m_hostname;

  cout << m_urlId << endl;
  if(m_URLScheme == "sip")
  {
    m_sipProto = "*";
    if(m_partyUrl.Find("transport=udp") != P_MAX_INDEX) m_sipProto = "udp";
    if(m_partyUrl.Find("transport=tcp") != P_MAX_INDEX) m_sipProto = "tcp";
  }
}
