/*
 * html.cxx
 *
 * Project html class
 *
 */

#include "html.h"

static unsigned long html_template_size; // count on zero initialization
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

///////////////////////////////////////////////////////////////////////////////////////////////////

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

  msg << PHTML::Title() << "Accepted Control Command" << PHTML::Body()
    << PHTML::Heading(1) << "Accepted Control Command" << PHTML::Heading(1);

  if (!OnPostControl(data, msg))
    msg << PHTML::Heading(2) << "No calls or endpoints!" << PHTML::Heading(2);

  msg << PHTML::Paragraph()
      << PHTML::HotLink(request.url.AsString()) << "Reload page" << PHTML::HotLink()
      << PHTML::NonBreakSpace(4)
      << PHTML::HotLink("/") << "Home page" << PHTML::HotLink();

  PServiceHTML::ProcessMacros(request, msg, "html/status.html",
                              PServiceHTML::LoadFromFile | PServiceHTML::NoSignatureForFile);
  return TRUE;
}


CallStatusPage::CallStatusPage(MyManager & mgr, const PHTTPAuthority & auth)
  : BaseStatusPage(mgr, auth, "CallStatus")
{
}


void CallStatusPage::GetCalls(PArray<PString> & copy) const
{
  PWaitAndSignal lock(m_mutex);
  copy = m_calls;
  copy.MakeUnique();
}


PString CallStatusPage::LoadText(PHTTPRequest & request)
{
  m_mutex.Wait();
  m_calls = m_manager.GetAllCalls();
  m_mutex.Signal();
  return BaseStatusPage::LoadText(request);
}


