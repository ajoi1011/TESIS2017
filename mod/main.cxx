


#include "main.h"
#include "custom.h"
#include "html.h"

int MyProcess::defaultRoomCount = 5;

VideoMixConfigurator MyProcess::vmcfg;
PCREATE_PROCESS(MyProcess);

MyProcess::MyProcess()
 : PHTTPServiceProcess (ProductInfo)
 , m_manager(NULL)
{
  m_manager = new OpalManager();
  manager = new ConferenceManager(*m_manager);
  
}

MyProcess::~MyProcess()
{
}

PBoolean MyProcess::Initialise(const char * initMsg)
{
  PSYSTEMLOG(Warning, "Service " << GetName() << ' ' << initMsg);

  Params params("Parameters");
  params.m_httpPort = DefaultHTTPPort;
  if (!InitialiseBase(params))
    return false;
  
  vmcfg.go(vmcfg.bfw,vmcfg.bfh);
  CreateHTTPResource("HomePage");
  CreateHTTPResource("Control");
  CreateHTTPResource("Comm");
  CreateHTTPResource("Jpeg");
  
  httpBuffer=100;
  httpBufferedEvents.SetSize(httpBuffer);
  httpBufferIndex=0; httpBufferComplete=0;
  
  // Definiciones implementadas en MyProcess-ru
#ifdef SYS_RESOURCE_DIR
#  define WEBSERVER_LINK(r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, PString(SYS_RESOURCE_DIR) + PATH_SEPARATOR + r1), PHTTPSpace::Overwrite)
#  define WEBSERVER_LINK_MIME(mt1,r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, PString(SYS_RESOURCE_DIR) + PATH_SEPARATOR + r1, mt1), PHTTPSpace::Overwrite)
#  define WEBSERVER_LINK_MIME_CFG(mt1,r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, PString(SYS_CONFIG_DIR) + PATH_SEPARATOR + r1, mt1), PHTTPSpace::Overwrite)
#else
#  define WEBSERVER_LINK(r1) m_httpNameSpace.AddResource(new PHTTPFile(r1), PHTTPSpace::Overwrite)
#  define WEBSERVER_LINK_MIME(mt1,r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, r1, mt1), PHTTPSpace::Overwrite)
#  define WEBSERVER_LINK_MIME_CFG(mt1,r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, r1, mt1), PHTTPSpace::Overwrite)
#endif
  WEBSERVER_LINK_MIME("text/javascript", "control.js");
  WEBSERVER_LINK_MIME("text/javascript", "locale_en.js");
  WEBSERVER_LINK_MIME("text/css",  "main.css");
  WEBSERVER_LINK_MIME("text/css",  "bootstrap.css");
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
  
 
  if (ListenForHTTP(params.m_httpPort))
    PSYSTEMLOG(Info, "Opened master socket for HTTP: " << m_httpListeningSockets.front().GetPort());
  else {
    PSYSTEMLOG(Fatal, "Cannot run without HTTP port.");
    return false;
  }

  PSYSTEMLOG(Info, "Completed configuring service " << GetName() << " (" << initMsg << ')');

  return true;
}

void MyProcess::OnConfigChanged()
{
}

void MyProcess::OnControl()
{
}

PBoolean MyProcess::OnStart()
{
  Params params(NULL);
  params.m_forceRotate = true;
  InitialiseBase(params);  

  return PHTTPServiceProcess::OnStart();
}

void MyProcess::OnStop()
{
  PHTTPServiceProcess::OnStop();
}

void MyProcess::Main()
{
  Suspend();
}

void MyProcess::CreateHTTPResource(const PString & name)
{
  PHTTPMultiSimpAuth authConference(GetName());

  /*if (name == "CallDetailRecord")
    m_httpNameSpace.AddResource(new CDRPage(GetManager(), authConference), PHTTPSpace::Overwrite);
  else if (name == "CallDetailRecords")
    m_httpNameSpace.AddResource(new CDRListPage(GetManager(), authConference), PHTTPSpace::Overwrite);
  else if (name == "CallStatus")
    m_httpNameSpace.AddResource(new CallStatusPage(GetManager(), authConference), PHTTPSpace::Overwrite);
  else*/ if (name == "Control")
    m_httpNameSpace.AddResource(new ControlRoomPage(*this, authConference), PHTTPSpace::Overwrite);
  else if (name == "Comm")
    m_httpNameSpace.AddResource(new InteractiveHTTP(*this, authConference), PHTTPSpace::Overwrite);
  else if (name == "Jpeg")
    m_httpNameSpace.AddResource(new JpegFrameHTTP(*this, authConference), PHTTPSpace::Overwrite);
/*#if OPAL_H323
  else if (name == "GkStatus")
    m_httpNameSpace.AddResource(new GkStatusPage(GetManager(), authConference), PHTTPSpace::Overwrite);
#endif // OPAL_H323
  else */if (name == "HomePage")
    m_httpNameSpace.AddResource(new HomePage(*this, authConference), PHTTPSpace::Overwrite);
  /*else if (name == "Invite")
    m_httpNameSpace.AddResource(new InvitePage(*this, authConference), PHTTPSpace::Overwrite);
#if OPAL_SIP
  else if (name == "RegistrarStatus")
    m_httpNameSpace.AddResource(new RegistrarStatusPage(GetManager(), authConference), PHTTPSpace::Overwrite);
#endif // OPAL_SIP
  else if (name == "RegistrationStatus")
    m_httpNameSpace.AddResource(new RegistrationStatusPage(GetManager(), authConference), PHTTPSpace::Overwrite);*/
}

