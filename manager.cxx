
#include "config.h"
#include "call.h"
#include "h323.h"
#include "manager.h"
#include "mixer.h"
#include "sip.h"

#if OPAL_PTLIB_NAT
struct NATInfo {
  PString m_method;
  PString m_friendly;
  bool    m_active;
  PString m_server;
  PString m_interface;
  NATInfo(const PNatMethod & method)
    : m_method(method.GetMethodName())
    , m_friendly(method.GetFriendlyName())
    , m_active(method.IsActive())
    , m_server(method.GetServer())
    , m_interface(method.GetInterface())
  { }

  __inline bool operator<(const NATInfo & other) const { return m_method < other.m_method; }
};
#endif // OPAL_PTLIB_NAT

#if OPAL_SIP
void ExpandWildcards(const PStringArray & input, 
                     const PString & defaultServer, 
                     PStringArray & names, 
                     PStringArray & servers)
{
  for (PINDEX i = 0; i < input.GetSize(); ++i) {
    PString str = input[i];

    PIntArray starts(4), ends(4);
    static PRegularExpression const Wildcards("([0-9]+)\\.\\.([0-9]+)(@.*)?$", PRegularExpression::Extended);
    if (Wildcards.Execute(str, starts, ends)) {
      PString server = (ends[3] - starts[3]) > 2 ? str(starts[3] + 1, ends[3] - 1) : defaultServer;
      uint64_t number = str(starts[1], ends[1] - 1).AsUnsigned64();
      uint64_t lastNumber = str(starts[2], ends[2] - 1).AsUnsigned64();
      unsigned digits = ends[1] - starts[1];
      str.Delete(starts[1], P_MAX_INDEX);
      while (number <= lastNumber) {
        names.AppendString(PSTRSTRM(str << setfill('0') << setw(digits) << number++));
        servers.AppendString(server);
      }
    }
    else {
      PString name, server;
      if (str.Split('@', name, server)) {
        names.AppendString(name);
        servers.AppendString(server);
      }
      else {
        names.AppendString(str);
        servers.AppendString(defaultServer);
      }
    }
  }
}
#endif // OPAL_SIP

MyManager::MyManager()
  : OpalManagerConsole(OPAL_CONSOLE_PREFIXES OPAL_PREFIX_MIXER)
  , m_savedProductInfo(GetProductInfo())
  , m_maxCalls(9999)
  , m_mediaTransferMode(MediaTransferForward)
  , m_verbose(false)
  , m_cdrListMax(100)
{
  OpalMediaFormat::RegisterKnownMediaFormats(); 
  DisableDetectInBandDTMF(true);

#if OPAL_VIDEO
  for (OpalVideoFormat::ContentRole role = OpalVideoFormat::BeginContentRole; role < OpalVideoFormat::EndContentRole; ++role)
    m_videoInputDevice[role].deviceName = m_videoPreviewDevice[role].deviceName = m_videoOutputDevice[role].deviceName = P_NULL_VIDEO_DEVICE;
  
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
#endif

}

MyManager::~MyManager()
{
}

OpalCall * MyManager::CreateCall(void *)
{
  if (m_activeCalls.GetSize() < m_maxCalls) { 
   return new MyCall(*this);
  }

  if (m_verbose)
   cout << "Maximum simultaneous calls (" << m_maxCalls << ") exceeeded." << endl;

  PTRACE(2, "Maximum simultaneous calls (" << m_maxCalls << ") exceeeded.");
  return NULL;
}

struct OpalCmdPresentationToken
{
  P_DECLARE_STREAMABLE_ENUM(Cmd, request, release);
};

