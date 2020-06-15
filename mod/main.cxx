#include "html.h"
#include "config.h"
#include <jpeglib.h>

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
    FILE * fs = fopen(PString(SYS_RESOURCE_DIR) + PATH_SEPARATOR + "template.html", "r");
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
    else html_template_size = -1;
  }

  if (html_template_size <= 0) { 
    PTRACE(1,"WebCtrl\tCan't read HTML template from file"); 
    return; 
  }

  PString lang = "en";

  PString html0(html_template_buffer); 
  html0 = html0.Left(html0.Find("$BODY$"));
  html0.Replace("$LANG$",     lang,     TRUE, 0);
  html0.Replace("$PMETA$",    pmeta,    TRUE, 0);
  html0.Replace("$PTITLE$",   ptitle,   TRUE, 0);
  html0.Replace("$TITLE$",    title,    TRUE, 0);
  html0.Replace("$QUOTE$",    quotekey, TRUE, 0);
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

HomePage::HomePage(MyProcess & app, PHTTPAuthority & auth)
  : PServiceHTTPString("welcome.html", "", "text/html; charset=utf-8", auth)
  , app(app)
{
  PString peerAddr  = "N/A",
          localAddr = "127.0.0.1";
  WORD    localPort = 80;

  PStringStream html, meta;
  BeginPage(html, meta, "OpalMCU-EIE", "window.l_welcome", "window.l_info_welcome");
  PString timeFormat = "dd/MM/yyyy hh:mm:ss";
  PTime now;

  html << "<script src=\"control.js\"></script>"
  
       << "<br><b>Info del servidor (<span style='cursor:pointer;text-decoration:underline' onclick='javascript:{if(document.selection){var range=document.body.createTextRange();range.moveToElementText(document.getElementById(\"monitorTextId\"));range.select();}else if(window.getSelection){var range=document.createRange();range.selectNode(document.getElementById(\"monitorTextId\"));window.getSelection().addRange(range);}}'>seleccionar todo</span>)</b><div style='padding:5px;border:1pxdotted #595;width:100%;height:auto;max-height:300px;overflow:auto'><pre style='margin:0px;padding:0px' id='monitorTextId'>"
       << "Inicio del servidor: "  << MyProcess::Current().GetStartTime().AsString(timeFormat) << "\n"
       << "Host local: "           << PIPSocket::GetHostName() << "\n"
       << "Direccion local: "      << localAddr << "\n"
       << "Puerto local: "         << localPort << "\n"
       /*<< app.GetManager().GetMonitorText()*/ << "</pre></div>";

  EndPage(html, MyProcess::Current().GetHtmlCopyright());
  m_string = html;
} 

ControlRoomPage::ControlRoomPage(MyProcess & app, PHTTPAuthority & auth)
  : PServiceHTTPString("Select", "", "text/html; charset=utf-8", auth)
  , app(app)
{
}

