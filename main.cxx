
#include "config.h"
#include "custom.h"
#include "html.h"
#include "main.h"
#include "precompile.h"

PCREATE_PROCESS(MyProcess);

MyProcess::MyProcess()
  : PHTTPServiceProcess(ProductInfo)
  , m_manager(NULL)
  , m_pageConfigure(NULL)
{
}

MyProcess::~MyProcess()
{
  delete m_manager;
}

PBoolean MyProcess::Initialise(const char * initMsg)
{
  PSYSTEMLOG(Warning, "Service " << GetName() << ' ' << initMsg);

  Params params("Parametros");
  params.m_httpPort = DefaultHTTPPort;
  if (!InitialiseBase(params))
    return false;

  PConfig cfg(params.m_section);
  if (!m_manager->Configure(cfg, params.m_configPage))
    return false;

  PHTML cfgHTML;
  m_pageConfigure = (MyPConfigPage*)params.m_configPage;
  m_pageConfigure->BuildHTML(cfgHTML);
  
  CreateHTTPResource("Invite");
  CreateHTTPResource("CallStatus");
  CreateHTTPResource("Select");
  CreateHTTPResource("HomePage");
  CreateHTTPResource("CallDetailRecords");
  CreateHTTPResource("CallDetailRecord");
  CreateHTTPResource("RegistrationStatus");
  CreateHTTPResource("RegistrarStatus");
  CreateHTTPResource("GkStatus");
  
  //Definición tomada de OpenMCU-ru
#ifdef SYS_RESOURCE_DIR
#  define WEBSERVER_LINK(r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, PString(SYS_RESOURCE_DIR) + PATH_SEPARATOR + r1), PHTTPSpace::Overwrite)
#  define WEBSERVER_LINK_MIME(mt1,r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, PString(SYS_RESOURCE_DIR) + PATH_SEPARATOR + r1, mt1), PHTTPSpace::Overwrite)
#  define WEBSERVER_LINK_MIME_CFG(mt1,r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, PString(SYS_CONFIG_DIR) + PATH_SEPARATOR + r1, mt1), PHTTPSpace::Overwrite)
#else
#  define WEBSERVER_LINK(r1) m_httpNameSpace.AddResource(new PHTTPFile(r1), PHTTPSpace::Overwrite)
#  define WEBSERVER_LINK_MIME(mt1,r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, r1, mt1), PHTTPSpace::Overwrite)
#  define WEBSERVER_LINK_MIME_CFG(mt1,r1) m_httpNameSpace.AddResource(new PHTTPFile(r1, r1, mt1), PHTTPSpace::Overwrite)
#endif
  WEBSERVER_LINK_MIME("text/javascript", "locale_en.js");
  WEBSERVER_LINK_MIME("text/css",  "main.css");
  WEBSERVER_LINK_MIME("text/css",  "bootstrap.css");
  WEBSERVER_LINK_MIME("image/png", "openmcu.ru_logo_text.png");
  
  if (ListenForHTTP(params.m_httpPort))
    PSYSTEMLOG(Info, "Opened master socket for HTTP: " << m_httpListeningSockets.front().GetPort());
  else {
    PSYSTEMLOG(Fatal, "Cannot run without HTTP port.");
    return false;
  }

  PSYSTEMLOG(Info, "Completed configuring service " << GetName() << " (" << initMsg << ')');

  return true;
}