void MyManager::CmdPresentationToken()
{
  /*PSafePtr<OpalRTPConnection> connection;
  if (GetConnectionFromArgs(args, connection)) {
    if (args.GetCount() == 0)
      args.GetContext() << "Presentation token is " << (connection->HasPresentationRole() ? "acquired." : "released.") << endl;
    else {
      switch (OpalCmdPresentationToken::CmdFromString(args[0], false)) {
        case OpalCmdPresentationToken::request :
          if (connection->HasPresentationRole())
            args.GetContext() << "Presentation token is already acquired." << endl;
          else if (connection->RequestPresentationRole(false))
            args.GetContext() << "Presentation token requested." << endl;
          else
            args.WriteError("Presentation token not supported by remote.");
          break;

        case OpalCmdPresentationToken::release :
          if (!connection->HasPresentationRole())
            args.GetContext() << "Presentation token is already released." << endl;
          else if (connection->RequestPresentationRole(true))
            args.GetContext() << "Presentation token released." << endl;
          else
            args.WriteError("Presentation token release failed.");
          break;

        default :
          args.WriteUsage();
      }
    }
  }*/
}

void MyManager::EndRun(bool)
{
  PServiceProcess::Current().OnStop();
}

bool MyManager::Initialise(PArgList & args, bool verbose, const PString & defaultRoute)
{
  if (!PreInitialise(args, verbose))
    return false;

  if (verbose)
    cout << "Manager creado." << endl;
  
  for (PINDEX i = 0; i < m_endpointPrefixes.GetSize(); ++i) {
    OpalConsoleEndPoint * ep = GetConsoleEndPoint(m_endpointPrefixes[i]);
    if (ep != NULL) {
      if (!ep->Initialise(args, verbose, defaultRoute))
        return false;
    }
  }
  
  if(verbose)
    cout << "Rutas predefinidas: " << GetRouteTable() << endl;
  
  return true;
}

void MyManager::OnStartMediaPatch(OpalConnection & connection, OpalMediaPatch & patch)
{
  ostream & output(cout);
  if (connection.IsNetworkConnection()) {
    OpalMediaStreamPtr stream(patch.GetSink());
    if (stream == NULL || &stream->GetConnection() != &connection)
      stream = &patch.GetSource();
    stream->PrintDetail(output, "Started");
  }

  dynamic_cast<MyCall &>(connection.GetCall()).OnStartMediaPatch(connection, patch);
  OpalManager::OnStartMediaPatch(connection, patch);
}

void MyManager::OnStopMediaPatch(OpalConnection & connection, OpalMediaPatch & patch)
{
  dynamic_cast<MyCall &>(connection.GetCall()).OnStopMediaPatch(patch);
  OpalManager::OnStopMediaPatch(connection, patch);
}

bool MyManager::OnChangedPresentationRole(OpalConnection & connection, const PString & newChairURI, bool request)
{
  PStringStream output;
  if(m_verbose)
    output << '\n' << connection.GetCall().GetToken() << ": token de rol de presentation adquirido ahora por ";
  if (newChairURI.IsEmpty())
    output << "nadie";
  else if (newChairURI == connection.GetLocalPartyURL())
    output << "usuario local";
  else
    output << '"' << newChairURI << '"';
  output << '.';
  cout << output << endl;

  return OpalManager::OnChangedPresentationRole(connection, newChairURI, request);
}

bool MyManager::PreInitialise(PArgList & args, bool verbose)
{
  PrintVersion();
  PTRACE_INITIALISE(args);
  m_verbose = verbose;
  return true;
}

void MyManager::PrintVersion() const 
{
  const PProcess & process = PProcess::Current();
  cout << "\n**************************************************\n*" << process.GetName()
       << "\n*Version: " << process.GetVersion(true) << "\n*Desarrollador: " 
       << process.GetManufacturer() << "\n*S.O: " << process.GetOSClass() << ' ' << process.GetOSName()
       << " (" << process.GetOSVersion() << '-' << process.GetOSHardware()
       << ")\n*Librerias: PTLib v" << PProcess::GetLibVersion() << " y OPAL v" << OpalGetVersion()
       << "\n*Trabajo Final de Grado \n**************************************************" << endl;
}