PBoolean ControlRoomPage::OnGET (PHTTPServer & server, const PHTTPConnectionInfo & connectInfo)
{
  { PHTTPRequest * req = CreateRequest(server, connectInfo);
    if(!CheckAuthority(server, *req, connectInfo)) {
      delete req; 
      return false;
    }
    delete req;
  }

  PStringToString data;
  { PString request = connectInfo.GetURL().AsString(); 
	PINDEX q;
    if((q = request.Find("?")) != P_MAX_INDEX) { 
      request = request.Mid(q+1, P_MAX_INDEX); 
      PURL::SplitQueryVars(request, data); 
    }
  }

  if(data.Contains("action"))
  {
    PString action = data("action");
    PString room = data("room");
    if(action == "create" && (!room.IsEmpty()))
    { ConferenceManager & cm = MyProcess::Current().GetConferenceManager();
      cm.MakeAndLockConference(room);
      cm.UnlockConference();
    }
    else if(action == "delete" && (!room.IsEmpty()))
    { ConferenceManager & cm = MyProcess::Current().GetConferenceManager();
      if(cm.HasConference(room))
      { Conference * conference = cm.FindConferenceWithLock(room); // find & get locked
        if(conference != NULL)
        { cm.RemoveConference(conference->GetID());
          cm.UnlockConference();
        }
      }
    }
    else if(action == "startRecorder" && (!room.IsEmpty()))
    { ConferenceManager & cm = MyProcess::Current().GetConferenceManager();
      if(cm.HasConference(room))
      { Conference * conference = cm.FindConferenceWithLock(room); // find & get locked
        if(conference != NULL)
        {
          conference->StartRecorder();
          cm.UnlockConference();
        }
      }
    }
    else if(action == "stopRecorder" && (!room.IsEmpty()))
    { ConferenceManager & cm = MyProcess::Current().GetConferenceManager();
      if(cm.HasConference(room))
      { Conference * conference = cm.FindConferenceWithLock(room); // find & get locked
        if(conference != NULL)
        {
          conference->StopRecorder();
          cm.UnlockConference();
        }
      }
    }
  }

  //MyH323EndPoint & ep=MyProcess::Current().GetEndpoint();

  PStringStream html, meta;
  BeginPage(html,meta,"Rooms","window.l_rooms","window.l_info_rooms");

  if(data.Contains("action")) html << "<script language='javascript'>location.href='Select';</script>";

  PString nextRoom;
  { ConferenceManager & cm = MyProcess::Current().GetConferenceManager();
    ConferenceListType::const_iterator r;
    PWaitAndSignal m(cm.GetConferenceListMutex());
    for(r = cm.GetConferenceList().begin(); r != cm.GetConferenceList().end(); ++r)
    { PString room0 = r->second->GetNumber().Trim(); PINDEX lastCharPos=room0.GetLength()-1;
      if(room0.IsEmpty()) continue;
#     if ENABLE_ECHO_MIXER
        if(room0.Left(4) *= "echo") continue;
#     endif
#     if ENABLE_TEST_ROOMS
        if(room0.Left(8) == "testroom") continue;
#     endif
      PINDEX i, d1=-1, d2=-1;
      for (i=lastCharPos; i>=0; i--)
      { char c=room0[i];
        BOOL isDigit = (c>='0' && c<='9');
        if (isDigit) { if (d2==-1) d2=i; }
        else { if (d2!=-1) { if (d1==-1) { d1 = i+1; break; } } }
      }
      if(d1==-1 || d2==-1) continue;
      if(d2-d1>6)d1=d2-6;
      PINDEX roomStart=room0.Mid(d1,d2).AsInteger(); PString roomText=room0.Left(d1);
      PString roomText2; if(d2<lastCharPos) roomText2=room0.Mid(d2+1,lastCharPos);
      while(1)
      { roomStart++;
        PString testName = roomText + PString(roomStart) + roomText2;
        for (r = cm.GetConferenceList().begin(); r != cm.GetConferenceList().end(); ++r) if(r->second->GetNumber()==testName) break;
        if(r == cm.GetConferenceList().end()) { nextRoom = testName; break; }
      }
      break;
    }
    if(nextRoom.IsEmpty()) nextRoom = MyProcess::Current().GetDefaultRoomName();
  }

  html
    << "<form method=\"post\" onsubmit=\"javascript:{if(document.getElementById('newroom').value!='')location.href='?action=create&room='+encodeURIComponent(document.getElementById('newroom').value);return false;}\"><input name='room' id='room' type=hidden>"
    << "<table class=\"table table-striped table-bordered table-condensed\">"

    << "<tr>"
    << "<td colspan='7'><input type='text' class='input-small' name='newroom' id='newroom' value='" << nextRoom << "' /><input type='button' class='btn btn-large btn-info' id='l_select_create' onclick=\"location.href='?action=create&room='+encodeURIComponent(document.getElementById('newroom').value);\"></td>"
    << "</tr>"

    << "<tr>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_enter);</script><br></th>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_record);</script><br></th>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_moderated);</script><br></th>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_visible);</script><br></th>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_unvisible);</script><br></th>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_duration);</script><br></th>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_delete);</script><br></th>"
    << "</tr>"
  ;
  
  { PWaitAndSignal m(MyProcess::Current().GetConferenceManager().GetConferenceListMutex());
    ConferenceListType & conferenceList = MyProcess::Current().GetConferenceManager().GetConferenceList();

    ConferenceListType::iterator r;
    for (r = conferenceList.begin(); r != conferenceList.end(); ++r)
    {
      BOOL controlled = TRUE;
      Conference & conference = *(r->second);
      PString roomNumber = conference.GetNumber();
#if ENABLE_TEST_ROOMS
      controlled &= (!((roomNumber.Left(8)=="testroom") && (roomNumber.GetLength()>8))) ;
#endif
#if ENABLE_ECHO_MIXER
      controlled &= (!(roomNumber.Left(4)*="echo"));
#endif
      BOOL moderated=FALSE; PString charModerated = "-";
      if(controlled) { charModerated = conference.IsModerated(); moderated=(charModerated=="+"); }
      if(charModerated=="-") charModerated = "<script type=\"text/javascript\">document.write(window.l_select_moderated_no);</script>";
      else charModerated = "<script type=\"text/javascript\">document.write(window.l_select_moderated_yes);</script>";
      PINDEX   visibleMemberCount = conference.GetVisibleMemberCount();
      PINDEX unvisibleMemberCount = conference.GetMemberCount() - visibleMemberCount;

      PString roomButton = "<span class=\"btn btn-large btn-";
      if(moderated) roomButton+="success";
      else if(controlled) roomButton+="primary";
      else roomButton+="inverse";
      roomButton += "\"";
      if(controlled) roomButton+=" onclick='document.getElementById(\"room\").value=\""
        + roomNumber + "\";document.forms[0].submit();'";
      roomButton += ">" + roomNumber + "</span>";

      PStringStream recordButton; if(controlled)
      { BOOL recState = conference.externalRecorder!=NULL; recordButton
        << "<input type='button' class='btn btn-large "
        << (recState ? "btn-inverse" : "btn-danger")
        << "' style='width:36px;height:36px;"
        << (recState ? "border-radius:0px" : "border-radius:18px")
        << "' value=' ' title='"
        << (recState ? "Stop recording" : "Start recording")
        << "' onclick=\"location.href='?action="
        << (recState ? "stop" : "start")
        << "Recorder&room="
        << PURL::TranslateString(roomNumber,PURL::QueryTranslation)
        << "'\">";
      }

      html << "<tr>"
        << "<td style='text-align:left'>"   << roomButton                            << "</td>"
        << "<td style='text-align:center'>" << recordButton                          << "</td>"
        << "<td style='text-align:center'>" << charModerated                         << "</td>"
        << "<td style='text-align:right'>"  << visibleMemberCount                    << "</td>"
        << "<td style='text-align:right'>"  << unvisibleMemberCount                  << "</td>"
        << "<td style='text-align:right'>"  << (PTime() - conference.GetStartTime()).AsString(0, PTimeInterval::IncludeDays, 10) << "</td>"
        << "<td style='text-align:center'><span class=\"btn btn-large btn-danger\" onclick=\"if(confirm('Вы уверены? Are you sure?')){location.href='?action=delete&room=" << PURL::TranslateString(roomNumber,PURL::QueryTranslation) << "';}\">X</span></td>"
        << "</tr>";
    }
  }

  html << "</table></form>";

  EndPage(html, MyProcess::Current().GetHtmlCopyright());
  
  { PStringStream message; 
    PTime now; 
    message << "HTTP/1.1 200 OK\r\n"
            << "Date: " << now.AsString(PTime::RFC1123, PTime::GMT) << "\r\n"
            << "Server: OpalMCU-EIE\r\n"
            << "MIME-Version: 1.0\r\n"
            << "Cache-Control: no-cache, must-revalidate\r\n"
            << "Expires: Sat, 26 Jul 1997 05:00:00 GMT\r\n"
            << "Content-Type: text/html;charset=utf-8\r\n"
            << "Content-Length: " << html.GetLength() << "\r\n"
            << "Connection: Close\r\n"
            << "\r\n";  //that's the last time we need to type \r\n instead of just \n
    server.Write((const char*)message, message.GetLength());
  }

  server.Write((const char*)html, html.GetLength());
  server.flush();
  return true;
}

