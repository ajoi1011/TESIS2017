
#include "html.h"
#include "manager.h"
#include "mixer.h"
#include "h323.h"
#include "sip.h"

static unsigned long html_template_size;
char * html_template_buffer;
char * html_quote_buffer;
PMutex html_mutex;

void BeginPage(PStringStream & html, 
               PStringStream & pmeta, 
               const char * ptitle, 
               const char * title, 
               const char * quotekey)  
{ 
  PWaitAndSignal m(html_mutex);
  
  if (html_template_size <= 0) { 
    FILE * fs = fopen("/home/ajoi1011/proyecto/project/resource/template.html", "r");
    if (fs) { 
      fseek(fs, 0L, SEEK_END); 
      html_template_size = ftell(fs); 
      rewind(fs);
      html_template_buffer = new char[html_template_size + 1];
      if (html_template_size != fread(html_template_buffer, 1, html_template_size, fs)) 
        html_template_size = -1;
      else 
        html_template_buffer[html_template_size] = 0;
      fclose(fs);
    }
    else html_template_size = -1; // read error indicator
  }
 
  if (html_template_size <= 0) { 
    cout << "No se pudo cargar la plantilla html!\n"; 
    PTRACE(1,"WebCtrl\tCan't read HTML template from file"); 
    return; 
  }

  PString lang = "en";//MCUConfig("Parameters").GetString("Language").ToLower();
  
  PString html0(html_template_buffer); 
  html0 = html0.Left(html0.Find("$BODY$"));
  html0.Replace("$LANG$",     lang,     TRUE, 0);
  html0.Replace("$PMETA$",    pmeta,    TRUE, 0);
  html0.Replace("$PTITLE$",   ptitle,   TRUE, 0);
  html0.Replace("$TITLE$",    title,    TRUE, 0);
  html0.Replace("$QUOTE$",    quotekey, TRUE, 0);
  //html0.Replace("$INIT$",     jsInit,   TRUE, 0);
  html << html0;
}

void EndPage(PStringStream & html, PString copyr) 
{
  PWaitAndSignal m(html_mutex);
  
  if (html_template_size <= 0) 
    return;
    
  PString html0(html_template_buffer); 
  html0 = html0.Mid(html0.Find("$BODY$")+6,P_MAX_INDEX);
  html0.Replace("$COPYRIGHT$", copyr,   TRUE, 0);
  html << html0;
}

static PINDEX GetListenerCount(PHTTPRequest & resource, const char * prefix)
{
  RegistrationStatusPage * status = dynamic_cast<RegistrationStatusPage *>(resource.m_resource);
  if (PAssertNULL(status) == NULL)
    return 2;

  OpalEndPoint * ep = status->GetManager().FindEndPoint(prefix);
  if (ep == NULL)
    return 2;

  return ep->GetListeners().GetSize() + 1;
}

static PString GetListenerStatus(PHTTPRequest & resource, const PString htmlBlock, const char * prefix)
{
  PString substitution;

  RegistrationStatusPage * status = dynamic_cast<RegistrationStatusPage *>(resource.m_resource);
  if (PAssertNULL(status) != NULL) {
    OpalEndPoint * ep = status->GetManager().FindEndPoint(prefix);
    if (ep != NULL) {
      const OpalListenerList & listeners = ep->GetListeners();
      for (OpalListenerList::const_iterator it = listeners.begin(); it != listeners.end(); ++it) {
        PString insert = htmlBlock;
        PServiceHTML::SpliceMacro(insert, "status Address", it->GetLocalAddress());
        PServiceHTML::SpliceMacro(insert, "status Status", it->IsOpen() ? "Active" : "Offline");
        substitution += insert;
      }
    }
  }

  if (substitution.IsEmpty()) {
    substitution = htmlBlock;
    PServiceHTML::SpliceMacro(substitution, "status Address", PHTML::GetNonBreakSpace());
    PServiceHTML::SpliceMacro(substitution, "status Status", "Not listening");
  }

  return substitution;
}

static PINDEX GetRegistrationCount(PHTTPRequest & resource, size_t (RegistrationStatusPage::*func)() const)
{
  RegistrationStatusPage * status = dynamic_cast<RegistrationStatusPage *>(resource.m_resource);
  if (PAssertNULL(status) == NULL)
    return 2;

  PINDEX count = (status->*func)();
  if (count == 0)
    return 2;

  return count+1;
}

static PString GetRegistrationStatus(PHTTPRequest & resource,
                                     const PString htmlBlock,
                                     void (RegistrationStatusPage::*func)(RegistrationStatusPage::StatusMap &) const)
{
  PString substitution;

  RegistrationStatusPage * status = dynamic_cast<RegistrationStatusPage *>(resource.m_resource);
  if (PAssertNULL(status) != NULL) {
    RegistrationStatusPage::StatusMap statuses;
    (status->*func)(statuses);
    for (RegistrationStatusPage::StatusMap::const_iterator it = statuses.begin(); it != statuses.end(); ++it) {
      PString insert = htmlBlock;
      PServiceHTML::SpliceMacro(insert, "status Name", it->first);
      PServiceHTML::SpliceMacro(insert, "status Status", it->second);
      substitution += insert;
    }
  }

  if (substitution.IsEmpty()) {
    substitution = htmlBlock;
    PServiceHTML::SpliceMacro(substitution, "status Name", PHTML::GetNonBreakSpace());
    PServiceHTML::SpliceMacro(substitution, "status Status", "Not registered");
  }

  return substitution;
}

