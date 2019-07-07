/*************************************************************************/
/*************************************************************************/
/*                                                                       */
/*                       C O N F I G  H E A D E R                        */
/*                                                                       */
/*************************************************************************/
/*                                                                       */
/* <Descripción>                                                         */
/*   Header que contiene los "keys" para la plantilla de configuración   */
/*  de la aplicación y otras definiciones.                               */
/*                                                                       */
/*************************************************************************/

#ifndef _CONFIG_H
#define _CONFIG_H

#include "precompile.h"

//MyClearLogPage
static const PConstString ClearLogFileStr("Clear Log File");
static const PConstString RotateLogFilesStr("Rotate Log Files");

//MyProcess
const  WORD  DefaultHTTPPort = 1719;
static const PConstString UserNameKey("Usuario");
static const PConstString PasswordKey("Contraseña");
static const PConstString LevelKey("Nivel Trazo Log");
static const PConstString FileKey("Archivo Trazo Log");
static const PConstString RotateDirKey("Directorio Trazo Log");
static const PConstString RotatePrefixKey("Prefijo Trazo Log");
static const PConstString RotateTemplateKey("Plantilla Trazo Log");
static const PConstString RotateSizeKey("Tamaño Trazo Log");
static const PConstString RotateCountKey("Conteo Trazo Log");
static const PConstString RotateAgeKey("Historial Trazo Log");
static const PConstString HTTPPortKey("Puerto HTTP");
static const PConstString HTTPInterfacesKey("Interfaces HTTP");

//MyManager
static const PConstString DisplayNameKey("Display");
static const PConstString OverrideProductInfoKey("Anular Info");
static const PConstString DeveloperNameKey("Desarrollador");
static const PConstString ProductNameKey("Nombre");
static const PConstString ProductVersionKey("Version");
static const PConstString MaxSimultaneousCallsKey("N° Llamadas");
static const PConstString MediaTransferModeKey("Transferencia de Media");
static const PConstString AutoStartKeyPrefix("Auto Start");
static const PConstString PreferredMediaKey("Codecs Preferidos");
static const PConstString RemovedMediaKey("Codecs Removidos");
static const PConstString MinJitterKey("Mínimo Jitter");
static const PConstString MaxJitterKey("Máximo Jitter");
static const PConstString InBandDTMFKey("Deshabilitar In-DTMF");
static const PConstString SilenceDetectorKey("Detector de Silencio");
static const PConstString NoMediaTimeoutKey("Timeout RX Media");
static const PConstString TxMediaTimeoutKey("Timeout TX Media");
static const PConstString TCPPortBaseKey("Puerto TCP Base");
static const PConstString TCPPortMaxKey("Puerto TCP Max");
static const PConstString RTPPortBaseKey("Puerto RTP Base");
static const PConstString RTPPortMaxKey("Puerto RTP Max");
static const PConstString RTPTOSKey("Tipo de Servicio RTP");
#if P_NAT
static const PConstString NATActiveKey("Activo");
static const PConstString NATServerKey("Servidor");
static const PConstString NATInterfaceKey("Interfaz");
#endif
#if OPAL_VIDEO
static const PConstString ConfVideoManagerKey("Resolución Estándar de Video");
static const PConstString ConfVideoMaxManagerKey("Resolución max de Video");
static const PConstString FrameRateManagerKey("Video Frame Rate");
static const PConstString BitRateManagerKey("Video Bit Rate");
#endif

