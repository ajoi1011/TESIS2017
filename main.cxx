/*
 * main.cxx
 *
 * Project main program
 *
 */

#include "precompile.h"
#include "main.h"
#include "custom.h"
#include "html.h"

#define PATH_SEPARATOR   "/"
#define SYS_RESOURCE_DIR "/home/ajoi1011/proyecto/project/resource"

PCREATE_PROCESS(MyProcess);
const WORD DefaultHTTPPort = 1719;

MyProcess::MyProcess()
  : MyProcessHTTP(ProductInfo)
  , m_manager(NULL)
  , m_pageConfigure(NULL)
{
}

void MyProcess::Main()
{
  Suspend();
}

MyProcess::~MyProcess()
{
  delete m_manager;
}

PBoolean MyProcess::OnStart()
{
  m_manager = new MyManager();
  new MyPCSSEndPoint(*m_manager);
  m_manager->Initialise(GetArguments(), true, OPAL_PREFIX_MIXER":<du>");

  return PHTTPServiceProcess::OnStart();
}

void MyProcess::OnStop()
{
  PHTTPServiceProcess::OnStop();
}

void MyProcess::OnControl()
{
}

void MyProcess::OnConfigChanged()
{
}

bool MyProcess::InitialiseBase(Params & params)
{
  if (!PAssert(params.m_usernameKey != NULL && params.m_passwordKey != NULL, PInvalidParameter))
    return false;

  PConfig cfg(params.m_section);

  // Get the HTTP basic authentication info
  params.m_authority = PHTTPSimpleAuth(cfg.GetString(params.m_realmKey, params.m_authority.GetLocalRealm()),
                                       cfg.GetString(params.m_usernameKey, params.m_authority.GetUserName()),
                                       PHTTPPasswordField::Decrypt(cfg.GetString(params.m_passwordKey)));

  if (params.m_configPage == NULL && params.m_configPageName != NULL) {
    // Create the parameters URL page, and start adding fields to it
    params.m_configPage = new PConfigPage(*this, params.m_configPageName, params.m_section, params.m_authority);
    m_httpNameSpace.AddResource(params.m_configPage, PHTTPSpace::Overwrite);
  }


  PSystemLog::Level level;
  PString fileName;
  PSystemLogToFile::RotateInfo info(GetHomeDirectory());

  PSystemLogToFile * logFile = dynamic_cast<PSystemLogToFile *>(&PSystemLog::GetTarget());

  if (logFile != NULL)
    info = logFile->GetRotateInfo();

  if (params.m_configPage != NULL) {
    params.m_configPage->AddStringField(params.m_usernameKey, 30, params.m_authority.GetUserName(), "User name to access HTTP user interface for server.");
    params.m_configPage->Add(new PHTTPPasswordField(params.m_passwordKey, 30, params.m_authority.GetPassword()));

    level = PSystemLog::LevelFromInt(
                params.m_configPage->AddIntegerField(params.m_levelKey,
                                               PSystemLog::Fatal, PSystemLog::NumLogLevels-1,
                                               GetLogLevel(),
                                               "", "0=Fatal only, 1=Errors, 2=Warnings, 3=Info, 4=Debug, 5=Detailed"));
    fileName = params.m_configPage->AddStringField(params.m_fileKey, 0, logFile != NULL ? logFile->GetFilePath() : PString::Empty(),
                                             "File for logging output, empty string disables logging", 1, 30);
    info.m_directory = params.m_configPage->AddStringField(params.m_rotateDirKey, 0, info.m_directory,
                                                     "Directory path for log file rotation", 1, 30);
    info.m_maxSize = params.m_configPage->AddIntegerField(params.m_rotateSizeKey, 0, INT_MAX, info.m_maxSize / 1000,
                                                    "kb", "Size of log file to trigger rotation, zero disables")*1000;
    info.m_maxFileCount = params.m_configPage->AddIntegerField(params.m_rotateCountKey, 0, 10000, info.m_maxFileCount,
                                                         "", "Number of rotated log file to keep, zero is infinite");
    info.m_maxFileAge.SetInterval(0, 0, 0, 0,
                     params.m_configPage->AddIntegerField(params.m_rotateAgeKey,
                                                    0, 10000, info.m_maxFileAge.GetDays(),
                                                    "days", "Number of days to keep rotated log files, zero is infinite"));

  // HTTP Port number to use.
    params.m_httpPort = (WORD)params.m_configPage->AddIntegerField(params.m_httpPortKey, 1, 65535, params.m_httpPort,
                                                                   "", "Port for HTTP user interface for server.");
    params.m_httpInterfaces = params.m_configPage->AddStringField(params.m_httpInterfacesKey, 30, params.m_httpInterfaces,
                                                                 "Local network interface(s) for HTTP user interface for server.");
  }
  else {
    level = GetLogLevel();
    fileName = cfg.GetString(params.m_fileKey);
    info.m_directory = cfg.GetString(params.m_rotateDirKey, info.m_directory);
    info.m_maxSize = cfg.GetInteger(params.m_rotateSizeKey, info.m_maxSize / 1000) * 1000;
    info.m_maxFileCount = cfg.GetInteger(params.m_rotateCountKey, info.m_maxFileCount);
    info.m_maxFileAge.SetInterval(0, 0, 0, 0, cfg.GetInteger(params.m_rotateAgeKey, info.m_maxFileAge.GetDays()));
    params.m_httpPort = (WORD)cfg.GetInteger(params.m_httpPortKey,params.m_httpPort);
    params.m_httpInterfaces = cfg.GetString(params.m_httpInterfacesKey, params.m_httpInterfaces);
  }

  SetLogLevel(level);

  if (fileName.IsEmpty()) {
    if (logFile != NULL) {
      logFile = NULL;
      PSystemLog::SetTarget(new PSystemLogToNowhere);
    }
  }
  else {
    if (logFile == NULL || logFile->GetFilePath() != fileName) {
      logFile = new PSystemLogToFile(fileName);
      PSystemLog::SetTarget(logFile);
    }
  }

  if (logFile != NULL) {
    logFile->SetRotateInfo(info, params.m_forceRotate);

    if (params.m_fullLogPageName != NULL) {
      params.m_fullLogPage = new PHTTPFile(params.m_fullLogPageName, logFile->GetFilePath(), PMIMEInfo::TextPlain(), params.m_authority);
      m_httpNameSpace.AddResource(params.m_fullLogPage, PHTTPSpace::Overwrite);
    }

    if (params.m_clearLogPageName != NULL) {
      params.m_clearLogPage = new ClearLogPage(*this, params.m_clearLogPageName, params.m_authority);
      m_httpNameSpace.AddResource(params.m_clearLogPage, PHTTPSpace::Overwrite);
    }

    if (params.m_tailLogPageName != NULL) {
      params.m_tailLogPage = new PHTTPTailFile(params.m_tailLogPageName, logFile->GetFilePath(), PMIMEInfo::TextPlain(), params.m_authority);
      m_httpNameSpace.AddResource(params.m_tailLogPage, PHTTPSpace::Overwrite);
    }
  }
  
  return true;
  
}