#if OPAL_H323
PCREATE_SERVICE_MACRO(H323ListenerCount, resource, P_EMPTY)
{
  return GetListenerCount(resource, OPAL_PREFIX_H323);
}

PCREATE_SERVICE_MACRO_BLOCK(H323ListenerStatus, resource, P_EMPTY, htmlBlock)
{
  return GetListenerStatus(resource, htmlBlock, OPAL_PREFIX_H323);
}

PCREATE_SERVICE_MACRO(H323RegistrationCount, resource, P_EMPTY)
{
  return GetRegistrationCount(resource, &RegistrationStatusPage::GetH323Count);
}

PCREATE_SERVICE_MACRO_BLOCK(H323RegistrationStatus, resource, P_EMPTY, htmlBlock)
{
  return GetRegistrationStatus(resource, htmlBlock, &RegistrationStatusPage::GetH323);
}
#endif // OPAL_H323

#if OPAL_SIP
PCREATE_SERVICE_MACRO(SIPListenerCount, resource, P_EMPTY)
{
  return GetListenerCount(resource, OPAL_PREFIX_SIP);
}

PCREATE_SERVICE_MACRO_BLOCK(SIPListenerStatus, resource, P_EMPTY, htmlBlock)
{
  return GetListenerStatus(resource, htmlBlock, OPAL_PREFIX_SIP);
}

PCREATE_SERVICE_MACRO(SIPRegistrationCount, resource, P_EMPTY)
{
  return GetRegistrationCount(resource, &RegistrationStatusPage::GetSIPCount);
}

PCREATE_SERVICE_MACRO_BLOCK(SIPRegistrationStatus, resource, P_EMPTY, htmlBlock)
{
  return GetRegistrationStatus(resource, htmlBlock, &RegistrationStatusPage::GetSIP);
}
#endif // OPAL_SIP

#if OPAL_PTLIB_NAT
static bool GetSTUN(PHTTPRequest & resource, PSTUNClient * & stun)
{
  RegistrationStatusPage * status = dynamic_cast<RegistrationStatusPage *>(resource.m_resource);
  if (PAssertNULL(status) == NULL)
    return false;

  stun = dynamic_cast<PSTUNClient *>(status->GetManager().GetNatMethods().GetMethodByName(PSTUNClient::MethodName()));
  return stun != NULL;
}

PCREATE_SERVICE_MACRO(STUNServer, resource, P_EMPTY)
{
  PSTUNClient * stun;
  if (!GetSTUN(resource, stun))
    return PString::Empty();

  PHTML html(PHTML::InBody);
  html << stun->GetServer();

  PIPAddressAndPort ap;
  if (stun->GetServerAddress(ap))
    html << PHTML::BreakLine() << ap;

  return html;
}

PCREATE_SERVICE_MACRO(STUNStatus, resource, P_EMPTY)
{
  PSTUNClient * stun;
  if (!GetSTUN(resource, stun))
    return PString::Empty();

  PHTML html(PHTML::InBody);
  PNatMethod::NatTypes type = stun->GetNatType();
  html << type;

  PIPAddress ip;
  if (stun->GetExternalAddress(ip))
    html << PHTML::BreakLine() << ip;

  return html;
}
#endif // OPAL_PTLIB_NAT

PCREATE_SERVICE_MACRO(CallCount, resource, P_EMPTY)
{
  CallStatusPage * status = dynamic_cast<CallStatusPage *>(resource.m_resource);
  return PAssertNULL(status) == NULL ? 0 : status->GetCallCount();
}

static PString BuildPartyStatus(const PString & party, const PString & name)
{
  if (name.IsEmpty())
    return party;

  return PSTRSTRM(name << "<BR>" << party);
}

