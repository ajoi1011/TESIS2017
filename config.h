/*
 * config.h
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

#ifndef _OPAL_SERVER_CONFIG_H
#define _OPAL_SERVER_CONFIG_H
#include "precompile.h"
const WORD DefaultHTTPPort = 1719;

// don't forget to add the same into quote.txt:
#define OTFC_UNMUTE                    0
#define OTFC_MUTE                      1
#define OTFC_MUTE_ALL                  2
#define OTFC_REMOVE_FROM_VIDEOMIXERS   3
#define OTFC_REFRESH_VIDEO_MIXERS      4
#define OTFC_ADD_AND_INVITE            5
#define OTFC_DROP_MEMBER               7
#define OTFC_VAD_NORMAL                8
#define OTFC_VAD_CHOSEN_VAN            9
#define OTFC_VAD_DISABLE_VAD          10
#define OTFC_REMOVE_VMP               11
#define OTFC_MOVE_VMP                 12
#define OTFC_SET_VMP_STATIC           13
#define OTFC_VAD_CLICK                14
#define OTFC_MIXER_ARRANGE_VMP        15
#define OTFC_MIXER_SCROLL_LEFT        16
#define OTFC_MIXER_SHUFFLE_VMP        17
#define OTFC_MIXER_SCROLL_RIGHT       18
#define OTFC_MIXER_CLEAR              19
#define OTFC_MIXER_REVERT             20
#define OTFC_GLOBAL_MUTE              21
#define OTFC_SET_VAD_VALUES           22
#define OTFC_TEMPLATE_RECALL          23
#define OTFC_SAVE_TEMPLATE            24
#define OTFC_DELETE_TEMPLATE          25
#define OTFC_INVITE                   32
#define OTFC_REMOVE_OFFLINE_MEMBER    33
#define OTFC_DROP_ALL_ACTIVE_MEMBERS  64
#define OTFC_INVITE_ALL_INACT_MMBRS   65
#define OTFC_REMOVE_ALL_INACT_MMBRS   66
#define OTFC_SAVE_MEMBERS_CONF        67
#define OTFC_YUV_FILTER_MODE          68
#define OTFC_TAKE_CONTROL             69
#define OTFC_DECONTROL                70
#define OTFC_ADD_VIDEO_MIXER          71
#define OTFC_DELETE_VIDEO_MIXER       72
#define OTFC_SET_VIDEO_MIXER_LAYOUT   73
#define OTFC_SET_MEMBER_VIDEO_MIXER   74
#define OTFC_VIDEO_RECORDER_START     75
#define OTFC_VIDEO_RECORDER_STOP      76
#define OTFC_TOGGLE_TPL_LOCK          77
#define OTFC_UNMUTE_ALL               78
#define OTFC_AUDIO_GAIN_LEVEL_SET     79
#define OTFC_OUTPUT_GAIN_SET          80
///////////////////////////////////////////////

// enable/disable libjpeg (live video frames in Room Control Page)
#define USE_LIBJPEG	0

#define MAX_SUBFRAMES        100
#define FRAMESTORE_TIMEOUT 60 /* s */

#define CIF_WIDTH     352
#define CIF_HEIGHT    288
#define CIF_SIZE      (CIF_WIDTH*CIF_HEIGHT*3/2)

#define QCIF_WIDTH    (CIF_WIDTH / 2)
#define QCIF_HEIGHT   (CIF_HEIGHT / 2)
#define QCIF_SIZE     (QCIF_WIDTH*QCIF_HEIGHT*3/2)

#define SQCIF_WIDTH    (QCIF_WIDTH / 2)
#define SQCIF_HEIGHT   (QCIF_HEIGHT / 2)
#define SQCIF_SIZE     (SQCIF_WIDTH*SQCIF_HEIGHT*3/2)

#define CIF4_WIDTH     (CIF_WIDTH * 2)
#define CIF4_HEIGHT    (CIF_HEIGHT * 2)
#define CIF4_SIZE      (CIF4_WIDTH*CIF4_HEIGHT*3/2)

#define CIF16_WIDTH     (CIF4_WIDTH * 2)
#define CIF16_HEIGHT    (CIF4_HEIGHT * 2)
#define CIF16_SIZE      (CIF16_WIDTH*CIF16_HEIGHT*3/2)

