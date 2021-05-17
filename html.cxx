/*
 * html.cxx
 * 
 * Copyright 2020 ajoi1011 <ajoi1011@debian>
 * 
*/

#include "config.h"
#include "html.h"
#include "h323.h"

#if OPAL_STATISTICS
static void SetMediaStats(PString & insert, OpalCall & call, const OpalMediaType & mediaType, bool fromAparty)
{
  static PConstString NA("N/A");
  PString name = PSTRSTRM("status " << (fromAparty ? 'A' : 'B') << '-' << mediaType << "-bytes");
  OpalMediaStatistics stats;
  if (call.GetStatistics(mediaType, true, stats))
    PServiceHTML::SpliceMacro(insert, name, PString(PString::ScaleSI, stats.m_totalBytes, 3));
  else
    PServiceHTML::SpliceMacro(insert, name, NA);
}
#endif // OPAL_STATISTICS

PDECLARE_MUTEX(m_htmlMutex);  
static unsigned long   m_htmlTemplateSize;
char                 * m_htmlTemplateBuffer,
                     * m_htmlQuoteBuffer;


void BeginPage(PStringStream & html, PStringStream & pmeta, 
                                     const char    * ptitle, 
                                     const char    * title, 
                                     const char    * quotekey
                                     )
{
  PWaitAndSignal m(m_htmlMutex);

  if (m_htmlTemplateSize <= 0) { 
    FILE * fs = fopen(PString(SYS_RESOURCE_DIR) + PATH_SEPARATOR + "template.html", "r");
    if (fs) { 
      fseek(fs, 0L, SEEK_END); 
      m_htmlTemplateSize = ftell(fs); 
      rewind(fs);
      m_htmlTemplateBuffer = new char[m_htmlTemplateSize + 1];
      if (m_htmlTemplateSize != fread(m_htmlTemplateBuffer, 1, m_htmlTemplateSize, fs)) 
        m_htmlTemplateSize = -1;
      else 
        m_htmlTemplateBuffer[m_htmlTemplateSize] = 0;
      fclose(fs);
    }
    else m_htmlTemplateSize = -1;
  }
  else if (m_htmlTemplateSize < 0) { 
    PTRACE(1,"WebCtrl\tCan't read HTML template from file"); 
    return; 
  }

  PString lang = "en";

  PString html0(m_htmlTemplateBuffer); 
  html0 = html0.Left(html0.Find("$BODY$"));
  html0.Replace("$LANG$", lang, true, 0);
  html0.Replace("$PMETA$", pmeta, true, 0);
  html0.Replace("$PTITLE$", ptitle, true, 0);
  html0.Replace("$TITLE$", title, true, 0);
  html0.Replace("$QUOTE$", quotekey, true, 0);
  html << html0;
}

void EndPage(PStringStream & html, PString copyr) 
{
  PWaitAndSignal m(m_htmlMutex);

  if (m_htmlTemplateSize <= 0) 
    return;

  PString html0(m_htmlTemplateBuffer); 
  html0 = html0.Mid(html0.Find("$BODY$")+6,P_MAX_INDEX);
  html0.Replace("$COPYRIGHT$", copyr, true, 0);
  html << html0;
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
    PSafePtr<OpalCall> call = status->GetManager().FindCallWithLock(calls[i], PSafeReadOnly);
    if (call == NULL)
      continue;

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

#if OPAL_STATISTICS
    SetMediaStats(insert, *call, OpalMediaType::Audio(), true);
    SetMediaStats(insert, *call, OpalMediaType::Audio(), false);
#if OPAL_VIDEO
    SetMediaStats(insert, *call, OpalMediaType::Video(), true);
    SetMediaStats(insert, *call, OpalMediaType::Video(), false);
#endif // OPAL_VIDEO
#endif // OPAL_STATISTICS

    // Then put it into the page, moving insertion point along after it.

    substitution += insert;
  }

  return substitution;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BaseStatusPage::BaseStatusPage(Manager & mgr, const PHTTPAuthority & auth, const char * name)
  : PServiceHTTPString(name, PString::Empty(), "text/html; charset=UTF-8", auth)
  , m_manager(mgr)
  , m_refreshRate(30)
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

  PStringStream meta;
  BeginPage(msg, meta, GetTitle(), "window.l_param_general","window.l_info_param_general");
  msg.Set(PHTML::InBody);
  msg << "<h2>" << "Accepted Control Command" << "</h2>";

  if (!OnPostControl(data, msg))
    msg << "<h2>" << "No calls or endpoints!" << "</h2>";

  msg << PHTML::Paragraph()
      << PHTML::HotLink(request.url.AsString()) << "Reload Page" << PHTML::HotLink()
      << PHTML::NonBreakSpace(4)
      << PHTML::HotLink("/") << "Home Page" << PHTML::HotLink();

  EndPage(msg,OpalMCUEIE::Current().GetHtmlCopyright());

  PServiceHTML::ProcessMacros(request, msg, "html/status.html",
                              PServiceHTML::LoadFromFile | PServiceHTML::NoSignatureForFile);
  return true;
}

