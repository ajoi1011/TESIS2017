/*
 * managers.cxx
 * 
*/

#include "config.h"
#include "html.h"
#include "h323.h"

Manager::Manager()
  : OpalManagerConsole(OPAL_PREFIX_H323 " " OPAL_PREFIX_SIP " " OPAL_PREFIX_MIXER)
  , m_savedProductInfo(GetProductInfo())
  , m_verbose(false)
  , m_maxCalls(9999)
  , m_mediaTransferMode(MediaTransferForward)
  , m_cdrListMax(100)
{
  OpalMediaFormat::RegisterKnownMediaFormats(); 
  DisableDetectInBandDTMF(false);

#if OPAL_VIDEO
  for (OpalVideoFormat::ContentRole role = OpalVideoFormat::BeginContentRole; role < OpalVideoFormat::EndContentRole; ++role)
    m_videoOutputDevice[role].deviceName = P_NULL_VIDEO_DEVICE;

  PStringArray devices = PVideoOutputDevice::GetDriversDeviceNames("*");
  PINDEX i;
  for (i = 0; i < devices.GetSize(); ++i) {
    PCaselessString dev = devices[i];
    if (dev[0] != '*' && dev != P_NULL_VIDEO_DEVICE) {
      m_videoOutputDevice[OpalVideoFormat::ePresentation].deviceName = dev;
      SetAutoStartReceiveVideo(true);
      PTRACE(3, "Default video output/preview device set to \"" << devices[i] << '"');
      break;
    }
  }
#endif // OPAL_VIDEO

  
}

OpalCall * Manager::CreateCall(void *)
{
  if (m_activeCalls.GetSize() < m_maxCalls) { 
    return new MCUCall(*this);
  }

  PTRACE(2, "Maximum simultaneous calls (" << m_maxCalls << ") exceeeded.");
  return NULL;
}

void Manager::EndRun(bool)
{
  PServiceProcess::Current().OnStop();
}

void Manager::PrintVersion() const 
{
  const PProcess & process = PProcess::Current();
  cout << "\n**************************************************\n*" 
       << process.GetName()
       << "\n*Version: " << process.GetVersion(true) << "\n*Desarrollador: " 
       << process.GetManufacturer() << "\n*S.O: " << process.GetOSClass() << ' ' 
       << process.GetOSName() << " (" << process.GetOSVersion() << '-' 
       << process.GetOSHardware()
       << ")\n*Librerias: PTLib v" << PProcess::GetLibVersion() << " y OPAL v" 
       << OpalGetVersion()
       << "\n*Trabajo Final de Grado \n**************************************************" << endl;
}

bool Manager::PreInitialise(PArgList & args, bool verbose)
{
  PrintVersion();
  PTRACE_INITIALISE(args);
  m_verbose = verbose;
  return true;
}