PBoolean MyProcess::Initialise(const char * initMsg)
{
  PSYSTEMLOG(Warning, "Service " << GetName() << ' ' << initMsg);

  Params params("Parameters");
  params.m_httpPort = DefaultHTTPPort;
  if (!InitialiseBase(params))
    return false;

  // Configure the core of the system
  PConfig cfg(params.m_section);
  if (!m_manager->Configure(cfg, params.m_configPage))
    return false;

  // Finished the resource to add, generate HTML for it and add to name space
  PHTML cfgHTML;
  m_pageConfigure = (MyPConfigPage*)params.m_configPage;
  m_pageConfigure->BuildHTML(cfgHTML);
  
  PHTTPMultiSimpAuth authSettings(GetName());
  PHTTPMultiSimpAuth authConference(GetName());

 
  m_httpNameSpace.AddResource(new InvitePage(*this, authConference), PHTTPSpace::Overwrite);
  m_httpNameSpace.AddResource(new CallStatusPage(*m_manager, params.m_authority), PHTTPSpace::Overwrite);
  m_httpNameSpace.AddResource(new SelectRoomPage(*this, authConference), PHTTPSpace::Overwrite);
  m_httpNameSpace.AddResource(new WelcomePage(*this, authConference), PHTTPSpace::Overwrite);
  m_httpNameSpace.AddResource(new CDRListPage(*m_manager, params.m_authority), PHTTPSpace::Overwrite);
  m_httpNameSpace.AddResource(new CDRPage(*m_manager, params.m_authority), PHTTPSpace::Overwrite);
  

#ifdef SYS_RESOURCE_DIR
#  define WEBSERVER_LINK(r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, PString(SYS_RESOURCE_DIR) + PATH_SEPARATOR + r1), PHTTPSpace::Overwrite)
#  define WEBSERVER_LINK_MIME(mt1,r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, PString(SYS_RESOURCE_DIR) + PATH_SEPARATOR + r1, mt1), PHTTPSpace::Overwrite)
#  define WEBSERVER_LINK_MIME_CFG(mt1,r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, PString(SYS_CONFIG_DIR) + PATH_SEPARATOR + r1, mt1), PHTTPSpace::Overwrite)
#else
#  define WEBSERVER_LINK(r1) m_httpNameSpace.AddResource(new PHTTPFile(r1), PHTTPSpace::Overwrite)
#  define WEBSERVER_LINK_MIME(mt1,r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, r1, mt1), PHTTPSpace::Overwrite)
#  define WEBSERVER_LINK_MIME_CFG(mt1,r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, r1, mt1), PHTTPSpace::Overwrite)
#endif
#define WEBSERVER_LINK_LOGS(mt1,r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, PString(SERVER_LOGS) + PATH_SEPARATOR + r1, mt1), PHTTPSpace::Overwrite)
  WEBSERVER_LINK_MIME("text/javascript"          , "locale_en.js");
  WEBSERVER_LINK_MIME("text/css"                 , "main.css");
  WEBSERVER_LINK_MIME("text/css"                 , "bootstrap.css");
  WEBSERVER_LINK_MIME("image/png"                , "openmcu.ru_logo_text.png");
  
  // set up the HTTP port for listening & start the first HTTP thread
  if (ListenForHTTP(DefaultHTTPPort))
   PSYSTEMLOG(Info, "Opened master socket for HTTP: " << m_httpListeningSockets.front().GetPort());
  /*else {
    PSYSTEMLOG(Fatal, "Cannot run without HTTP port: " *//*<< httpListeningSocket->GetErrorText());
    return false;
  }*/

  //PSYSTEMLOG(Info, "Service " << GetName() << ' ' << initMsg);
  return true;
}