void BaseStatusPage::CreateHTML(PHTML & html_page, const PStringToString & query)
{
  PStringStream html_begin, html_end, meta;
  PHTML html(PHTML::InBody);

  if (m_refreshRate > 0) {
    meta << "<meta http-equiv=\"Refresh\" content=\"" << m_refreshRate << "\" />";
    BeginPage(html_begin, meta, GetTitle(), "window.l_param_general","window.l_info_param_general");
  }
  else {
    BeginPage(html_begin, meta, GetTitle(), "window.l_param_general","window.l_info_param_general");
  }

  html << PHTML::Form("POST");

  CreateContent(html, query);

  if (m_refreshRate > 0)
    html << PHTML::TableStart()
         << PHTML::TableRow()
         << PHTML::TableData()
         << "Refresh rate" 
         << PHTML::TableData()
         << PHTML::InputNumber("Page Refresh Time", 1, 3600, m_refreshRate)  
         << PHTML::TableData()
         << PHTML::SubmitButton("Set", "Set Page Refresh Time") 
         << PHTML::TableEnd();

  html << PHTML::Form()
       << PHTML::HRule()
       << "<p>Last update: <!--#macro LongDateTime--></p>";

  EndPage(html_end,OpalMCUEIE::Current().GetHtmlCopyright());
  html_page << html_begin << html << html_end;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CallStatusPage::CallStatusPage(Manager & mgr, const PHTTPAuthority & auth)
  : BaseStatusPage(mgr, auth, "Status")
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
           << PHTML::TableStart(PHTML::Border1, PHTML::NoCellSpacing, "width=\"100%\"")
           << PHTML::TableRow()
             << PHTML::TableHeader()
               << "Audio"
             << PHTML::TableHeader()
               << "Video"
           << PHTML::TableRow()
             << PHTML::TableData()
               << "<!--#status A-audio-bytes-->"
             << PHTML::TableData()
               << "<!--#status A-video-bytes-->"
           << PHTML::TableEnd()
         << PHTML::TableData(PHTML::NoWrap)
         << "<!--#status B-Party-->"
           << PHTML::TableStart(PHTML::Border1, PHTML::NoCellSpacing, "width=\"100%\"")
           << PHTML::TableRow()
             << PHTML::TableHeader()
               << "Audio"
             << PHTML::TableHeader()
               << "Video"
           << PHTML::TableRow()
             << PHTML::TableData()
               << "<!--#status B-audio-bytes-->"
             << PHTML::TableData()
               << "<!--#status B-video-bytes-->"
           << PHTML::TableEnd()
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

CDRListPage::CDRListPage(Manager & mgr, const PHTTPAuthority & auth)
  : BaseStatusPage(mgr, auth, "CDRLista")
{
  m_refreshRate = 30;
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
  for (Manager::CDRList::const_iterator it = m_manager.BeginCDR(); m_manager.NotEndCDR(it); ++it)
    it->OutputSummaryHTML(html);

  html << PHTML::TableEnd();
}

const char * CDRListPage::GetTitle() const
{
  return "OPAL Server Call Detail Records";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CDRPage::CDRPage(Manager & mgr, const PHTTPAuthority & auth)
  : BaseStatusPage(mgr, auth, "CDR")
{
  m_refreshRate = 0;
}

void CDRPage::CreateContent(PHTML & html, const PStringToString & query) const
{
  MCUCallDetailRecord cdr;
  if (m_manager.FindCDR(query("guid"), cdr))
    cdr.OutputDetailedHTML(html);
  else
    html << "No records found.";
}

const char * CDRPage::GetTitle() const
{
  return "OPAL Server Call Detail Record";
}


TablePConfigPage::TablePConfigPage(PHTTPServiceProcess & app, const PString & title, 
                                                              const PString & section, 
                                                              const PHTTPAuthority & auth
                                                              )
  : PConfigPage(app, title, section, auth)
  , cfg(section)
{
  deleteSection = true;
  columnColor = "#d9e5e3";
  rowColor = "#d9e5e3";
  itemColor = "#f7f4d8";
  itemInfoColor = "#f7f4d8";
  separator = ",";
  firstEditRow = 1;
  firstDeleteRow = 1;
  buttonUp = buttonDown = buttonClone = buttonDelete = 0;
  colStyle = "<td align='middle' style='background-color:"+columnColor+";padding:0px;border-bottom:2px solid white;border-right:2px solid white;";
  rowStyle = "<td align='left' style='background-color:"+rowColor+";padding:0px 4px 0px 4px;border-bottom:2px solid white;border-right:2px solid white;'>";
  rowArrayStyle = "<td align='left' style='background-color:"+itemColor+";padding:0px 4px 0px 4px;'>";
  itemStyle = "<td align='left' style='background-color:"+itemColor+";padding:0px 4px 0px 4px;border-bottom:2px solid white;border-right:2px solid white;'>";
  itemInfoStyle = "<td rowspan='%ROWSPAN%' valign='top' align='left' style='background-color:"+itemInfoColor+";padding:0px 4px 0px 4px;border-bottom:2px solid white;border-right:2px solid white;'>";
  textStyle = "margin-top:5px;margin-bottom:5px;padding-left:5px;padding-right:5px;";
  inputStyle = "margin-top:5px;margin-bottom:5px;padding-left:5px;padding-right:5px;";
  buttonStyle = "margin-top:5px;margin-bottom:5px;margin-left:1px;margin-right:1px;width:24px;";
 }

PBoolean TablePConfigPage::Post(PHTTPRequest & request, const PStringToString & data, PHTML & reply)
{
  if(deleteSection)
    cfg.DeleteSection();
  for(PINDEX i = 0; i < dataArray.GetSize(); i++) {
    PString key = dataArray[i].Tokenise("=")[0];
    if(key == "") continue;
    PString value = "";
    PINDEX valuePos = dataArray[i].Find("=");
    if(valuePos != P_MAX_INDEX)
      value = dataArray[i].Right(dataArray[i].GetSize()-valuePos-2);
    cfg.SetString(key, value);
  }
  if(cfg.GetBoolean("Restore Defaults", false))
    cfg.DeleteSection();
     
  return PConfigPage::Post(request, data, reply);
     
}

PBoolean TablePConfigPage::OnPOST(PHTTPServer & server, const PHTTPConnectionInfo & connectInfo)
{
  PStringArray entityData = connectInfo.GetEntityBody().Tokenise("&");
  PString TableItemId = "";
  PINDEX num = 0;
  int integerMin=-2147483647, integerMax=2147483647, integerIndex=-1;
  PString curKey = "";
  for(PINDEX i = 0; i < entityData.GetSize(); i++) {
    PString item = PURL::UntranslateString(entityData[i], PURL::QueryTranslation);
    PString key = item.Tokenise("=")[0]; 
    PString value = "";
    PINDEX valuePos = item.Find("=");
    if(valuePos != P_MAX_INDEX)
      value = item.Right(item.GetSize()-valuePos-2);
    PString valueNext;
    if((i+1) < entityData.GetSize()) 
      valueNext = PURL::UntranslateString(entityData[i+1].Tokenise("=")[1], PURL::QueryTranslation);
    if(value == "FALSE" && valueNext == "TRUE") continue;
    if(key == "submit") continue;
    if(key == "MIN") { integerMin = atoi(value); continue; }
    if(key == "MAX") { integerMax = atoi(value); integerIndex=i+1; continue; }
    if(key == "TableItemId") { TableItemId = value; continue; }
    if(num == 1) curKey = key;
    PINDEX asize = dataArray.GetSize();
    if(key != curKey) {
      num = 0;
      dataArray.AppendString(value+"=");
    } 
    else {
      if(num != 1) dataArray[asize-1] += separator;
      if(integerIndex == (int)i) {
        int tmp = atoi(value);
        if(tmp < integerMin) tmp = integerMin;
        if(tmp > integerMax) tmp = integerMax;
        dataArray[asize-1] += PString(tmp);
      } 
      else {   
        dataArray[asize-1] += value;
      }
    }
    num++;
  }
     
  PConfigPage::OnPOST(server, connectInfo);
  return true;
}

GeneralPConfigPage::GeneralPConfigPage(OpalMCUEIE & app, const PString & title, 
                                                                  const PString & section, 
                                                                  const PHTTPAuthority & auth
                                                                  )
  : TablePConfigPage(app, title, section, auth)
{
  
  PConfig cfg(section);
  Manager & manager = OpalMCUEIE::Current().GetManager();
  PStringStream htmlBegin, htmlEnd, htmlPage, s, metaPage;
  s << BeginTable();

  // Reset section
  s << BoolField("RESTORE DEFAULTS", false);
  
  s << StringField("DisplayNameKey", cfg.GetString("DisplayNameKey",  
                                     manager.GetDefaultDisplayName()), 30 , 
                                     "Display usado en varios protocolos.");
                                     
  s << BoolField("OverrideProductInfoKey", cfg.GetBoolean("OverrideProductInfoKey",false), "Anula info"); 
  s << StringField("DeveloperNameKey",  cfg.GetString("DeveloperNameKey",
                                                      manager.GetProductInfo().vendor),
                                        30);
  s << StringField("ProductNameKey",    cfg.GetString("ProductNameKey",
                                                      manager.GetProductInfo().name),
                                        30);
  s << StringField("ProductVersionKey", cfg.GetString("ProductVersionKey",
                                                      manager.GetProductInfo().version),
                                        30);
  s << IntegerField("MaxSimultaneousCallsKey", cfg.GetInteger("MaxSimultaneousCallsKey", manager.GetMaxCalls()), 1,9999,
                                        15, "N° max de llamadas simultáneas.");
  s << SelectField("MediaTransferModeKey", cfg.GetString("MediaTransferModeKey", "1"),
                                           "0,1,2", 120,
                                           "Transferencia de media entre los terminales."
                                           " 0 = ByPass, 1 = Forward, 2 = Transcode.");
  
  
  OpalMediaTypeList mediaTypes = OpalMediaType::GetList();
  for (OpalMediaTypeList::iterator it = mediaTypes.begin(); it != mediaTypes.end(); ++it) {
    PString key = "Auto Start";
    key &= it->c_str();
    if (key == "Auto Start audio" || key == "Auto Start video" || key == "Auto Start presentation")
      s << SelectField(key, cfg.GetString(key, "3"), 
                            "0,1,2,3,4",
                            120,
                            "Inicio automático para tipo de media.\n0 = inactive, 1= Recive only, 2 = Send only, 3 = Send/Recive, 4 = Don't offer.");
  }
 
  s << IntegerField("MinJitterKey",  cfg.GetInteger("MinJitterKey", manager.GetMinAudioJitterDelay()), 20, 2000, 15,"Tamaño min de buffer jitter (ms).");
  s << IntegerField("MaxJitterKey",  cfg.GetInteger("MaxJitterKey", manager.GetMaxAudioJitterDelay()), 20, 2000, 15,"Tamaño max de buffer jitter (ms).");
  s << BoolField   ("InBandDTMFKey", cfg.GetBoolean("InBandDTMFKey", manager.DetectInBandDTMFDisabled()), 
                                     "Deshabilita filtro digital para detección in-band DTMF (reduce uso CPU).");
  s << StringField("SilenceDetectorKey", cfg.GetString("SilenceDetectorKey", "none") , 30,
                                         "Detector de Silencio (VAD), ej. adaptativo/fijo/ninguno.");
  s << IntegerField("NoMediaTimeoutKey", cfg.GetInteger("NoMediaTimeoutKey", manager.GetNoMediaTimeout().GetSeconds()), 1, 365*24*60*60, 
                                         15, "Termina llamada cuando no se recibe media desde remoto en este tiempo (segs)."); 
  s << IntegerField("TxMediaTimeoutKey", cfg.GetInteger("TxMediaTimeoutKey", manager.GetTxMediaTimeout().GetSeconds()), 1, 365*24*60*60,
                                         15, "Termina llamada cuando no se transmite media al remoto en este tiempo (segs)."); 
  
  s << IntegerField("TCPPortBaseKey", cfg.GetInteger("TCPPortBaseKey", manager.GetTCPPortBase()), 0, 65535,
                                         15, "Puerto base TCP para rango de puertos TCP."); 
  s << IntegerField("TCPPortMaxKey", cfg.GetInteger("TCPPortMaxKey", manager.GetTCPPortMax()), 0, 65535,
                                         15, "Puerto max TCP para rango de puertos TCP."); 
  
  s << IntegerField("RTPPortBaseKey", cfg.GetInteger("RTPPortBaseKey", manager.GetRtpIpPortBase()), 0, 65535,
                                         15, "Puerto base para rango de puertos RTP."); 
  
  s << IntegerField("RTPPortMaxKey", cfg.GetInteger("RTPPortMaxKey", manager.GetRtpIpPortMax()), 0, 65535,
                                         15, "Puerto max para rango de puertos RTP."); 
  
  s << IntegerField("RTPTOSKey", cfg.GetInteger("RTPTOSKey", manager.GetMediaTypeOfService()), 0, 255,
                                         15, "Valor para calidad de servicio (QoS)."); 
  s << SelectField("ConfVideoManagerKey", cfg.GetString("ConfVideoManagerKey", "CIF"), 
                            "SQCIF,QCIF,CIF,CIF4,HD480",
                            120,
                            "Resolución de video estándar del manager. SQCIF = 128x96, QCIF = 176x144," 
                            " CIF = 352x288, CIF4 = 704x576, HD480 = 704x480.");
  
  s << SelectField("ConfVideoMaxManagerKey", cfg.GetString("ConfVideoMaxManagerKey", "CIF16"), 
                            "CIF16,HD720p,HD1080",
                            120,
                            "Resolución de video máxima del manager. CIF16 = 1408x1152, HD720 = 1280x720," 
                            " HD1080 = 1920x1080.");
  s << IntegerField("FrameRateManagerKey", cfg.GetInteger("FrameRateManagerKey",15), 1,60,
                                           15, "Video Frame Rate, valor entre 1 y 60 fps.");
  
  s << StringField("BitRateManagerKey", cfg.GetString("BitRateManagerKey", "1Mbps") , 30,
                                        "Video Bit Rate, valor min 16kbps.");
  
  {
   PStringArray mediaFormatOrder = manager.GetMediaFormatOrder();
   PString mediaOrder;
   for (PINDEX i=0; i < mediaFormatOrder.GetSize() ; i++) {
	if(!mediaFormatOrder[i].IsEmpty()){
     mediaOrder += mediaFormatOrder[i] + ","; 
    }
   }      
   s << ArrayField("PreferredMediaKey",cfg.GetString("PreferredMediaKey",mediaOrder));
  }
  
  {
   PStringArray mediaFormatMask = manager.GetMediaFormatMask();
   PString mediaMask;
   for (PINDEX i=0; i < mediaFormatMask.GetSize() ; i++) {
	if(!mediaFormatMask[i].IsEmpty()){
     mediaMask += mediaFormatMask[i] + ","; 
    }
   }      
   s << ArrayField("RemovedMediaKey",cfg.GetString("RemovedMediaKey",mediaMask)); 
  }
  
#if OPAL_PTLIB_NAT
  PSYSTEMLOG(Info, "Configuring NAT");

  {
    std::set<NATInfo> natInfo;
    for (PNatMethods::iterator it = manager.GetNatMethods().begin(); it != manager.GetNatMethods().end(); ++it)
      natInfo.insert(*it);

    for (std::set<NATInfo>::iterator it = natInfo.begin(); it != natInfo.end(); ++it) {
      /*PHTTPCompositeField * fields = new PHTTPCompositeField("NAT\\" + it->m_method, it->m_method,
                                                             "Habilita flag y Servidor IP/hostname para NAT traversal usando " + it->m_friendly);*/
      
      s << BoolField(it->m_friendly+"NATActiveKey", cfg.GetBoolean(it->m_friendly+"NATActiveKey", it->m_active))
        << StringField(it->m_friendly+"NATInterfaceKey", cfg.GetString(it->m_friendly+"NATInterfaceKey",it->m_interface));
      PINDEX index = cfg.GetString(it->m_friendly+"NATServerKey",it->m_server).Find("http");
      PString server = cfg.GetString(it->m_friendly+"NATServerKey",it->m_server).Mid(index);  
      s << StringFieldUrl(it->m_friendly+"NATServerKey", server);
     }
  }
#endif // OPAL_PTLIB_NAT
  
  // Language
  //s << SelectField("Language", cfg.GetString("Language"), ",EN,JP,RU,UK");
  // OpenMCU Server Id
  //s << StringField("OpalMCUEIE Server Id", cfg.GetString("OpalMCUEIE Server Id",  OpalMCUEIE::Current().GetName()+" v"+ OpalMCUEIE::Current().GetVersion()), 35);

  s << SeparatorField("Port setup");
  // HTTP Port number to use.
  s << SeparatorField();
  s << IntegerField("HttpPortKey", cfg.GetInteger("HttpPortKey", DefaultHTTPPort), 1, 32767);
  
  s << SeparatorField("Log setup");
#if PTRACING
  // Trace level
  s << SelectField("TraceLevelKey", cfg.GetString("TraceLevelKey", 0), "0,1,2,3,4,5,6,7,8,9", 120, "0=No tracing ... 6=Very detailed");
  s << IntegerField("TraceRotateKey", cfg.GetInteger("TraceRotateKey", 0), 0, 200, 10, "0 (don't rotate) ... 200");
#endif
#ifdef SERVER_LOGS
  // Log level for messages
  s << SelectField("LogLevelKey", cfg.GetString("LogLevelKey", 0), "0,1,2,3,4,5", 120, "1=Fatal only, 2=Errors, 3=Warnings, 4=Info, 5=Debug");
  // Log filename
  s << StringField("CallLogFilenameKey", app.m_logFilename, 35);
#endif
  
#if OPAL_VIDEO
  s << SeparatorField("Video setup");
  
#endif

  s << SeparatorField("Room setup");
  // Default room
  s << StringField("CDRTextFileKey", cfg.GetString("CDRTextFileKey", app.GetManager().m_cdrTextFile.GetFilePath()));
  s << StringField("CDRTextHeadingsKey", cfg.GetString("CDRTextHeadingsKey", PString::Empty()));
  s << StringField("CDRTextFormatKey", cfg.GetString("CDRTextFormatKey",PString::Empty()));
  
  s << StringField("DefaultRoomKey", cfg.GetString("DefaultRoomKey", "OPALSERVER"));
  // create/don't create empty room with default name at start
  s << BoolField("CreateEmptyRoomKey", cfg.GetBoolean("CreateEmptyRoomKey", false));
  // recall last template after room created
   // reject duplicate name
  s << BoolField("RejectDuplicateNameKey", cfg.GetBoolean("RejectDuplicateNameKey", false), "Reject duplicate participants (Rename if unchecked)");
  // get conference time limit 
  s << IntegerField("DefaultRoomTimeLimitKey", cfg.GetInteger("DefaultRoomTimeLimitKey", 0), 0, 10800);
  s << IntegerField("CDRWebPageLimitKey", cfg.GetInteger("CDRWebPageLimitKey", 1), 1, 1000000);
  // allow/disallow self-invite:
  s << BoolField("AllowLoopbackCallsKey", cfg.GetBoolean("AllowLoopbackCallsKey", false))
  // auto delete empty rooms:
    << BoolField("AutoDeleteRoomKey", app.m_autoDeleteRoom, "Auto delete room when last participant disconnected")
  ;

  s << EndTable();
  
  
  BuildHTML("");
  BeginPage(htmlBegin,metaPage, section, "window.l_param_general","window.l_info_param_general");
  EndPage(htmlEnd,  OpalMCUEIE::Current().GetHtmlCopyright());
  htmlPage << htmlBegin << s << htmlEnd;
  m_string = htmlPage;
}

H323PConfigPage::H323PConfigPage(OpalMCUEIE & app, const PString & title, 
                                                           const PString & section, 
                                                           const PHTTPAuthority & auth
                                                           )
  : TablePConfigPage(app, title, section, auth)
{
  
  PConfig cfg(section);
  MCUH323EndPoint & h323Ep = OpalMCUEIE::Current().GetManager().GetMCUH323EndPoint();
  PStringStream htmlBegin, htmlEnd, htmlPage, s, metaPage;
  s << BeginTable();
  s << BoolField("RESTORE DEFAULTS", false);
  s << BoolField("H323EnableKey", cfg.GetBoolean("H323EnableKey",true), "Habilita/deshabilita protocolo H.323"); 
  s << ArrayField("H323ListenerKey",cfg.GetString("H323ListenerKey",""),12,"Interfaces y puertos de red local para terminales. ");
  
  {
   PStringArray aliasArray = h323Ep.m_configuredAliases;
   PString alias;
   for(PINDEX i=0; i < aliasArray.GetSize(); i++){
     alias += aliasArray[i] + ",";
   }
   
   s << ArrayField("H323AliasesKey",cfg.GetString("H323AliasesKey",h323Ep.GetAliasNames()[0]),12,"Alias H.323 para usuario local.");
  }
  
  s << BoolField("DisableFastStartKey", cfg.GetBoolean("DisableFastStartKey",h323Ep.IsFastStartDisabled()), "Deshabilita H.323 Fast Connect."); 
  s << BoolField("DisableH245TunnelingKey", cfg.GetBoolean("DisableH245TunnelingKey",h323Ep.IsH245TunnelingDisabled()), "Deshabilita  H.245 Tunneling en canal de señalización H.225.0."); 
  s << BoolField("DisableH245inSetupKey", cfg.GetBoolean("DisableH245inSetupKey",h323Ep.IsH245inSetupDisabled()), "Deshabilita H.245 in SETUP PDU."); 
  s << BoolField("ForceSymmetricTCSKey", cfg.GetBoolean("ForceSymmetricTCSKey",h323Ep.IsForcedSymmetricTCS()), "Forza indicación de codecs simétricos en TCS."); 
  s << BoolField("H239ControlKey", cfg.GetBoolean("H239ControlKey",false), "Habilita control H.239."); 
  
  s << IntegerField("H323BandwidthKey", cfg.GetInteger("H323BandwidthKey",h323Ep.GetInitialBandwidth(OpalBandwidth::RxTx)/1000), 1,OpalBandwidth::Max()/1000,
                                           15, "Ancho de banda requerido por el gatekeeper para originar/recibir llamadas (kb/s).");
  
  
  
  s << EndTable();
  
  
  BuildHTML("");
  BeginPage(htmlBegin,metaPage, section, "window.l_param_general","window.l_info_param_general");
  EndPage(htmlEnd,  OpalMCUEIE::Current().GetHtmlCopyright());
  htmlPage << htmlBegin << s << htmlEnd;
  m_string = htmlPage;
}

ConferencePConfigPage::ConferencePConfigPage(OpalMCUEIE & app, const PString & title, 
                                                                        const PString & section, 
                                                                        const PHTTPAuthority & auth
                                                                        )
  : TablePConfigPage(app, title, section, auth)
{
  
  PConfig cfg(section);
  ConferenceManager & cmanager = OpalMCUEIE::Current().GetManager().GetConferenceManager();
  PStringStream htmlBegin, htmlEnd, htmlPage, s, metaPage;
  s << BeginTable();
  s << BoolField("RESTORE DEFAULTS", false);
  s << BoolField("ConfMediaPassThruKey", cfg.GetBoolean("ConfMediaPassThruKey",cmanager.m_adHoc.m_mediaPassThru), "Optimiza transferencia de media en conferencia."); 
  s << BoolField("ConfAudioOnlyKey", cfg.GetBoolean("ConfAudioOnlyKey",cmanager.m_adHoc.m_audioOnly), "Deshabilita video en conferencia, solo audio."); 
  s << StringField("ConfVideoResolutionKey",cfg.GetString("ConfVideoResolutionKey",PVideoFrameInfo::AsString(cmanager.m_adHoc.m_width, cmanager.m_adHoc.m_height)), 30 ,
                   "Resolución del frame de video en videoconferencia.");
  
  s << BoolField("RESTORE DEFAULTS", false);
  s << EndTable();
  
  
  BuildHTML("");
  BeginPage(htmlBegin,metaPage, section, "window.l_param_general","window.l_info_param_general");
  EndPage(htmlEnd,  OpalMCUEIE::Current().GetHtmlCopyright());
  htmlPage << htmlBegin << s << htmlEnd;
  m_string = htmlPage;
}

ControlRoomPage::ControlRoomPage(OpalMCUEIE & app, PHTTPAuthority & auth)
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
  
  ConferenceManager & cm = OpalMCUEIE::Current().GetManager().GetConferenceManager();

  if(data.Contains("action") && !data("room").IsEmpty()) {

    PString action = data("action");
    PString room = data("room");
    if(action == "create")
    {
      cm.CreateConferenceRoom(room);
    }
    else if(action == "delete")
    {
      cm.RemoveConferenceRoom(room);
    }
  }

  PStringStream html, meta;
  BeginPage(html, meta, "Rooms", "window.l_rooms", "window.l_info_rooms");

  if(data.Contains("action")) html << "<script language='javascript'>location.href='Select';</script>";

  PString nextRoom = "OpalServer";

  html
    << "<form method=\"post\" onsubmit=\"javascript:{if(document.getElementById('newroom').value!='')location.href='?action=create&room='+encodeURIComponent(document.getElementById('newroom').value);return false;}\"><input name='room' id='room' type=hidden>"
    << "<table id=\"Table\" class=\"table table-striped table-bordered table-condensed\">"

    << "<tr>"
    << "<td colspan='7'><input type='text' class='input-small' name='newroom' id='newroom' value='" << nextRoom 
    << "' /><input type='button' class='btn btn-large btn-info' value='Crear Sala de Conferencia' onclick=\"location.href='?action=create&room='+encodeURIComponent(document.getElementById('newroom').value);\"></td>"
    << "</tr>"

    << "<tr>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_enter);</script><br></th>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_visible);</script><br></th>"
    //<< "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_record);</script><br></th>"
    << "<th style='text-align:center'><script type=\"text/javascript\">document.write(window.l_select_delete);</script><br></th>"
    << "</tr>";

    ConferenceManager::NodesByName & nodesByName = cm.GetNodesByName();
    ConferenceManager::NodesByName::const_iterator r;
    for (r = nodesByName.begin(); r != nodesByName.end(); ++r){
      OpalMixerNode * node = r->second;
      
      bool controlled = true;
      PString roomNumber = node->GetNodeInfo().m_name;
      bool moderated=false; 
      PString charModerated = "-";
      if(controlled) { 
	    charModerated = ((ConferenceNode*)node)->IsModerated(); 
	    moderated = (charModerated == "+"); 
	  }
      if(charModerated=="-") 
        charModerated = "<script type=\"text/javascript\">document.write(window.l_select_moderated_no);</script>";
      else 
        charModerated = "<script type=\"text/javascript\">document.write(window.l_select_moderated_yes);</script>";
      //PINDEX   visibleMemberCount = conference.GetVisibleMemberCount();
      //PINDEX unvisibleMemberCount = conference.GetMemberCount() - visibleMemberCount;
      
      PString roomButton = "<span class=\"btn btn-large btn-";
      if(moderated) 
        roomButton += "success";
      else if(controlled) 
        roomButton += "primary";
      else 
        roomButton += "inverse";
      roomButton += "\"";
      if(controlled) 
        roomButton += " onclick='document.getElementById(\"room\").value=\""
        + roomNumber + "\";document.forms[0].submit();'";
        roomButton += ">" + roomNumber + "</span>";

      html << "<tr>"
           << "<td style='text-align:left'>"   << roomButton                            << "</td>"
        /*<< "<td style='text-align:right'>"
      for (PSafePtr<OpalConnection> connection = node->GetFirstConnection(); connection != NULL; ++connection) {
      html <<  connection->GetCall().GetPartyB() << '\n'; }*/
           << "<td style='text-align:left'><a href='Invitar'>"+  node->GetNodeInfo().m_name  +"</a></td>"
        << "</td>"
      //<< "<td style='text-align:center'>" << recordButton                            << "</td>"
      << "<td style='text-align:center'><span class=\"btn btn-large btn-danger\" onclick=\"if(confirm('Вы уверены? Are you sure?')){ location.href='?action=delete&room="+node->GetNodeInfo().m_name+"';}\">X</td>"
        << "</tr>";
    }
  html << "</table></form>";

  EndPage(html, OpalMCUEIE::Current().GetHtmlCopyright());
  
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
  
  msg << OpalMCUEIE::Current().GetManager().GetConferenceManager().SetRoomParams(data);
  return true;
  
  
}



InteractiveHTTP::InteractiveHTTP(OpalMCUEIE & app, PHTTPAuthority & auth)
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
  message << OpalMCUEIE::Current().HttpStartEventReading(idx,room);

//PTRACE(1,"!!!!!\tsha1('123')=" << PMessageDigestSHA1::Encode("123")); // sha1 works!! I'll try with websocket in future

  if(room!="")
  {
    ConferenceManager::NodesByName & nodesByName = OpalMCUEIE::Current().GetManager().GetConferenceManager().GetNodesByName();
    ConferenceManager::NodesByName::const_iterator r;
    for (r = nodesByName.begin(); r != nodesByName.end(); ++r)
     if(r->second->GetNodeInfo().m_name == room) break;
    if(r != nodesByName.end() )
    {
      OpalMixerNode * node = r->second;
      message << "<script>p.splitdata=Array(";
      for (unsigned i=0;i<OpalMCUEIE::vmcfg.vmconfs;i++)
      {
        PString split=OpalMCUEIE::vmcfg.vmconf[i].splitcfg.Id;
        split.Replace("\"","\\x22",true,0);
        if(i!=0) message << ",";
        message << "\"" << split << "\"";
      }
      message << ");\np.splitdata2={";
      for (unsigned i=0;i<OpalMCUEIE::vmcfg.vmconfs;i++)
      {
        PString split=OpalMCUEIE::vmcfg.vmconf[i].splitcfg.Id;
        split.Replace("\"","\\x22",true,0);
        message << "\"" << split << "\"" << ":[" << OpalMCUEIE::vmcfg.vmconf[i].splitcfg.vidnum;
        for(unsigned j=0;j<OpalMCUEIE::vmcfg.vmconf[i].splitcfg.vidnum; j++)
        {
          VMPCfgOptions & vo=OpalMCUEIE::vmcfg.vmconf[i].vmpcfg[j];
          message << ",[" << vo.posx << "," << vo.posy << "," << vo.width << "," << vo.height << "," << vo.border << "," << vo.label_mask << "]";
        }
        message << "]";
        if(i+1<OpalMCUEIE::vmcfg.vmconfs) message << ",";
      }
      message << "};\n"
        << "p." << OpalMCUEIE::Current().GetManager().GetConferenceManager().GetMemberListOptsJavascript(node) << "\n"
        << "p." << OpalMCUEIE::Current().GetManager().GetConferenceManager().GetConferenceOptsJavascript(node) << "\n"
        //<< "p.tl=Array" << conference.GetTemplateList() << "\n"
        //<< "p.seltpl=\"" << conference.GetSelectedTemplateName() << "\"\n"
        << "p.build_page();\n"
        << "</script>\n";
    }
    else
    { // no (no more) room -- redirect to /
      
      message << "<script>top.location.href='/';</script>\n";
      server.Write((const char*)message,message.GetLength());
      server.flush();
      return false;
    }
    
  }
   
  while(server.Write((const char*)message,message.GetLength())) {
    server.flush();
    int count=0;
    message = OpalMCUEIE::Current().HttpGetEvents(idx,room);
    while (message.GetLength()==0 && count < 20){
      count++;
      PThread::Sleep(100);
      message = OpalMCUEIE::Current().HttpGetEvents(idx,room);
    }
    if(message.Find("<script>")==P_MAX_INDEX) message << "<script>p.alive()</script>\n";
  }
  return false;
 }