PBoolean ControlRoomPage::Post(PHTTPRequest & request,
                          const PStringToString & data,
                          PHTML & msg)
{
  /*if(!MyProcess::Current().GetForceScreenSplit())
  {
    msg << "Locked Room Control feature is locked To unlock the page: click &laquo;<a href='/Parameters'>Parameters</a>&raquo;, check &laquo;Force split screen video and enable Room Control feature&raquo; and accept.<br/><br/>";
    return TRUE;
  }

# if ENABLE_TEST_ROOMS||ENABLE_ECHO_MIXER
    if(data.Contains("room"))
    {
      const PString room=data("room");
#     if ENABLE_TEST_ROOMS
        if((room.Left(8) == "testroom") && (room.GetLength() > 8))
        {
          //msg << ErrorPage(request.localAddr.AsString(),request.localPort,423,"Locked","Room Control feature is locked","The room you've tried to control is for test purposes only, control features not implemented yet. Please use &laquo;testroom&raquo; (without a number) if you want to test several video layouts.<br/><br/>");
          return TRUE;
        }
#     endif
#     if ENABLE_ECHO_MIXER
        if(room.Left(4)*="echo")
        {
          //msg << ErrorPage(request.localAddr.AsString(),request.localPort,423,"Locked","Room Control feature is locked","This is the personal echo room, it does not supply control functions.<br/><br/>");
          return TRUE;
        }
#     endif
    }
# endif*/

  msg << MyProcess::Current().GetConferenceManager().SetRoomParams(data);
  return TRUE;
}