PBoolean MyManager::Configure(PConfig & cfg, PConfigPage * rsrc)
{
  for (PINDEX i = 0; i < m_endpointPrefixes.GetSize(); ++i)
    GetConsoleEndPoint(m_endpointPrefixes[i]);

  PString defaultSection = cfg.GetDefaultSection();
  PSYSTEMLOG(Info, "Configuring Globals");

  SetDefaultDisplayName(rsrc->AddStringField(DisplayNameKey, 30, GetDefaultDisplayName(), "Display usado en varios protocolos."));
  bool overrideProductInfo = rsrc->AddBooleanField(OverrideProductInfoKey, false, "Anula información del producto.");
  m_savedProductInfo.vendor = rsrc->AddStringField(DeveloperNameKey, 30, m_savedProductInfo.vendor);
  m_savedProductInfo.name = rsrc->AddStringField(ProductNameKey, 30, m_savedProductInfo.name);
  m_savedProductInfo.version = rsrc->AddStringField(ProductVersionKey, 30, m_savedProductInfo.version);
  if (overrideProductInfo) 
    SetProductInfo(m_savedProductInfo);
  
  m_maxCalls = rsrc->AddIntegerField(MaxSimultaneousCallsKey, 1, 9999, m_maxCalls, "", "N° max de llamadas simultáneas.");
  
  m_mediaTransferMode = cfg.GetEnum(MediaTransferModeKey, m_mediaTransferMode);
  static const char * const MediaTransferModeValues[] = { "0", "1", "2" };
  static const char * const MediaTransferModeTitles[] = { "Bypass", "Forward", "Transcode" };
  rsrc->Add(new PHTTPRadioField(MediaTransferModeKey,
    PARRAYSIZE(MediaTransferModeValues), MediaTransferModeValues, MediaTransferModeTitles,
    m_mediaTransferMode, "Modo de transferencia entre los terminales."));

  {
    OpalMediaTypeList mediaTypes = OpalMediaType::GetList();
    for (OpalMediaTypeList::iterator it = mediaTypes.begin(); it != mediaTypes.end(); ++it) {
      PString key = AutoStartKeyPrefix;
      key &= it->c_str();
      
      if (key == "Auto Start audio" || key == "Auto Start video" || key == "Auto Start presentation") {
      (*it)->SetAutoStart(cfg.GetEnum<OpalMediaType::AutoStartMode::Enumeration>(key, (*it)->GetAutoStart()));
      static const char * const AutoStartValues[] = { "Inactive", "Receive only", "Send only", "Send & Receive", "Don't offer" };
      rsrc->Add(new PHTTPEnumField<OpalMediaType::AutoStartMode::Enumeration>(key,
                        PARRAYSIZE(AutoStartValues), AutoStartValues, (*it)->GetAutoStart(),
                        "Inicio automático para tipos de media."));
     }
    }
  }

  SetAudioJitterDelay(rsrc->AddIntegerField(MinJitterKey, 20, 2000, GetMinAudioJitterDelay(), "ms", "Tamaño min buffer jitter."),
                      rsrc->AddIntegerField(MaxJitterKey, 20, 2000, GetMaxAudioJitterDelay(), "ms", "Tamaño max buffer jitter."));

  DisableDetectInBandDTMF(rsrc->AddBooleanField(InBandDTMFKey, DetectInBandDTMFDisabled(),
                                                "Deshabilita filtro digital para detección in-band DTMF (reduce uso CPU)."));
  
  SetNoMediaTimeout(PTimeInterval(0, rsrc->AddIntegerField(NoMediaTimeoutKey, 1, 365*24*60*60, GetNoMediaTimeout().GetSeconds(),
                                                           "segs", "Terminar llamada cuando no se recibe media desde remoto en este tiempo.")));
  SetTxMediaTimeout(PTimeInterval(0, rsrc->AddIntegerField(TxMediaTimeoutKey, 1, 365*24*60*60, GetTxMediaTimeout().GetSeconds(),
                                                           "segs", "Terminar llamada cuando no se transmite media al remoto en este tiempo.")));

  /*SetTCPPorts(rsrc->AddIntegerField(TCPPortBaseKey, 0, 65535, GetTCPPortBase(), "", "Rango de puerto base para streams TCP, ej. canal de señalización H.323."),
              rsrc->AddIntegerField(TCPPortMaxKey, 0, 65535, GetTCPPortMax(), "", "Rango de puerto máx para streams TCP."));
  SetUDPPorts(rsrc->AddIntegerField(UDPPortBaseKey, 0, 65535, GetUDPPortBase(), "", "Rango de puerto base para streams UDP, ej. canal de señalización SIP."),
              rsrc->AddIntegerField(UDPPortMaxKey, 0, 65535, GetUDPPortMax(), "", "Rango de puerto máx para streams UDP."));
  SetRtpIpPorts(rsrc->AddIntegerField(RTPPortBaseKey, 0, 65535, GetRtpIpPortBase(), "", "Rango de puerto base para streams RTP/UDP."),
                rsrc->AddIntegerField(RTPPortMaxKey, 0, 65535, GetRtpIpPortMax(), "", "Rango de puerto máx para streams RTP/UDP."));

  SetMediaTypeOfService(rsrc->AddIntegerField(RTPTOSKey, 0, 255, GetMediaTypeOfService(), "", "Valor para calidad de servicio (QoS)"));
  */
  
#if OPAL_PTLIB_NAT
  PSYSTEMLOG(Info, "Configuring NAT");

  {
    std::set<NATInfo> natInfo;
    for (PNatMethods::iterator it = GetNatMethods().begin(); it != GetNatMethods().end(); ++it)
      natInfo.insert(*it);

    for (std::set<NATInfo>::iterator it = natInfo.begin(); it != natInfo.end(); ++it) {
      PHTTPCompositeField * fields = new PHTTPCompositeField("NAT\\" + it->m_method, it->m_method,
                   "Habilitar flag y Servidor IP/hostname para NAT traversal usando " + it->m_friendly);
      fields->Append(new PHTTPBooleanField(NATActiveKey, it->m_active));
      fields->Append(new PHTTPStringField(NATServerKey, 0, 0, it->m_server, NULL, 1, 15));
      fields->Append(new PHTTPStringField(NATInterfaceKey, 0, 0, it->m_interface, NULL, 1, 15));
      rsrc->Add(fields);
      if (!fields->LoadFromConfig(cfg))
        SetNATServer(it->m_method, (*fields)[1].GetValue(), (*fields)[0].GetValue() *= "true", 0, (*fields)[2].GetValue());
    }
  }
#endif // OPAL_PTLIB_NAT

#if OPAL_VIDEO
  {
    unsigned prefWidth = 0, prefHeight = 0;
    PVideoFrameInfo::ParseSize(rsrc->AddStringField(ConfVideoManagerKey, 30,"CIF",
                                                    "Resolución de video preferida del manager."), prefWidth, prefHeight); 
    if (m_verbose)
      cout << "Resolución de video preferida: " << PVideoFrameInfo::AsString(prefWidth, prefHeight) << endl;

    unsigned maxWidth = 0, maxHeight = 0;
    PVideoFrameInfo::ParseSize(rsrc->AddStringField(ConfVideoMaxManagerKey, 30,"HD1080",
                                                    "Resolución de video máxima del manager"), maxWidth, maxHeight); 
    if (m_verbose)
      cout << "Resolución de video máxima: " << PVideoFrameInfo::AsString(maxWidth, maxHeight) << endl;

    double rate = rsrc->AddIntegerField(FrameRateManagerKey, 1, 60, 15, "", "Video Frame Rate");
    if (rate < 1 || rate > 60) {
      cout << "Inválido video frame rate." << endl;
    }
    if (m_verbose)
      cout << "Video frame rate: " << rate << " fps\n";

    unsigned frameTime = (unsigned)(OpalMediaFormat::VideoClockRate/rate);
    OpalBandwidth bitrate(rsrc->AddStringField(BitRateManagerKey, 30,"1Mbps", "Video Bit Rate"));
    if (bitrate < 10000) {
      cout << "Inválido video bit rate." << endl;
    }
    if (m_verbose)
      cout << "Video target bit rate: " << bitrate << '\n';

    OpalMediaFormatList formats = OpalMediaFormat::GetAllRegisteredMediaFormats();
    for (OpalMediaFormatList::iterator it = formats.begin(); it != formats.end(); ++it) {
      if (it->GetMediaType() == OpalMediaType::Video()) {
        OpalMediaFormat format = *it;
        format.SetOptionInteger(OpalVideoFormat::FrameWidthOption(), prefWidth);
        format.SetOptionInteger(OpalVideoFormat::FrameHeightOption(), prefHeight);
        format.SetOptionInteger(OpalVideoFormat::MaxRxFrameWidthOption(), maxWidth);
        format.SetOptionInteger(OpalVideoFormat::MaxRxFrameHeightOption(), maxHeight);
        format.SetOptionInteger(OpalVideoFormat::FrameTimeOption(), frameTime);
        format.SetOptionInteger(OpalVideoFormat::TargetBitRateOption(), bitrate);
        OpalMediaFormat::SetRegisteredMediaFormat(format);
      }
    }
  }
#endif

  {
    OpalMediaFormatList allFormats;
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
    }
    SetMediaFormatOrder(rsrc->AddStringArrayField(PreferredMediaKey, true, 15, GetMediaFormatOrder(), help));
  }

  SetMediaFormatMask(rsrc->AddStringArrayField(RemovedMediaKey, true, 15, GetMediaFormatMask(), "Codecs no usados o removidos."));