bool MyProcess::InitialiseBase(Params & params)
{
  if (!PAssert(UserNameKey != NULL && PasswordKey != NULL, PInvalidParameter))
    return false;

  PConfig cfg(params.m_section);

  params.m_authority = PHTTPSimpleAuth(cfg.GetString(params.m_realmKey, params.m_authority.GetLocalRealm()), 
                                       cfg.GetString(UserNameKey, params.m_authority.GetUserName()), 
                                       PHTTPPasswordField::Decrypt(cfg.GetString(PasswordKey)));

  if (params.m_configPage == NULL && params.m_configPageName != NULL) {
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
    params.m_configPage->AddStringField(UserNameKey, 30, params.m_authority.GetUserName(), 
                                        "Nombre de usuario para acceder al servidor.");

    params.m_configPage->Add(new PHTTPPasswordField(PasswordKey, 30, params.m_authority.GetPassword(), 
                                                    "Contraseña para acceder al servidor."));

    level = PSystemLog::LevelFromInt(params.m_configPage->AddIntegerField(LevelKey, PSystemLog::Fatal, 
                                                                          PSystemLog::NumLogLevels-1, GetLogLevel(), 
                                                                          "", "0=Fatal, 1=Errores, 2=Alertas, 3=Info, 4=Debug, 5=Detallado."));

    fileName = params.m_configPage->AddStringField(FileKey, 0, 
                                                  logFile != NULL ? logFile->GetFilePath() : PString::Empty(), 
                                                  "Archivo de salida para trazo log.", 1, 30);

    info.m_directory = params.m_configPage->AddStringField(RotateDirKey, 0, info.m_directory, 
                                                          "Ruta del directorio para archivo de trazo log.", 1, 30);

    info.m_prefix = params.m_configPage->AddStringField(RotatePrefixKey, 0, info.m_prefix, 
                                                        "Prefijo para archivo de trazo log.", 1, 30);

    info.m_timestamp = params.m_configPage->AddStringField(RotateTemplateKey, 0, info.m_timestamp, 
                                                           "Formato de tiempo para archivo de trazo log.", 1, 30);

    info.m_maxSize = params.m_configPage->AddIntegerField(RotateSizeKey, 0, INT_MAX, info.m_maxSize / 1000, "kb", 
                                                          "Tamaño del archivo de trazo log.")*1000;

    info.m_maxFileCount = params.m_configPage->AddIntegerField(RotateCountKey, 0, 10000, info.m_maxFileCount, "", 
                                                               "Número de archivos de trazo log a guardar, cero indica un número indefinido.");

    info.m_maxFileAge.SetInterval(0, 0, 0, 0, params.m_configPage->AddIntegerField(RotateAgeKey, 0, 10000,
                                                                                   info.m_maxFileAge.GetDays(), 
                                                                                   "días", "Número de días para guardar trazos log en el historial, cero indica un número indefinido."));

    params.m_httpPort = (WORD)params.m_configPage->AddIntegerField(HTTPPortKey, 1, 65535, params.m_httpPort, "", 
                                                                   "Puerto HTTP para acceder al servidor.");

    params.m_httpInterfaces = params.m_configPage->AddStringField(HTTPInterfacesKey, 30, params.m_httpInterfaces, 
                                                                  "Interfaces HTTP para acceder al servidor.");
  }
  else {
    level = GetLogLevel();
    fileName = cfg.GetString(FileKey);
    info.m_directory = cfg.GetString(RotateDirKey, info.m_directory);
    info.m_maxSize = cfg.GetInteger(RotateSizeKey, info.m_maxSize / 1000) * 1000;
    info.m_maxFileCount = cfg.GetInteger(RotateCountKey, info.m_maxFileCount);
    info.m_maxFileAge.SetInterval(0, 0, 0, 0, cfg.GetInteger(RotateAgeKey, info.m_maxFileAge.GetDays()));
    params.m_httpPort = (WORD)cfg.GetInteger(HTTPPortKey,params.m_httpPort);
    params.m_httpInterfaces = cfg.GetString(HTTPInterfacesKey, params.m_httpInterfaces);
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
      params.m_fullLogPage = new PHTTPFile(params.m_fullLogPageName, logFile->GetFilePath(), PMIMEInfo::TextPlain(), 
                                           params.m_authority);
      
      m_httpNameSpace.AddResource(params.m_fullLogPage, PHTTPSpace::Overwrite);
    }

    if (params.m_clearLogPageName != NULL) {
      params.m_clearLogPage = new ClearLogPage(*this, params.m_clearLogPageName, params.m_authority);
      m_httpNameSpace.AddResource(params.m_clearLogPage, PHTTPSpace::Overwrite);
    }

    if (params.m_tailLogPageName != NULL) {
      params.m_tailLogPage = new PHTTPTailFile(params.m_tailLogPageName, logFile->GetFilePath(), PMIMEInfo::TextPlain(), 
                                               params.m_authority);
                                               
      m_httpNameSpace.AddResource(params.m_tailLogPage, PHTTPSpace::Overwrite);
    }
  }
  
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

  m_manager = new MyManager();
