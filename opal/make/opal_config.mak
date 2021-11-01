#
# opal_config.mak
#
# Make symbols include file for Open Phone Abstraction library
#
# Copyright (c) 2001 Equivalence Pty. Ltd.
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
# The Original Code is Open Phone Abstraction library.
#
# The Initial Developer of the Original Code is Equivalence Pty. Ltd.
#
# Contributor(s): ______________________________________.
#

OPAL_MAJOR :=3
OPAL_MINOR :=19
OPAL_STAGE :=-alpha
OPAL_PATCH :=5
OPAL_OEM   :=0

# detected platform
target_cpu       := x86
target_os        := linux
target           := linux_x86

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

OPAL_PLUGIN_DIR  := ${libdir}/opal-3.19.5

# Tool names detected by configure
CC               := gcc
CXX              := g++
LD               := g++
AR               := ar
RANLIB           := ranlib
LN_S             := ln -s
MKDIR_P          := mkdir -p
INSTALL          := /usr/bin/install -c -C
SVN              := /usr/bin/svn
SWIG             := 

# Compile/tool flags
CPPFLAGS         :=  -Wall -Wno-unknown-pragmas  -I${includedir}/opal $(CPPFLAGS)
CXXFLAGS         := -Wno-overloaded-virtual -fno-omit-frame-pointer -march=i686  -Wno-deprecated-declarations $(CXXFLAGS)
CFLAGS           := -fno-omit-frame-pointer -march=i686  $(CFLAGS)
LDFLAGS          :=  $(LDFLAGS)
LIBS             := -lspeexdsp   $(LIBS)
SHARED_CPPFLAGS  := -fPIC
SHARED_LDFLAGS    = -shared -Wl,--build-id,-soname,$(LIB_SONAME)
DEBUG_CPPFLAGS   := -D_DEBUG 
DEBUG_CFLAGS     := -gdwarf-4 
OPT_CPPFLAGS     := -DNDEBUG 
OPT_CFLAGS       := -O3 
ARFLAGS          := rc

SHAREDLIBEXT     := so
STATICLIBEXT     := a

# Configuration options
OPAL_PLUGINS     := yes
OPAL_SAMPLES     := no

OPAL_H323        := yes
OPAL_SDP         := yes
OPAL_SIP         := yes
OPAL_IAX2        := no
OPAL_SKINNY      := no
OPAL_VIDEO       := yes
OPAL_ZRTP        := no
OPAL_LID         := yes
OPAL_CAPI        := no
OPAL_DAHDI       := no
OPAL_IVR         := yes
OPAL_HAS_H224    := yes
OPAL_HAS_H281    := yes
OPAL_H235_6      := yes
OPAL_H235_8      := no
OPAL_H450        := no
OPAL_H460        := no
OPAL_H501        := no
OPAL_T120DATA    := no
OPAL_SRTP        := yes
SRTP_SYSTEM      := no
OPAL_RFC4175     := yes
OPAL_RFC2435     := no
OPAL_AEC         := yes
OPAL_G711PLC     := yes
OPAL_T38_CAP     := yes
OPAL_FAX         := yes
OPAL_JAVA        := no
SPEEXDSP_SYSTEM  := yes
OPAL_HAS_PRESENCE:= yes
OPAL_HAS_MSRP    := yes
OPAL_HAS_SIPIM   := yes
OPAL_HAS_RFC4103 := yes
OPAL_HAS_MIXER   := yes
OPAL_HAS_PCSS    := yes

# PTLib interlocks
OPAL_PTLIB_SSL          := no
OPAL_PTLIB_SSL_AES      := no
OPAL_PTLIB_ASN          := yes
OPAL_PTLIB_EXPAT        := yes
OPAL_PTLIB_AUDIO        := yes
OPAL_PTLIB_VIDEO        := yes
OPAL_PTLIB_WAVFILE      := yes
OPAL_PTLIB_DTMF         := yes
OPAL_PTLIB_IPV6         := yes
OPAL_PTLIB_DNS_RESOLVER := yes
OPAL_PTLIB_LDAP         := yes
OPAL_PTLIB_VXML         := yes
OPAL_PTLIB_CONFIG_FILE  := yes
OPAL_PTLIB_HTTPSVC      := yes
OPAL_PTLIB_STUN         := yes
OPAL_PTLIB_CLI          := yes
OPAL_GSTREAMER          := yes

PTLIB_MAKE_DIR   := /usr/local/share/ptlib/make
PTLIB_LIB_DIR    := /usr/local/lib


# Remember where this make file is, it is the platform specific one and there
# is a corresponding platform specific include file that goes with it
OPAL_PLATFORM_INC_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../include)


# End of file