#if OPAL_H323
  PSYSTEMLOG(Info, "Configuring H.323");
  if (!GetH323EndPoint().Configure(cfg, rsrc))
    return false;
#endif // OPAL_H323

#if OPAL_SIP
  PSYSTEMLOG(Info, "Configuring SIP");
  if (!GetSIPEndPoint().Configure(cfg, rsrc))
    return false;
#endif // OPAL_SIP

#if OPAL_HAS_MIXER
  PSYSTEMLOG(Info, "Configuring Mixer");
  if (!GetMixerEndPoint().Configure(cfg, rsrc))
    return false;
#endif // OPAL_HAS_MIXER
  
  return ConfigureCDR(cfg, rsrc);
}

bool MyManager::ConfigureCommon(OpalEndPoint * ep,
                                const PString & cfgPrefix,
                                PConfig & cfg,
                                PConfigPage * rsrc)
{
  PString normalPrefix = ep->GetPrefixName();
  
  bool enabled = rsrc->AddBooleanField(cfgPrefix & "Enabled", true);
  
  if (cfgPrefix == "H.323" && enabled) { 
    PString defaultH323Interfaces = "*:1720";
    if (!ep->StartListeners(defaultH323Interfaces)) {
      if (m_verbose)
        cout << "No se pudo abrir listeners para " << defaultH323Interfaces << endl;
      return false;
    }
    else if (m_verbose)
      cout  << "Listeners " << cfgPrefix << ':' << ep->GetListeners() << endl;
  }
  else if (cfgPrefix == "SIP" && enabled) { 
    PString defaultSIPInterfaces = "*:5060";
    if (!ep->StartListeners(defaultSIPInterfaces)) {
      if (m_verbose)
        cout << "No se pudo abrir listeners para " << defaultSIPInterfaces << endl;
        return false;
    }
    else if (m_verbose)
      cout  << "Listeners " << cfgPrefix << ':' << ep->GetListeners() << endl;
  }
  else if (!enabled) {
    PSYSTEMLOG(Info, "Disabled " << cfgPrefix);
    ep->RemoveListener(NULL);
    if (m_verbose)
      cout << "Disable " << cfgPrefix << endl;
  }
  
  return true;
}

