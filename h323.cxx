/*
 * h323.cxx
 *
 * Project h323 class
 *
 */
#include "h323.h"
#include "html.h"
#include <ptclib/random.h>

#define H323RegistrationSection "H.323 Registration\\"
#define H323RegistrationNewSection H323RegistrationSection"New"
#define H323RegistrationEncryptedSection H323RegistrationSection"Encrypted"

static const PINDEX H323GatekeeperPasswordSize = 30;

PCREATE_SERVICE_MACRO_BLOCK(H323EndPointStatus,resource,P_EMPTY,block)
{
  GkStatusPage * status = dynamic_cast<GkStatusPage *>(resource.m_resource);
  return PAssertNULL(status)->m_gkServer.OnLoadEndPointStatus(block);
}


///////////////////////////////////////////////////////////////

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

  if (verbose)
   cout << "Terminal H323 inicializado" << endl;
  
  return true;
}

bool MyH323EndPoint::Configure(PConfig & cfg, PConfigPage * rsrc)
{
  if (!m_manager.ConfigureCommon(this, "H.323", cfg, rsrc))
    return false;

  // Add H.323 parameters
  {
    PStringArray newAliases = rsrc->AddStringArrayField("H323AliasesKey", false, 0, m_configuredAliases, "H.323 Alias names for local user", 1, 20);
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

  DisableFastStart(rsrc->AddBooleanField("DisableFastStartKey",  IsFastStartDisabled(), "Disable H.323 Fast Connect feature"));
  DisableH245Tunneling(rsrc->AddBooleanField("DisableH245TunnelingKey",  IsH245TunnelingDisabled(), "Disable H.245 tunneled in H.225.0 signalling channel"));
  DisableH245inSetup(rsrc->AddBooleanField("DisableH245inSetupKey",  IsH245inSetupDisabled(), "Disable sending initial tunneled H.245 PDU in SETUP PDU"));
  ForceSymmetricTCS(rsrc->AddBooleanField("ForceSymmetricTCSKey", IsForcedSymmetricTCS(), "Force indication of symmetric codecs in TCS"));
  bool h239Control = rsrc->AddBooleanField("H239 Control", false);
  if (h239Control) {
    SetDefaultH239Control(true); 
    cout << "H239 Control is " << GetDefaultH239Control() <<endl;
  }
  
  SetInitialBandwidth(OpalBandwidth::RxTx, rsrc->AddIntegerField("H323BandwidthKey", 1, OpalBandwidth::Max()/1000,
                                                                  GetInitialBandwidth(OpalBandwidth::RxTx)/1000,
                              "kb/s", "Bandwidth to request to gatekeeper on originating/answering calls")*1000);

  bool gkEnable = rsrc->AddBooleanField("GatekeeperEnableKey", false, "Enable registration with gatekeeper as client");

  PString gkAddress = rsrc->AddStringField("GatekeeperAddressKey", 0, PString::Empty(),
      "IP/hostname of gatekeeper to register with, if blank a broadcast is used", 1, 30);

  SetAliasPasswords(GetMyAliasPasswords(cfg), gkAddress);

  PString gkIdentifier = rsrc->AddStringField("RemoteGatekeeperIdentifierKey", 0, PString::Empty(),
                "Gatekeeper identifier to register with, if blank any gatekeeper is used", 1, 30);

  PString gkInterface = rsrc->AddStringField("GatekeeperInterfaceKey", 0, PString::Empty(),
            "Local network interface to use to register with gatekeeper, if blank all are used", 1, 30);

  PString gkPassword = PHTTPPasswordField::Decrypt(cfg.GetString("GatekeeperPasswordKey"));
  if (!gkPassword.IsEmpty())
    SetGatekeeperPassword(gkPassword);
  rsrc->Add(new PHTTPPasswordField("GatekeeperPasswordKey", H323GatekeeperPasswordSize, gkPassword,
            "Password for gatekeeper authentication, user is the first alias"));

  SetGkAccessTokenOID(rsrc->AddStringField("GatekeeperTokenOIDKey", 0, GetGkAccessTokenOID(),
                                   "Gatekeeper access token OID for H.235 support", 1, 30));

  SetGatekeeperTimeToLive(PTimeInterval(0,rsrc->AddIntegerField("GatekeeperTimeToLiveKey",
                          10, 24*60*60, GetGatekeeperTimeToLive().GetSeconds(), "seconds",
                          "Time to Live for gatekeeper re-registration.")));

  SetGatekeeperAliasLimit(rsrc->AddIntegerField("GatekeeperAliasLimitKey",
            1, H323EndPoint::MaxGatekeeperAliasLimit, GetGatekeeperAliasLimit(), NULL,
            "Compatibility issue with some gatekeepers not being able to register large numbers of aliases in single RRQ."));

  SetGatekeeperStartDelay(rsrc->AddIntegerField("GatekeeperRegistrationDelayKey",
            0, 10000, GetGatekeeperStartDelay().GetSeconds(), "milliseconds",
            "Delay the GRQ messages to reduce the load on the remote gatekeeper."));

  SetGatekeeperSimulatePattern(rsrc->AddBooleanField("GatekeeperSimulatePatternKey", GetGatekeeperSimulatePattern(),
            "Compatibility issue with some gatekeepers not supporting alias patterns, generate separate aliases for ranges."));

  SetGatekeeperRasRedirect(rsrc->AddBooleanField("GatekeeperRasRedirectKey", GetGatekeeperRasRedirect(),
            "Compatibility issue with some gatekeepers using RAS address for alternate gatekeeper."));

  if (gkEnable)
    UseGatekeeper(gkAddress, gkIdentifier, gkInterface);
  else {
    PSYSTEMLOG(Info, "Not using remote gatekeeper.");
    RemoveGatekeeper();
  }

  m_firstConfig = false;

  return m_gkServer.Configure(cfg, rsrc);
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

MyGatekeeperServer::MyGatekeeperServer(H323EndPoint & ep)
  : H323GatekeeperServer(ep),
    endpoint(ep)
{
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


bool MyGatekeeperServer::ForceUnregister(const PString id)
{
  PSafePtr<H323RegisteredEndPoint> ep = FindEndPointByIdentifier(id);
  return ep != NULL && ep->Unregister();
}


bool MyGatekeeperServer::Configure(PConfig & cfg, PConfigPage * rsrc)
{
  PINDEX i;

  PWaitAndSignal mutex(reconfigurationMutex);

  bool srvEnable = rsrc->AddBooleanField("ServerGatekeeperEnableKey", true, "Enable gatekeeper server");
  if (!srvEnable) {
    RemoveListener(NULL);
    return true;
  }

  SetGatekeeperIdentifier(rsrc->AddStringField("ServerGatekeeperIdentifierKey", 0,
                                               MyProcess::Current().GetName() + " on " + PIPSocket::GetHostName(),
                                               "Identifier (name) for gatekeeper server", 1, 30));

  // Interfaces to listen on
  AddListeners(rsrc->AddStringArrayField("GkServerListenersKey", false, 0, PStringArray(),
                  "Local network interfaces to listen for RAS, blank means all", 1, 25));

  SetAvailableBandwidth(rsrc->AddIntegerField("AvailableBandwidthKey", 1, INT_MAX, GetAvailableBandwidth()/10,
                       "kb/s", "Total bandwidth to allocate across all calls through gatekeeper server")*10);

  SetDefaultBandwidth(rsrc->AddIntegerField("DefaultBandwidthKey", 1, INT_MAX, GetDefaultBandwidth()/10,
               "kb/s", "Default bandwidth to allocate for a call through gatekeeper server")*10);

  SetMaximumBandwidth(rsrc->AddIntegerField("MaximumBandwidthKey", 1, INT_MAX, GetMaximumBandwidth() / 10,
                  "kb/s", "Maximum bandwidth to allow for a call through gatekeeper server")*10);

  SetTimeToLive(rsrc->AddIntegerField("DefaultTimeToLiveKey", 10, 86400, GetTimeToLive(),
              "seconds", "Default time before assume endpoint is offline to gatekeeper server"));

  SetInfoResponseRate(rsrc->AddIntegerField("CallHeartbeatTimeKey", 0, 86400, GetInfoResponseRate(),
                    "seconds", "Time between validation requests on call controlled by gatekeeper server"));

  SetDisengageOnHearbeatFail(rsrc->AddBooleanField("DisengageOnHearbeatFailKey", GetDisengageOnHearbeatFail(),
                                                                  "Hang up call if heartbeat (IRR) fails."));

  SetOverwriteOnSameSignalAddress(rsrc->AddBooleanField("OverwriteOnSameSignalAddressKey", GetOverwriteOnSameSignalAddress(),
               "Allow new registration to gatekeeper on a specific signal address to override previous registration"));

  SetCanHaveDuplicateAlias(rsrc->AddBooleanField("CanHaveDuplicateAliasKey", GetCanHaveDuplicateAlias(),
      "Different endpoint can register with gatekeeper the same alias name as another endpoint"));

  SetCanOnlyCallRegisteredEP(rsrc->AddBooleanField("CanOnlyCallRegisteredEPKey", GetCanOnlyCallRegisteredEP(),
                           "Gatekeeper will only allow EP to call another endpoint registered localy"));

  SetCanOnlyAnswerRegisteredEP(rsrc->AddBooleanField("CanOnlyAnswerRegisteredEPKey", GetCanOnlyAnswerRegisteredEP(),
                         "Gatekeeper will only allow endpoint to answer another endpoint registered localy"));

  SetAnswerCallPreGrantedARQ(rsrc->AddBooleanField("AnswerCallPreGrantedARQKey", GetAnswerCallPreGrantedARQ(),
                                               "Gatekeeper pre-grants all incoming calls to endpoint"));

  SetMakeCallPreGrantedARQ(rsrc->AddBooleanField("MakeCallPreGrantedARQKey", GetMakeCallPreGrantedARQ(),
                                       "Gatekeeper pre-grants all outgoing calls from endpoint"));

  SetAliasCanBeHostName(rsrc->AddBooleanField("AliasCanBeHostNameKey", GetAliasCanBeHostName(),
              "Gatekeeper allows endpoint to simply register its host name/IP address"));

  SetMinAliasToAllocate(rsrc->AddIntegerField("MinAliasToAllocateKey", 0, INT_MAX, GetMinAliasToAllocate(), "",
       "Minimum value for aliases gatekeeper will allocate when endpoint does not provide one, 0 disables"));

  SetMaxAliasToAllocate(rsrc->AddIntegerField("MaxAliasToAllocateKey", 0, INT_MAX, GetMaxAliasToAllocate(), "",
       "Maximum value for aliases gatekeeper will allocate when endpoint does not provide one, 0 disables"));

  SetGatekeeperRouted(rsrc->AddBooleanField("IsGatekeeperRoutedKey", IsGatekeeperRouted(),
             "All endpoionts will route sigaling for all calls through the gatekeeper"));

  PHTTPCompositeField * routeFields = new PHTTPCompositeField("AliasRouteMapsKey", "AliasRouteMapsName",
                                                              "Fixed mapping of alias names to hostnames for calls routed through gatekeeper");
  routeFields->Append(new PHTTPStringField("RouteAliasKey", 0, NULL, NULL, 1, 20));
  routeFields->Append(new PHTTPStringField("RouteHostKey", 0, NULL, NULL, 1, 30));
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

  SetRequiredH235(rsrc->AddBooleanField("RequireH235Key", IsRequiredH235(),
      "Gatekeeper requires H.235 cryptographic authentication for registrations"));

  PHTTPCompositeField * security = new PHTTPCompositeField("AuthenticationCredentialsKey", "AuthenticationCredentialsName",
                                                           "Table of username/password for authenticated endpoints on gatekeeper, requires H.235");
  security->Append(new PHTTPStringField("UsernameKey", 25));
  security->Append(new PHTTPPasswordField("PasswordKey", 25));
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


MyGatekeeperServer::RouteMap::RouteMap(const PString & theAlias, const PString & theHost)
  : m_alias(theAlias),
    m_regex('^' + theAlias + '$'),
    m_host(theHost)
{
}


void MyGatekeeperServer::RouteMap::PrintOn(ostream & strm) const
{
  strm << '"' << m_alias << "\" => " << m_host;
}


bool MyGatekeeperServer::RouteMap::IsValid() const
{
  return !m_alias.IsEmpty() && m_regex.GetErrorCode() == PRegularExpression::NoError && !m_host.IsEmpty();
}


bool MyGatekeeperServer::RouteMap::IsMatch(const PString & alias) const
{
  PINDEX start;
  return m_regex.Execute(alias, start);
}


///////////////////////////////////////////////////////////////

MyGatekeeperCall::MyGatekeeperCall(MyGatekeeperServer & gk,
                                   const OpalGloballyUniqueID & id,
                                   Direction dir)
  : H323GatekeeperCall(gk, id, dir)
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


MyGatekeeperCall::~MyGatekeeperCall()
{
}