void MyProcess::CreateHTTPResource(const PString & name)
{
  // Get the HTTP authentication info
  /*MCUConfig("Managing Groups").SetString("administrator", "");
  MCUConfig("Managing Groups").SetString("conference manager", "");

  PHTTPMultiSimpAuth authSettings(GetName());
  PHTTPMultiSimpAuth authConference(GetName());
  PStringList keysUsers = MCUConfig("Managing Users").GetKeys();
  for(PINDEX i = 0; i < keysUsers.GetSize(); i++)
  {
    PStringArray params = MCUConfig("Managing Users").GetString(keysUsers[i]).Tokenise(",");
    if(params.GetSize() < 2) continue;
    if(params[1] == "administrator")
    {
      authSettings.AddUser(keysUsers[i], PHTTPPasswordField::Decrypt(params[0]));
      authConference.AddUser(keysUsers[i], PHTTPPasswordField::Decrypt(params[0]));
    }
    if(params[1] == "conference manager")
    {
      authConference.AddUser(keysUsers[i], PHTTPPasswordField::Decrypt(params[0]));
    }
  }

  if(name == "Parameters")
    httpNameSpace.AddResource(new GeneralPConfigPage(*this, name, "Parameters", authSettings), PHTTPSpace::Overwrite);
  else if(name == "ConferenceParameters")
    httpNameSpace.AddResource(new ConferencePConfigPage(*this, name, "Conference Parameters", authSettings), PHTTPSpace::Overwrite);
  else if(name == "ExportParameters")
    httpNameSpace.AddResource(new ExportPConfigPage(*this, name, "Export Parameters", authSettings), PHTTPSpace::Overwrite);
  else if(name == "RegistrarParameters")
    httpNameSpace.AddResource(new RegistrarPConfigPage(*this, name, "Registrar Parameters", authSettings), PHTTPSpace::Overwrite);
  else if(name == "ManagingUsers")
    httpNameSpace.AddResource(new ManagingUsersPConfigPage(*this, name, "Managing Users", authSettings), PHTTPSpace::Overwrite);
  else if(name == "ManagingGroups")
    httpNameSpace.AddResource(new ManagingGroupsPConfigPage(*this, name, "Managing Groups", authSettings), PHTTPSpace::Overwrite);
  else if(name == "ControlCodes")
    httpNameSpace.AddResource(new ControlCodesPConfigPage(*this, name, "Control Codes", authSettings), PHTTPSpace::Overwrite);
  //else if(name == "RoomCodes")
  //  httpNameSpace.AddResource(new RoomCodesPConfigPage(*this, name, "Room Codes", authSettings), PHTTPSpace::Overwrite);
  else if(name == "TelnetServer")
    httpNameSpace.AddResource(new TelnetServerPConfigPage(*this, name, "Telnet Server", authSettings), PHTTPSpace::Overwrite);
  else if(name == "H323Parameters")
    httpNameSpace.AddResource(new H323PConfigPage(*this, name, "H323 Parameters", authSettings), PHTTPSpace::Overwrite);
  else if(name == "SIPParameters")
    httpNameSpace.AddResource(new SIPPConfigPage(*this, name, "SIP Parameters", authSettings), PHTTPSpace::Overwrite);
  else if(name == "H323EndpointsParameters")
    httpNameSpace.AddResource(new H323EndpointsPConfigPage(*this, name, "H323 Endpoints", authSettings), PHTTPSpace::Overwrite);
  else if(name == "SipEndpointsParameters")
    httpNameSpace.AddResource(new SipEndpointsPConfigPage(*this, name, "SIP Endpoints", authSettings), PHTTPSpace::Overwrite);
  else if(name == "RtspParameters")
    httpNameSpace.AddResource(new RtspPConfigPage(*this, name, "RTSP Parameters", authSettings), PHTTPSpace::Overwrite);
  else if(name == "RtspServers")
    httpNameSpace.AddResource(new RtspServersPConfigPage(*this, name, "RTSP Servers", authSettings), PHTTPSpace::Overwrite);
  else if(name == "RtspEndpoints")
    httpNameSpace.AddResource(new RtspEndpointsPConfigPage(*this, name, "RTSP Endpoints", authSettings), PHTTPSpace::Overwrite);
  else if(name == "VideoParameters")
    httpNameSpace.AddResource(new VideoPConfigPage(*this, name, "Video", authSettings), PHTTPSpace::Overwrite);
  //else if(name == "RoomAccess")
  //  httpNameSpace.AddResource(new RoomAccessSIPPConfigPage(*this, name, "RoomAccess", authSettings), PHTTPSpace::Overwrite);
  else if(name == "ProxyServers")
    httpNameSpace.AddResource(new ProxySIPPConfigPage(*this, name, "ProxyServers", authSettings), PHTTPSpace::Overwrite);
  else if(name == "ReceiveSoundCodecs")
    httpNameSpace.AddResource(new CodecsPConfigPage(*this, name, "RECEIVE_SOUND", authSettings), PHTTPSpace::Overwrite);
  else if(name == "TransmitSoundCodecs")
    httpNameSpace.AddResource(new CodecsPConfigPage(*this, name, "TRANSMIT_SOUND", authSettings), PHTTPSpace::Overwrite);
  else if(name == "ReceiveVideoCodecs")
    httpNameSpace.AddResource(new CodecsPConfigPage(*this, name, "RECEIVE_VIDEO", authSettings), PHTTPSpace::Overwrite);
  else if(name == "TransmitVideoCodecs")
    httpNameSpace.AddResource(new CodecsPConfigPage(*this, name, "TRANSMIT_VIDEO", authSettings), PHTTPSpace::Overwrite);
  else if(name == "SipSoundCodecs")
    httpNameSpace.AddResource(new SIPCodecsPConfigPage(*this, name, "SIP Audio", authSettings), PHTTPSpace::Overwrite);
  else if(name == "SipVideoCodecs")
    httpNameSpace.AddResource(new SIPCodecsPConfigPage(*this, name, "SIP Video", authSettings), PHTTPSpace::Overwrite);

  else if(name == "Status")
    httpNameSpace.AddResource(new MainStatusPage(*this, authConference), PHTTPSpace::Overwrite);
  else if(name == "Invite")
    httpNameSpace.AddResource(new InvitePage(*this, authConference), PHTTPSpace::Overwrite);
  else if(name == "Select")
    httpNameSpace.AddResource(new SelectRoomPage(*this, authConference), PHTTPSpace::Overwrite);
  else if(name == "Records")
    httpNameSpace.AddResource(new RecordsBrowserPage(*this, authConference), PHTTPSpace::Overwrite);
  else if(name == "Jpeg")
    httpNameSpace.AddResource(new JpegFrameHTTP(*this, authConference), PHTTPSpace::Overwrite);
  else if(name == "Comm")
    httpNameSpace.AddResource(new InteractiveHTTP(*this, authConference), PHTTPSpace::Overwrite);

  else if(name == "welcome.html")
    httpNameSpace.AddResource(new WelcomePage(*this, authConference), PHTTPSpace::Overwrite);
  else if(name == "monitor.txt")
  {
    PString monitorText =
#ifdef GIT_REVISION
                        (PString(PRODUCT_NAME_TEXT) + " REVISION " + MCU_STRINGIFY(GIT_REVISION) +"\n\n") +
#endif
                        "<!--#equival monitorinfo-->"
                        "<!--#equival mcuinfo-->";
    httpNameSpace.AddResource(new PServiceHTTPString("monitor.txt", monitorText, "text/plain", authConference), PHTTPSpace::Overwrite);
  }*/

  // Add log file links
/*
  if (!systemLogFileName && (systemLogFileName != "-")) {
    httpNameSpace.AddResource(new PHTTPFile("logfile.txt", systemLogFileName, authority));
    httpNameSpace.AddResource(new PHTTPTailFile("tail_logfile", systemLogFileName, authority));
  }
*/

}