PBoolean Manager::Configure(PConfig & cfg)
{
  for (PINDEX i = 0; i < m_endpointPrefixes.GetSize(); ++i)
    GetConsoleEndPoint(m_endpointPrefixes[i]);
  
  cfg.SetDefaultSection("Generals");
  PString section = cfg.GetDefaultSection();

  PSYSTEMLOG(Info, "Configuring Globals");

  SetDefaultDisplayName(cfg.GetString("Generals", "DisplayNameKey", GetDefaultDisplayName()));
  bool overrideProductInfo = cfg.GetBoolean("Generals", "OverrideProductInfoKey", false);
  m_savedProductInfo.vendor = cfg.GetString("Generals", "DeveloperNameKey", m_savedProductInfo.vendor);
  m_savedProductInfo.name = cfg.GetString("Generals", "ProductNameKey", m_savedProductInfo.name);
  m_savedProductInfo.version = cfg.GetString("Generals", "ProductVersionKey", m_savedProductInfo.version);
  if (overrideProductInfo) 
    SetProductInfo(m_savedProductInfo);

  m_maxCalls = cfg.GetInteger("Generals","MaxSimultaneousCallsKey", m_maxCalls);
  
  m_mediaTransferMode = (OpalManager::MediaTransferMode)cfg.GetInteger("Generals","MediaTransferModeKey", 1);
  
  {
    OpalMediaTypeList mediaTypes = OpalMediaType::GetList();
    for (OpalMediaTypeList::iterator it = mediaTypes.begin(); it != mediaTypes.end(); ++it) {
      PString key = "Auto Start";
      key &= it->c_str();

      if (key == "Auto Start audio" || key == "Auto Start video" || key == "Auto Start presentation") {
      (*it)->SetAutoStart((OpalMediaType::AutoStartMode::Enumeration)cfg.GetInteger("Generals", key,3));
      }
    }
  }

  SetAudioJitterDelay(cfg.GetInteger("Generals","MinJitterKey", GetMinAudioJitterDelay()),
                      cfg.GetInteger("Generals","MaxJitterKey", GetMaxAudioJitterDelay()));

  DisableDetectInBandDTMF(cfg.GetBoolean("Generals", "InBandDTMFKey", DetectInBandDTMFDisabled()));

  OpalSilenceDetector::Params params = GetSilenceDetectParams();
  PCaselessString vad = cfg.GetString("Generals","SilenceDetectorKey","none");
  if (vad == "adaptativo")
    params.m_mode = OpalSilenceDetector::AdaptiveSilenceDetection;
  else if (vad == "fijo")
    params.m_mode = OpalSilenceDetector::FixedSilenceDetection;
  else
    params.m_mode = OpalSilenceDetector::NoSilenceDetection;
  SetSilenceDetectParams(params);
  
  SetNoMediaTimeout(PTimeInterval(0, cfg.GetInteger("Generals", "NoMediaTimeoutKey",GetNoMediaTimeout().GetSeconds())));
  SetTxMediaTimeout(PTimeInterval(0, cfg.GetInteger("Generals","TxMediaTimeoutKey", GetTxMediaTimeout().GetSeconds())));

  SetTCPPorts(cfg.GetInteger("Generals","TCPPortBaseKey", GetTCPPortBase()),
              cfg.GetInteger("Generals","TCPPortMaxKey",GetTCPPortMax()));
              
  SetRtpIpPorts(cfg.GetInteger("Generals","RTPPortBaseKey", GetRtpIpPortBase()),
                cfg.GetInteger("Generals","RTPPortMaxKey",GetRtpIpPortMax()));

  SetMediaTypeOfService(cfg.GetInteger("Generals","RTPTOSKey", GetMediaTypeOfService()));

#if OPAL_PTLIB_NAT
  
  {
    std::set<NATInfo> natInfo;
    for (PNatMethods::iterator it = GetNatMethods().begin(); it != GetNatMethods().end(); ++it)
      natInfo.insert(*it);

    for (std::set<NATInfo>::iterator it = natInfo.begin(); it != natInfo.end(); ++it) {
      PINDEX index = cfg.GetString("Generals", it->m_friendly+"NATServerKey",it->m_server).Find("http");
      PString server = cfg.GetString("Generals", it->m_friendly+"NATServerKey",it->m_server).Mid(index);
      
      SetNATServer(it->m_method, server, cfg.GetBoolean("Generals", it->m_friendly+"NATActiveKey", it->m_active), 
                   0, cfg.GetString("Generals", it->m_friendly+"NATInterfaceKey",it->m_interface));
    }
  }
  
#endif // OPAL_PTLIB_NAT

#if OPAL_VIDEO
  {
    unsigned prefWidth = 0, prefHeight = 0;
    static const char * const StandardSizes[] = { "SQCIF", "QCIF", "CIF", "CIF4", "HD480" };

    m_prefVideo = cfg.GetString("Generals","ConfVideoManagerKey", "CIF");
    
    for (PINDEX i = 0; i < PARRAYSIZE(StandardSizes); ++i) {
      if (m_prefVideo == StandardSizes[i]) {
        PVideoFrameInfo::ParseSize(StandardSizes[i], prefWidth, prefHeight);
      }
    }

    unsigned maxWidth = 0, maxHeight = 0;
    static const char * const MaxSizes[] = { "CIF16", "HD720", "HD1080" };
    m_maxVideo = cfg.GetString("Generals","ConfVideoMaxManagerKey","CIF16");
    
    for (PINDEX i = 0; i < PARRAYSIZE(MaxSizes); ++i) {
      if (m_maxVideo == MaxSizes[i]) {
        PVideoFrameInfo::ParseSize(MaxSizes[i], maxWidth, maxHeight);
      }
    } 

    m_rate = cfg.GetInteger("Generals","FrameRateManagerKey", 15);
    if (m_rate < 1 || m_rate > 60) {
      PTRACE(2, "Invalid video frame rate parameter");
    }

    unsigned frameTime = (unsigned)(OpalMediaFormat::VideoClockRate/m_rate);
    m_bitrate = cfg.GetString("Generals","BitRateManagerKey", "1Mbps");
    if (m_bitrate < 16000) {
      PTRACE(2, "Invalid video bit rate parameter");
    }

    OpalMediaFormatList formats = OpalMediaFormat::GetAllRegisteredMediaFormats();
    for (OpalMediaFormatList::iterator it = formats.begin(); it != formats.end(); ++it) {
      if (it->GetMediaType() == OpalMediaType::Video()) {
        OpalMediaFormat format = *it;
        format.SetOptionInteger(OpalVideoFormat::FrameWidthOption(), prefWidth);
        format.SetOptionInteger(OpalVideoFormat::FrameHeightOption(), prefHeight);
        format.SetOptionInteger(OpalVideoFormat::MaxRxFrameWidthOption(), maxWidth);
        format.SetOptionInteger(OpalVideoFormat::MaxRxFrameHeightOption(), maxHeight);
        format.SetOptionInteger(OpalVideoFormat::FrameTimeOption(), frameTime);
        format.SetOptionInteger(OpalVideoFormat::TargetBitRateOption(), m_bitrate);
        OpalMediaFormat::SetRegisteredMediaFormat(format);
      }
    }
  }
#endif    

  /*OpalMediaFormatList allFormats;
    PList<OpalEndPoint> endpoints = GetEndPoints();
    for (PList<OpalEndPoint>::iterator it = endpoints.begin(); it != endpoints.end(); ++it)
      allFormats += it->GetMediaFormats();
    OpalMediaFormatList transportableFormats;
    for (OpalMediaFormatList::iterator it = allFormats.begin(); it != allFormats.end(); ++it) {
      if (it->IsTransportable())
        transportableFormats += *it;
    }
    PStringStream help;
    help << "Orden de codecs preferidos para ofrecer a terminales remotos.<p>"
      "Formatos de codecs:<br>";
    for (OpalMediaFormatList::iterator it = transportableFormats.begin(); it != transportableFormats.end(); ++it) {
      if (it != transportableFormats.begin())
        help << ", ";
      help << *it;
      
    }Formatos de codecs: Orden de codecs preferidos para ofrecer a terminales remotos.
     UserInput/RFC2833, NamedSignalEvent, MSRP, SIP-IM, T.140, FECC-RTP, FECC-HDLC, G.711-uLaw-64k, G.711-ALaw-64k, RFC4175_YCbCr-4:2:0, RFC4175_RGB, GSM-AMR, Opus-8, Opus-8S, Opus-12, Opus-12S, Opus-16, Opus-16S, Opus-24, Opus-24S, Opus-48, Opus-48S, SILK-8, SILK-16, iSAC-16kHz, iSAC-32kHz, SpeexIETFNarrow-5.95k, SpeexIETFNarrow-8k, SpeexIETFNarrow-11k, SpeexIETFNarrow-15k, SpeexIETFNarrow-18.2k, SpeexIETFNarrow-24.6k, SpeexIETFWide-20.6k, SpeexWNarrow-8k, SpeexWide-20.6k, SpeexNB, SpeexWB, iLBC, iLBC-13k3, iLBC-15k2, G.726-40k, G.726-32k, G.726-24k, G.726-16k, G.722-64k, G.722.2, LPC-10, VP8-WebM, VP8-OM, MPEG4, H.261, H.263, H.263plus, theora, H.264-0, H.264-1, H.264-High, H.264-F, xH.264-0, xH.264-1, xH.264-High, xH.264-F, G.722.1-24K, G.722.1-32K, G.728, G.729, G.729A, G.729B, G.729A/B, G.723.1, G.723.1(5.3k), G.723.1A(6.3k), G.723.1A(5.3k), G.723.1-Cisco-a, G.723.1-Cisco-ar, GSM-06.10, T.38, T.38-RTP, rtx-audio, rtx-video, rtx-H.224, rtx-im-sip, rtx-im-t140, rtx-presentation, rtx-audio, rtx-video, rtx-H.224, rtx-im-sip, rtx-im-t140, rtx-presentation, rtx-audio, rtx-video, rtx-H.224, rtx-im-sip, rtx-im-t140, rtx-presentation
    cout << help << endl;*/
  
  {
   PConfig config("Generals");
   PStringArray order = config.GetString("PreferredMediaKey").Tokenise(",");
   
   if(order.IsEmpty()){
    SetMediaFormatOrder(GetMediaFormatOrder());
   }
   else {
     SetMediaFormatOrder(order);
   }
 
   PStringArray mask = config.GetString("RemovedMediaKey").Tokenise(",");
   if(mask.IsEmpty()) {
    SetMediaFormatMask(GetMediaFormatMask());
   }
   else {
     SetMediaFormatMask(mask);
   }
   
  } 
  
  
#if OPAL_H323
  PSYSTEMLOG(Info, "Configuring H.323");
  if (!GetMCUH323EndPoint().Configure(cfg))
    return false;
#endif // OPAL_H323

/*#if OPAL_SIP
  PSYSTEMLOG(Info, "Configuring SIP");
  if (!GetSIPEndPoint().Configure(cfg, rsrc))
    return false;
#endif // OPAL_SIP*/

#if OPAL_HAS_MIXER
  PSYSTEMLOG(Info, "Configuring Mixer");
  if (!GetConferenceManager().Configure(cfg))
    return false;
#endif // OPAL_HAS_MIXER

  return ConfigureCDR(cfg);
}

bool Manager::Initialise(PArgList & args, bool verbose, const PString & defaultRoute)
{
  if (!PreInitialise(args, verbose))
    return false;

  for (PINDEX i = 0; i < m_endpointPrefixes.GetSize(); ++i) {
    OpalConsoleEndPoint * ep = GetConsoleEndPoint(m_endpointPrefixes[i]);
    if (ep != NULL) {
      if (!ep->Initialise(args, verbose, defaultRoute))
        return false;
    }
  }
  
  //cout << GetRouteTable() << endl;

  return true;
}

void Manager::OnStartMediaPatch(OpalConnection & connection, OpalMediaPatch & patch)
{
  ostream & output(cout);
  if (connection.IsNetworkConnection()) {
    OpalMediaStreamPtr stream(patch.GetSink());
    if (stream == NULL || &stream->GetConnection() != &connection)
      stream = &patch.GetSource();
    stream->PrintDetail(output, "Started");
  }

  dynamic_cast<MCUCall &>(connection.GetCall()).OnStartMediaPatch(connection, patch);
  OpalManager::OnStartMediaPatch(connection, patch);
}