MyManager::CDRList::const_iterator MyManager::BeginCDR()
{
  m_cdrMutex.Wait();
  return m_cdrList.begin();
}

bool MyManager::FindCDR(const PString & guid, MyCallDetailRecord & cdr)
{
  PWaitAndSignal mutex(m_cdrMutex);

  for (MyManager::CDRList::const_iterator it = m_cdrList.begin(); it != m_cdrList.end(); ++it) {
    if (it->GetGUID() == guid) {
      cdr = *it;
      return true;
    }
  }

  return false;
}

bool MyManager::NotEndCDR(const CDRList::const_iterator & it)
{
  if (it != m_cdrList.end())
    return true;

  m_cdrMutex.Signal();
  return false;
}

void MyManager::StartRecordingCall(MyCall & call) const
{
  // A Implementar
}

void MyManager::OnChangedRegistrarAoR(const PURL & aor, bool registering)
{
#if OPAL_H323
  if (aor.GetScheme() == "h323") {
    if (aor.GetUserName().IsEmpty())
      GetH323EndPoint().AutoRegister(aor.GetHostName(), PString::Empty(), registering);
    else if (aor.GetHostName().IsEmpty())
      GetH323EndPoint().AutoRegister(aor.GetUserName(), PString::Empty(), registering);
    else if (aor.GetParamVars()("type") *= "gk")
      GetH323EndPoint().AutoRegister(aor.GetUserName(), aor.GetHostName(), registering);
    else
      GetH323EndPoint().AutoRegister(PSTRSTRM(aor.GetUserName() << '@' << aor.GetHostName()), PString::Empty(), registering);
  }
  else if (GetSIPEndPoint().GetAutoRegisterH323() && aor.GetScheme().NumCompare("sip") == EqualTo)
    GetH323EndPoint().AutoRegister(aor.GetUserName(), PString::Empty(), registering);
#endif // OPAL_H323

  if (aor.GetScheme() == "sip" && (aor.GetParamVars()("type") *= "cisco"))
    GetSIPEndPoint().AutoRegisterCisco(aor.GetHostPort(), aor.GetUserName(), aor.GetParamVars()("device"), registering);
}