#if OPAL_HAS_PCSS
  new MyPCSSEndPoint(*m_manager);
#endif // OPAL_HAS_PCSS

  m_manager->Initialise(GetArguments(), true, OPAL_PREFIX_MIXER":<du>");

  return PHTTPServiceProcess::OnStart();
}

void MyProcess::OnStop()
{
  if (m_manager != NULL)
    m_manager->ShutDownEndpoints();

  PHTTPServiceProcess::OnStop();
}

void MyProcess::Main()
{
  Suspend();
}

void MyProcess::CreateHTTPResource(const PString & name)
{
  PHTTPMultiSimpAuth authConference(GetName());

  if (name == "CallStatus")
    m_httpNameSpace.AddResource(new CallStatusPage(GetManager(), authConference), PHTTPSpace::Overwrite);
  else if (name == "Invite")
    m_httpNameSpace.AddResource(new InvitePage(*this, authConference), PHTTPSpace::Overwrite);
  else if (name == "Select")
    m_httpNameSpace.AddResource(new SelectRoomPage(*this, authConference), PHTTPSpace::Overwrite);
  else if (name == "CallDetailRecords")
    m_httpNameSpace.AddResource(new CDRListPage(GetManager(), authConference), PHTTPSpace::Overwrite);
  else if (name == "CallDetailRecord")
    m_httpNameSpace.AddResource(new CDRPage(GetManager(), authConference), PHTTPSpace::Overwrite);
  else if (name == "HomePage")
    m_httpNameSpace.AddResource(new HomePage(*this, authConference), PHTTPSpace::Overwrite);
  else if (name == "RegistrationStatus")
    m_httpNameSpace.AddResource(new RegistrationStatusPage(GetManager(), authConference), PHTTPSpace::Overwrite);
#if OPAL_SIP
  else if (name == "RegistrarStatus")
    m_httpNameSpace.AddResource(new RegistrarStatusPage(GetManager(), authConference), PHTTPSpace::Overwrite);
#endif // OPAL_SIP
#if OPAL_H323
  else if (name == "GkStatus")
    m_httpNameSpace.AddResource(new GkStatusPage(GetManager(), authConference), PHTTPSpace::Overwrite);
#endif // OPAL_H323

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MyPConfigPage::BuildHTML(PHTML & html, BuildOptions option)
{
  PStringStream html_begin, html_end, html_page, meta;
  
  html.Set(PHTML::InBody);
  html << PHTML::Form("POST")
       << PHTML::TableStart("style='border:1px solid black;'");
  for (PINDEX fld = 0; fld < m_fields.GetSize(); fld++) {
    PHTTPField & field = m_fields[fld];
    bool isDivider = dynamic_cast<PHTTPDividerField *>(&field) != NULL;
    if (field.NotYetInHTML()) {
      html << PHTML::TableRow("style='border-bottom:1px solid black;'")
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
         << ' ' << PHTML::SubmitButton("Aceptar")
         << ' ' << PHTML::ResetButton("Reset")
         << PHTML::Form();

  if (option == CompleteHTML) {
    BeginPage(html_begin, meta, "Parámetros", "window.l_param_general","window.l_info_param_general");
    EndPage(html_end, MyProcess::Current().GetHtmlCopyright());
    html_page << html_begin << html << html_end;
    m_string = html_page;
  }

} // Final del Archivo