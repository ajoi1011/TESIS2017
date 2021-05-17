/*
 * h323.cxx
 * 
 * Copyright 2020 ajoi1011 <ajoi1011@debian>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

#include "managers.h"
#include "h323.h"

MCUH323EndPoint::MCUH323EndPoint(Manager & manager)
  : H323ConsoleEndPoint(manager)
  , m_manager(manager)
  //, P_DISABLE_MSVC_WARNINGS(4355, m_gkServer(*this))
  , m_firstConfig(true)
  , m_configuredAliases(GetAliasNames())
  , m_configuredAliasPatterns(GetAliasNamePatterns())
{
  terminalType = e_MCUWithAVMP;
}

bool MCUH323EndPoint::Initialise(PArgList & args, bool verbose, const PString & defaultRoute)
{
  AddRoutesFor(this, defaultRoute);
  return true;
}

bool MCUH323EndPoint::Configure(PConfig & cfg)
{
  cfg.SetDefaultSection("H323Parameters");
  PString section = cfg.GetDefaultSection();
  
  bool enabled = cfg.GetBoolean(section,"H323EnableKey",true);
  PStringArray listeners = cfg.GetString(section,"H323ListenerKey","").Lines();
  
  if (!enabled) {
    PSYSTEMLOG(Info, "Disabled H.323");
    RemoveListener(NULL);
  }
  else if (!StartListeners(listeners)) {
    PSYSTEMLOG(Error, "Could not open any listeners for H.323");
  }

  {
	PConfig config(section);
    PStringArray newAliases = config.GetString("H323AliasesKey",GetAliasNames()[0]).Tokenise(",");
    if (m_firstConfig)
      SetAliasNames(m_configuredAliases);
    
    if(!newAliases.IsEmpty()) {
      for (PINDEX i = 0; i < m_configuredAliases.GetSize(); ++i) {
        if(!m_configuredAliases[i].IsEmpty() && newAliases.GetValuesIndex(m_configuredAliases[i]) == P_MAX_INDEX)
          RemoveAliasName(m_configuredAliases[i]);
      }
      
	  for(PINDEX i = 0; i < m_configuredAliases.GetSize(); ++i) {
	    if(!m_configuredAliases[i].IsEmpty())
          AddAliasNames(m_configuredAliases[i]);
      }
      m_configuredAliases = newAliases;
    }
    else
      m_configuredAliases = GetAliasNames();
  }
  
  DisableFastStart(cfg.GetBoolean(section, "DisableFastStartKey",IsFastStartDisabled()));
  DisableH245Tunneling(cfg.GetBoolean(section,"DisableH245TunnelingKey",IsH245TunnelingDisabled()));
  DisableH245inSetup(cfg.GetBoolean(section,"DisableH245inSetupKey",IsH245inSetupDisabled()));
  ForceSymmetricTCS(cfg.GetBoolean(section,"ForceSymmetricTCSKey",IsForcedSymmetricTCS()));
  bool h239Control = cfg.GetBoolean(section,"H239ControlKey",false);
  if (h239Control)
    SetDefaultH239Control(true); 

  SetInitialBandwidth(OpalBandwidth::RxTx, cfg.GetInteger(section,"H323BandwidthKey", GetInitialBandwidth(OpalBandwidth::RxTx)/1000)*1000);
  
  /*bool gkEnable = rsrc->AddBooleanField(GatekeeperEnableKey, false, "Habilita registro gatekeeper como cliente.");
  
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
  */
  m_firstConfig = false;

  return true;//m_gkServer.Configure(cfg, rsrc);
}