//MyH323EndPoint
#if OPAL_H323
#define H323RegistrationSection "Registro H.323\\"
#define H323RegistrationNewSection H323RegistrationSection "Nuevo"
#define H323RegistrationEncryptedSection H323RegistrationSection"Encriptado"
static const PINDEX H323GatekeeperPasswordSize = 30;
static const PConstString H323AliasesKey("Alias H.323");
static const PConstString DisableFastStartKey("Deshabilitar Fast Start");
static const PConstString DisableH245TunnelingKey("Deshabilitar H.245 Tunneling");
static const PConstString DisableH245inSetupKey("Deshabilitar H.245 in Setup");
static const PConstString ForceSymmetricTCSKey("Forzar TCS Simétrico");
static const PConstString H239ControlKey("Habilitar Control H.239");
static const PConstString H323BandwidthKey("Ancho de Banda H.323");
static const PConstString GatekeeperEnableKey("Habilitar GK Remoto");
static const PConstString GatekeeperAddressKey("Dirección GK Remoto");
static const PConstString RemoteGatekeeperIdentifierKey("ID GK Remoto");
static const PConstString GatekeeperInterfaceKey("Interfaz GK Remoto");
static const PConstString GatekeeperPasswordKey("Contraseña GK Remoto");
static const PConstString GatekeeperTokenOIDKey("Token OID GK Remoto");
static const PConstString GatekeeperTimeToLiveKey("Tiempo GK Remoto");
static const PConstString GatekeeperAliasLimitKey("Limite Alias GK Remoto");
static const PConstString GatekeeperRegistrationDelayKey("Delay en Registro GK Remoto");
static const PConstString GatekeeperSimulatePatternKey("Patrón simulado GK Remoto");
static const PConstString GatekeeperRasRedirectKey("Redirección RAS GK Remoto");
static const PConstString ServerGatekeeperEnableKey("Habilitar Servidor GK");
static const PConstString ServerGatekeeperIdentifierKey("ID Servidor GK");
static const PConstString AvailableBandwidthKey("Ancho de Banda Total GK");
static const PConstString DefaultBandwidthKey("Designar Ancho de Banda Por Defecto");
static const PConstString MaximumBandwidthKey("Designar Ancho de Banda Máximo");
static const PConstString DefaultTimeToLiveKey("Tiempo GK Por Defecto");
static const PConstString CallHeartbeatTimeKey("Tiempo de validación llamada GK");
static const PConstString DisengageOnHearbeatFailKey("Colgar Si Falla Tiempo de Validación");
static const PConstString OverwriteOnSameSignalAddressKey("Sobreescritura de registro GK");
static const PConstString CanHaveDuplicateAliasKey("Duplicar Alias GK");
static const PConstString CanOnlyCallRegisteredEPKey("Solo LLamada de Terminales Registrados");
static const PConstString CanOnlyAnswerRegisteredEPKey("Solo Respuesta de Terminales Registrados");
static const PConstString AnswerCallPreGrantedARQKey("Responder Pregarantizando ARQ");
static const PConstString MakeCallPreGrantedARQKey("Llamar Pregarantizando ARQ");
static const PConstString IsGatekeeperRoutedKey("GK Enrutado");
static const PConstString AliasCanBeHostNameKey("Alias H.323 como Hostname");
static const PConstString MinAliasToAllocateKey("Alias Mínimos a Designar");
static const PConstString MaxAliasToAllocateKey("Alias Máximos a Designar");
static const PConstString RequireH235Key("GK Requiere Autentificación H.235");
static const PConstString UserNameH235Key("Usuario");
static const PConstString PasswordH235Key("Password");
static const PConstString RouteAliasKey("Alias");
static const PConstString RouteHostKey("Host");
static const PConstString GkServerListenersKey("Interfaces Servidor GK");
static const PConstString AuthenticationCredentialsName("Usuarios Autentificados GK");
static const PConstString AuthenticationCredentialsKey("Credenciales %u\\");
static const PConstString AliasRouteMapsName("Mapas de Alias");
static const PConstString AliasRouteMapsKey("Mapa de Rutas \\Mapping %u\\");
#endif // OPAL_H323

//MySIPEndPoint
#if OPAL_SIP
#define REGISTRATIONS_SECTION "Registro SIP"
#define REGISTRATIONS_KEY     REGISTRATIONS_SECTION"\\Registro %u\\"
static const PConstString SIPUsernameKey("Usuario SIP");
static const PConstString SIPPrackKey("Respuestas SIP Provisionales");
static const PConstString SIPProxyKey("Proxy URL SIP");
static const PConstString SIPLocalRegistrarKey("Registrar Dominios SIP");
static const PConstString SIPCiscoDeviceTypeKey("Tipo Dispositivo SIP Cisco");
static const PConstString SIPCiscoDevicePatternKey("Patrón Dispositivo SIP Cisco");
#if OPAL_H323
static const PConstString SIPAutoRegisterH323Key("Auto-Registro H.323");
#endif
static const PConstString SIPAddressofRecordKey("Dirección");
static const PConstString SIPAuthIDKey("ID");
static const PConstString SIPPasswordKey("Contraseña");
static const PConstString SIPRegTTLKey("TTL");
static const PConstString SIPCompatibilityKey("Compatibilidad");
#endif // OPAL_SIP

static const PConstString CDRTextFileKey("Archivo de Texto CDR");
static const PConstString CDRTextHeadingsKey("Headings CDR");
static const PConstString CDRTextFormatKey("Formato Texto CDR");
static const PConstString CDRWebPageLimitKey("Limite CDR WebPage");

#if OPAL_HAS_MIXER
static const PConstString ConfAudioOnlyKey("Solo Audio en Conferencia");
static const PConstString ConfMediaPassThruKey("Media Pass Through");
static const PConstString ConfVideoResolutionKey("Resolución de VideoConferencia");
static const PConstString ConfMixingModeKey("Modo de video");
#endif // OPAL_HAS_MIXER

#define PATH_SEPARATOR   "/"
#define SYS_RESOURCE_DIR "/home/ajoi1011/proyecto/project/resource"

#endif // CONFIG_H
/************************Final del Header*********************************/