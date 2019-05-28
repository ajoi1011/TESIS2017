/*
 * manager.cxx
 *
 * Project manager class
 *
 */

#include "call.h"
#include "h323.h"
#include "manager.h"
#include "mixer.h"

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

MyManager::MyManager()
  : OpalManagerConsole(OPAL_CONSOLE_PREFIXES OPAL_PREFIX_MIXER)
  , m_savedProductInfo(GetProductInfo())
  , m_maxCalls(9999)
  , m_mediaTransferMode(MediaTransferForward)
  , m_verbose(false)
  , m_cdrListMax(100)
{
#if OPAL_VIDEO
  for (OpalVideoFormat::ContentRole role = OpalVideoFormat::BeginContentRole; role < OpalVideoFormat::EndContentRole; ++role)
    m_videoInputDevice[role].deviceName = m_videoPreviewDevice[role].deviceName = m_videoOutputDevice[role].deviceName = P_NULL_VIDEO_DEVICE;
  
  PStringArray devices = PVideoOutputDevice::GetDriversDeviceNames("*"); // Get all devices on all drivers
  PINDEX i;
  for (i = 0; i < devices.GetSize(); ++i) {
    PCaselessString dev = devices[i];
    if (dev[0] != '*' && dev != P_NULL_VIDEO_DEVICE) {
      m_videoOutputDevice[OpalVideoFormat::ePresentation].deviceName = dev;
      SetAutoStartReceiveVideo(true);
      cout << "Default video output/preview device set to \"" << devices[i] << '"' << endl;
      break;
    }
  }
#endif

}

