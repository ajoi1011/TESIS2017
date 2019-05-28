/*
 * h323.cxx
 *
 * Project h323 class
 *
 */
#include "h323.h"

MyH323EndPoint::MyH323EndPoint(MyManager & manager)
  : H323ConsoleEndPoint(manager)
  , m_manager(manager)
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

  /*bool gkEnable = rsrc->AddBooleanField(GatekeeperEnableKey, false, "Enable registration with gatekeeper as client");

  PString gkAddress = rsrc->AddStringField(GatekeeperAddressKey, 0, PString::Empty(),
      "IP/hostname of gatekeeper to register with, if blank a broadcast is used", 1, 30);

  SetAliasPasswords(GetMyAliasPasswords(cfg), gkAddress);

  PString gkIdentifier = rsrc->AddStringField(RemoteGatekeeperIdentifierKey, 0, PString::Empty(),
                "Gatekeeper identifier to register with, if blank any gatekeeper is used", 1, 30);

  PString gkInterface = rsrc->AddStringField(GatekeeperInterfaceKey, 0, PString::Empty(),
            "Local network interface to use to register with gatekeeper, if blank all are used", 1, 30);

  PString gkPassword = PHTTPPasswordField::Decrypt(cfg.GetString(GatekeeperPasswordKey));
  if (!gkPassword.IsEmpty())
    SetGatekeeperPassword(gkPassword);
  rsrc->Add(new PHTTPPasswordField(GatekeeperPasswordKey, H323GatekeeperPasswordSize, gkPassword,
            "Password for gatekeeper authentication, user is the first alias"));

  SetGkAccessTokenOID(rsrc->AddStringField(GatekeeperTokenOIDKey, 0, GetGkAccessTokenOID(),
                                   "Gatekeeper access token OID for H.235 support", 1, 30));

  SetGatekeeperTimeToLive(PTimeInterval(0,rsrc->AddIntegerField(GatekeeperTimeToLiveKey,
                          10, 24*60*60, GetGatekeeperTimeToLive().GetSeconds(), "seconds",
                          "Time to Live for gatekeeper re-registration.")));

  SetGatekeeperAliasLimit(rsrc->AddIntegerField(GatekeeperAliasLimitKey,
            1, H323EndPoint::MaxGatekeeperAliasLimit, GetGatekeeperAliasLimit(), NULL,
            "Compatibility issue with some gatekeepers not being able to register large numbers of aliases in single RRQ."));

  SetGatekeeperStartDelay(rsrc->AddIntegerField(GatekeeperRegistrationDelayKey,
            0, 10000, GetGatekeeperStartDelay().GetSeconds(), "milliseconds",
            "Delay the GRQ messages to reduce the load on the remote gatekeeper."));

  SetGatekeeperSimulatePattern(rsrc->AddBooleanField(GatekeeperSimulatePatternKey, GetGatekeeperSimulatePattern(),
            "Compatibility issue with some gatekeepers not supporting alias patterns, generate separate aliases for ranges."));

  SetGatekeeperRasRedirect(rsrc->AddBooleanField(GatekeeperRasRedirectKey, GetGatekeeperRasRedirect(),
            "Compatibility issue with some gatekeepers using RAS address for alternate gatekeeper."));

  if (gkEnable)
    UseGatekeeper(gkAddress, gkIdentifier, gkInterface);
  else {
    PSYSTEMLOG(Info, "Not using remote gatekeeper.");
    RemoveGatekeeper();
  }*/

  m_firstConfig = false;

  return true;//m_gkServer.Configure(cfg, rsrc);
}