void Manager::OnStopMediaPatch(OpalConnection & connection, OpalMediaPatch & patch)
{
  dynamic_cast<MCUCall &>(connection.GetCall()).OnStopMediaPatch(patch);
  OpalManager::OnStopMediaPatch(connection, patch);
}

#if OPAL_H323
H323ConsoleEndPoint * Manager::CreateMCUH323EndPoint()
{
  return new MCUH323EndPoint(*this);
}

MCUH323EndPoint & Manager::GetMCUH323EndPoint() const 
{
  return *FindEndPointAs<MCUH323EndPoint>(OPAL_PREFIX_H323);
}
#endif

#if OPAL_HAS_MIXER
OpalConsoleMixerEndPoint * Manager::CreateConferenceManager()
{
  return new ConferenceManager(*this);
}
ConferenceManager & Manager::GetConferenceManager() const
{
  return *FindEndPointAs<ConferenceManager>(OPAL_PREFIX_MIXER);
}

#endif

OpalConsoleEndPoint * Manager::GetConsoleEndPoint(const PString & prefix)
{
  OpalEndPoint * ep = FindEndPoint(prefix);
  if (ep == NULL) {
#if OPAL_H323
    if (prefix == OPAL_PREFIX_H323)
      ep = CreateMCUH323EndPoint();
    else
#endif // OPAL_H323
/*#if OPAL_SIP
    if (prefix == OPAL_PREFIX_SIP)
      ep = CreateSIPEndPoint();
    else
#endif // OPAL_SIP*/
#if OPAL_HAS_MIXER
    if (prefix == OPAL_PREFIX_MIXER)
      ep = CreateConferenceManager();
    else
#endif // OPAL_HAS_MIXER
    {
      PTRACE(1, "Unknown prefix " << prefix);
      return NULL;
    }
  }

  return dynamic_cast<OpalConsoleEndPoint *>(ep);
}

PString Manager::GetMonitorText()
{
  PStringStream output;
  output << "Usuario: " << GetDefaultDisplayName() << '\n'
         << "N° llamadas simultáneas: " << m_maxCalls << '\n'
         << "\n[ Puertos TCP/RTP ]" << '\n' 
         << "Puertos TCP: " << GetTCPPortRange() << '\n'
         << "Puertos RTP: " << GetRtpIpPortRange() << '\n'
         << "\n[ Servidores NAT ]" << '\n' << GetNatMethods() << '\n'
         << "\n[ Audio ]" << '\n'
         << "Rango de audio delay: " << '[' << GetMinAudioJitterDelay() << ',' << GetMaxAudioJitterDelay() << "]\n";
  if (DetectInBandDTMFDisabled())
    output << "InBandDTMF deshabilitado" << '\n';
  output << "Detector de silencio: " << GetSilenceDetectParams().m_mode << '\n';
#if OPAL_VIDEO
  output << "\n[ Video ]" << '\n'
         << "Resolución de video estándar: " << m_prefVideo << '\n'
         << "Resolución de video maxima: " << m_maxVideo << '\n'
         << "Video frame rate: " << m_rate << " fps\n"
         << "Video target bit rate: " << m_bitrate << '\n';
#endif // OPAL_VIDEO
#if OPAL_H323
  output << "\n[ H.323 ]" << '\n'
         << "Alias H.323: " << GetMCUH323EndPoint().GetAliasNames() << '\n';
  if (GetMCUH323EndPoint().IsFastStartDisabled())
    output << "Fast Connect deshabilitado" << '\n';
  if (GetMCUH323EndPoint().IsH245TunnelingDisabled())
    output << "H.245 Tunneling deshabilitado" << '\n';
  if (GetMCUH323EndPoint().IsH245inSetupDisabled())
    output << "H.245 in Setup deshabilitado" << '\n';  
  if (GetMCUH323EndPoint().IsForcedSymmetricTCS())
    output << "Force symmetric TCS deshabilitado" << '\n';
  if (GetMCUH323EndPoint().GetDefaultH239Control() == false )
    output << "H.239 Control deshabilitado" << '\n';
#endif // OPAL_H323
/*#if OPAL_SIP
  output << "\n[ SIP ]" << '\n'
         << "Usuario SIP: " << GetSIPEndPoint().GetDefaultLocalPartyName() << '\n'
         << "Servidor Proxy: " << GetSIPEndPoint().GetProxy() << '\n'
         << "Dominios Servidor Registrar: " << GetSIPEndPoint().GetRegistrarDomains() << '\n';
#endif // OPAL_SIP*/
  return output; 
}

/////////////////////////////////////////////////////////////////////////////////////

ConferenceManager::ConferenceManager(Manager & manager)
 : OpalConsoleMixerEndPoint(manager)
 
{
}  

ConferenceManager::~ConferenceManager()
{
}

bool ConferenceManager::Initialise(PArgList & args, bool verbose, const PString &)
{
  SetDeferredAnswer(false);
  //SetDefaultAudioSynchronicity(e_Synchronous);
  //SetDefaultVideoSourceSynchronicity(e_Asynchronous);
  //cout << "Conference Manager started" <<endl;
  
  
  return true;
}

void ConferenceManager::CreateConferenceRoom(const PString & room)
{
  if (FindNode(room) != NULL) {
    cout << "Conference name \"" << room << "\" already exists." << endl;
  }
  else {
    OpalMixerNodeInfo * info = new OpalMixerNodeInfo(room);
    PSafePtr<OpalMixerNode> node = AddNode(info);
    cout << "Added conference " << *node << endl; 
  }

}

void ConferenceManager::RemoveConferenceRoom(const PString & room)
{
  PSafePtr<OpalMixerNode> node = FindNode(room);
  if (node == NULL) {
    cout << "Conference \"" << room << "\" does not exist" << endl;
  }
  else {
    RemoveNode(*node);
    cout << "Removed conference \"" << room << "\" " << *node << endl;
  }
}

bool ConferenceManager::Configure(PConfig & cfg)
{
  cfg.SetDefaultSection("Conference");
  PString section = cfg.GetDefaultSection();
 
  if (GetAdHocNodeInfo() != NULL)
    m_adHoc = *GetAdHocNodeInfo();
  m_adHoc.m_name = "OPALMCU";
  m_adHoc.m_mediaPassThru = cfg.GetBoolean(section,"ConfMediaPassThruKey", m_adHoc.m_mediaPassThru);

#if OPAL_VIDEO
  m_adHoc.m_audioOnly = cfg.GetBoolean(section, "ConfAudioOnlyKey", m_adHoc.m_audioOnly);

  PVideoFrameInfo::ParseSize(cfg.GetString(section,"ConfVideoResolutionKey", PVideoFrameInfo::AsString(m_adHoc.m_width, m_adHoc.m_height)),
                                          m_adHoc.m_width, m_adHoc.m_height);
  
#endif // OPAL_VIDEO
  
  m_adHoc.m_closeOnEmpty = true;
  SetAdHocNodeInfo(m_adHoc);
  
  return true;
}

OpalVideoStreamMixer * ConferenceManager::CreateVideoMixer(const OpalMixerNodeInfo & info)
{
  return new MCUVideoMixer(info);
}

OpalMixerConnection * ConferenceManager::CreateConnection(PSafePtr<OpalMixerNode> node,
                                                        OpalCall & call,
                                                        void * userData,
                                                        unsigned options,
                                                        OpalConnection::StringOptions * stringOptions)
{
  return new ConferenceConnection(node, call, *this, userData, options, stringOptions);
}

OpalMixerNode * ConferenceManager::CreateNode(OpalMixerNodeInfo * info)
{
  return new ConferenceNode(*this,info);
}


