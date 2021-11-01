#
# ptlib_config.mak
#
# mak rules to be included in a ptlib application Makefile.
#
# Portable Tools Library
#
# Copyright (c) 1993-2013 Equivalence Pty. Ltd.
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is Portable Windows Library.
#
# The Initial Developer of the Original Code is Equivalence Pty. Ltd.
#
# Portions are Copyright (C) 1993 Free Software Foundation, Inc.
# All Rights Reserved.
# 
# Contributor(s): ______________________________________.
#

PTLIB_MAJOR :=2
PTLIB_MINOR :=19
PTLIB_STAGE :=-alpha
PTLIB_PATCH :=4
PTLIB_OEM   :=0

# detected platform
target      := linux_x86
target_os   := linux
target_cpu  := x86

# The install directories
ifndef prefix
  prefix := /usr/local
endif
ifndef exec_prefix
  exec_prefix := ${prefix}
endif
ifndef libdir
  libdir := ${exec_prefix}/lib
endif
ifndef includedir
  includedir := ${prefix}/include
endif
ifndef datarootdir
  datarootdir := ${prefix}/share
endif

# Tool names detected by configure
CC      := gcc
CXX     := g++
LD      := g++
AR      := ar
RANLIB  := ranlib
STRIP   := strip
OBJCOPY := objcopy
DSYMUTIL:= 
LN_S    := ln -s
MKDIR_P := mkdir -p
BISON   := /usr/bin/bison
INSTALL := /usr/bin/install -c -C
SVN     := /usr/bin/svn
GIT     := /usr/bin/git

CPPFLAGS          :=   -pthread -I/usr/include/gstreamer-1.0 -I/usr/include/glib-2.0 -I/usr/lib/i386-linux-gnu/glib-2.0/include  -D_REENTRANT -I/usr/include/SDL2  -I/usr/include/ncursesw   -I/usr/include/lua5.1     -Wall -Wno-unknown-pragmas  -fno-diagnostics-show-caret -D_REENTRANT -DPTRACING=2  $(CPPFLAGS)
CXXFLAGS          := -fexceptions -Wreorder -felide-constructors -Wno-overloaded-virtual -fno-omit-frame-pointer -march=i686  -Wno-deprecated-declarations -pthread $(CXXFLAGS)
CFLAGS            := -fno-omit-frame-pointer -march=i686  -pthread -D__STDC_CONSTANT_MACROS $(CFGLAGS)
LDFLAGS           :=  $(LDFLAGS)
LIBS              := -ldl -lodbc -lgio-2.0 -lgstapp-1.0 -lgstbase-1.0 -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0  -lSDL2  -lncurses  -lv8  -llua5.1  -lexpat  -lldap -llber -lsasl2 -ljpeg  -lpcap -lresolv -lrt   $(LIBS)
SHARED_CPPFLAGS   := -fPIC
SHARED_LDFLAGS     = -shared -Wl,--build-id,-soname,$(LIB_SONAME)
DEBUG_CPPFLAGS    := -D_DEBUG 
DEBUG_CFLAGS      := -gdwarf-4 
OPT_CPPFLAGS      := -DNDEBUG 
OPT_CFLAGS        := -O3 
CPLUSPLUS_STD     := 
ARFLAGS           := rc
YFLAGS            := @YFLAGS@

SHAREDLIBEXT      := so
STATICLIBEXT      := a
DEBUGINFOEXT      := debug

PTLIB_PLUGIN_SUFFIX := _ptplugin

PTLIB_PLUGIN_DIR  := ptlib-2.19.4

P_PROFILING       := 
HAS_ADDRESS_SANITIZER := 
HAS_THREAD_SANITIZER := 

HAS_NETWORKING    := 1
HAS_IPV6          := 1
HAS_DNS_RESOLVER  := 1
HAS_PCAP          := 1
HAS_OPENSSL       := 
HAS_SSL           := 
HAS_OPENLDAP      := 1
HAS_LDAP          := 1
HAS_SASL          := 
HAS_SASL2         := 1
HAS_EXPAT         := 1
HAS_REGEX         := 1
HAS_SDL           := 1
HAS_PLUGINMGR     := 1
HAS_PLUGINS       := 1
HAS_SAMPLES       := 

HAS_CYPHER        := 1
HAS_VARTYPE       := 1
HAS_GUID          := 1
HAS_SCRIPTS       := 1
HAS_SPOOLDIR      := 1
HAS_SYSTEMLOG     := 1
HAS_CHANNEL_UTILS := 1
HAS_TTS           := 1
HAS_ASN           := 1
HAS_NAT           := 1
HAS_STUN          := 1
HAS_STUNSRVR      := 1
HAS_PIPECHAN      := 1
HAS_DTMF          := 1
HAS_VCARD         := 1
HAS_WAVFILE       := 1
HAS_SOCKS         := 1
HAS_FTP           := 1
HAS_SNMP          := 1
HAS_TELNET        := 1
HAS_CLI           := 1
HAS_REMCONN       := 1
HAS_SERIAL        := 1
HAS_POP3SMTP      := 1
HAS_AUDIO         := 1
HAS_VIDEO         := 1
HAS_SHM_VIDEO     := 
HAS_SHM_AUDIO     := 
HAS_PORTAUDIO     := 
HAS_SUN_AUDIO     := 
HAS_VFW_CAPTURE   := 
HAS_GSTREAMER     := 1

HAS_VXML          := 1
HAS_JABBER        := 1
HAS_XMLRPC        := 1
HAS_SOAP          := 1
HAS_URL           := 1
HAS_HTTP          := 1
HAS_HTTPFORMS     := 1
HAS_HTTPSVC       := 1
HAS_SSDP          := 1
HAS_CONFIG_FILE   := 1
HAS_VIDFILE       := 1
HAS_MEDIAFILE     := 1
HAS_ODBC          := 1
HAS_DIRECTSHOW    := 
HAS_DIRECTSOUND   := 
HAS_LUA           := 1
HAS_V8            := 1

HAS_ALSA          := 1
HAS_AUDIOSHM      := @HAS_AUDIOSHM@
HAS_OSS           := 1
HAS_PULSE         := 
HAS_ESD           := 
HAS_SUNAUDIO      := @HAS_SUNAUDIO@
HAS_V4L           := 
HAS_V4L2          := 1
HAS_BSDVIDEOCAP   := 
HAS_AVC1394       := 
HAS_DC1394        := 

ESD_CFLAGS  := 
ESD_LIBS    := 
V4L2_CFLAGS := -DV4L2_HEADER="\"linux/videodev2.h\""
V4L2_LIBS   := 
DC_CFLAGS   := 


# Remember where this make file is, it is the platform specific one and there
# is a corresponding platform specific include file that goes with it
PTLIB_PLATFORM_INC_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../include)


# End of ptlib_config.mak
