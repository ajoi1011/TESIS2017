/* plugin_config.h.  Generated from plugin_config.h.in by configure.  */
/* plugin_config.h.in.  Generated from configure.ac by autoheader.  */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you have the system header */
#define HAVE_UNISTD_H 1
/* #undef HAVE_MALLOC_H */

/* Standard integer types */
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
/* #undef int8_t */
/* #undef int16_t */
/* #undef int32_t */
/* #undef int64_t */
/* #undef intptr_t */
/* #undef uint8_t */
/* #undef uint16_t */
/* #undef uint32_t */
/* #undef uint64_t */
/* #undef uintptr_t */

/* define if posix_memalign is available */
#define HAVE_POSIX_MEMALIGN 1

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to 1 if you have the <capi20.h> header file. */
/* #undef HAVE_CAPI20_H */

/* celt */
/* #undef HAVE_CELT */

/* iLBC variants */
/* #undef OPAL_SYSTEM_ILBC */

/* Speex */
#define OPAL_SYSTEM_SPEEX 1

/* Define to 1 if you have the <linux/telephony.h> header file. */
/* #undef HAVE_LINUX_TELEPHONY_H */

/* Define to 1 if you have the <sys/telephony.h> header file. */
/* #undef HAVE_SYS_TELEPHONY_H */

/* Directory with libavcodec source code, for MPEG4 rate control correction */
/* #undef LIBAVCODEC_HAVE_SOURCE_DIR */

/* Filename of the libavcodec library */
#define LIBAVCODEC_LIB_NAME "libavcodec.so"

/* Filename of the libavcodec library */
#define LIBAVUTIL_LIB_NAME "libavutil.so"

/* Compatibilty with various versions of the libavcodec library */
/* #undef AV_PICTURE_TYPE_NONE */

/* Compatibilty with various versions of the libavcodec library */
/* #undef AV_PICTURE_TYPE_I */

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "OPAL-Plugins"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "OPAL-Plugins 3.19alpha5"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "opal-plugins"

/* Define to the version of this package. */
#define PACKAGE_VERSION "3.19alpha5"

/* For win32 build */
/* #undef PLUGIN_CODEC_DLL_EXPORTS */