void ConferenceManager::Unmoderate(OpalMixerNode * node)
{
  ConferenceNode * conference = (ConferenceNode*)node;
  MCUVideoMixer * mixer = conference->m_videoMixerList->mixer;
  if(mixer == NULL)
  {
    return;
  }
  mixer->RemoveAllVideoSource();
  
  for(PINDEX pos = 0; pos < node->GetConnectionCount(); ++pos){
    //if(!info.IsVisible()) continue;
    if(!mixer->AddVideoSource((void*)pos, node)) 
	 break;
  }
  
  while(conference->m_videoMixerCount>1) 
    conference->VMLDel(conference->m_videoMixerCount-1);
  
}

PString ConferenceManager::SetRoomParams(const PStringToString & data)
{
  PString room = data("room");

  // "On-the-Fly" Control via XMLHTTPRequest:
  if(data.Contains("otfc")) return OTFControl(room,data);

  if(data.Contains("refresh")) // JavaScript data refreshing
  {
    PTRACE(6,"WebCtrl\tJS refresh");
    NodesByName::const_iterator r;
    
    NodesByName & nodesByName = GetNodesByName();
    for (r = nodesByName.begin(); r != nodesByName.end(); ++r) 
      if(r->second->GetNodeInfo().m_name == room) break;
    if(r == nodesByName.end() ) return "";
    OpalMixerNode * node = r->second;
    return GetMemberListOptsJavascript(node);
  }

  OpalMCUEIE::Current().HttpWriteEventRoom("MCU Operator connected",room);
  PTRACE(6,"WebCtrl\tOperator connected");
  NodesByName::const_iterator r;
  
  NodesByName & nodesByName = GetNodesByName();
  for (r = nodesByName.begin(); r != nodesByName.end(); ++r)
    if(r->second->GetNodeInfo().m_name == room) 
      break;
  if(r == nodesByName.end() ) 
    return "Bad room";
  OpalMixerNode * node = r->second;
  if((ConferenceNode*)node == NULL) return  "FIX IT " ;
  
  MCUVideoMixer * mixer = ((ConferenceNode*)node)->GetVideoMixerList().mixer;
  if(mixer == NULL) return  "FIX IT AGAIN " ;
  Id idr[100];
  for(int i=0; i<100; i++) idr[i]=mixer->GetPositionId(i);
  return RoomCtrlPage(room,((ConferenceNode*)node)->IsModerated() == '+', mixer->GetPositionSet(), *node, idr);
  
}

PString ConferenceManager::RoomCtrlPage(const PString room, bool ctrl, int n, OpalMixerNode & node, Id *idp)
{

 PStringStream page, meta;

 BeginPage(page,meta,"Room Control","window.l_control","window.l_info_control");

 page << "<script src=\"control1.js\"></script>";

 page << "<div id='cb1' name='cb1' class='input-append'>&nbsp;</div>"
   << "<div style='text-align:center'>"
     << "<div id='cb2' name='cb2' style='margin-left:auto;margin-right:auto;width:100px;height:100px;background-color:#ddd'>"
       << "<div id='logging0' style='position:relative;top:0px;left:-50px;width:0px;height:0px'>"
         << "<div id='logging1' style='position:absolute;width:50px;height:50px'>"
           << "<iframe style='background-color:#eef;border:1px solid #55c;padding:0px;margin:0px' id='loggingframe' name='loggingframe' src='Comm?room=" << room << "' width=50 height=50>"
             << "Your browser does not support IFRAME tag :("
           << "</iframe>"
         << "</div>"
       << "</div>"
       << "<div id='cb3' name='cb3' style='position:relative;top:0px;left:0px;width:0px;height:0px'>"
         << "&nbsp;"
       << "</div>"
     << "</div>"
   << "</div>"
 ;
 EndPage(page,OpalMCUEIE::Current().GetHtmlCopyright());
 return page;
}

PString ConferenceManager::GetConferenceOptsJavascript(OpalMixerNode * node)
{
  
  PStringStream r;                               //conf[0]=[videoMixerCount,bfw,bfh):
  PString jsRoom = node->GetNodeInfo().m_name; 
  jsRoom.Replace("&","&amp;",true,0); 
  jsRoom.Replace("\"","&quot;",true,0);
  r << "conf=Array(Array("                       //l1&l2 open
    << ((ConferenceNode*)node)->m_videoMixerCount                        // [0][0]  = mixerCount
    << ","   << OpalMCUEIE::vmcfg.bfw                                     // [0][1]  = base frame width
    << ","   << OpalMCUEIE::vmcfg.bfh                                     // [0][2]  = base frame height
    << ",\"" << jsRoom << "\""                                            // [0][3]  = room name
    << ",'"  << ((ConferenceNode*)node)->IsModerated() << "'"                                    // [0][4]  = control
    << ",'"  << 0 << "'"                                // [0][5]  = global mute
    << ","   << 0 << "," << 0 << "," << 0       // [0][6-8]= vad

    << ",Array("; // l3 open
      
    NodesByName & nodesByName = GetNodesByName();
    NodesByName::iterator l;
    for (l = nodesByName.begin(); l != nodesByName.end(); ++l) {
	  OpalMixerNode * node = l->second;
      jsRoom = node->GetNodeInfo().m_name;
      jsRoom.Replace("&", "&amp;", true, 0); 
      jsRoom.Replace("\"", "&quot;", true, 0);
      if(l != nodesByName.begin()) r << ",";                          // [0][9][ci][0-2] roomName & memberCount & isModerated
        r << "Array(\"" << jsRoom << "\"," << (*node).GetConnectionCount() << ",\"" << ((ConferenceNode*)node)->IsModerated() << "\")";
    }
      
  r << ")"; // l3 close

#if USE_LIBYUV
    r << "," << OpalMCUEIE::Current().GetScaleFilter();                  // [0][10] = libyuv resizer filter mode
#else
    r << ",-1";
#endif

  r << ",0";          // [0][11] = external video recording state (1=recording, 0=NO)
  r << ",0";                    // [0][12] = member list locked by template (1=yes, 0=no)

  r << ")"; //l2 close

  
  ConferenceNode::VideoMixerRecord * vmr = ((ConferenceNode*)node)->m_videoMixerList; 
  while (vmr != NULL) {
    r << ",Array("; //l2 open
    MCUVideoMixer * mixer = vmr->mixer;
    unsigned n = mixer->GetPositionSet();
    VMPCfgSplitOptions & split = OpalMCUEIE::vmcfg.vmconf[n].splitcfg;
    VMPCfgOptions      * p     = OpalMCUEIE::vmcfg.vmconf[n].vmpcfg;


    r << "Array(" //l3 open                                           // conf[n][0]: base parameters:
      << split.mockup_width << "," << split.mockup_height                 // [n][0][0-1]= mw*mh
      << "," << n                                                         // [n][0][2]  = position set (layout)
    << "),Array("; //l3 reopen

    for(unsigned i=0; i<split.vidnum; i++)
      r << "Array(" << p[i].posx //l4 open                            // conf[n][1]: frame geometry for each position i:
        << "," << p[i].posy                                               // [n][1][i][0-1]= posx & posy
        << "," << p[i].width                                              // [n][1][i][2-3]= width & height
        << "," << p[i].height
        << "," << p[i].border                                             // [n][1][i][4]  = border
      << ")" << ((i==split.vidnum-1)?"":","); //l4 close

    r << ")," << mixer->VMPListScanJS() //l3 close                    // conf[n][2], conf[n][3]: members' ids & types
    << ")"; //l2 close

    vmr=vmr->next;
  }
  r << ");"; //l1 close
  
  //cout << r << endl;
  return r;
}