////////////////////////////////////////////////////////////////////////////////////////// 
MCUVideoMixer* jpegMixer;

void jpeg_init_destination(j_compress_ptr cinfo)
{
  if(jpegMixer->myjpeg.GetSize()<32768)
    jpegMixer->myjpeg.SetSize(32768);
  cinfo->dest->next_output_byte=&jpegMixer->myjpeg[0];
  cinfo->dest->free_in_buffer=jpegMixer->myjpeg.GetSize();
}

boolean jpeg_empty_output_buffer(j_compress_ptr cinfo)
{
  PINDEX oldsize=jpegMixer->myjpeg.GetSize();
  jpegMixer->myjpeg.SetSize(oldsize+16384);
  cinfo->dest->next_output_byte = &jpegMixer->myjpeg[oldsize];
  cinfo->dest->free_in_buffer = jpegMixer->myjpeg.GetSize() - oldsize;
  return true;
}

void jpeg_term_destination(j_compress_ptr cinfo)
{
  jpegMixer->jpegSize=jpegMixer->myjpeg.GetSize() - cinfo->dest->free_in_buffer;
  jpegMixer->jpegTime=(long)time(0);
}

JpegFrameHTTP::JpegFrameHTTP(OpalMCUEIE & app, PHTTPAuthority & auth)
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
  
  PString room=data("room"); 
  if (room.GetLength()==0) 
    return false;

  int width=atoi(data("w"));
  int height=atoi(data("h"));

  unsigned requestedMixer=0;
  if(data.Contains("mixer")) 
    requestedMixer=(unsigned)data("mixer").AsInteger();

  const unsigned long t1=(unsigned long)time(0);

