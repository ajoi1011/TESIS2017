/*
 * sip.cxx
 *
 * Project sip class
 *
 */
#include "sip.h"

#define REGISTRATIONS_SECTION "SIP Registrations"
#define REGISTRATIONS_KEY     REGISTRATIONS_SECTION"\\Registration %u\\"

MySIPEndPoint::MySIPEndPoint(MyManager & manager)
  : SIPConsoleEndPoint(manager)
  , m_manager(manager)
{
}

bool MySIPEndPoint::Initialise(PArgList & args, bool verbose, const PString & defaultRoute)
{
  AddRoutesFor(this, defaultRoute);

  if (verbose)
   cout << "Terminal SIP inicializado" << endl;
  
  return true;
}

bool MySIPEndPoint::Configure(PConfig & cfg, PConfigPage * rsrc)
{
  if (!m_manager.ConfigureCommon(this, "SIP", cfg, rsrc))
    return false;

  // Add SIP parameters
  SetDefaultLocalPartyName(rsrc->AddStringField("SIPUsernameKey", 72, GetDefaultLocalPartyName(), "SIP local user name"));

  SetInitialBandwidth(OpalBandwidth::RxTx, rsrc->AddIntegerField("SIPBandwidthKey", 1, OpalBandwidth::Max()/1000,
                                                                  GetInitialBandwidth(OpalBandwidth::RxTx)/1000,
                              "kb/s", "Bandwidth to request to gatekeeper on originating/answering calls")*1000);

  SIPConnection::PRACKMode prack = cfg.GetEnum("SIPPrackKey", GetDefaultPRACKMode());
  static const char * const prackModes[] = { "Disabled", "Supported", "Required" };
  rsrc->Add(new PHTTPRadioField("SIPPrackKey", PARRAYSIZE(prackModes), prackModes, prack,
    "SIP provisional responses (100rel) handling."));
  SetDefaultPRACKMode(prack);

  SetProxy(rsrc->AddStringField("SIPProxyKey", 100, GetProxy().AsString(), "SIP outbound proxy IP/hostname", 1, 72));

  // Registrars
  PHTTPCompositeField * registrationsFields = new PHTTPCompositeField(REGISTRATIONS_KEY, REGISTRATIONS_SECTION,
                                                  "Registration of SIP username at domain/hostname/IP address");
  registrationsFields->Append(new PHTTPStringField("Address", 0, NULL, NULL, 1, 5));
  registrationsFields->Append(new PHTTPStringField("AuthID", 0, NULL, NULL, 1, 5));
  registrationsFields->Append(new PHTTPIntegerField("RegTTL", 1, 86400, 300));
  registrationsFields->Append(new PHTTPPasswordField("Password", 5));
  static const char * const compatibilityModes[] = {
      "Compliant", "Single Contact", "No Private", "ALGw", "RFC 5626", "Cisco"
  };
  registrationsFields->Append(new PHTTPEnumField<SIPRegister::CompatibilityModes>("CMode",
                                                   PARRAYSIZE(compatibilityModes), compatibilityModes));
  PHTTPFieldArray * registrationsArray = new PHTTPFieldArray(registrationsFields, false);
  rsrc->Add(registrationsArray);

  SetRegistrarDomains(rsrc->AddStringArrayField("SIPLocalRegistrarKey", false, 58,
    PStringArray(m_registrarDomains), "SIP local registrar domain names"));

  /*m_ciscoDeviceType = rsrc->AddIntegerField(SIPCiscoDeviceTypeKey, 1, 32767, m_ciscoDeviceType, "",
    "Device type for SIP Cisco Devices. Default 30016 = Cisco IP Communicator.");

  m_ciscoDevicePattern = rsrc->AddStringField(SIPCiscoDevicePatternKey, 0, m_ciscoDevicePattern,
    "Pattern used to register SIP Cisco Devices. Default SEPFFFFFFFFFFFF", 1, 80);

#if OPAL_H323
  m_autoRegisterH323 = rsrc->AddBooleanField(SIPAutoRegisterH323Key, m_autoRegisterH323,
                                             "Auto-register H.323 alias of same name as incoming SIP registration");
#endif*/


  if (!registrationsArray->LoadFromConfig(cfg)) {
    for (PINDEX i = 0; i < registrationsArray->GetSize(); ++i) {
      PHTTPCompositeField & item = dynamic_cast<PHTTPCompositeField &>((*registrationsArray)[i]);

      SIPRegister::Params registrar;
      registrar.m_addressOfRecord = item[0].GetValue();
      if (!registrar.m_addressOfRecord.IsEmpty()) {
        registrar.m_authID = item[1].GetValue();
        registrar.m_expire = item[2].GetValue().AsUnsigned();
        registrar.m_password = PHTTPPasswordField::Decrypt(item[3].GetValue());
        registrar.m_compatibility = (SIPRegister::CompatibilityModes)item[4].GetValue().AsUnsigned();
        //registrar.m_ciscoDeviceType = PString(m_ciscoDeviceType);
        //registrar.m_ciscoDevicePattern = m_ciscoDevicePattern;
        PString aor;
        if (Register(registrar, aor))
          PSYSTEMLOG(Info, "Started register of " << aor);
        else
          PSYSTEMLOG(Error, "Could not register " << registrar.m_addressOfRecord);
      }
    }
  }

  return true;
}