#if OPAL_H323
H323ConsoleEndPoint * MyManager::CreateH323EndPoint()
{
  return new MyH323EndPoint(*this);
}

MyH323EndPoint & MyManager::GetH323EndPoint() const 
{
  return *FindEndPointAs<MyH323EndPoint>(OPAL_PREFIX_H323);
}
#endif

#if OPAL_SIP
SIPConsoleEndPoint * MyManager::CreateSIPEndPoint()
{
  return new MySIPEndPoint(*this);
}

MySIPEndPoint & MyManager::GetSIPEndPoint() const
{
  return *FindEndPointAs<MySIPEndPoint>(OPAL_PREFIX_SIP);
}
#endif

#if OPAL_HAS_MIXER
OpalConsoleMixerEndPoint * MyManager::CreateMixerEndPoint()
{
  return new MyMixerEndPoint(*this);
}

MyMixerEndPoint & MyManager::GetMixerEndPoint() const 
{
  return *FindEndPointAs<MyMixerEndPoint>(OPAL_PREFIX_MIXER);
}
#endif

OpalConsoleEndPoint * MyManager::GetConsoleEndPoint(const PString & prefix)
{
  OpalEndPoint * ep = FindEndPoint(prefix);
  if (ep == NULL) {
#if OPAL_H323
    if (prefix == OPAL_PREFIX_H323)
      ep = CreateH323EndPoint();
    else
#endif // OPAL_H323
#if OPAL_SIP
    if (prefix == OPAL_PREFIX_SIP)
      ep = CreateSIPEndPoint();
    else
#endif // OPAL_SIP
#if OPAL_HAS_MIXER
    if (prefix == OPAL_PREFIX_MIXER)
      ep = CreateMixerEndPoint();
    else
#endif // OPAL_HAS_MIXER
    {
      PTRACE(1, "Unknown prefix " << prefix);
      return NULL;
    }
  }

  return dynamic_cast<OpalConsoleEndPoint *>(ep);
}
//Final del Archivo/////////////////////////////////////////////////////////////////////////////////////////////////////