//  PWaitAndSignal m(mutex); // no more required: the following mutex will do the same:
  //app.GetManager().GetConferenceManager().GetConferenceListMutex().Wait(); // fix it - browse read cause access_v on serv. stop

  ConferenceManager & cm = OpalMCUEIE::Current().GetManager().GetConferenceManager();
  ConferenceManager::NodesByName & nodesByName = cm.GetNodesByName();
  ConferenceManager::NodesByName::const_iterator r;
  for (r = nodesByName.begin(); r != nodesByName.end(); ++r){
     OpalMixerNode & node = *(r->second);
    if(node.GetNodeInfo().m_name == room)
    { 
      if(((ConferenceNode&)node).m_videoMixerList == NULL){ 
	    //app.GetManager().GetConferenceManager().GetConferenceListMutex().Signal(); 
	    return false; 
	  }
      //PWaitAndSignal m3(conference.videoMixerListMutex);
      jpegMixer=((ConferenceNode&)node).VMLFind(requestedMixer);
      if(jpegMixer==NULL) { 
	    //app.GetManager().GetConferenceManager().GetConferenceListMutex().Signal(); 
		return false; 
      }
      if(t1-(jpegMixer->jpegTime)>1)
      {
        if(width<1||height<1||width>2048||height>2048)
        { width=OpalMCUEIE::vmcfg.vmconf[jpegMixer->GetPositionSet()].splitcfg.mockup_width;
          height=OpalMCUEIE::vmcfg.vmconf[jpegMixer->GetPositionSet()].splitcfg.mockup_height;
        }
        struct jpeg_compress_struct cinfo; 
        struct jpeg_error_mgr jerr;
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

        jpegMixer->ReadMixedFrame((void*)videoData,width,height,amount);
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
        jpeg_start_compress(&cinfo,true);
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

      //app.GetManager().GetConferenceManager().GetConferenceListMutex().Signal();
      server.flush();
      return false;
    }
  }
  //app.GetManager().GetConferenceManager().GetConferenceListMutex().Signal();
  return false;
}