PCREATE_SERVICE_MACRO_BLOCK(CallStatus,resource,P_EMPTY,htmlBlock)
{
  CallStatusPage * status = dynamic_cast<CallStatusPage *>(resource.m_resource);
  if (PAssertNULL(status) == NULL)
    return PString::Empty();

  PString substitution;

  PArray<PString> calls;
  status->GetCalls(calls);
  for (PINDEX i = 0; i < calls.GetSize(); ++i) {
    PSafePtr<OpalCall> call = status->GetManager().FindCallWithLock(calls[i], PSafeReadOnly);
    if (call == NULL)
      continue;

    // make a copy of the repeating html chunk
    PString insert = htmlBlock;

    PServiceHTML::SpliceMacro(insert, "status Token",    call->GetToken());
    PServiceHTML::SpliceMacro(insert, "status A-Party",  BuildPartyStatus(call->GetPartyA(), call->GetNameA()));
    PServiceHTML::SpliceMacro(insert, "status B-Party",  BuildPartyStatus(call->GetPartyB(), call->GetNameB()));

    PStringStream duration;
    duration.precision(0);
    duration.width(5);
    if (call->GetEstablishedTime().IsValid())
      duration << call->GetEstablishedTime().GetElapsed();
    else
      duration << '(' << call->GetStartTime().GetElapsed() << ')';
    PServiceHTML::SpliceMacro(insert, "status Duration", duration);

    // Then put it into the page, moving insertion point along after it.
    substitution += insert;
  }

  return substitution;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BaseStatusPage::BaseStatusPage(MyManager & mgr, const PHTTPAuthority & auth, const char * name)
  : PServiceHTTPString(name, PString::Empty(), "text/html; charset=UTF-8", auth)
  , m_manager(mgr)
  , m_refreshRate(30)
  , m_cdr(NULL)
{
}

PString BaseStatusPage::LoadText(PHTTPRequest & request)
{
  PHTML page;
  CreateHTML(page, request.url.GetQueryVars());
  SetString(page);
  PServiceHTML::ProcessMacros(request, m_string, "", PServiceHTML::LoadFromFile);

  return m_string;
}

PBoolean BaseStatusPage::Post(PHTTPRequest & request,
                              const PStringToString & data,
                              PHTML & msg)
{
  PTRACE(2, "Main\tClear call POST received " << data);

  if (data("Set Page Refresh Time") == "Set") {
    m_refreshRate = data["Page Refresh Time"].AsUnsigned();
    CreateHTML(msg, request.url.GetQueryVars());
    PServiceHTML::ProcessMacros(request, msg, "", PServiceHTML::LoadFromFile);
    return true;
  }
  
  PStringStream meta_page;
  BeginPage(msg, meta_page, GetTitle(), "window.l_param_general","window.l_info_param_general");
  msg.Set(PHTML::InBody);
  msg << "<h2>" << "Accepted Control Command" << "</h2>";

  if (!OnPostControl(data, msg))
    msg << "<h2>" << "No calls or endpoints!" << "</h2>";

  msg << PHTML::Paragraph()
      << PHTML::HotLink(request.url.AsString()) << "Recargar página" << PHTML::HotLink()
      << PHTML::NonBreakSpace(4)
      << PHTML::HotLink("/") << "Home" << PHTML::HotLink();
  
  EndPage(msg,MyProcess::Current().GetHtmlCopyright());

  PServiceHTML::ProcessMacros(request, msg, "html/status.html",
                              PServiceHTML::LoadFromFile | PServiceHTML::NoSignatureForFile);
  return true;
}

void BaseStatusPage::CreateHTML(PHTML & html_page, const PStringToString & query)
{
  PStringStream html_begin, html_end, meta_page;
  PHTML html(PHTML::InBody);
  
  if (m_refreshRate > 0) {
    meta_page << "<meta http-equiv=\"Refresh\" content=\"" << m_refreshRate << "\" />";
    BeginPage(html_begin, meta_page, GetTitle(), "window.l_param_general","window.l_info_param_general");
  }
  else {
    BeginPage(html_begin, meta_page, GetTitle(), "window.l_param_general","window.l_info_param_general");
  }
  
  html << PHTML::Form("POST");

  CreateContent(html, query);

  if (m_refreshRate > 0)
    html << PHTML::TableStart()
         << PHTML::TableRow()
         << PHTML::TableData()
         << "Refresh rate" 
         << PHTML::TableData()
         << PHTML::InputNumber("Page Refresh Time", 5, 3600, m_refreshRate)  
         << PHTML::TableData()
         << PHTML::SubmitButton("Set", "Set Page Refresh Time") 
         << PHTML::TableEnd();

  html << PHTML::Form()
       << PHTML::HRule()
       << "<p>Last update: <!--#macro LongDateTime--></p>";
  
  EndPage(html_end,MyProcess::Current().GetHtmlCopyright());
  html_page << html_begin << html << html_end;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RegistrationStatusPage::RegistrationStatusPage(MyManager & mgr, const PHTTPAuthority & auth)
  : BaseStatusPage(mgr, auth, "StatusRegistro")
{
}

#if OPAL_H323
void RegistrationStatusPage::GetH323(StatusMap & copy) const
{
  PWaitAndSignal lock(m_mutex);
  copy = m_h323;
}
#endif

#if OPAL_SIP
void RegistrationStatusPage::GetSIP(StatusMap & copy) const
{
  PWaitAndSignal lock(m_mutex);
  copy = m_sip;
}
#endif

void RegistrationStatusPage::CreateContent(PHTML & html, const PStringToString &) const
{
  html << PHTML::TableStart(PHTML::Border1, PHTML::CellPad4)
       << PHTML::TableRow()
       << PHTML::TableHeader() << ' '
       << PHTML::TableHeader(PHTML::NoWrap) << "Name/Address"
       << PHTML::TableHeader(PHTML::NoWrap) << "Status"
#if OPAL_H323
       << PHTML::TableRow()
       << PHTML::TableHeader(PHTML::NoWrap, "rowspan=<!--#macro H323ListenerCount-->")
       << " H.323 Listeners"
       << "<!--#macrostart H323ListenerStatus-->"
           << PHTML::TableRow()
           << PHTML::TableData(PHTML::NoWrap)
           << "<!--#status Address-->"
           << PHTML::TableData(PHTML::NoWrap, PHTML::AlignCentre)
           << "<!--#status Status-->"
       << "<!--#macroend H323ListenerStatus-->"
       << PHTML::TableRow()
       << PHTML::TableHeader(PHTML::NoWrap, "rowspan=<!--#macro H323RegistrationCount-->")
       << " H.323 Gatekeeper"
       << "<!--#macrostart H323RegistrationStatus-->"
           << PHTML::TableRow()
           << PHTML::TableData(PHTML::NoWrap)
           << "<!--#status Name-->"
           << PHTML::TableData(PHTML::NoWrap)
           << "<!--#status Status-->"
       << "<!--#macroend H323RegistrationStatus-->"
#endif // OPAL_H323

#if OPAL_SIP
       << PHTML::TableRow()
       << PHTML::TableHeader(PHTML::NoWrap, "rowspan=<!--#macro SIPListenerCount-->")
       << " SIP Listeners "
       << "<!--#macrostart SIPListenerStatus-->"
           << PHTML::TableRow()
           << PHTML::TableData(PHTML::NoWrap)
           << "<!--#status Address-->"
           << PHTML::TableData(PHTML::NoWrap, PHTML::AlignCentre)
           << "<!--#status Status-->"
       << "<!--#macroend SIPListenerStatus-->"
       << PHTML::TableRow()
       << PHTML::TableHeader(PHTML::NoWrap, "rowspan=<!--#macro SIPRegistrationCount-->")
       << " SIP Registrars "
       << "<!--#macrostart SIPRegistrationStatus-->"
           << PHTML::TableRow()
           << PHTML::TableData(PHTML::NoWrap)
           << "<!--#status Name-->"
           << PHTML::TableData(PHTML::NoWrap)
           << "<!--#status Status-->"
       << "<!--#macroend SIPRegistrationStatus-->"
#endif // OPAL_SIP

#if OPAL_PTLIB_NAT
       << PHTML::TableRow()
       << PHTML::TableHeader(PHTML::NoWrap)
       << " STUN Server "
       << PHTML::TableData(PHTML::NoWrap)
       << "<!--#macro STUNServer-->"
       << PHTML::TableData(PHTML::NoWrap)
       << "<!--#macro STUNStatus-->"
#endif // OPAL_PTLIB_NAT

       << PHTML::TableEnd();
}

PString RegistrationStatusPage::LoadText(PHTTPRequest & request)
{
  m_mutex.Wait();

#if OPAL_H323
  m_h323.clear();
  const PList<H323Gatekeeper> gkList = m_manager.GetH323EndPoint().GetGatekeepers();
  for (PList<H323Gatekeeper>::const_iterator gk = gkList.begin(); gk != gkList.end(); ++gk) {
    PString addr = '@' + gk->GetTransport().GetRemoteAddress().GetHostName(true);

    PStringStream status;
    if (gk->IsRegistered())
      status << "Registered";
    else
      status << "Failed: " << gk->GetRegistrationFailReason();

    const PStringList & aliases = gk->GetAliases();
    for (PStringList::const_iterator it = aliases.begin(); it != aliases.end(); ++it)
      m_h323[*it + addr] = status;
  }
#endif

#if OPAL_SIP
  m_sip.clear();
  MySIPEndPoint & sipEP = m_manager.GetSIPEndPoint();
  const PStringList & registrations = sipEP.GetRegistrations(true);
  for (PStringList::const_iterator it = registrations.begin(); it != registrations.end(); ++it)
    m_sip[*it] = sipEP.IsRegistered(*it) ? "Registered" : (sipEP.IsRegistered(*it, true) ? "Offline" : "Failed");

#endif

  m_mutex.Signal();

  return BaseStatusPage::LoadText(request);
}

const char * RegistrationStatusPage::GetTitle() const
{
  return "OPAL Server Registration Status";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if OPAL_H323
GkStatusPage::GkStatusPage(MyManager & mgr, const PHTTPAuthority & auth)
  : BaseStatusPage(mgr, auth, "StatusGk")
  , m_gkServer(mgr.FindEndPointAs<MyH323EndPoint>(OPAL_PREFIX_H323)->GetGatekeeperServer())
{
}

void GkStatusPage::CreateContent(PHTML & html, const PStringToString &) const
{
  html << PHTML::TableStart(PHTML::Border1)
       << PHTML::TableRow()
       << PHTML::TableHeader()
       << PHTML::NonBreakSpace() << "End" << PHTML::NonBreakSpace() << "Point" << PHTML::NonBreakSpace() << "Identifier" << PHTML::NonBreakSpace()
       << PHTML::TableHeader()
       << PHTML::NonBreakSpace() << "Call" << PHTML::NonBreakSpace() << "Signal" << PHTML::NonBreakSpace() << "Addresses" << PHTML::NonBreakSpace()
       << PHTML::TableHeader()
       << PHTML::NonBreakSpace() << "Aliases" << PHTML::NonBreakSpace()
       << PHTML::TableHeader()
       << PHTML::NonBreakSpace() << "Application" << PHTML::NonBreakSpace()
       << PHTML::TableHeader()
       << PHTML::NonBreakSpace() << "Active" << PHTML::NonBreakSpace() << "Calls" << PHTML::NonBreakSpace()
       << "<!--#macrostart H323EndPointStatus-->"
       << PHTML::TableRow()
       << PHTML::TableData()
       << "<!--#status EndPointIdentifier-->"
       << PHTML::TableData()
       << "<!--#status CallSignalAddresses-->"
       << PHTML::TableData(PHTML::NoWrap)
       << "<!--#status EndPointAliases-->"
       << PHTML::TableData(PHTML::NoWrap)
       << "<!--#status Application-->"
       << PHTML::TableData("align=center")
       << "<!--#status ActiveCalls-->"
       << PHTML::TableData()
       << PHTML::SubmitButton("Unregister", "!--#status EndPointIdentifier--")
       << "<!--#macroend H323EndPointStatus-->"
       << PHTML::TableEnd();
}

const char * GkStatusPage::GetTitle() const
{
  return "OPAL Gatekeeper Status";
}

PBoolean GkStatusPage::OnPostControl(const PStringToString & data, PHTML & msg)
{
  bool gotOne = false;

  for (PStringToString::const_iterator it = data.begin(); it != data.end(); ++it) {
    PString id = it->first;
    if (it->second == "Unregister" && m_gkServer.ForceUnregister(id)) {
      msg << PHTML::Heading(2) << "Unregistered " << id << PHTML::Heading(2);
      gotOne = true;
    }
  }

  return gotOne;
}
#endif //OPAL_H323

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if OPAL_SIP
RegistrarStatusPage::RegistrarStatusPage(MyManager & mgr, const PHTTPAuthority & auth)
  : BaseStatusPage(mgr, auth, "StatusRegistrar")
  , m_registrar(*mgr.FindEndPointAs<MySIPEndPoint>(OPAL_PREFIX_SIP))
{
}

void RegistrarStatusPage::CreateContent(PHTML & html, const PStringToString &) const
{
  html << PHTML::TableStart(PHTML::Border1)
       << PHTML::TableRow()
       << PHTML::TableHeader()
       << PHTML::NonBreakSpace() << "End" << PHTML::NonBreakSpace() << "Point" << PHTML::NonBreakSpace() << "Identifier" << PHTML::NonBreakSpace()
       << PHTML::TableHeader()
       << PHTML::NonBreakSpace() << "Call" << PHTML::NonBreakSpace() << "Signal" << PHTML::NonBreakSpace() << "Addresses" << PHTML::NonBreakSpace()
       << PHTML::TableHeader()
       << PHTML::NonBreakSpace() << "Application" << PHTML::NonBreakSpace()
       << PHTML::TableHeader()
       << PHTML::NonBreakSpace() << "Active" << PHTML::NonBreakSpace() << "Calls" << PHTML::NonBreakSpace()
       << "<!--#macrostart SIPEndPointStatus-->"
       << PHTML::TableRow()
       << PHTML::TableData()
       << "<!--#status EndPointIdentifier-->"
       << PHTML::TableData()
       << "<!--#status CallSignalAddresses-->"
       << PHTML::TableData(PHTML::NoWrap)
       << "<!--#status Application-->"
       << PHTML::TableData("align=center")
       << "<!--#status ActiveCalls-->"
       << PHTML::TableData()
       << PHTML::SubmitButton("Unregister", "!--#status EndPointIdentifier--")
       << "<!--#macroend SIPEndPointStatus-->"
       << PHTML::TableEnd();
}

const char * RegistrarStatusPage::GetTitle() const
{
  return "OPAL Registrar Status";
}

PBoolean RegistrarStatusPage::OnPostControl(const PStringToString & data, PHTML & msg)
{
  bool gotOne = false;

  for (PStringToString::const_iterator it = data.begin(); it != data.end(); ++it) {
    PString aor = it->first;
    if (it->second == "Unregister" && m_registrar.ForceUnregister(aor)) {
      msg << PHTML::Heading(2) << "Unregistered " << aor << PHTML::Heading(2);
      gotOne = true;
    }
  }

  return gotOne;
}
#endif //OPAL_SIP

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CallStatusPage::CallStatusPage(MyManager & mgr, const PHTTPAuthority & auth)
  : BaseStatusPage(mgr, auth, "StatusLlamada")
{
}

void CallStatusPage::GetCalls(PArray<PString> & copy) const
{
  PWaitAndSignal lock(m_mutex);
  copy = m_calls;
  copy.MakeUnique();
}

void CallStatusPage::CreateContent(PHTML & html, const PStringToString &) const
{
 html << "Current call count: <!--#macro CallCount-->" << PHTML::Paragraph()
       << PHTML::TableStart(PHTML::Border1, PHTML::CellPad4)
       << PHTML::TableRow()
       << PHTML::TableHeader()
       << PHTML::NonBreakSpace() << 'A' << PHTML::NonBreakSpace() << "Party" << PHTML::NonBreakSpace()
       << PHTML::TableHeader()
       << PHTML::NonBreakSpace() << 'B' << PHTML::NonBreakSpace() << "Party" << PHTML::NonBreakSpace()
       << PHTML::TableHeader()
       << PHTML::NonBreakSpace() << "Duration" << PHTML::NonBreakSpace()
       << "<!--#macrostart CallStatus-->"
         << PHTML::TableRow()
         << PHTML::TableData(PHTML::NoWrap)
         << "<!--#status A-Party-->"
         << PHTML::TableData(PHTML::NoWrap)
         << "<!--#status B-Party-->"
         << PHTML::TableData()
         << "<!--#status Duration-->"
         << PHTML::TableData()
         << PHTML::SubmitButton("Clear", "!--#status Token--")
       << "<!--#macroend CallStatus-->"
       << PHTML::TableEnd();
}

const char * CallStatusPage::GetTitle() const
{
  return "OPAL Server Call Status";
}

PString CallStatusPage::LoadText(PHTTPRequest & request)
{
  m_mutex.Wait();
  m_calls = m_manager.GetAllCalls();
  m_mutex.Signal();
  return BaseStatusPage::LoadText(request);
}

bool CallStatusPage::OnPostControl(const PStringToString & data, PHTML & msg)
{
  bool gotOne = false;

  for (PStringToString::const_iterator it = data.begin(); it != data.end(); ++it) {
    PString key = it->first;
    PString value = it->second;
    if (value == "Clear") {
      PSafePtr<OpalCall> call = m_manager.FindCallWithLock(key, PSafeReference);
      if (call != NULL) {
        msg << PHTML::Heading(2) << "Clearing call " << *call << PHTML::Heading(2);
        call->Clear();
        gotOne = true;
      }
    }
  }

  return gotOne;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CDRListPage::CDRListPage(MyManager & mgr, const PHTTPAuthority & auth)
  : BaseStatusPage(mgr, auth, "CDRLista")
{
}

void CDRListPage::CreateContent(PHTML & html, const PStringToString &) const
{
  html << PHTML::TableStart(PHTML::Border1, PHTML::CellPad4)
       << PHTML::TableRow()
       << PHTML::TableHeader() << "Call" << PHTML::NonBreakSpace() << "Identifier"
       << PHTML::TableHeader() << "Originator"
       << PHTML::TableHeader() << "Destination"
       << PHTML::TableHeader() << "Start" << PHTML::NonBreakSpace() << "Time"
       << PHTML::TableHeader() << "End" << PHTML::NonBreakSpace() << "Time"
       << PHTML::TableHeader() << "Call" << PHTML::NonBreakSpace() << "State";
  for (MyManager::CDRList::const_iterator it = m_manager.BeginCDR(); m_manager.NotEndCDR(it); ++it)
    it->OutputSummaryHTML(html);

  html << PHTML::TableEnd();
}

const char * CDRListPage::GetTitle() const
{
  return "OPAL Server Call Detail Records";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CDRPage::CDRPage(MyManager & mgr, const PHTTPAuthority & auth)
  : BaseStatusPage(mgr, auth, "CDR")
{
  m_refreshRate = 0;
}

void CDRPage::CreateContent(PHTML & html, const PStringToString & query) const
{
  MyCallDetailRecord cdr;
  if (m_manager.FindCDR(query("guid"), cdr))
    cdr.OutputDetailedHTML(html);
  else
    html << "No records found.";
}

const char * CDRPage::GetTitle() const
{
  return "OPAL Server Call Detail Record";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

InvitePage::InvitePage(MyProcess & app, PHTTPAuthority & auth)
  : PServiceHTTPString("Invitar", "", "text/html; charset=utf-8", auth)
  , app(app)
{
}

void InvitePage::CreateHTML(PHTML & html)
{
  PStringStream meta_page;
  PStringArray myAddressBook = MyProcess::Current().GetManager().GetAddressBook();
  MyMixerEndPoint & mixer = MyProcess::Current().GetManager().GetMixerEndPoint();
  
  PHTML html_page;
  BeginPage(html_page,meta_page,"Invite","window.l_invite","window.l_info_invite");
  html_page.Set(PHTML::InBody);
  
  html_page << PHTML::Paragraph() << "<center>"
       
       << PHTML::Form("POST") 
       << "<b>Room Name</b>" << PHTML::Select("room");
 for (PSafePtr<OpalMixerNode> node = mixer.GetFirstNode(PSafeReadOnly); node != NULL; ++node)
   html_page << PHTML::Option(node != NULL ? PHTML::Selected : PHTML::NotSelected)
        << PHTML::Escaped(node->GetNodeInfo().m_name);
  html_page << PHTML::Select()
       << PHTML::TableStart()
         << PHTML::TableRow("align='left' style='background-color:#d9e5e3;padding:0px 4px 0px 4px;border-bottom:2px solid white;border-right:2px solid white;'")
          << PHTML::TableHeader()
          << "&nbsp;Address&nbsp;"
          << PHTML::TableData("align='middle' style='background-color:#d9e5e3;padding:0px;border-bottom:2px solid white;border-right:2px solid white;'")
          << PHTML::InputText("address", 40,NULL,"style='margin-top:5px;margin-bottom:5px;padding-left:5px;padding-right:5px;'")
          << "<b>Registro de llamadas</b>" << PHTML::Select("book");
 for (PINDEX i = 0; i < myAddressBook.GetSize(); i++)
   html_page << PHTML::Option(i != 0 ? PHTML::Selected : PHTML::NotSelected)
        << PHTML::Escaped(myAddressBook[i]);
   html_page << PHTML::Select()
       << PHTML::TableEnd() 
       << PHTML::Paragraph()
       << PHTML::SubmitButton("Invite") 
       << PHTML::Form() 
       << PHTML::HRule();
  
  EndPage(html_page,MyProcess::Current().GetHtmlCopyright());
  html = html_page;
}

PBoolean InvitePage::OnGET (PHTTPServer & server, const PHTTPConnectionInfo & connectInfo)
{
  { PHTTPRequest * req = CreateRequest(server, connectInfo); // check authorization
    if(!CheckAuthority(server, *req, connectInfo)) {delete req; return FALSE;}
    delete req;
  }

  PStringToString data;
  { PString request=connectInfo.GetURL().AsString(); PINDEX q;
    if((q=request.Find("?"))!=P_MAX_INDEX) { request=request.Mid(q+1,P_MAX_INDEX); PURL::SplitQueryVars(request,data); }
  }

  PHTML html;
  CreateHTML(html);
       
  
  { PStringStream message; PTime now; message
      << "HTTP/1.1 200 OK\r\n"
      << "Date: " << now.AsString(PTime::RFC1123, PTime::GMT) << "\r\n"
      << "Server: OpalServer\r\n"
      << "MIME-Version: 1.0\r\n"
      << "Cache-Control: no-cache, must-revalidate\r\n"
      << "Expires: Sat, 26 Jul 1997 05:00:00 GMT\r\n"
      << "Content-Type: text/html;charset=utf-8\r\n"
      << "Content-Length: " << html.GetLength() << "\r\n"
      << "Connection: Close\r\n"
      << "\r\n";  //that's the last time we need to type \r\n instead of just \n
    server.Write((const char*)message,message.GetLength());
  }
  server.Write((const char*)html,html.GetLength());
  server.flush();
  return true;
}

PBoolean InvitePage::Post(PHTTPRequest & request,
                          const PStringToString & data,
                          PHTML & msg)
{
  PStringStream meta_page;
  PString room    = data("room");
  PString address = data("address");
  PString book    = data("book");

  MyMixerEndPoint & m_mixer = MyProcess::Current().GetManager().GetMixerEndPoint();
  PStringArray addressBook = MyProcess::Current().GetManager().GetAddressBook();
  PString token;

  if(address.IsEmpty() || room.IsEmpty()){
    BeginPage(msg,meta_page,"Invite","window.l_invite","window.l_info_invite");
    msg.Set(PHTML::InBody);
    msg << "<b>Invitacion fallida, no existe sala de conferencia o direccion de remoto</b>"
        << PHTML::Paragraph()
        << PHTML::HotLink(request.url.AsString()) << "Reload page" << PHTML::HotLink();
    EndPage(msg,MyProcess::Current().GetHtmlCopyright());
    return true;
  }
  else {
   MyProcess::Current().GetManager().GetAddressBook().push_back(address);
  }
  
  PSafePtr<OpalMixerNode> node = m_mixer.FindNode(room);
  if (node == NULL) {
    cout << "Conference \"" << room << "\" does not exist" << endl;
  }
  
  if (MyProcess::Current().GetManager().SetUpCall("mcu:"+node->GetGUID().AsString(), address, token))
    cout << "Adding new member \"" << address << "\" to conference " << *node << endl;
  else
    cout << "Could not add new member \"" << address << "\" to conference " << *node << endl;
  
  CreateHTML(msg);
    
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SelectRoomPage::SelectRoomPage(MyProcess & app, PHTTPAuthority & auth)
  : PServiceHTTPString("Control", "", "text/html; charset=utf-8", auth)
  , app(app)
{
}

PBoolean SelectRoomPage::OnGET (PHTTPServer & server, const PHTTPConnectionInfo & connectInfo)
{
  { PHTTPRequest * req = CreateRequest(server, connectInfo); // check authorization
    if(!CheckAuthority(server, *req, connectInfo)) {delete req; return FALSE;}
    delete req;
  }

  PStringToString data;
  { PString request=connectInfo.GetURL().AsString(); PINDEX q;
    if((q=request.Find("?"))!=P_MAX_INDEX) { request=request.Mid(q+1,P_MAX_INDEX); PURL::SplitQueryVars(request,data); }
  }

  MyMixerEndPoint & m_mixer = MyProcess::Current().GetManager().GetMixerEndPoint();
  MyManager & manager = MyProcess::Current().GetManager();
 
  PWaitAndSignal m(html_mutex);
  if(data.Contains("action") && !data("room").IsEmpty())
  {
    
    PString action = data("action");
    PString room = data("room");
    if(action == "create")
    {
      
      if (m_mixer.FindNode(room) != NULL) {
       cout << "Conference name \"" << room << "\" already exists." << endl;
      }
      else {
       OpalMixerNodeInfo * info = new OpalMixerNodeInfo();
       info->m_name = room;
       PSafePtr<OpalMixerNode> node = m_mixer.AddNode(info);
       cout << "Added conference " << *node << endl; 
      }
    
    }
    else if(action == "delete")
    {
      PSafePtr<OpalMixerNode> node = m_mixer.FindNode(room);
      if (node == NULL) {
      cout << "Conference \"" << room << "\" does not exist" << endl;
      
      }
      else {
      m_mixer.RemoveNode(*node);
      cout << "Removed conference \"" << room << "\" " << *node << endl;
      }
    }
  }
  
  PStringStream html, meta_page;
  BeginPage(html,meta_page,"Rooms","window.l_rooms","window.l_info_rooms");

  if(data.Contains("action")) html << "<script language='javascript'>location.href='Control';</script>";

  PString nextRoom = "OpalServer";

  html
    << "<form method=\"post\" onsubmit=\"javascript:{if(document.getElementById('newroom').value!='')location.href='?action=create&room='+encodeURIComponent(document.getElementById('newroom').value);return false;}\"><input name='room' id='room' type=hidden>"
    << "<table id=\"Table\" class=\"table table-striped table-bordered table-condensed\">"

    << "<tr>"
    << "<td colspan='7'><input type='text' class='input-small' name='newroom' id='newroom' value='" << nextRoom << "' /><input type='button' class='btn btn-large btn-info' value='Crear Sala de Conferencia' onclick=\"location.href='?action=create&room='+encodeURIComponent(document.getElementById('newroom').value);\"></td>"
    << "</tr>"

    << "<tr>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_enter);</script><br></th>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_visible);</script><br></th>"
    //<< "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_record);</script><br></th>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_delete);</script><br></th>"
    << "</tr>"
  ;
    
    for (PSafePtr<OpalMixerNode> node = m_mixer.GetFirstNode(PSafeReadOnly); node != NULL; ++node){
      
      html << "<tr>"
        << "<td style='text-align:left'><a href='Invitar'>"+  node->GetNodeInfo().m_name  +"</a></td>"
        << "<td style='text-align:right'>";
      for (PSafePtr<OpalConnection> connection = node->GetFirstConnection(); connection != NULL; ++connection) {
      html <<  connection->GetCall().GetPartyB() << '\n'; }
      html << "</td>"
      //<< "<td style='text-align:center'>" << recordButton                            << "</td>"
      << "<td style='text-align:center'><span class=\"btn btn-large btn-danger\" onclick=\"if(confirm('Вы уверены? Are you sure?')){ location.href='?action=delete&room="+node->GetNodeInfo().m_name+"';}\">X</td>"
        << "</tr>";
    }
  html << "</table></form>";
       
  EndPage(html,MyProcess::Current().GetHtmlCopyright());
  { PStringStream message; PTime now; message
      << "HTTP/1.1 200 OK\r\n"
      << "Date: " << now.AsString(PTime::RFC1123, PTime::GMT) << "\r\n"
      << "Server: OpalMCU-ru\r\n"
      << "MIME-Version: 1.0\r\n"
      << "Cache-Control: no-cache, must-revalidate\r\n"
      << "Expires: Sat, 26 Jul 1997 05:00:00 GMT\r\n"
      << "Content-Type: text/html;charset=utf-8\r\n"
      << "Content-Length: " << html.GetLength() << "\r\n"
      << "Connection: Close\r\n"
      << "\r\n";  //that's the last time we need to type \r\n instead of just \n
    server.Write((const char*)message,message.GetLength());
  }
  server.Write((const char*)html,html.GetLength());
  server.flush();
  return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HomePage::HomePage(MyProcess & app, PHTTPAuthority & auth)
  : PServiceHTTPString("welcome.html", "", "text/html; charset=utf-8", auth)
  , app(app)
{
  PString peerAddr  = "N/A",
          localAddr = "127.0.0.1";
  WORD    localPort = 80;
  
  PStringStream html, meta_page;
  BeginPage(html, meta_page, "OpalMCU-ru","window.l_welcome","window.l_info_welcome");
  PString timeFormat = "dd/MM/yyyy hh:mm:ss";
  PTime now;

  html << "<br><b>Monitor Text (<span style='cursor:pointer;text-decoration:underline' onclick='javascript:{if(document.selection){var range=document.body.createTextRange();range.moveToElementText(document.getElementById(\"monitorTextId\"));range.select();}else if(window.getSelection){var range=document.createRange();range.selectNode(document.getElementById(\"monitorTextId\"));window.getSelection().addRange(range);}}'>select all</span>)</b><div style='padding:5px;border:1pxdotted #595;width:100%;height:auto;max-height:300px;overflow:auto'><pre style='margin:0px;padding:0px' id='monitorTextId'>"
       << "Inicio del servidor: "  << MyProcess::Current().GetStartTime().AsString(timeFormat) << "\n"
       << "Host local: "           << PIPSocket::GetHostName() << "\n"
       << "Direccion local: "      << localAddr << "\n"
       << "Puerto local: "         << localPort << "\n"
       << app.GetManager().GetMonitorText() << "</pre></div>";
  
  EndPage(html, MyProcess::Current().GetHtmlCopyright());
  m_string = html;
}
// Final del Archivo