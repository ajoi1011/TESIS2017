#
# Configuration Makefile for plug ins
#
# Copyright (C) 2011 Vox Lucida
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
# The Original Code is OPAL.
#
# The Initial Developer of the Original Code is Robert Jongbloed
#
# Contributor(s): ______________________________________.
#

prefix         := /usr/local
exec_prefix    := ${prefix}
libdir         := ${exec_prefix}/lib
target_os      := linux
target         := linux_x86

CC             := gcc
CXX            := g++
LD             := g++
AR             := ar
INSTALL        := /usr/bin/install -c -C
MKDIR_P        := mkdir -p

PLUGIN_SRC_DIR := /home/ajoi1011/Applications/opal/plugins
AUD_PLUGIN_DIR := ${libdir}/opal-3.19.5/codecs/audio
VID_PLUGIN_DIR := ${libdir}/opal-3.19.5/codecs/video
FAX_PLUGIN_DIR := ${libdir}/opal-3.19.5/fax

CPPFLAGS       :=    -DX264_API_IMPORTS -I/usr/include/i386-linux-gnu -I/usr/include/i386-linux-gnu -I/usr/include/i386-linux-gnu -I/usr/include/opus   -Wall -Wno-unknown-pragmas  -I/usr/local/include -I$(PLUGIN_SRC_DIR)/../include -I$(PLUGIN_SRC_DIR) $(CPPFLAGS)
CXXFLAGS       := -Wno-overloaded-virtual -fno-omit-frame-pointer -march=i686  -Wno-deprecated-declarations $(CXXFLAGS)
CFLAGS         := -fno-omit-frame-pointer -march=i686  $(CFLAGS)
LDFLAGS        :=  $(LDFLAGS)
LIBS           :=  $(LIBS)
ARFLAGS        := rc
SHARED_CPPFLAGS = -fPIC
SHARED_LDFLAGS  = -shared -Wl,--build-id,-soname,$(LIB_SONAME)
SHAREDLIBEXT   :=so

CELT_CFLAGS    := 
CELT_LIBS      := 
GSM_CFLAGS     := 
GSM_LIBS       := -lgsm
GSM_SYSTEM     := yes
ILBC_CFLAGS    := 
ILBC_LIBS      := 
ILBC_SYSTEM    := no
OPUS_CFLAGS    := -I/usr/include/opus
OPUS_LIBS      := -lopus
OPUS_SYSTEM    := yes
SPEEX_CFLAGS   := 
SPEEX_LIBS     := -lspeex
SPEEX_SYSTEM   := yes
X264_CFLAGS    := -DX264_API_IMPORTS
X264_LIBS      := -lx264
X264_HELPER    := gpl
THEORA_CFLAGS  := 
THEORA_LIBS    := -ltheora -logg
VP8_CFLAGS     := 
VP8_LIBS       := -lvpx -lm
SPANDSP_CFLAGS := 
SPANDSP_LIBS   := -lspandsp

LIBAVCODEC_CFLAGS        := -I/usr/include/i386-linux-gnu
LIBAVCODEC_LIBS          := -lavcodec -lavutil -lswresample
HAVE_LIBAVCODEC_RTP_MODE := 0

DLFCN_LIBS := -ldl

PLUGIN_SUBDIRS :=  /home/ajoi1011/Applications/opal/plugins/audio/SILK /home/ajoi1011/Applications/opal/plugins/audio/gsm-amr /home/ajoi1011/Applications/opal/plugins/audio/G726 /home/ajoi1011/Applications/opal/plugins/audio/IMA_ADPCM /home/ajoi1011/Applications/opal/plugins/audio/G722 /home/ajoi1011/Applications/opal/plugins/audio/G.722.2 /home/ajoi1011/Applications/opal/plugins/audio/LPC_10 /home/ajoi1011/Applications/opal/plugins/audio/iSAC /home/ajoi1011/Applications/opal/plugins/audio/GSM0610 /home/ajoi1011/Applications/opal/plugins/audio/iLBC /home/ajoi1011/Applications/opal/plugins/audio/Speex /home/ajoi1011/Applications/opal/plugins/audio/Opus /home/ajoi1011/Applications/opal/plugins/video/H.261-vic /home/ajoi1011/Applications/opal/plugins/video/H.263-1998 /home/ajoi1011/Applications/opal/plugins/video/MPEG4-ffmpeg /home/ajoi1011/Applications/opal/plugins/video/x264 /home/ajoi1011/Applications/opal/plugins/video/VP8-WebM /home/ajoi1011/Applications/opal/plugins/video/THEORA /home/ajoi1011/Applications/opal/plugins/fax/fax_spandsp
               
               
# End of file