InteractiveHTTP::InteractiveHTTP(MyProcess & app, PHTTPAuthority & auth)
  : PServiceHTTPString("Comm", "", "text/html; charset=utf-8", auth)
  , app(app)
{
}

PBoolean InteractiveHTTP::OnGET (PHTTPServer & server, const PHTTPConnectionInfo & connectInfo)
{
  { PHTTPRequest * req = CreateRequest(server, connectInfo);
    if(!CheckAuthority(server, *req, connectInfo)) {
      delete req; 
      return false;
    }
    delete req;
  }

  PStringToString data;
  { PString request = connectInfo.GetURL().AsString(); 
	PINDEX q;
    if((q = request.Find("?")) != P_MAX_INDEX) { 
      request = request.Mid(q+1, P_MAX_INDEX); 
      PURL::SplitQueryVars(request, data); 
    }
  }

  
  PString room=data("room");

  PStringStream message;
  PTime now;
  int idx;

  message << "HTTP/1.1 200 OK\r\n"
          << "Date: " << now.AsString(PTime::RFC1123, PTime::GMT) << "\r\n"
          << "Server: MyProcess-ru\r\n"
          << "MIME-Version: 1.0\r\n"
          << "Cache-Control: no-cache, must-revalidate\r\n"
          << "Expires: Sat, 26 Jul 1997 05:00:00 GMT\r\n"
          << "Content-Type: text/html;charset=utf-8\r\n"
          << "Connection: Close\r\n"
          << "\r\n";  //that's the last time we need to type \r\n instead of just \n
  server.Write((const char*)message,message.GetLength());
  server.flush();

  message="<html><body style='font-size:9px;font-family:Verdana,Arial;padding:0px;margin:1px;color:#000'><script>p=parent</script>\n";
  message << MyProcess::Current().HttpStartEventReading(idx,room);

//PTRACE(1,"!!!!!\tsha1('123')=" << PMessageDigestSHA1::Encode("123")); // sha1 works!! I'll try with websocket in future

  if(room!="")
  {
    MyProcess::Current().GetConferenceManager().GetConferenceListMutex().Wait();
    ConferenceListType & conferenceList = MyProcess::Current().GetConferenceManager().GetConferenceList();
    ConferenceListType::iterator r;
    for (r = conferenceList.begin(); r != conferenceList.end(); ++r) if(r->second->GetNumber() == room) break;
    if(r != conferenceList.end() )
    {
      Conference & conference = *(r->second);
      message << "<script>p.splitdata=Array(";
      for (unsigned i=0;i<MyProcess::vmcfg.vmconfs;i++)
      {
        PString split=MyProcess::vmcfg.vmconf[i].splitcfg.Id;
        split.Replace("\"","\\x22",TRUE,0);
        if(i!=0) message << ",";
        message << "\"" << split << "\"";
      }
      message << ");\np.splitdata2={";
      for (unsigned i=0;i<MyProcess::vmcfg.vmconfs;i++)
      {
        PString split=MyProcess::vmcfg.vmconf[i].splitcfg.Id;
        split.Replace("\"","\\x22",TRUE,0);
        message << "\"" << split << "\"" << ":[" << MyProcess::vmcfg.vmconf[i].splitcfg.vidnum;
        for(unsigned j=0;j<MyProcess::vmcfg.vmconf[i].splitcfg.vidnum; j++)
        {
          VMPCfgOptions & vo=MyProcess::vmcfg.vmconf[i].vmpcfg[j];
          message << ",[" << vo.posx << "," << vo.posy << "," << vo.width << "," << vo.height << "," << vo.border << "," << vo.label_mask << "]";
        }
        message << "]";
        if(i+1<MyProcess::vmcfg.vmconfs) message << ",";
      }
      message << "};\n"
        << "p." << MyProcess::Current().GetConferenceManager().GetMemberListOptsJavascript(conference) << "\n"
        << "p." << MyProcess::Current().GetConferenceManager().GetConferenceOptsJavascript(conference) << "\n"
        << "p.tl=Array" << conference.GetTemplateList() << "\n"
        << "p.seltpl=\"" << conference.GetSelectedTemplateName() << "\"\n"
        << "p.build_page();\n"
        << "</script>\n";
    }
    else
    { // no (no more) room -- redirect to /
      MyProcess::Current().GetConferenceManager().GetConferenceListMutex().Signal();
      message << "<script>top.location.href='/';</script>\n";
      server.Write((const char*)message,message.GetLength());
      server.flush();
      return FALSE;
    }
    MyProcess::Current().GetConferenceManager().GetConferenceListMutex().Signal();
  }
   
  while(server.Write((const char*)message,message.GetLength())) {
    server.flush();
    int count=0;
    message = MyProcess::Current().HttpGetEvents(idx,room);
    while (message.GetLength()==0 && count < 20){
      count++;
      PThread::Sleep(100);
      message = MyProcess::Current().HttpGetEvents(idx,room);
    }
    if(message.Find("<script>")==P_MAX_INDEX) message << "<script>p.alive()</script>\n";
  }
  return FALSE;
 }
 /////////////////////////////////////////////////////////////////////////////////////////
 MCUVideoMixer* jpegMixer;