void MyProcess::LogMessage(const PString & str)
{
  static PMutex logMutex;
  static PTextFile logFile;

  PTime now;
  PString msg = now.AsString("dd/MM/yyyy") & str;
  logMutex.Wait();

  if (!logFile.IsOpen()) {
    if(!logFile.Open(logFilename, PFile::ReadWrite))
    {
      PTRACE(1,"MyProcess\tCan not open log file: " << logFilename << "\n" << msg << flush);
      logMutex.Signal();
      return;
    }
    if(!logFile.SetPosition(0, PFile::End))
    {
      PTRACE(1,"MyProcess\tCan not change log position, log file name: " << logFilename << "\n" << msg << flush);
      logFile.Close();
      logMutex.Signal();
      return;
    }
  }

  if(!logFile.WriteLine(msg))
  {
    PTRACE(1,"MyProcess\tCan not write to log file: " << logFilename << "\n" << msg << flush);
  }
  logFile.Close();
  logMutex.Signal();
}

void MyProcess::LogMessageHTML(PString str)
{ // de-html :) special for palexa, http://openmcu.ru/forum/index.php/topic,351.msg6240.html#msg6240
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
  BOOL tag=FALSE;
  for (PINDEX i=0; i< str.GetLength(); i++)
  if(str[i]=='<') tag=TRUE;
  else if(str[i]=='>') tag=FALSE;
  else if(!tag) str2+=str[i];
  LogMessage(str2);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MCUURL::MCUURL(PString str)
{
  PINDEX delim1 = str.FindLast("[");
  PINDEX delim2 = str.FindLast("]");
  PINDEX delim3 = str.FindLast("<sip:");
  PINDEX delim4 = str.FindLast(">");

  if(delim3 != P_MAX_INDEX && delim4 != P_MAX_INDEX)
  {
    displayName = str.Left(delim3-1);
    displayName.Replace("\"","",TRUE,0);
    partyUrl = str.Mid(delim3+1, delim4-delim3-1);
  }
  else if(delim1 != P_MAX_INDEX && delim2 != P_MAX_INDEX)
  {
    displayName = str.Left(delim1-1);
    partyUrl = str.Mid(delim1+1, delim2-delim1-1);
  } else {
    partyUrl = str;
  }

  if(partyUrl.Left(4) == "sip:") URLScheme = "sip";
  else if(partyUrl.Left(5) == "h323:") URLScheme = "h323";
  else { partyUrl = "h323:"+partyUrl; URLScheme = "h323"; }

  Parse((const char *)partyUrl, URLScheme);
  // parse old H.323 scheme
  if(URLScheme == "h323" && partyUrl.Left(5) != "h323" && partyUrl.Find("@") == P_MAX_INDEX &&
     m_hostname == "" && m_username != "")
  {
    m_hostname = m_username;
    m_username = "";
  }

  memberName = displayName+" ["+partyUrl+"]";

  urlId = URLScheme+":";
  if(m_username != "") urlId += m_username;
  else               urlId += displayName+"@"+m_hostname;

  if(URLScheme == "sip")
  {
    sipProto = "*";
    if(partyUrl.Find("transport=udp") != P_MAX_INDEX) sipProto = "udp";
    if(partyUrl.Find("transport=tcp") != P_MAX_INDEX) sipProto = "tcp";
  }
}

ExternalVideoRecorderThread::ExternalVideoRecorderThread(const PString & _roomName)
  : roomName(_roomName)
{
  state = 0;

  PStringStream t; t << roomName << "__" // fileName format: room101__2013-0516-1058270__704x576x10
    << PTime().AsString("yyyy-MMdd-hhmmssu", PTime::Local) << "__"
    << MyProcess::Current().vr_framewidth << "x"
    << MyProcess::Current().vr_frameheight << "x"
    << MyProcess::Current().vr_framerate;
  fileName = t;

  t = MyProcess::Current().ffmpegCall;
  t.Replace("%o",fileName,TRUE,0);
  PString audio, video;
#ifdef _WIN32
  audio = "\\\\.\\pipe\\sound_" + roomName;
  video = "\\\\.\\pipe\\video_" + roomName;
#else
#  ifdef SYS_PIPE_DIR
  audio = PString(SYS_PIPE_DIR)+"/sound." + roomName;
  video = PString(SYS_PIPE_DIR)+"/video." + roomName;
#  else
  audio = "sound." + roomName;
  video = "video." + roomName;
#  endif
#endif
  t.Replace("%A",audio,TRUE,0);
  t.Replace("%V",video,TRUE,0);
  PINDEX i;
#ifdef _WIN32
  PString t2; for(i=0;i<t.GetLength();i++) { char c=t[i]; if(c=='\\') t2+='\\'; t2+=c; } t=t2;
#endif
  //if(!ffmpegPipe.Open(t, /* PPipeChannel::ReadWrite */ PPipeChannel::WriteOnly, FALSE, FALSE)) { state=2; return; }
  /*if(!ffmpegPipe.IsOpen()) { state=2; return; }
  for (i=0;i<100;i++) if(ffmpegPipe.IsRunning()) {state=1; break; } else PThread::Sleep(12);
  if(state != 1) { ffmpegPipe.Close(); state=2; return; }
  */
  PTRACE(2,"EVRT\tStarted new external recording thread for room " << roomName << ", CL: " << t);
//  char buf[100]; while(ffmpegPipe.Read(buf, 100)) PTRACE(1,"EVRT\tRead data: " << buf);//<--thread overloaded, never FALSE?
}

ExternalVideoRecorderThread::~ExternalVideoRecorderThread()
{
}

 // Final del Archivo
