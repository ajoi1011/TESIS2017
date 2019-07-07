
#include "config.h"
#include "sip.h"

MySIPEndPoint::MySIPEndPoint(MyManager & manager)
  : SIPConsoleEndPoint(manager)
  , m_manager(manager)
#if OPAL_H323
  , m_autoRegisterH323(false)
#endif
  , m_ciscoDeviceType(0)
  , m_ciscoDevicePattern("SEPFFFFFFFFFFFF")
{
}

bool MySIPEndPoint::Initialise(PArgList & args, bool verbose, const PString & defaultRoute)
{
  AddRoutesFor(this, defaultRoute);
  return true;
}

bool MySIPEndPoint::Configure(PConfig & cfg, PConfigPage * rsrc)
{
  if (!m_manager.ConfigureCommon(this, "SIP", cfg, rsrc))
    return false;

  SetDefaultLocalPartyName(rsrc->AddStringField(SIPUsernameKey, 30, GetDefaultLocalPartyName(), "Usuario SIP."));

  SIPConnection::PRACKMode prack = cfg.GetEnum(SIPPrackKey, GetDefaultPRACKMode());
  static const char * const prackModes[] = { "Disabled", "Supported", "Required" };
  rsrc->Add(new PHTTPRadioField(SIPPrackKey, PARRAYSIZE(prackModes), prackModes, prack,
                                "Manejo de respuestas SIP provisionales (100rel)."));
  SetDefaultPRACKMode(prack);

  SetProxy(rsrc->AddStringField(SIPProxyKey, 100, GetProxy().AsString(), "Proxy SIP outbound IP/hostname.", 1, 30));

  PHTTPCompositeField * registrationsFields = new PHTTPCompositeField(REGISTRATIONS_KEY, REGISTRATIONS_SECTION,
                                                                      "Registro de usuarios SIP en el dominio/hostname/dirección IP.");
  registrationsFields->Append(new PHTTPStringField(SIPAddressofRecordKey, 0, NULL, NULL, 1, 5));
  registrationsFields->Append(new PHTTPStringField(SIPAuthIDKey, 0, NULL, NULL, 1, 5));
  registrationsFields->Append(new PHTTPIntegerField(SIPRegTTLKey, 1, 86400, 300));
  registrationsFields->Append(new PHTTPPasswordField(SIPPasswordKey, 5));
  static const char * const compatibilityModes[] = {
      "Compliant", "Single Contact", "No Private", "ALGw", "RFC 5626", "Cisco"
  };
  registrationsFields->Append(new PHTTPEnumField<SIPRegister::CompatibilityModes>(SIPCompatibilityKey,
                                                                                  PARRAYSIZE(compatibilityModes), compatibilityModes));
  PHTTPFieldArray * registrationsArray = new PHTTPFieldArray(registrationsFields, false);
  rsrc->Add(registrationsArray);

  SetRegistrarDomains(rsrc->AddStringArrayField(SIPLocalRegistrarKey, false, 58, PStringArray(m_registrarDomains), 
                                                "Registrar nombres de dominio SIP."));

  m_ciscoDeviceType = rsrc->AddIntegerField(SIPCiscoDeviceTypeKey, 1, 32767, m_ciscoDeviceType, "",
                                            "Tipo de recurso para dispositivos SIP Cisco. Por defecto 30016 = Cisco IP Communicator.");

  m_ciscoDevicePattern = rsrc->AddStringField(SIPCiscoDevicePatternKey, 0, m_ciscoDevicePattern,
                                              "Patrón usado para registrar dispositivos SIP Cisco. Por defecto SEPFFFFFFFFFFFF", 1, 30);

#if OPAL_H323
  m_autoRegisterH323 = rsrc->AddBooleanField(SIPAutoRegisterH323Key, m_autoRegisterH323,
                                             "Auto-registra alias H.323 del mismo nombre como registro SIP.");
#endif

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
        registrar.m_ciscoDeviceType = PString(m_ciscoDeviceType);
        registrar.m_ciscoDevicePattern = m_ciscoDevicePattern;
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

PString MySIPEndPoint::OnLoadEndPointStatus(const PString & htmlBlock)
{
  PString substitution;

  for (PSafePtr<RegistrarAoR> ua(m_registeredUAs); ua != NULL; ++ua) {
    PString insert = htmlBlock;

    PServiceHTML::SpliceMacro(insert, "status EndPointIdentifier", ua->GetAoR());

    SIPURLList contacts = ua->GetContacts();
    PStringStream addresses;
    for (SIPURLList::iterator it = contacts.begin(); it != contacts.end(); ++it) {
      if (it != contacts.begin())
        addresses << "<br>";
      addresses << *it;
    }
    PServiceHTML::SpliceMacro(insert, "status CallSignalAddresses", addresses);

    PString str = "<i>Name:</i> " + ua->GetProductInfo().AsString();
    str.Replace("\t", "<BR><i>Version:</i> ");
    str.Replace("\t", " <i>Vendor:</i> ");
    PServiceHTML::SpliceMacro(insert, "status Application", str);

    PServiceHTML::SpliceMacro(insert, "status ActiveCalls", "N/A");

    substitution += insert;
  }

  return substitution;
}

bool MySIPEndPoint::ForceUnregister(const PString id)
{
  return m_registeredUAs.RemoveAt(id);
}

#if OPAL_H323 
void MySIPEndPoint::OnChangedRegistrarAoR(RegistrarAoR & ua)
{
  m_manager.OnChangedRegistrarAoR(ua.GetAoR(), ua.HasBindings());
}

void MySIPEndPoint::AutoRegisterCisco(const PString & server, const PString & wildcard, const PString & deviceType, bool registering)
{
  PStringArray names, servers;
  ExpandWildcards(wildcard, server, names, servers);

  for (PINDEX i = 0; i < names.GetSize(); ++i) {
    PString addressOfRecord = "sip:" + names[i] + "@" + server;

    if (registering) {
      SIPRegister::Params registrar;
      registrar.m_addressOfRecord = addressOfRecord;
      registrar.m_compatibility = SIPRegister::e_Cisco;
      if (deviceType.IsEmpty())
        registrar.m_ciscoDeviceType = PString(m_ciscoDeviceType);
      else
        registrar.m_ciscoDeviceType = deviceType;

      registrar.m_ciscoDevicePattern = m_ciscoDevicePattern;
      PString aor;
      if (Register(registrar, aor))
        PSYSTEMLOG(Info, "Started register of " << aor);
      else
        PSYSTEMLOG(Error, "Could not register " << registrar.m_addressOfRecord);
    }
    else {
      Unregister(addressOfRecord);
    }
  }
}
#endif // OPAL_H323 Final del Archivo