InvitePage::InvitePage(OpalMCUEIE & app, PHTTPAuthority & auth)
  : PServiceHTTPString("Invitar", "", "text/html; charset=utf-8", auth)
  , app(app)
{
}

void InvitePage::CreateHTML(PHTML & html)
{
  PStringStream meta;
  
  ConferenceManager & mixer = OpalMCUEIE::Current().GetManager().GetConferenceManager();
  PStringArray myAddressBook = mixer.GetAddressBook();
  PHTML html_page;
  BeginPage(html_page, meta, "Invite", "window.l_invite", "window.l_info_invite");
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

  EndPage(html_page,OpalMCUEIE::Current().GetHtmlCopyright());
  html = html_page;
}

PBoolean InvitePage::OnGET (PHTTPServer & server, const PHTTPConnectionInfo & connectInfo)
{
  { PHTTPRequest * req = CreateRequest(server, connectInfo);
    if(!CheckAuthority(server, *req, connectInfo)) {
      delete req; return FALSE;
    }
    delete req;
  }

  PStringToString data;
  { PString request=connectInfo.GetURL().AsString(); 
    PINDEX q;
    if((q=request.Find("?"))!=P_MAX_INDEX) { 
      request=request.Mid(q+1,P_MAX_INDEX); 
      PURL::SplitQueryVars(request,data); 
    }
  }

  PHTML html;
  CreateHTML(html);

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

PBoolean InvitePage::Post(PHTTPRequest & request,
                          const PStringToString & data,
                          PHTML & msg)
{
  PStringStream meta;
  PString room    = data("room");
  PString address = data("address");
  PString book    = data("book");
  PString token;

  ConferenceManager & mixer = OpalMCUEIE::Current().GetManager().GetConferenceManager();
  PStringArray addressBook = mixer.GetAddressBook();

  if(address.IsEmpty() || room.IsEmpty()) {
    BeginPage(msg, meta, "Invite", "window.l_invite", "window.l_info_invite");
    msg.Set(PHTML::InBody);
    msg << "<b>Invitacion fallida, no existe sala de conferencia o direccion de remoto</b>"
        << PHTML::Paragraph()
        << PHTML::HotLink(request.url.AsString()) << "Reload page" << PHTML::HotLink();
    EndPage(msg, OpalMCUEIE::Current().GetHtmlCopyright());
    return true;
  }
  else {
   mixer.GetAddressBook().push_back(address);
  }

  //PSafePtr<OpalMixerNode> node = mixer.FindNode(room);
  ConferenceManager::NodesByName & nodesByName = mixer.GetNodesByName();
  ConferenceManager::NodesByName::const_iterator r = nodesByName.find(room);
  if(r == nodesByName.end()) return false; 
  
  OpalMixerNode * node = r->second;
  if (node == NULL) {
    cout << "Conference \"" << room << "\" does not exist" << endl;
  }

  if (OpalMCUEIE::Current().GetManager().SetUpCall("mcu:"+node->GetGUID().AsString(), address, token)){
    MCUVideoMixer * mixer = ((ConferenceNode*)node)->m_videoMixerList->mixer;
    for(PINDEX pos = 0; pos < node->GetConnectionCount(); ++pos){
        if(!mixer->AddVideoSource((void*)pos,node))
          break;
    }
    cout << "Adding new member \"" << address << "\" to conference " << *node << endl;
  }
  else
    cout << "Could not add new member \"" << address << "\" to conference " << *node << endl;

  CreateHTML(msg);

  return true;
}

HomePage::HomePage(OpalMCUEIE & app, PHTTPAuthority & auth)
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

  html << "<br><b>Info del servidor (<span style='cursor:pointer;text-decoration:underline' onclick='javascript:{if(document.selection){var range=document.body.createTextRange();range.moveToElementText(document.getElementById(\"monitorTextId\"));range.select();}else if(window.getSelection){var range=document.createRange();range.selectNode(document.getElementById(\"monitorTextId\"));window.getSelection().addRange(range);}}'>seleccionar todo</span>)</b><div style='padding:5px;border:1pxdotted #595;width:100%;height:auto;max-height:300px;overflow:auto'><pre style='margin:0px;padding:0px' id='monitorTextId'>"
       << "Inicio del servidor: "  << OpalMCUEIE::Current().GetStartTime().AsString(timeFormat) << "\n"
       << "Host local: "           << PIPSocket::GetHostName() << "\n"
       << "Direccion local: "      << localAddr << "\n"
       << "Puerto local: "         << localPort << "\n"
       << app.GetManager().GetMonitorText()
       << "</pre></div>";

  EndPage(html, OpalMCUEIE::Current().GetHtmlCopyright());
  m_string = html;
} 