PString ConferenceManager::GetMemberListOptsJavascript(OpalMixerNode * node)
{
 PStringStream members;
 //PWaitAndSignal m(conference.GetMutex());
 members << "members=Array(";
 int i=0;
 for (PSafePtr<OpalConnection> connection = node->GetFirstConnection(); connection != NULL; ++connection) {
  OpalCall & call = connection->GetCall();
  if(&call == NULL){ // inactive member
    if(i>0) members << ",";
    members << "Array(0"
      << ",0"
      << ",\"" << call.GetPartyB() << "\""
      << ",0"
      << ",0"
      << ",0"
      << ",0"
      << ",0"
      << ",\"" << call.GetPartyB() << "\""
      << ")";
    i++;
  } else {          //   active member
    if(i>0) members << ",";
    members << "Array(1"                                // [i][ 0] = 1 : ONLINE
      << ",\"" << connection->GetConnectionInfo().m_identifier << "\""  // [i][ 1] = long id
      << ",\"" << call.GetRemoteName() << "\""                      // [i][ 2] = name [ip]
      << "," << 0//member->muteMask                        // [i][ 3] = mute
      << "," << 0//member->disableVAD                      // [i][ 4] = disable vad
      << "," << 0//member->chosenVan                       // [i][ 5] = chosen van
      << "," << 0//member->GetAudioLevel()                 // [i][ 6] = audiolevel (peak)
      << "," << 0//member->GetVideoMixerNumber()           // [i][ 7] = number of mixer member receiving
      << ",\"" << call.GetPartyB() << "\""   // [i][ 8] = URL
      << "," << 0//(unsigned short)member->channelCheck    // [i][ 9] = RTP channels checking bit mask 0000vVaA
      << "," << 0//member->kManualGainDB                   // [i][10] = Audio level gain for manual tune, integer: -20..60
      << "," << 0//member->kOutputGainDB                   // [i][11] = Output audio gain, integer: -20..60
      << ")";
    i++;
  }
 }
 members << ");";

 members << "\np.addressbook=Array(";
 PStringArray abook = GetAddressBook();
 for(PINDEX i = 0; i < abook.GetSize(); i++)
 {
   if(i>0) members << ",";
   PString username = abook[i];
   username.Replace("&","&amp;",TRUE,0);
   username.Replace("\"","&quot;",TRUE,0);
   members << "Array("
      << "0"
      << ",\"" << abook[i] << "\""
      << ",\"" << username << "\""
      << ")";
 }
 members << ");";

 return members;
}