///////////////////////////////////////////////////////////////////////////////////////////////////

void MyPConfigPage::BuildHTML(PHTML & html, BuildOptions option)
{
  PStringStream html_begin, html_end, html_page, meta_page;
  html.Set(PHTML::InBody);
  html <<PHTML::Form("POST");
  html << PHTML::TableStart("cellspacing=8; align='center' style='border:1px solid black;'");
  for (PINDEX fld = 0; fld < m_fields.GetSize(); fld++) {
    PHTTPField & field = m_fields[fld];
    bool isDivider = dynamic_cast<PHTTPDividerField *>(&field) != NULL;
    if (field.NotYetInHTML()) {
      html <<PHTML::TableRow("align='left' style='background-color:#d9e5e3; padding:0px 4px 0px 4px; border-bottom:1px solid black;'")
           << PHTML::TableData("align='middle' style='background-color:#d9e5e3; padding:0px; border-right:1px solid black;'");
      if (isDivider)
        html << PHTML::HRule();
      else
        html <<PHTML::Escaped(field.GetTitle());
      html << PHTML::TableData("align='left' style='background-color:#d9e5e3; padding:5px; border-right:1px solid black;'")
           << "<!--#form html " << field.GetName() << "-->"
           << PHTML::TableData("align='left' style='background-color:#d9e5e3; padding:5px; border-right:1px solid black;'");
      if (isDivider)
        html << PHTML::HRule();
      else
        html << field.GetHelp();
      field.SetInHTML();
    }
  }
  html << PHTML::TableEnd();
  if (option != InsertIntoForm)
    html << PHTML::Paragraph()
         << ' ' << PHTML::SubmitButton("Accept")
         << ' ' << PHTML::ResetButton("Reset")
         << PHTML::Form();

  if (option == CompleteHTML) {
    BeginPage(html_begin, meta_page, "Parametros", "window.l_param_general","window.l_info_param_general");
    EndPage(html_end, MyProcess::Current().GetHtmlCopyright());
    html_page << html_begin << html << html_end;
    m_string = html_page;
  }
}  

// End of File ////////////////////////////////////////////////////////////////////////////////////