void jpeg_init_destination(j_compress_ptr cinfo){
  if(jpegMixer->myjpeg.GetSize()<32768)jpegMixer->myjpeg.SetSize(32768);
  cinfo->dest->next_output_byte=&jpegMixer->myjpeg[0];
  cinfo->dest->free_in_buffer=jpegMixer->myjpeg.GetSize();
}

boolean jpeg_empty_output_buffer(j_compress_ptr cinfo){
  PINDEX oldsize=jpegMixer->myjpeg.GetSize();
  jpegMixer->myjpeg.SetSize(oldsize+16384);
  cinfo->dest->next_output_byte = &jpegMixer->myjpeg[oldsize];
  cinfo->dest->free_in_buffer = jpegMixer->myjpeg.GetSize() - oldsize;
  return true;
}

void jpeg_term_destination(j_compress_ptr cinfo){
  jpegMixer->jpegSize=jpegMixer->myjpeg.GetSize() - cinfo->dest->free_in_buffer;
  jpegMixer->jpegTime=(long)time(0);
}

JpegFrameHTTP::JpegFrameHTTP(MyProcess & app, PHTTPAuthority & auth)
  : PServiceHTTPString("Jpeg", "", "image/jpeg", auth),
    app(app)
{
}

bool JpegFrameHTTP::OnGET (PHTTPServer & server, const PHTTPConnectionInfo & connectInfo)
{
  { PHTTPRequest * req = CreateRequest(server, connectInfo);
    if(!CheckAuthority(server, *req, connectInfo)) {
      delete req; 
      return false;
    }
    delete req;
  }

  PStringToString data;
  { PString request = connectInfo.GetURL().AsString(); 
	PINDEX q;
    if((q = request.Find("?")) != P_MAX_INDEX) { 
      request = request.Mid(q+1, P_MAX_INDEX); 
      PURL::SplitQueryVars(request, data); 
    }
  }
  
  PString room=data("room"); if (room.GetLength()==0) return FALSE;

  int width=atoi(data("w"));
  int height=atoi(data("h"));

  unsigned requestedMixer=0;
  if(data.Contains("mixer")) requestedMixer=(unsigned)data("mixer").AsInteger();

  const unsigned long t1=(unsigned long)time(0);

//  PWaitAndSignal m(mutex); // no more required: the following mutex will do the same:
  app.GetConferenceManager().GetConferenceListMutex().Wait(); // fix it - browse read cause access_v on serv. stop

  ConferenceListType & conferenceList = app.GetConferenceManager().GetConferenceList();
  for(ConferenceListType::iterator r = conferenceList.begin(); r != conferenceList.end(); ++r)
  {
    Conference & conference = *(r->second);
    if(conference.GetNumber()==room)
    {
      if(conference.videoMixerList==NULL){ app.GetConferenceManager().GetConferenceListMutex().Signal(); return FALSE; }
      PWaitAndSignal m3(conference.videoMixerListMutex);
      jpegMixer=conference.VMLFind(requestedMixer);
      if(jpegMixer==NULL) { app.GetConferenceManager().GetConferenceListMutex().Signal(); return FALSE; }
      if(t1-(jpegMixer->jpegTime)>1)
      {
        if(width<1||height<1||width>2048||height>2048)
        { width=MyProcess::vmcfg.vmconf[jpegMixer->GetPositionSet()].splitcfg.mockup_width;
          height=MyProcess::vmcfg.vmconf[jpegMixer->GetPositionSet()].splitcfg.mockup_height;
        }
        struct jpeg_compress_struct cinfo; struct jpeg_error_mgr jerr;
        JSAMPROW row_pointer[1];
        int row_stride;
        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_compress(&cinfo);
        cinfo.image_width = width;
        cinfo.image_height = height;
        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_RGB;

        PINDEX amount=width*height*3/2;
        unsigned char *videoData=new unsigned char[amount];

        ((MCUSimpleVideoMixer*)jpegMixer)->ReadMixedFrame((void*)videoData,width,height,amount);
        PColourConverter * converter = PColourConverter::Create("YUV420P", "RGB24", width, height);
        converter->SetDstFrameSize(width, height);
        unsigned char * bitmap = new unsigned char[width*height*3];
        converter->Convert(videoData,bitmap);
        delete converter;
        delete videoData;

        jpeg_set_defaults(&cinfo);
        cinfo.dest = new jpeg_destination_mgr;
        cinfo.dest->init_destination = &jpeg_init_destination;
        cinfo.dest->empty_output_buffer = &jpeg_empty_output_buffer;
        cinfo.dest->term_destination = &jpeg_term_destination;
        jpeg_start_compress(&cinfo,TRUE);
        row_stride = cinfo.image_width * 3;
        while (cinfo.next_scanline < cinfo.image_height)
        { row_pointer[0] = (JSAMPLE *) & bitmap [cinfo.next_scanline * row_stride];
          (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }
        jpeg_finish_compress(&cinfo);
        delete bitmap; delete cinfo.dest; cinfo.dest=NULL;
        jpeg_destroy_compress(&cinfo);
        jpegMixer->jpegTime=t1;
      }

      PTime now;
      PStringStream message;
      message << "HTTP/1.1 200 OK\r\n"
              << "Date: " << now.AsString(PTime::RFC1123, PTime::GMT) << "\r\n"
              << "Server: OpenMCU-ru\r\n"
              << "MIME-Version: 1.0\r\n"
              << "Cache-Control: no-cache, must-revalidate\r\n"
              << "Expires: Sat, 26 Jul 1997 05:00:00 GMT\r\n"
              << "Content-Type: image/jpeg\r\n"
              << "Content-Length: " << jpegMixer->jpegSize << "\r\n"
              << "Connection: Close\r\n"
              << "\r\n";  //that's the last time we need to type \r\n instead of just \n

      server.Write((const char*)message,message.GetLength());
      server.Write(jpegMixer->myjpeg.GetPointer(),jpegMixer->jpegSize);

      app.GetConferenceManager().GetConferenceListMutex().Signal();
      server.flush();
      return TRUE;
    }
  }
  app.GetConferenceManager().GetConferenceListMutex().Signal();
  return FALSE;
}
// Final del Archivo
