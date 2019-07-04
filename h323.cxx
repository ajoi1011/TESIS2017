
#include "config.h"
#include "h323.h"
#include "html.h"
#include <ptclib/random.h>

PCREATE_SERVICE_MACRO_BLOCK(H323EndPointStatus,resource,P_EMPTY,block)
{
  GkStatusPage * status = dynamic_cast<GkStatusPage *>(resource.m_resource);
  return PAssertNULL(status)->m_gkServer.OnLoadEndPointStatus(block);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static PStringToString GetMyAliasPasswords(PConfig & cfg)
{
  PStringToString clearPwd;

  PStringToString encryptedPwd = cfg.GetAllKeyValues(H323RegistrationEncryptedSection);
  for (PStringToString::iterator encryptedPwdIter = encryptedPwd.begin(); encryptedPwdIter != encryptedPwd.end(); ++encryptedPwdIter)
  {
    clearPwd.SetAt(encryptedPwdIter->first, PHTTPPasswordField::Decrypt(encryptedPwdIter->second));
  }

  PStringToString newPwd = cfg.GetAllKeyValues(H323RegistrationNewSection);
  for (PStringToString::iterator newPwdIter = newPwd.begin(); newPwdIter != newPwd.end(); ++newPwdIter)
  {
    PHTTPPasswordField encryptedValue("", H323GatekeeperPasswordSize, newPwdIter->second);
    cfg.SetString(H323RegistrationEncryptedSection, newPwdIter->first, encryptedValue.GetValue());
    cfg.DeleteKey(H323RegistrationNewSection, newPwdIter->first);
  }

  clearPwd.Merge(newPwd, PStringToString::e_MergeOverwrite);
  return clearPwd;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MyH323EndPoint::MyH323EndPoint(MyManager & manager)
  : H323ConsoleEndPoint(manager)
  , m_manager(manager)
  , P_DISABLE_MSVC_WARNINGS(4355, m_gkServer(*this))
  , m_firstConfig(true)
  , m_configuredAliases(GetAliasNames())
  , m_configuredAliasPatterns(GetAliasNamePatterns())
{
  terminalType = e_MCUWithAVMP;
}

bool MyH323EndPoint::Initialise(PArgList & args, bool verbose, const PString & defaultRoute)
{
  AddRoutesFor(this, defaultRoute);
  return true;
}

void MyH323EndPoint::AutoRegister(const PString & alias, const PString & gk, bool registering)
{
  if (alias.IsEmpty())
    return;

  if (alias.Find('-') == P_MAX_INDEX && alias.Find("..") != P_MAX_INDEX) {
    if (registering)
      AddAliasNamePattern(alias, gk);
    else
      RemoveAliasNamePattern(alias);
  }
  else {
    if (registering)
      AddAliasName(alias, gk);
    else
      RemoveAliasName(alias);
  }
}

bool MyH323EndPoint::Configure(PConfig & cfg, PConfigPage * rsrc)
{
  if (!m_manager.ConfigureCommon(this, "H.323", cfg, rsrc))
    return false;

  {
    PStringArray newAliases = rsrc->AddStringArrayField(H323AliasesKey, 
                                                        false, 0, m_configuredAliases, "Alias H.323 para usuario local.", 1, 20);
    for (PINDEX i = 0; i < m_configuredAliases.GetSize(); ++i) {
      if (newAliases.GetValuesIndex(m_configuredAliases[i]) == P_MAX_INDEX)
        RemoveAliasName(m_configuredAliases[i]);
    }
    if (m_firstConfig)
      SetAliasNames(newAliases);
    else
      AddAliasNames(newAliases);
    m_configuredAliases = newAliases;
  }

  DisableFastStart(rsrc->AddBooleanField(DisableFastStartKey, IsFastStartDisabled(), "Deshabilita H.323 Fast Connect."));
  DisableH245Tunneling(rsrc->AddBooleanField(DisableH245TunnelingKey, IsH245TunnelingDisabled(), "Deshabilita  tunelizado H.245 en canal de señalización H.225.0."));
  DisableH245inSetup(rsrc->AddBooleanField(DisableH245inSetupKey, IsH245inSetupDisabled(), "Deshabilita envio inicial H.245 PDU tunelizado en SETUP PDU."));
  ForceSymmetricTCS(rsrc->AddBooleanField(ForceSymmetricTCSKey, IsForcedSymmetricTCS(), "Forza indicación de codecs simétricos en TCS."));
  bool h239Control = rsrc->AddBooleanField(H239ControlKey, false, "Habilita control H.239.");
  if (h239Control)
    SetDefaultH239Control(true); 

  SetInitialBandwidth(OpalBandwidth::RxTx, rsrc->AddIntegerField(H323BandwidthKey, 1, OpalBandwidth::Max()/1000,
                                                                 GetInitialBandwidth(OpalBandwidth::RxTx)/1000,
                                                                 "kb/s", "Ancho de banda requerido por el gatekeeper para originar/recibir llamadas.")*1000);

  bool gkEnable = rsrc->AddBooleanField(GatekeeperEnableKey, false, "Habilita registro gatekeeper como cliente.");
  
  PString gkAddress = rsrc->AddStringField(GatekeeperAddressKey, 0, PString::Empty(),
                                           "IP/hostname del gatekeeper para registro, en blanco se usa un broadcast.", 1, 30);

  SetAliasPasswords(GetMyAliasPasswords(cfg), gkAddress);

  PString gkIdentifier = rsrc->AddStringField(RemoteGatekeeperIdentifierKey, 0, PString::Empty(),
                                              "Identificador del gatekeeper para registro, en blanco se usa cualquier gatekeeper.", 1, 30);

  PString gkInterface = rsrc->AddStringField(GatekeeperInterfaceKey, 0, PString::Empty(),
                                             "Interfaz de red local para registro del gatekeeper, en blanco todas las interfaces de red son usadas.", 1, 30);

  PString gkPassword = PHTTPPasswordField::Decrypt(cfg.GetString(GatekeeperPasswordKey));
  if (!gkPassword.IsEmpty())
    SetGatekeeperPassword(gkPassword);
  rsrc->Add(new PHTTPPasswordField(GatekeeperPasswordKey, H323GatekeeperPasswordSize, gkPassword,
                                   "Contraseña para autentificación del gatekeeper, usuario es el primer alias."));

  SetGkAccessTokenOID(rsrc->AddStringField(GatekeeperTokenOIDKey, 0, GetGkAccessTokenOID(),
                                           "Token de acceso OID del gatekeeper para soporte H.235.", 1, 30));

  SetGatekeeperTimeToLive(PTimeInterval(0,rsrc->AddIntegerField(GatekeeperTimeToLiveKey, 0, 24*60*60, 
                                                                GetGatekeeperTimeToLive().GetSeconds(), "segundos",
                                                                "Tiempo de vida del gatekeeper para el re-registro.")));

  SetGatekeeperAliasLimit(rsrc->AddIntegerField(GatekeeperAliasLimitKey, 1, H323EndPoint::MaxGatekeeperAliasLimit, 
                                                GetGatekeeperAliasLimit(), NULL,
                                                "Por asuntos de compatibilidad con algunos gatekeepers para registrar un largo número de alias en un simple RRQ."));

  SetGatekeeperStartDelay(rsrc->AddIntegerField(GatekeeperRegistrationDelayKey, 0, 10000, 
                                                GetGatekeeperStartDelay().GetSeconds(), "milisegundos",
                                                "Delay de mensajes GRQ para reducir la carga en gatekeeper remoto."));

  SetGatekeeperSimulatePattern(rsrc->AddBooleanField(GatekeeperSimulatePatternKey, GetGatekeeperSimulatePattern(),
                                                     "Por asuntos de compatibilidad con algunos gatekeepers por no soportar patrones de alias, genera alias separados en rangos."));

  SetGatekeeperRasRedirect(rsrc->AddBooleanField(GatekeeperRasRedirectKey, GetGatekeeperRasRedirect(),
                                                 "Por asuntos de compatibilidad con algunos gatekeepers usando dirección RAS para gatekeeper alterno."));

  if (gkEnable)
    UseGatekeeper(gkAddress, gkIdentifier, gkInterface);
  else {
    PSYSTEMLOG(Info, "Not using remote gatekeeper.");
    RemoveGatekeeper();
  }

  m_firstConfig = false;

  return m_gkServer.Configure(cfg, rsrc);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MyGatekeeperServer::MyGatekeeperServer(H323EndPoint & ep)
  : H323GatekeeperServer(ep)
  , endpoint(ep)
{
}

H323GatekeeperCall * MyGatekeeperServer::CreateCall(const OpalGloballyUniqueID & id,
                                                    H323GatekeeperCall::Direction dir)
{
  return new MyGatekeeperCall(*this, id, dir);
}

PBoolean MyGatekeeperServer::TranslateAliasAddress(const H225_AliasAddress & alias,
                                                   H225_ArrayOf_AliasAddress & aliases,
                                                   H323TransportAddress & address,
                                                   PBoolean & isGkRouted,
                                                   H323GatekeeperCall * call)
{

  if (H323GatekeeperServer::TranslateAliasAddress(alias, aliases, address, isGkRouted, call))
    return true;

  PString aliasString = H323GetAliasAddressString(alias);
  PTRACE(4, "Translating \"" << aliasString << "\" through route maps");

  PWaitAndSignal mutex(reconfigurationMutex);

  for (PINDEX i = 0; i < routes.GetSize(); i++) {
    PTRACE(4, "Checking route map " << routes[i]);
    if (routes[i].IsMatch(aliasString)) {
      address = routes[i].GetHost();
      PTRACE(3, "Translated \"" << aliasString << "\" to " << address);
      break;
    }
  }

  return true;
}

bool MyGatekeeperServer::Configure(PConfig & cfg, PConfigPage * rsrc)
{
  PINDEX i;

  PWaitAndSignal mutex(reconfigurationMutex);

  bool srvEnable = rsrc->AddBooleanField(ServerGatekeeperEnableKey, true, "Habilita servidor gatekeeper.");
  if (!srvEnable) {
    RemoveListener(NULL);
    return true;
  }

  SetGatekeeperIdentifier(rsrc->AddStringField(ServerGatekeeperIdentifierKey, 0,
                                               MyProcess::Current().GetName() + " en " + PIPSocket::GetHostName(),
                                               "Identificador del servidor gatekeeper.", 1, 30));

  AddListeners(rsrc->AddStringArrayField(GkServerListenersKey, false, 0, PStringArray(),
                                         "Interfaces de red local para escuchar RAS, en blanco se usan todas.", 1, 25));

  SetAvailableBandwidth(rsrc->AddIntegerField(AvailableBandwidthKey, 1, INT_MAX, GetAvailableBandwidth()/10,
                                              "kb/s", "Ancho de banda total a designar para todas las llamadas a través del servidor gatekeeper.")*10);

  SetDefaultBandwidth(rsrc->AddIntegerField(DefaultBandwidthKey, 1, INT_MAX, GetDefaultBandwidth()/10,
                                            "kb/s", "Ancho de banda (por defecto) a designar para todas las llamadas a través del servidor gatekeeper")*10);

  SetMaximumBandwidth(rsrc->AddIntegerField(MaximumBandwidthKey, 1, INT_MAX, GetMaximumBandwidth() / 10,
                                            "kb/s", "Ancho de banda (máximo) a designar para todas las llamadas a través del servidor gatekeeper")*10);

  SetTimeToLive(rsrc->AddIntegerField(DefaultTimeToLiveKey, 10, 86400, GetTimeToLive(),
                                      "segundos", "Tiempo del servidor gatekeeper para asumir si el terminal remoto esta fuera de línea."));

  SetInfoResponseRate(rsrc->AddIntegerField(CallHeartbeatTimeKey, 0, 86400, GetInfoResponseRate(),
                                            "segundos", "Tiempo de validación de peticiones en llamada controlada por el servidor gatekeeper."));

  SetDisengageOnHearbeatFail(rsrc->AddBooleanField(DisengageOnHearbeatFailKey, GetDisengageOnHearbeatFail(), "Colgar llamada si tiempo de validación (IRR) falla."));

  SetOverwriteOnSameSignalAddress(rsrc->AddBooleanField(OverwriteOnSameSignalAddressKey, GetOverwriteOnSameSignalAddress(),
                                                        "Sobreescribe en una dirección de señal específica para anular previo registro."));

  SetCanHaveDuplicateAlias(rsrc->AddBooleanField(CanHaveDuplicateAliasKey, GetCanHaveDuplicateAlias(),
                                                 "Diferentes terminales pueden registrarse en el gatekeeper con el mismo alias de otro terminal."));

  SetCanOnlyCallRegisteredEP(rsrc->AddBooleanField(CanOnlyCallRegisteredEPKey, GetCanOnlyCallRegisteredEP(),
                                                   "Gatekeeper permitirá a un terminal llamar a otro registrado localmente."));

  SetCanOnlyAnswerRegisteredEP(rsrc->AddBooleanField(CanOnlyAnswerRegisteredEPKey, GetCanOnlyAnswerRegisteredEP(),
                                                     "Gatekeeper permitirá a un terminal responder a otro registrado localmente."));

  SetAnswerCallPreGrantedARQ(rsrc->AddBooleanField(AnswerCallPreGrantedARQKey, GetAnswerCallPreGrantedARQ(),
                                                   "Gatekeeper garantiza todas las llamadas entrantes al terminal."));

  SetMakeCallPreGrantedARQ(rsrc->AddBooleanField(MakeCallPreGrantedARQKey, GetMakeCallPreGrantedARQ(),
                                                 "Gatekeeper garantiza todas las llamadas salientes del terminal."));

  SetAliasCanBeHostName(rsrc->AddBooleanField(AliasCanBeHostNameKey, GetAliasCanBeHostName(),
                                              "Gatekeeper permite registrar simplemente su hostname/dirección IP."));

  SetMinAliasToAllocate(rsrc->AddIntegerField(MinAliasToAllocateKey, 0, INT_MAX, GetMinAliasToAllocate(), "",
                                              "Mínimo valor para alias que gatekeeper podrá designar cuando el terminal no provea ninguna, 0 deshabilitadas."));

  SetMaxAliasToAllocate(rsrc->AddIntegerField(MaxAliasToAllocateKey, 0, INT_MAX, GetMaxAliasToAllocate(), "",
                                              "Máximo valor para alias que gatekeeper podrá designar cuando el terminal no provea ninguna, 0 deshabilitadas."));

  SetGatekeeperRouted(rsrc->AddBooleanField(IsGatekeeperRoutedKey, IsGatekeeperRouted(),
                                            "Habilita a todos los terminales a enrutar la señalización para todas las llamadas a través del gatekeeper."));

  PHTTPCompositeField * routeFields = new PHTTPCompositeField(AliasRouteMapsKey, AliasRouteMapsName,
                                                              "Fija mapeo de alias asociadas a hostnames para llamadas a través del gatekeeper.");
  routeFields->Append(new PHTTPStringField(RouteAliasKey, 0, NULL, NULL, 1, 20));
  routeFields->Append(new PHTTPStringField(RouteHostKey, 0, NULL, NULL, 1, 30));
  PHTTPFieldArray * routeArray = new PHTTPFieldArray(routeFields, true);
  rsrc->Add(routeArray);

  routes.RemoveAll();
  if (!routeArray->LoadFromConfig(cfg)) {
    for (i = 0; i < routeArray->GetSize(); ++i) {
      PHTTPCompositeField & item = dynamic_cast<PHTTPCompositeField &>((*routeArray)[i]);
      RouteMap map(item[0].GetValue(), item[1].GetValue());
      if (map.IsValid())
        routes.Append(new RouteMap(map));
    }
  }

  SetRequiredH235(rsrc->AddBooleanField(RequireH235Key, IsRequiredH235(),
                                        "Gatekeeper requiere autentificación criptográfica para registro."));

  PHTTPCompositeField * security = new PHTTPCompositeField(AuthenticationCredentialsKey, AuthenticationCredentialsName,
                                                           "Tabla de usuario/contraseña para terminales autentificados en el gatekeeper, requiere H.235.");
  security->Append(new PHTTPStringField("UsernameH235Key", 25));
  security->Append(new PHTTPPasswordField("PasswordH235Key", 25));
  PHTTPFieldArray * securityArray = new PHTTPFieldArray(security, false);
  rsrc->Add(securityArray);

  ClearPasswords();
  if (!securityArray->LoadFromConfig(cfg)) {
    for (i = 0; i < securityArray->GetSize(); ++i) {
      PHTTPCompositeField & item = dynamic_cast<PHTTPCompositeField &>((*securityArray)[i]);
      SetUsersPassword(item[0].GetValue(), PHTTPPasswordField::Decrypt(item[1].GetValue()));
    }
  }

  PTRACE(3, "Gatekeeper server configured");
  return true;
}

bool MyGatekeeperServer::ForceUnregister(const PString id)
{
  PSafePtr<H323RegisteredEndPoint> ep = FindEndPointByIdentifier(id);
  return ep != NULL && ep->Unregister();
}

PString MyGatekeeperServer::OnLoadEndPointStatus(const PString & htmlBlock)
{
  PString substitution;

  for (PSafePtr<H323RegisteredEndPoint> ep = GetFirstEndPoint(PSafeReadOnly); ep != NULL; ep++) {
    // make a copy of the repeating html chunk
    PString insert = htmlBlock;

    PServiceHTML::SpliceMacro(insert, "status EndPointIdentifier", ep->GetIdentifier());

    PStringStream addresses;
    for (PINDEX i = 0; i < ep->GetSignalAddressCount(); i++) {
      if (i > 0)
        addresses << "<br>";
      addresses << ep->GetSignalAddress(i);
    }
    PServiceHTML::SpliceMacro(insert, "status CallSignalAddresses", addresses);

    PStringStream aliases;
    for (PINDEX i = 0; i < ep->GetAliasCount(); i++) {
      if (i > 0)
        aliases << "<br>";
      aliases << ep->GetAlias(i);
    }
    PServiceHTML::SpliceMacro(insert, "status EndPointAliases", aliases);

    PString str = "<i>Name:</i> " + ep->GetApplicationInfo();
    str.Replace("\t", "<BR><i>Version:</i> ");
    str.Replace("\t", " <i>Vendor:</i> ");
    PServiceHTML::SpliceMacro(insert, "status Application", str);

    PServiceHTML::SpliceMacro(insert, "status ActiveCalls", ep->GetCallCount());

    // Then put it into the page, moving insertion point along after it.
    substitution += insert;
  }

  return substitution;
}

MyGatekeeperServer::RouteMap::RouteMap(const PString & theAlias, const PString & theHost)
  : m_alias(theAlias)
  , m_regex('^' + theAlias + '$')
  , m_host(theHost)
{
}

bool MyGatekeeperServer::RouteMap::IsMatch(const PString & alias) const
{
  PINDEX start;
  return m_regex.Execute(alias, start);
}

bool MyGatekeeperServer::RouteMap::IsValid() const
{
  return !m_alias.IsEmpty() && m_regex.GetErrorCode() == PRegularExpression::NoError && !m_host.IsEmpty();
}

void MyGatekeeperServer::RouteMap::PrintOn(ostream & strm) const
{
  strm << '"' << m_alias << "\" => " << m_host;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MyGatekeeperCall::MyGatekeeperCall(MyGatekeeperServer & gk,
                                   const OpalGloballyUniqueID & id,
                                   Direction dir)
  : H323GatekeeperCall(gk, id, dir)
{

}

MyGatekeeperCall::~MyGatekeeperCall()
{
}

H323GatekeeperRequest::Response MyGatekeeperCall::OnAdmission(H323GatekeeperARQ & info)
{
#ifdef TEST_TOKEN
  info.acf.IncludeOptionalField(H225_AdmissionConfirm::e_tokens);
  info.acf.m_tokens.SetSize(1);
  info.acf.m_tokens[0].m_tokenOID = "1.2.36.76840296.1";
  info.acf.m_tokens[0].IncludeOptionalField(H235_ClearToken::e_nonStandard);
  info.acf.m_tokens[0].m_nonStandard.m_nonStandardIdentifier = "1.2.36.76840296.1.1";
  info.acf.m_tokens[0].m_nonStandard.m_data = "SnfYt0jUuZ4lVQv8umRYaH2JltXDRW6IuYcnASVU";
#endif

  return H323GatekeeperCall::OnAdmission(info);
}
