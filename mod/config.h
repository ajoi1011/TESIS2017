#ifndef _OpenMCU_CONFIG_H
#define _OpenMCU_CONFIG_H


#  define PATH_SEPARATOR "/"

// specify ffmpeg path
#define FFMPEG_PATH	"/usr/bin/ffmpeg"

// enable test rooms for video mixer
#define ENABLE_TEST_ROOMS   1

// enable echo room for video mixer
#define ENABLE_ECHO_MIXER   1

// default log/trace level
#define DEFAULT_LOG_LEVEL   0
#define DEFAULT_TRACE_LEVEL 0

#define OPENMCU_VIDEO   1


// maximum Video frame rate (for outgoing video)
#ifndef MAX_FRAME_RATE
#define MAX_FRAME_RATE  999
#endif

// enable/disable swresample usage
#define USE_SWRESAMPLE  0
// enable/disable avresample usage
#define USE_AVRESAMPLE  0
// enable/disable libsamplerate usage
#define USE_LIBSAMPLERATE  0

// enable/disable freetype2 (rendering names)
#define USE_FREETYPE	0

// enable/disable libjpeg (live video frames in Room Control Page)
#define USE_LIBJPEG	1

// enable/disable libyuv (optimized frame resize)
#define USE_LIBYUV	0

// libyuv filtering type: kFilterNone|kFilterBilinear|kFilterBox
#define LIBYUV_FILTER	kFilterBilinear

#ifndef SYS_CONFIG_DIR
#  define SYS_CONFIG_DIR "/home/ajoi1011/proyectos/testing/mcu/openmcu-ru/conf"
#endif
#  define SYS_RESOURCE_DIR "/home/ajoi1011/proyectos/testing/mcu/openmcu-ru/resource"
#define CONFIG_PATH PString(SYS_CONFIG_DIR)+PATH_SEPARATOR+"openmcu.ini"


#endif // _OpenMCU-ru_CONFIG_H