#define SQ3CIF_WIDTH    116
#define SQ3CIF_HEIGHT   96
#define SQ3CIF_SIZE     (SQ3CIF_WIDTH*SQ3CIF_HEIGHT*3/2)

#define Q3CIF_WIDTH    (2*SQ3CIF_WIDTH)
#define Q3CIF_HEIGHT   (2*SQ3CIF_HEIGHT)
#define Q3CIF_SIZE     (Q3CIF_WIDTH*Q3CIF_HEIGHT*3/2)

#define Q3CIF4_WIDTH    (4*SQ3CIF_WIDTH)
#define Q3CIF4_HEIGHT   (4*SQ3CIF_HEIGHT)
#define Q3CIF4_SIZE     (Q3CIF4_WIDTH*Q3CIF4_HEIGHT*3/2)

#define Q3CIF16_WIDTH    (8*SQ3CIF_WIDTH)
#define Q3CIF16_HEIGHT   (8*SQ3CIF_HEIGHT)
#define Q3CIF16_SIZE     (Q3CIF16_WIDTH*Q3CIF16_HEIGHT*3/2)

#define SQ5CIF_WIDTH    140
#define SQ5CIF_HEIGHT   112
#define SQ5CIF_SIZE     (SQ5CIF_WIDTH*SQ5CIF_HEIGHT*3/2)

#define Q5CIF_WIDTH    (2*SQ5CIF_WIDTH)
#define Q5CIF_HEIGHT   (2*SQ5CIF_HEIGHT)
#define Q5CIF_SIZE     (Q5CIF_WIDTH*Q5CIF_HEIGHT*3/2)

#define TCIF_WIDTH    (CIF_WIDTH*3)
#define TCIF_HEIGHT   (CIF_HEIGHT*3)
#define TCIF_SIZE     (TCIF_WIDTH*TCIF_HEIGHT*3/2)

#define TQCIF_WIDTH    (CIF_WIDTH*3 / 2)
#define TQCIF_HEIGHT   (CIF_HEIGHT*3 / 2)
#define TQCIF_SIZE     (TQCIF_WIDTH*TQCIF_HEIGHT*3/2)

#define TSQCIF_WIDTH    (CIF_WIDTH*3 / 4)
#define TSQCIF_HEIGHT   (CIF_HEIGHT*3 / 4)
#define TSQCIF_SIZE     (TSQCIF_WIDTH*TSQCIF_HEIGHT*3/2)

#define _IMGST 1
#define _IMGST1 2
#define _IMGST2 4

/// Video Mixer Configurator - Begin ///
#define VMPC_CONFIGURATION_NAME                 "layouts.conf"
#define VMPC_DEFAULT_ID                         "undefined"
#define VMPC_DEFAULT_FW                         704
#define VMPC_DEFAULT_FH                         576
#define VMPC_DEFAULT_POSX                       0
#define VMPC_DEFAULT_POSY                       0
#define VMPC_DEFAULT_WIDTH                      VMPC_DEFAULT_FW/2
#define VMPC_DEFAULT_HEIGHT                     VMPC_DEFAULT_FH/2
#define VMPC_DEFAULT_MODE_MASK                  0
#define VMPC_DEFAULT_BORDER                     1
#define VMPC_DEFAULT_VIDNUM                     0
#define VMPC_DEFALUT_SCALE_MODE                 1
#define VMPC_DEFAULT_REALLOCATE_ON_DISCONNECT   1
#define VMPC_DEFAULT_NEW_FROM_BEGIN             1
#define VMPC_DEFAULT_MOCKUP_WIDTH               388
#define VMPC_DEFAULT_MOCKUP_HEIGHT              218

/// Video Mixer Configurator - End ///

#  define PATH_SEPARATOR   "/"
#  define SYS_CONFIG_DIR   "/home/ajoi1011/proyectos/testing/tesis2.0/conf"
#  define SYS_RESOURCE_DIR "/home/ajoi1011/proyectos/testing/tesis2.0/resource"
#  define SERVER_LOGS      "/home/ajoi1011/proyectos/testing/tesis2.0/log"

#endif