PString ConferenceManager::OTFControl(const PString room, const PStringToString & data)
{
  PTRACE(6,"WebCtrl\tRoom " << room << ", action " << data("action") << ", value " << data("v") << ", options " << data("o") << ", " << data("o2") << ", " << data("o3"));
  cout << "WebCtrl\tRoom " << room << ", action " << data("action") << ", value " << data("v") << ", options " << data("o") << ", " << data("o2") << ", " << data("o3") << endl;
#define OTF_RET_OK   { return "OK"; }
#define OTF_RET_FAIL { return "FAIL"; }
  
  if(!data.Contains("action")) 
    OTF_RET_FAIL; 
    
  int action=data("action").AsInteger();

  if(!data.Contains("v")) 
    OTF_RET_FAIL; 
    
  PString value=data("v");
  long v = value.AsInteger();
  
  //PSafePtr<OpalMixerNode> node = FindNode(room);
  NodesByName & nodesByName = GetNodesByName();
  NodesByName::const_iterator r = nodesByName.find(room);
  if(r == nodesByName.end()) OTF_RET_FAIL; 
  
  OpalMixerNode * node = r->second;
  
  if(action == OTFC_REFRESH_VIDEO_MIXERS)
  {
    OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(node),room);
    OpalMCUEIE::Current().HttpWriteCmdRoom("mixrfr()",room);
    OTF_RET_OK;
  }
  /*if(action == OTFC_VIDEO_RECORDER_START)
  { conference->StartRecorder(); OTF_RET_OK;
  }
  if(action == OTFC_VIDEO_RECORDER_STOP)
  { conference->StopRecorder(); OTF_RET_OK;
  }*/
  /*if(action == OTFC_TEMPLATE_RECALL)
  {
    OpalMCUEIE::Current().HttpWriteCmdRoom("alive()",room);

    if(conference->IsModerated()=="-")
    { conference->SetModerated(true);
      conference->videoMixerListMutex.Wait();
      conference->videoMixerList->mixer->SetForceScreenSplit(true);
      conference->videoMixerListMutex.Signal();
      conference->PutChosenVan();
      OpalMCUEIE::Current().HttpWriteEventRoom("<span style='background-color:#bfb'>Operator took the control</span>",room);
      OpalMCUEIE::Current().HttpWriteCmdRoom("r_moder()",room);
    }

    conference->confTpl = conference->ExtractTemplate(value);
    conference->LoadTemplate(conference->confTpl);
    conference->SetLastUsedTemplate(value);
    PStringStream msg;
    msg << GetMemberListOptsJavascript(*conference) << "\n"
        << "p." << GetConferenceOptsJavascript(*conference) << "\n"
        << "p.tl=Array" << conference->GetTemplateList() << "\n"
        << "p.seltpl=\"" << conference->GetSelectedTemplateName() << "\"\n"
        << "p.build_page()";
      OpalMCUEIE::Current().HttpWriteCmdRoom(msg,room);
    OTF_RET_OK;
  }
  if(action == OTFC_SAVE_TEMPLATE)
  {
    OpalMCUEIE::Current().HttpWriteCmdRoom("alive()",room);
    PString templateName=value.Trim();
    if(templateName=="") OTF_RET_FAIL;
    if(templateName.Right(1) == "*") templateName=templateName.Left(templateName.GetLength()-1).RightTrim();
    if(templateName=="") OTF_RET_FAIL;
    conference->confTpl = conference->SaveTemplate(templateName);
    conference->TemplateInsertAndRewrite(templateName, conference->confTpl);
    conference->LoadTemplate(conference->confTpl);
    PStringStream msg;
    msg << GetMemberListOptsJavascript(*conference) << "\n"
        << "p." << GetConferenceOptsJavascript(*conference) << "\n"
        << "p.tl=Array" << conference->GetTemplateList() << "\n"
        << "p.seltpl=\"" << conference->GetSelectedTemplateName() << "\"\n"
        << "p.build_page()";
      OpalMCUEIE::Current().HttpWriteCmdRoom(msg,room);
    OTF_RET_OK;
  }
  if(action == OTFC_DELETE_TEMPLATE)
  {
    OpalMCUEIE::Current().HttpWriteCmdRoom("alive()",room);
    PString templateName=value.Trim();
    if(templateName=="") OTF_RET_FAIL;
    if(templateName.Right(1) == "*") OTF_RET_FAIL;
    conference->DeleteTemplate(templateName);
    conference->LoadTemplate("");
    conference->SetLastUsedTemplate("");
    PStringStream msg;
    msg << GetMemberListOptsJavascript(*conference) << "\n"
        << "p." << GetConferenceOptsJavascript(*conference) << "\n"
        << "p.tl=Array" << conference->GetTemplateList() << "\n"
        << "p.seltpl=\"" << conference->GetSelectedTemplateName() << "\"\n"
        << "p.build_page()";
      OpalMCUEIE::Current().HttpWriteCmdRoom(msg,room);
    OTF_RET_OK;
  }
  if(action == OTFC_TOGGLE_TPL_LOCK)
  {
    PString templateName=value.Trim();
    if((templateName.IsEmpty())||(templateName.Right(1) == "*")) 
    if(!conference->m_lockedTemplate) OTF_RET_FAIL;
    conference->m_lockedTemplate = !conference->m_lockedTemplate;
    if(conference->m_lockedTemplate) 
      OpalMCUEIE::Current().HttpWriteCmdRoom("tpllck(1)",room);
    else OpalMCUEIE::Current().HttpWriteCmdRoom("tpllck(0)",room);
    OTF_RET_OK;
  }*/
  if(action == OTFC_INVITE)
  { 
	PStringStream msg;
    msg << "Inviting " << value;
    
	MCUVideoMixer * mixer = ((ConferenceNode*)node)->m_videoMixerList->mixer;
    PString token;
    if(OpalMCUEIE::Current().GetManager().SetUpCall("mcu:"+node->GetNodeInfo().m_name,value,token)){
      cout << "Adding new member \"" << value << "\" to conference " << *node << endl;
      OpalMCUEIE::Current().HttpWriteCmdRoom(msg,room); 
      for(PINDEX pos = 0; pos < node->GetConnectionCount(); ++pos){
        if(!mixer->AddVideoSource((void*)pos,node))
          break;
      }
      
    }
    else {
      cout << "Could not add new member \"" << value << "\" to conference " << *node << endl;
      OTF_RET_FAIL;
    }
    OTF_RET_OK; 
  }
  /*if(action == OTFC_ADD_AND_INVITE)
  {
    conference->AddOfflineMemberToNameList(value);
    conference->InviteMember(value);
    OTF_RET_OK;
  }
  if(action == OTFC_REMOVE_OFFLINE_MEMBER)
  {
    conference->RemoveOfflineMemberFromNameList(value);
    PStringStream msg;
    msg << GetMemberListOptsJavascript(*conference) << "\n"
        << "p.members_refresh()";
    OpalMCUEIE::Current().HttpWriteCmdRoom(msg,room);
    OTF_RET_OK;
  }*/
  if(action == OTFC_DROP_ALL_ACTIVE_MEMBERS)
  {
    
	MCUVideoMixer * mixer = ((ConferenceNode*)node)->m_videoMixerList->mixer;
    mixer->RemoveAllVideoSource();
    for (PSafePtr<OpalConnection> connection = node->GetFirstConnection(); connection != NULL; ++connection) {
      OpalCall & call = connection->GetCall();
      if(&call != NULL)
        call.Clear();
      else
        break;

    }
    OpalMCUEIE::Current().HttpWriteEventRoom("Active members dropped by operator",room);
    OpalMCUEIE::Current().HttpWriteCmdRoom("drop_all()",room);
    return "OK";
  }
  /*if((action == OTFC_MUTE_ALL)||(action == OTFC_UNMUTE_ALL))
  {
    UnlockConference();
    bool newValue = (action==OTFC_MUTE_ALL);
    PWaitAndSignal m(conference->GetMutex());
    Conference::MemberList & memberList = conference->GetMemberList();
    Conference::MemberList::iterator r;
    //H323Connection_ConferenceMember * connMember;
    for (r = memberList.begin(); r != memberList.end(); ++r)
    {
      ConferenceMember * member = r->second;
      if(member->GetName()=="file recorder") continue;
      if(member->GetName()=="cache") continue;
      
    return "OK";
  }
  if(action == OTFC_INVITE_ALL_INACT_MMBRS)
  { Conference::MemberNameList & memberNameList = conference->GetMemberNameList();
    Conference::MemberNameList::const_iterator r;
    for (r = memberNameList.begin(); r != memberNameList.end(); ++r) 
      if(r->second==NULL) 
        conference->InviteMember(r->first);
    OTF_RET_OK;
  }
  if(action == OTFC_REMOVE_ALL_INACT_MMBRS)
  { Conference::MemberNameList & memberNameList = conference->GetMemberNameList();
    Conference::MemberNameList::const_iterator r;
    for (r = memberNameList.begin(); r != memberNameList.end(); ++r) 
      if(r->second==NULL) 
        conference->RemoveOfflineMemberFromNameList((PString &)(r->first));
    OpalMCUEIE::Current().HttpWriteEventRoom("Offline members removed by operator",room);
    OpalMCUEIE::Current().HttpWriteCmdRoom("remove_all()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_SAVE_MEMBERS_CONF)
  { FILE *membLst;
    PString name="members_"+room+".conf";
    membLst = fopen(name,"w");
    if(membLst==NULL) { OpalMCUEIE::Current().HttpWriteEventRoom("<font color=red>Error: Can't save member list</font>",room); OTF_RET_FAIL; }
    fputs(conference->membersConf,membLst);
    fclose(membLst);
    OpalMCUEIE::Current().HttpWriteEventRoom("Member list saved",room);
    OTF_RET_OK;
  }*/
  if(action == OTFC_TAKE_CONTROL)
  { if( ((ConferenceNode*)node)->IsModerated() == "-")
    { ((ConferenceNode*)node)->SetModerated(true);
      //conference->videoMixerListMutex.Wait();
      //conference->videoMixerList->mixer->SetForceScreenSplit(true);
      //conference->videoMixerListMutex.Signal();
      //conference->PutChosenVan();
      OpalMCUEIE::Current().HttpWriteEventRoom("<span style='background-color:#bfb'>Operator took the control</span>",room);
      OpalMCUEIE::Current().HttpWriteCmdRoom("r_moder()",room);
    }
    OTF_RET_OK;
  }
  if(action == OTFC_DECONTROL)
  { if(((ConferenceNode*)node)->IsModerated() == "+")
    { ((ConferenceNode*)node)->SetModerated(false);
      {
        //PWaitAndSignal m(conference->videoMixerListMutex);
        if(!((ConferenceNode*)node)->m_videoMixerList) OTF_RET_FAIL;
        if(!((ConferenceNode*)node)->m_videoMixerList->mixer) OTF_RET_FAIL;
        //conference->videoMixerList->mixer->SetForceScreenSplit(true);
      }
      
      Unmoderate(node);  // before conference.GetMutex() usage
      OpalMCUEIE::Current().HttpWriteEventRoom("<span style='background-color:#acf'>Operator resigned</span>",room);
      OpalMCUEIE::Current().HttpWriteCmdRoom("r_unmoder()",room);
      OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(node),room);
      OpalMCUEIE::Current().HttpWriteCmdRoom("build_page()",room);
      return "OK";
    }
    OTF_RET_OK;
  }
  if(action == OTFC_ADD_VIDEO_MIXER)
  { if(((ConferenceNode*)node)->IsModerated()=="+")
    { unsigned n = ((ConferenceNode*)node)->VMLAdd();
      PStringStream msg; msg << "Video mixer " << n << " added";
      OpalMCUEIE::Current().HttpWriteEventRoom(msg,room);
      OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(node),room);
      OpalMCUEIE::Current().HttpWriteCmdRoom("mmw=-1;p.build_page()",room);
      OTF_RET_OK;
    }
    OTF_RET_FAIL;
  }
  if(action == OTFC_DELETE_VIDEO_MIXER)
  { if(((ConferenceNode*)node)->IsModerated()=="+")
    {
      unsigned n_old = ((ConferenceNode*)node)->m_videoMixerCount;
      unsigned n = ((ConferenceNode*)node)->VMLDel(v);
      if(n_old != n)
      { PStringStream msg; msg << "Video mixer " << v << " removed";
        OpalMCUEIE::Current().HttpWriteEventRoom(msg,room);
        OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(node),room);
        OpalMCUEIE::Current().HttpWriteCmdRoom("mmw=-1;p.build_page()",room);
        OTF_RET_OK;
      }
    }
    OTF_RET_FAIL;
  }
  if (action == OTFC_SET_VIDEO_MIXER_LAYOUT)
  { unsigned option = data("o").AsInteger();
    MCUVideoMixer * mixer = ((ConferenceNode*)node)->VMLFind(option);
    if(mixer!=NULL)
    { 
	  
	  mixer->ChangeLayout(v);
	  
      //conference->PutChosenVan();
      //conference->FreezeVideo(NULL);
      OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(node),room);
      OpalMCUEIE::Current().HttpWriteCmdRoom("build_page()",room);
      OTF_RET_OK;
    }
    OTF_RET_FAIL;
  }
  if (action == OTFC_REMOVE_VMP)
  { MCUVideoMixer * mixer = ((ConferenceNode*)node)->VMLFind(v); 
	if(mixer==NULL) OTF_RET_FAIL;
    unsigned pos = data("o").AsInteger();
    mixer->RemoveVideoSourceByPos(pos,true);
    //conference->FreezeVideo(NULL);
    OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(node),room);
    OpalMCUEIE::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_MIXER_ARRANGE_VMP)
  { MCUVideoMixer * mixer = ((ConferenceNode*)node)->VMLFind(v); if(mixer==NULL) OTF_RET_FAIL;
	
	for(PINDEX pos = 0; pos < node->GetConnectionCount(); ++pos){
       if(!mixer->AddVideoSourceToLayout((void*)pos,node))
         break;
	 
	}
    OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(node),room);
    OpalMCUEIE::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_MIXER_CLEAR)
  { MCUVideoMixer * mixer = ((ConferenceNode*)node)->VMLFind(v); if(mixer==NULL) OTF_RET_FAIL;
    mixer->RemoveAllVideoSource();
    OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(node),room);
    OpalMCUEIE::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_MIXER_SHUFFLE_VMP)
  { MCUVideoMixer * mixer = ((ConferenceNode*)node)->VMLFind(v); if(mixer==NULL) OTF_RET_FAIL;
    mixer->Shuffle();
    OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(node),room);
    OpalMCUEIE::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_MIXER_SCROLL_LEFT)
  { MCUVideoMixer * mixer = ((ConferenceNode*)node)->VMLFind(v); if(mixer==NULL) OTF_RET_FAIL;
    mixer->Scroll(true);
    OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(node),room);
    OpalMCUEIE::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_MIXER_SCROLL_RIGHT)
  { MCUVideoMixer * mixer = ((ConferenceNode*)node)->VMLFind(v); if(mixer==NULL) OTF_RET_FAIL;
    mixer->Scroll(false);
    OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(node),room);
    OpalMCUEIE::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_MIXER_REVERT)
  { MCUVideoMixer * mixer = ((ConferenceNode*)node)->VMLFind(v); if(mixer==NULL) OTF_RET_FAIL;
    mixer->Revert();
    OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(node),room);
    OpalMCUEIE::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  /*if(action == OTFC_GLOBAL_MUTE)
  { if(data("v")=="true")v=1; if(data("v")=="false") v=0; conference->SetMuteUnvisible((bool)v);
    OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
    OpalMCUEIE::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if(action == OTFC_SET_VAD_VALUES)
  { conference->VAlevel   = (unsigned short int) v;
    conference->VAdelay   = (unsigned short int) (data("o").AsInteger());
    conference->VAtimeout = (unsigned short int) (data("o2").AsInteger());
    OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
    OpalMCUEIE::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }*/
  if (action == OTFC_MOVE_VMP)
  { MCUVideoMixer * mixer1 = ((ConferenceNode*)node)->VMLFind(v); if(mixer1==NULL) OTF_RET_FAIL;
    if(!data.Contains("o2")) OTF_RET_FAIL; MCUVideoMixer * mixer2=((ConferenceNode*)node)->VMLFind(data("o2").AsInteger()); if(mixer2==NULL) OTF_RET_FAIL;
    int pos1 = data("o").AsInteger(); int pos2 = data("o3").AsInteger();
    if(mixer1==mixer2) mixer1->Exchange(pos1,pos2);
    else
    {
      Id id = mixer1->GetHonestId(pos1); if(((long)id<100)&&((long)id>=0)) id=NULL;
      Id id2 = mixer2->GetHonestId(pos2); if(((long)id2<100)&&((long)id2>=0)) id2=NULL;
      mixer2->PositionSetup(pos2, 1, id);
      mixer1->PositionSetup(pos1, 1, id2);
    }
    OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(node),room);
    OpalMCUEIE::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  if (action == OTFC_VAD_CLICK)
  { MCUVideoMixer * mixer = ((ConferenceNode*)node)->VMLFind(v); if(mixer==NULL) OTF_RET_FAIL;
    unsigned pos = data("o").AsInteger();
    int type = data("o2").AsInteger();
    if((type<1)||(type>3)) type=2;
    long id = (long)mixer->GetHonestId(pos);
    if((type==1)&&(id>=0)&&(id<100)) //static but no member
    {
      PINDEX pos1;
	  for(pos1 = 0; pos1 < node->GetConnectionCount(); ++pos1){
      {
        if((void*)pos1 != NULL)
        {
          
            if (mixer->VMPListFindVMP((void*)pos1) == NULL)
            {
              mixer->PositionSetup(pos, 1, (void*)pos1);
              
              break;
            }
          
        }
      }
      //if(r==stream.end()) mixer->PositionSetup(pos,1,NULL);
      if(pos > pos1) mixer->PositionSetup(pos,1,NULL);
      }
    }  
    else if((id>=0)&&(id<100)) mixer->PositionSetup(pos,type,NULL);
    else mixer->SetPositionType(pos,type);
    //conference->FreezeVideo(NULL);
    OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(node),room);
    OpalMCUEIE::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }

  /*ConferenceMember * member = GetConferenceMemberById(conference, v); 
  if(member==NULL) OTF_RET_FAIL;
  PStringStream cmd;*/

  if( action == OTFC_SET_VMP_STATIC )
  { unsigned n=data("o").AsInteger(); 
	MCUVideoMixer * mixer = ((ConferenceNode*)node)->VMLFind(n); 
	if(mixer==NULL) OTF_RET_FAIL;
    //int pos = data("o2").AsInteger(); mixer->PositionSetup(pos, 1, conference);
    cout <<"2conference->SetFreezeVideo(false)" << endl;
    OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(node),room);
    OpalMCUEIE::Current().HttpWriteCmdRoom("build_page()",room);
    OTF_RET_OK;
  }
  /*if( action == OTFC_AUDIO_GAIN_LEVEL_SET )
  {
    int n=data("o").AsInteger();
    if(n<0) n=0;
    if(n>80) n=80;
    member->m_kManualGainDB=n-20;
    member->m_kManualGain=(float)pow(10.0,((float)member->m_kManualGainDB)/20.0);
    cmd << "setagl(" << v << "," << member->m_kManualGainDB << ")";
    OpalMCUEIE::Current().HttpWriteCmdRoom(cmd,room); OTF_RET_OK;
  }
  if( action == OTFC_OUTPUT_GAIN_SET )
  {
    int n=data("o").AsInteger();
    if(n<0) n=0;
    if(n>80) n=80;
    member->m_kOutputGainDB=n-20;
    member->m_kOutputGain=(float)pow(10.0,((float)member->m_kOutputGainDB)/20.0);
    cmd << "setogl(" << v << "," << member->m_kOutputGainDB << ")";
    OpalMCUEIE::Current().HttpWriteCmdRoom(cmd,room); OTF_RET_OK;
  }
  if(action == OTFC_MUTE)
  {
    H323Connection_ConferenceMember * connMember = dynamic_cast<H323Connection_ConferenceMember *>(member);
    if(connMember) connMember->SetChannelPauses(data("o").AsInteger());
    OTF_RET_OK;
  }
  if(action == OTFC_UNMUTE)
  {
    H323Connection_ConferenceMember * connMember = dynamic_cast<H323Connection_ConferenceMember *>(member);
    if(connMember) connMember->UnsetChannelPauses(data("o").AsInteger());
    OTF_RET_OK;
  }
  if( action == OTFC_REMOVE_FROM_VIDEOMIXERS)
  { if(((ConferenceNode*)node)->IsModerated() == "+" )
    {
      //PWaitAndSignal m(conference->videoMixerListMutex);
      Conference::VideoMixerRecord * vmr = ((ConferenceNode*)node)->videoMixerList;
      while( vmr != NULL ) 
      { MCUVideoMixer * mixer = vmr->mixer;
        int oldPos = mixer->GetPositionNum(member->GetID());
        if(oldPos != -1) mixer->RemoveVideoSourceByPos(oldPos, true);
        vmr=vmr->next;
      }
      //H323Connection_ConferenceMember *connMember = dynamic_cast<H323Connection_ConferenceMember *>(member);
      //if(connMember!=NULL) connMember->SetFreezeVideo(TRUE);
      OpalMCUEIE::Current().HttpWriteCmdRoom(GetConferenceOptsJavascript(*conference),room);
      OpalMCUEIE::Current().HttpWriteCmdRoom("build_page()",room);
      OTF_RET_OK;
    }
  }*/
  /*if( action == OTFC_DROP_MEMBER )
  {
    // MAY CAUSE DEADLOCK PWaitAndSignal m(conference->GetMutex();
//    conference->GetMemberList().erase(member->GetGUID());
    member->Close();
    OTF_RET_OK;
  }
  if (action == OTFC_VAD_NORMAL)
  {
    member->m_disableVAD=false;
    member->m_chosenVan=false;
    UnlockConference();
    conference->PutChosenVan();
    cmd << "ivad(" << v << ",0)";
    OpalMCUEIE::Current().HttpWriteCmdRoom(cmd,room);
    return "OK";
  }
  if (action == OTFC_VAD_CHOSEN_VAN)
  {
    member->m_disableVAD=false;
    member->m_chosenVan=true;
    UnlockConference();
    conference->PutChosenVan();
    conference->FreezeVideo(member->GetID());
    cmd << "ivad(" << v << ",1)";
    OpalMCUEIE::Current().HttpWriteCmdRoom(cmd,room);
    return "OK";
  }
  if (action == OTFC_VAD_DISABLE_VAD)
  {
    member->m_disableVAD=true;
    member->m_chosenVan=false;
    UnlockConference();
    conference->PutChosenVan();
#if 1 // DISABLING VAD WILL CAUSE MEMBER REMOVAL FROM VAD POSITIONS
    {
      PWaitAndSignal m(conference->GetMutex());
      ConferenceMemberId id = member->GetID();
      conference->videoMixerListMutex.Wait();
      Conference::VideoMixerRecord * vmr = conference->videoMixerList;
      while(vmr!=NULL)
      {
        int type = vmr->mixer->GetPositionType(id);
        if(type<2 || type>3) { vmr=vmr->next; continue;} //-1:not found, 1:static, 2&3:VAD
        vmr->mixer->MyRemoveVideoSourceById(id, false);
        vmr = vmr->next;
      }
      conference->videoMixerListMutex.Signal();
      conference->FreezeVideo(id);
    }
#endif
    cmd << "ivad(" << v << ",2)";
    OpalMCUEIE::Current().HttpWriteCmdRoom(cmd,room);
    return "OK";
  }
  if (action == OTFC_SET_MEMBER_VIDEO_MIXER)
  { unsigned option = data("o").AsInteger();
    if(SetMemberVideoMixer(*conference,member,option))
    { cmd << "chmix(" << v << "," << option << ")";
      OpalMCUEIE::Current().HttpWriteCmdRoom(cmd,room);
      OTF_RET_OK;
    }
    if(SetMemberVideoMixer(*conference,member,0)) // rotate back to 0
    { cmd << "chmix(" << v << ",0)";
      OpalMCUEIE::Current().HttpWriteCmdRoom(cmd,room);
      OTF_RET_OK;
    }
    OTF_RET_FAIL;
  }*/

  OTF_RET_FAIL;
  
}