MyManager::~MyManager()
{
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

bool MyManager::PreInitialise(PArgList & args, bool verbose)
{
  PrintVersion();
  PTRACE_INITIALISE(args);
  m_verbose = verbose;
  return true;
}

bool MyManager::Initialise(PArgList & args, bool verbose, const PString & defaultRoute)
{
  if(!PreInitialise(args, verbose))
   return false;

  if(verbose)
   cout << "Manager OPAL creado" << endl;
  
  for (PINDEX i = 0; i < m_endpointPrefixes.GetSize(); ++i) {
   OpalConsoleEndPoint * ep = GetConsoleEndPoint(m_endpointPrefixes[i]);
   if (ep != NULL) {
    if (!ep->Initialise(args, verbose, defaultRoute))
     return false;
   }
  }

  cout << "Rutas predefinidas: " << GetRouteTable() << endl;

  return true;

}

/*#if OPAL_PTLIB_NAT
bool MyManager::SetNATServer(const PString & method, const PString & server, bool activate, unsigned priority)
{
  PNatMethod * natMethod = m_natMethods->GetMethodByName(method);
  if (natMethod == NULL) {
    PTRACE(2, "Unknown NAT method \"" << method << '"');
    return false;
  }

  natMethod->Activate(activate);
  m_natMethods->SetMethodPriority(method, priority);

  natMethod->SetPortRanges(GetUDPPortRange().GetBase(), GetUDPPortRange().GetMax(),
                           GetRtpIpPortRange().GetBase(), GetRtpIpPortRange().GetMax());
  if (!natMethod->SetServer(server)) {
    PTRACE(2, "Invalid server \"" << server << "\" for " << method << " NAT method");
    return false;
  }

  if (!natMethod->Open(PIPSocket::GetDefaultIpAny())) {
    PTRACE(2, "Could not open server \"" << server << " for " << method << " NAT method");
    return false;
  }

  PTRACE(3, "NAT " << *natMethod);
  return true;
}
#endif*/

PBoolean MyManager::Configure(PConfig & cfg, PConfigPage * rsrc)
{
  // Make sure all endpoints created (4)
  for (PINDEX i = 0; i < m_endpointPrefixes.GetSize(); ++i)
    GetConsoleEndPoint(m_endpointPrefixes[i]);

  PString defaultSection = cfg.GetDefaultSection();
  PSYSTEMLOG(Info, "Configuring Globals");

  // General parameters for all endpoint types
  SetDefaultDisplayName(rsrc->AddStringField("Display", 30, GetDefaultDisplayName(), "Display name used in various protocols"));

  bool overrideProductInfo = rsrc->AddBooleanField("Info de aplicacion", false, "Override the default product information");
  m_savedProductInfo.vendor = rsrc->AddStringField("Desarrollador", 30, m_savedProductInfo.vendor);
  m_savedProductInfo.name = rsrc->AddStringField("Nombre", 30, m_savedProductInfo.name);
  m_savedProductInfo.version = rsrc->AddStringField("Version", 30, m_savedProductInfo.version);
  if (overrideProductInfo) 
    SetProductInfo(m_savedProductInfo);

  m_maxCalls = rsrc->AddIntegerField("Llamadas Simultaneas", 1, 9999, m_maxCalls, "", "Maximum simultaneous calls");

  m_mediaTransferMode = cfg.GetEnum("MediaTransferModeKey", m_mediaTransferMode);
  static const char * const MediaTransferModeValues[] = { "0", "1", "2" };
  static const char * const MediaTransferModeTitles[] = { "Bypass", "Forward", "Transcode" };
  rsrc->Add(new PHTTPRadioField("MediaTransferModeKey",
    PARRAYSIZE(MediaTransferModeValues), MediaTransferModeValues, MediaTransferModeTitles,
    m_mediaTransferMode, "How media is to be routed between the endpoints."));

  {
    OpalMediaTypeList mediaTypes = OpalMediaType::GetList();
    for (OpalMediaTypeList::iterator it = mediaTypes.begin(); it != mediaTypes.end(); ++it) {
      PString key = "Auto Start";
      key &= it->c_str();
      
      if (key == "Auto Start audio" || key == "Auto Start video") {
      (*it)->SetAutoStart(cfg.GetEnum<OpalMediaType::AutoStartMode::Enumeration>(key, (*it)->GetAutoStart()));
      static const char * const AutoStartValues[] = { "Inactive", "Receive only", "Send only", "Send & Receive", "Don't offer" };
      rsrc->Add(new PHTTPEnumField<OpalMediaType::AutoStartMode::Enumeration>(key,
                        PARRAYSIZE(AutoStartValues), AutoStartValues, (*it)->GetAutoStart(),
                        "Initial start up mode for media type."));
     }
    }
  }

  SetAudioJitterDelay(rsrc->AddIntegerField("Minimo Jitter", 20, 2000, GetMinAudioJitterDelay(), "ms", "Minimum jitter buffer size"),
                      rsrc->AddIntegerField("Maximo Jitter", 20, 2000, GetMaxAudioJitterDelay(), "ms", "Maximum jitter buffer size"));

  DisableDetectInBandDTMF(rsrc->AddBooleanField("InBandaDTMF", DetectInBandDTMFDisabled(),
                                                "Disable digital filter for in-band DTMF detection (saves CPU usage)"));
  
#if OPAL_PTLIB_NAT
  PSYSTEMLOG(Info, "Configuring NAT");

  {
    std::set<NATInfo> natInfo;

    // Need to make a copy of info as call SetNATServer alters GetNatMethods() so iterator fails
    for (PNatMethods::iterator it = GetNatMethods().begin(); it != GetNatMethods().end(); ++it)
      natInfo.insert(*it);

    for (std::set<NATInfo>::iterator it = natInfo.begin(); it != natInfo.end(); ++it) {
      PHTTPCompositeField * fields = new PHTTPCompositeField("NAT\\" + it->m_method, it->m_method,
                   "Enable flag and Server IP/hostname for NAT traversal using " + it->m_friendly);
      fields->Append(new PHTTPBooleanField("NATActiveKey", it->m_active));
      fields->Append(new PHTTPStringField("NATServerKey", 0, 0, it->m_server, NULL, 1, 15));
      fields->Append(new PHTTPStringField("NATInterfaceKey", 0, 0, it->m_interface, NULL, 1, 15));
      rsrc->Add(fields);
      if (!fields->LoadFromConfig(cfg))
        SetNATServer(it->m_method, (*fields)[1].GetValue(), (*fields)[0].GetValue() *= "true", 0, (*fields)[2].GetValue());
    }
  }
#endif // P_NAT

#if OPAL_H323
  PSYSTEMLOG(Info, "Configuring H.323");
  if (!GetH323EndPoint().Configure(cfg, rsrc))
   return false;
#endif // OPAL_H323

#if OPAL_HAS_MIXER
  if (!GetMixerEndPoint().Configure(cfg, rsrc))
   return false;
#endif
  
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
   PString defaulth323Interfaces = "*:1720";
    if (!ep->StartListeners(defaulth323Interfaces)) {
     if (m_verbose)
      cout <<"No se pudo abrir listeners para " << defaulth323Interfaces << endl;
     return false;
     }
    else if (m_verbose)
     cout  <<"Listeners " << cfgPrefix << ':' << ep->GetListeners() << endl;
  }
  else if (!enabled) {
   PSYSTEMLOG(Info, "Disabled " << cfgPrefix);
   ep->RemoveListener(NULL);
   if (m_verbose)
    cout << "Disable " << cfgPrefix << endl;
  }
  
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


  /*PSafePtr<OpalConnection> other = connection.GetOtherPartyConnection();
  if (other != NULL) {
    // Detect if we have loopback endpoint, and start pas thro on the other connection
    if (connection.GetPrefixName() == LoopbackPrefix)
      SetMediaPassThrough(*other, *other, true, patch.GetSource().GetSessionID());
    else if (other->GetPrefixName() == LoopbackPrefix)
      SetMediaPassThrough(connection, connection, true, patch.GetSource().GetSessionID());
  }*/

  dynamic_cast<MyCall &>(connection.GetCall()).OnStartMediaPatch(connection, patch);
  OpalManager::OnStartMediaPatch(connection, patch);
}


void MyManager::OnStopMediaPatch(OpalConnection & connection, OpalMediaPatch & patch)
{
  dynamic_cast<MyCall &>(connection.GetCall()).OnStopMediaPatch(patch);
  OpalManager::OnStopMediaPatch(connection, patch);
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

MyManager::CDRList::const_iterator MyManager::BeginCDR()
{
  m_cdrMutex.Wait();
  return m_cdrList.begin();
}

bool MyManager::NotEndCDR(const CDRList::const_iterator & it)
{
  if (it != m_cdrList.end())
    return true;

  m_cdrMutex.Signal();
  return false;
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
/*#if OPAL_SIP
    if (prefix == OPAL_PREFIX_SIP)
      ep = CreateSIPEndPoint();
    else
#endif // OPAL_SIP*/
#if OPAL_HAS_MIXER
    if (prefix == OPAL_PREFIX_MIXER)
      ep = CreateMixerEndPoint();
    else
#endif
    {
      PTRACE(1, "Unknown prefix " << prefix);
      return NULL;
    }
  }

  return dynamic_cast<OpalConsoleEndPoint *>(ep);
}


