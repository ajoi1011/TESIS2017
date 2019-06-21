#ifndef _CONFIG_H
#define _CONFIG_H

#include "precompile.h"

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
static const PConstString PreferredMediaKey("Preferred Media");
static const PConstString RemovedMediaKey("Removed Media");
static const PConstString MinJitterKey("Mínimo Jitter");
static const PConstString MaxJitterKey("Máximo Jitter");
static const PConstString InBandDTMFKey("Deshabilita In-DTMF");
static const PConstString NoMediaTimeoutKey("Timeout RX Media");
static const PConstString TxMediaTimeoutKey("Timeout TX Media");
static const PConstString TCPPortBaseKey("Puerto TCP Base");
static const PConstString TCPPortMaxKey("Puerto TCP Max");
static const PConstString UDPPortBaseKey("Puerto UDP Base");
static const PConstString UDPPortMaxKey("Puerto UDP Max");
static const PConstString RTPPortBaseKey("Puerto RTP Base");
static const PConstString RTPPortMaxKey("Puerto RTP Max");
static const PConstString RTPTOSKey("Tipo de Servicio RTP");
#if P_NAT
static const PConstString NATActiveKey("Activo");
static const PConstString NATServerKey("Servidor");
static const PConstString NATInterfaceKey("Interfaz");
#endif
#if OPAL_HAS_MIXER
static const PConstString ConfAudioOnlyKey("Conference Audio Only");
static const PConstString ConfMediaPassThruKey("Conference Media Pass Through");
static const PConstString ConfVideoResolutionKey("Conference Video Resolution");

static const PConstString RecordAllCallsKey("Record All Calls");
static const PConstString RecordFileTemplateKey("Record File Template");
static const PConstString RecordStereoKey("Record Stereo");
static const PConstString RecordAudioFormatKey("Record Audio Format");
#endif







#define PATH_SEPARATOR   "/"
// specify the path executable files
#define SYS_BIN_DIR "/usr/local/bin"
// specify the path for .conf files
#define SYS_CONFIG_DIR "/usr/local/share/opalserver/config"
// specify the path for built-in web server resources
//#define SYS_RESOURCE_DIR "/usr/local/share/opalserver/resource"
#define SYS_RESOURCE_DIR "/home/ajoi1011/proyecto/project/resource"
// specify server logs folder
#define SERVER_LOGS "/usr/local/share/opalserver/log"

/*#ifndef SYS_CONFIG_DIR
#  define SYS_CONFIG_DIR "."
#endif
#define CONFIG_PATH PString(SYS_CONFIG_DIR)+PATH_SEPARATOR+"openmcu.ini"*/


#endif // CONFIG_H