const char * CallStatusPage::GetTitle() const
{
  return "OPAL Server Call Status";
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
    PSafePtr<OpalCall> call = status->m_manager.FindCallWithLock(calls[i], PSafeReadOnly);
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

CDRListPage::CDRListPage(MyManager & mgr, const PHTTPAuthority & auth)
  : BaseStatusPage(mgr, auth, "CallDetailRecords")
{
}


const char * CDRListPage::GetTitle() const
{
  return "OPAL Server Call Detail Records";
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


CDRPage::CDRPage(MyManager & mgr, const PHTTPAuthority & auth)
  : BaseStatusPage(mgr, auth, "CallDetailRecord")
{
  m_refreshRate = 0;
}


const char * CDRPage::GetTitle() const
{
  return "OPAL Server Call Detail Record";
}


void CDRPage::CreateContent(PHTML & html, const PStringToString & query) const
{
  MyCallDetailRecord cdr;
  if (m_manager.FindCDR(query("guid"), cdr))
    cdr.OutputDetailedHTML(html);
  else
    html << "No records found.";
}

InvitePage::InvitePage(MyProcess & _app, PHTTPAuthority & auth)
  : PServiceHTTPString("Invite", "", "text/html; charset=utf-8", auth),
    app(_app)
{
  PStringStream html_begin, html_end, html_page, meta_page;
  PHTML html;
  html.Set(PHTML::InBody);
 
  html << PHTML::Paragraph() << "<center>"

       << PHTML::Form("POST")

       << PHTML::TableStart()
         << PHTML::TableRow("align='left' style='background-color:#d9e5e3;padding:0px 4px 0px 4px;border-bottom:2px solid white;border-right:2px solid white;'")
          << PHTML::TableHeader()
          << "&nbsp;Room&nbsp;Name&nbsp;"
          << PHTML::TableData("align='middle' style='background-color:#d9e5e3;padding:0px;border-bottom:2px solid white;border-right:2px solid white;'")
          << PHTML::InputText("room", 40, "TelemedicinaUCV", "style='margin-top:5px;margin-bottom:5px;padding-left:5px;padding-right:5px;'")
         << PHTML::TableRow("align='left' style='background-color:#d9e5e3;padding:0px 4px 0px 4px;border-bottom:2px solid white;border-right:2px solid white;'")
          << PHTML::TableHeader()
          << "&nbsp;Address&nbsp;"
          << PHTML::TableData("align='middle' style='background-color:#d9e5e3;padding:0px;border-bottom:2px solid white;border-right:2px solid white;'")
          << PHTML::InputText("address", 40,NULL,"style='margin-top:5px;margin-bottom:5px;padding-left:5px;padding-right:5px;'")
       << PHTML::TableEnd() 
       << PHTML::Paragraph()
       << PHTML::SubmitButton("Invite") 
       << PHTML::Form()
       << PHTML::HRule();
  BeginPage(html_begin,meta_page,"Invite","window.l_invite","window.l_info_invite");
  EndPage(html_end,MyProcess::Current().GetHtmlCopyright());
  html_page << html_begin << html << html_end;
  m_string = html_page;

}

void InvitePage::CreateHTML(PHTML & msg)
{
  PStringStream html_begin, html_end, html_page, meta_page;
  PHTML html;
  html.Set(PHTML::InBody);
 
  html << PHTML::Paragraph() << "<center>"

       << PHTML::Form("POST")

       << "<div style='overflow-x:auto;overflow-y:hidden;'>" << PHTML::TableStart()
         << PHTML::TableRow()
          << PHTML::TableHeader()
          << "&nbsp;Room&nbsp;Name&nbsp;"
          << PHTML::TableData()
          << PHTML::InputText("room", 40, "TelemedicinaUCV", 40)
         << PHTML::TableRow()
          << PHTML::TableHeader()
          << "&nbsp;Address&nbsp;"
          << PHTML::TableData()
          << PHTML::InputText("address", 40)
       << PHTML::TableEnd() << "</div>"
       << PHTML::Paragraph()
       << PHTML::SubmitButton("Invite") 
       << PHTML::Form()
       << PHTML::HRule();
  BeginPage(html_begin,meta_page,"Invite","window.l_invite","window.l_info_invite");
  EndPage(html_end,MyProcess::Current().GetHtmlCopyright());
  html_page << html_begin << html << html_end;
  msg = html_page;

}

PBoolean InvitePage::Post(PHTTPRequest & request,
                          const PStringToString & data,
                          PHTML & msg)
{
  /*PString room    = data("room");
  PString address = data("address");

  if (room.IsEmpty() || address.IsEmpty()) {
   CreateHTML(msg);
   return true; 
  }
  
  MyProcess::Current().addressBook.push_back(address);
  MyMixerEndPoint & m_mixer = MyProcess::Current().GetManager().GetMixerEndPoint();
  PString token;
  CreateHTML(msg);  

  
  PSafePtr<OpalMixerNode> node = m_mixer.FindNode(room);
  if (node == NULL) {
    cout << "Conference \"" << room << "\" does not exist" << endl;
    return true;
  }

  if (MyProcess::Current().GetManager().SetUpCall("mcu:"+node->GetGUID().AsString(), address, token))
    cout << "Adding" << " new member \"" << address << "\" to conference " << *node << endl;
  else
    cout << "Could not add" << " new member \"" << address << "\" to conference " << *node << endl;*/

  
  return true;
}

SelectRoomPage::SelectRoomPage(MyProcess & app, PHTTPAuthority & auth)
  : PServiceHTTPString("Select", "", "text/html; charset=utf-8", auth)
  , app(app)
{
}

PBoolean SelectRoomPage::OnGET (PHTTPServer & server, const PHTTPConnectionInfo & connectInfo)
{
  { PHTTPRequest * req = CreateRequest(server, connectInfo); // check authorization
    if(!CheckAuthority(server, *req, connectInfo)) {delete req; return FALSE;}
    delete req;
  }

  /*{ PHTTPRequest * req = CreateRequest(connectInfo.GetURL(), connectInfo.GetMIME(), connectInfo.GetMultipartFormInfo(), server); // check authorization
    if(!CheckAuthority(server, *req, connectInfo)) {delete req; return FALSE;}
    delete req;
  }*/

  PStringToString data;
  { PString request=connectInfo.GetURL().AsString(); PINDEX q;
    if((q=request.Find("?"))!=P_MAX_INDEX) { request=request.Mid(q+1,P_MAX_INDEX); PURL::SplitQueryVars(request,data); }
  }

  /*MyMixerEndPoint & m_mixer = MyProcess::Current().GetManager().GetMixerEndPoint();
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
       MyMixerNodeInfo * info = new MyMixerNodeInfo();
       info->m_name = room;
       PSafePtr<OpalMixerNode> node = m_mixer.AddNode(info);
       if (!node->AddName(room))
        cout << "Could not add conference alias \"" << room << '"' << endl;
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
      cout << "Removed conference " << *node << endl;
      }

    }
  }*/
  


  PStringStream html, meta_page;
  BeginPage(html,meta_page,"Rooms","window.l_rooms","window.l_info_rooms");

  if(data.Contains("action")) html << "<script language='javascript'>location.href='Select';</script>";

  PString nextRoom;

  html
    << "<form method=\"post\" onsubmit=\"javascript:{if(document.getElementById('newroom').value!='')location.href='?action=create&room='+encodeURIComponent(document.getElementById('newroom').value);return false;}\"><input name='room' id='room' type=hidden>"
    << "<table id=\"Table\" class=\"table table-striped table-bordered table-condensed\">"

    << "<tr>"
    << "<td colspan='7'><input type='text' class='input-small' name='newroom' id='newroom' value='" << nextRoom << "' /><input type='button' class='btn btn-large btn-info' id='l_select_create' onclick=\"location.href='?action=create&room='+encodeURIComponent(document.getElementById('newroom').value);\"></td>"
    << "</tr>"

    << "<tr>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_enter);</script><br></th>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_record);</script><br></th>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_moderated);</script><br></th>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_visible);</script><br></th>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_delete);</script><br></th>"
    << "</tr>"
  ;
  
  
   /*//PWaitAndSignal m(MyProcess::Current().GetManager().GetManagerMutex());
    //{
    for (PSafePtr<OpalMixerNode> node = m_mixer.GetFirstNode(PSafeReadOnly); node != NULL; ++node) 
    {
      
      //PString roomNumber = "casa";
      //BOOL controlled = TRUE;
      //BOOL moderated=FALSE; PString charModerated = "-";
      /*if(controlled) { charModerated = MyProcess::Current().GetManager().IsModerated(); moderated=(charModerated=="+"); }
      if(charModerated=="-") charModerated = "<script type=\"text/javascript\">document.write(window.l_select_moderated_no);</script>";
      else charModerated = "<script type=\"text/javascript\">document.write(window.l_select_moderated_yes);</script>";*/
      //PINDEX   visibleMemberCount = 1;
      //PINDEX unvisibleMemberCount = 2;*/

      /*PString roomButton = "<span class=\"btn btn-large btn-";
      if(moderated) roomButton+="success";
      else if(controlled) roomButton+="primary";
      else roomButton+="inverse";
      roomButton += "\"";
      if(controlled) roomButton+=" onclick='document.forms[0].submit();'";
      roomButton += "></span>";*/
    
      /*html << "<tr>"
        << "<td style='text-align:left'><a href='Invite'>"+  node->GetNodeInfo().m_name  +"</a></td>"
        << "<td style='text-align:center'>" << recordButton                            << "</td>"
        //<< "<td style='text-align:center'>" << charModerated                         << "</td>"
        //<< "<td style='text-align:right'>"  << visibleMemberCount                    << "</td>"
        << "<td style='text-align:center'><span class=\"btn btn-large btn-danger\" onclick=\"if(confirm('Вы уверены? Are you sure?')){ location.href='?action=delete&room="+node->GetNodeInfo().m_name+"';}\">X</td>"
        << "</tr>";
    }*/
 

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


WelcomePage::WelcomePage(MyProcess & app, PHTTPAuthority & auth)
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
       << "Programa: "             << MyProcess::Current().GetProductName() << "\n"
       << "Version: "              << MyProcess::Current().GetVersion(TRUE) << "\n"
       << "Desarrollador: "        << MyProcess::Current().GetManufacturer() << "\n"
       << "S.O: "                  << MyProcess::Current().GetOSClass() << " " 
       << MyProcess::Current().GetOSName() << "\n"
       << "Version S.O: "          << MyProcess::Current().GetOSVersion() << "\n"
       << "Hardware: "             << MyProcess::Current().GetOSHardware() << "\n"
       << "Inicio del servidor: "  << MyProcess::Current().GetStartTime().AsString(timeFormat) << "\n"
       << "Host local: "           << PIPSocket::GetHostName() << "\n"
       << "Direccion local: "      << localAddr << "\n"
       << "Puerto local: "         << localPort << "\n"
     /*<< app.GetEndpoint().GetMonitorText()*/ << "</pre></div>";
  
  EndPage(html, MyProcess::Current().GetHtmlCopyright());
  m_string = html;
}