bool ConferenceManager::OnIncomingVideo(PString & room, Id id, const void * buffer, int width, int height, PINDEX amount)
{
  NodesByName & nodesByName = GetNodesByName();
  NodesByName::const_iterator r = nodesByName.find(room);
  if(r == nodesByName.end()) return false;
  
  OpalMixerNode * node = r->second;
  MCUVideoMixer * mixer = ((ConferenceNode*)node)->m_videoMixerList->mixer;
  mixer->WriteFrame(id, buffer, width, height, amount);
  
  return true;
}

bool ConferenceManager::OnOutgoingVideo(PString & room, void * buffer, int width, int height, PINDEX &amount)
{
  NodesByName & nodesByName = GetNodesByName();
  NodesByName::const_iterator r = nodesByName.find(room);
  if(r == nodesByName.end()) return false;
  
  OpalMixerNode * node = r->second;
  MCUVideoMixer * mixer = ((ConferenceNode*)node)->m_videoMixerList->mixer;
  mixer->ReadFrame(buffer, width, height, amount);
  
  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////
OpalMediaStream * OpalLocalConnection::CreateMediaStream(const OpalMediaFormat & mediaFormat,
                                                         unsigned sessionID,
                                                         PBoolean isSource)
{
  if (m_endpoint.UseCallback(mediaFormat, isSource))
    return new OpalLocalMediaStream(*this, mediaFormat, sessionID, isSource, GetSynchronicity(mediaFormat, isSource));

#if OPAL_VIDEO
  if (mediaFormat.GetMediaType() == OpalMediaType::Video()) {
    if (isSource) {
      PVideoInputDevice * videoDevice;
      PBoolean autoDeleteGrabber;
      if (CreateVideoInputDevice(mediaFormat, videoDevice, autoDeleteGrabber))
        PTRACE(4, "Created video input device \"" << videoDevice->GetDeviceName() << '"');
      else {
        PTRACE(3, "Creating fake text video input");
        PVideoDevice::OpenArgs args;
        args.deviceName = P_FAKE_VIDEO_TEXT "=OPALMCUEIE";
        mediaFormat.AdjustVideoArgs(args);
        videoDevice = PVideoInputDevice::CreateOpenedDevice(args, false);
      }

      PVideoOutputDevice * previewDevice;
      PBoolean autoDeletePreview;
      if (CreateVideoOutputDevice(mediaFormat, true, previewDevice, autoDeletePreview))
        PTRACE(4, "Created preview device \"" << previewDevice->GetDeviceName() << '"');
      else
        previewDevice = NULL;

      return new OpalVideoMediaStream(*this, mediaFormat, sessionID, videoDevice, previewDevice, autoDeleteGrabber, autoDeletePreview);
    }
    else {
      PVideoOutputDevice * videoDevice;
      PBoolean autoDelete;
      if (CreateVideoOutputDevice(mediaFormat, false, videoDevice, autoDelete)) {
        PTRACE(4, "Created display device \"" << videoDevice->GetDeviceName() << '"');
        return new OpalVideoMediaStream(*this, mediaFormat, sessionID, NULL, videoDevice, false, autoDelete);
      }
      PTRACE(2, "Could not create video output device");
    }

    return NULL;
  }
#endif // OPAL_VIDEO

  return OpalConnection::CreateMediaStream(mediaFormat, sessionID, isSource);
}
