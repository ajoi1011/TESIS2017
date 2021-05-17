/*
 * custom.cxx
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

#ifdef RC_INVOKED
#include <winver.h>
#else
#include "custom.h"
#endif

#include "version.h"

////////////////////////////////////////////////////
//
// Variables requeridas por PHTTPServiceProcess
//
////////////////////////////////////////////////////

#ifndef PRODUCT_NAME_TEXT
#define	PRODUCT_NAME_TEXT	"OpalMCU-EIE"
#endif

#ifndef EXE_NAME_TEXT
#define	EXE_NAME_TEXT	    "Servidor"
#endif

#ifndef MANUFACTURER_TEXT
#define	MANUFACTURER_TEXT	"Angel Oramas"
#endif

#ifndef COPYRIGHT_HOLDER
#define	COPYRIGHT_HOLDER	""
#endif

#ifndef GIF_NAME
#define GIF_NAME  		    NULL
#define	GIF_WIDTH  100
#define GIF_HEIGHT 100
#endif

#ifndef EMAIL
#define	EMAIL               "angeloramas87@gmail.com"
#endif

#ifndef HOME_PAGE
#define	HOME_PAGE NULL
#endif

#ifndef PRODUCT_NAME_HTML
#define	PRODUCT_NAME_HTML   NULL
#endif

#ifdef RC_INVOKED

#define AlphaCode alpha
#define BetaCode  beta
#define ReleaseCode pl

#define MkStr2(s) #s
#define MkStr(s)  MkStr2(s)

#if BUILD_NUMBER==0
#define VERSION_STRING \
    MkStr(MAJOR_VERSION) "." MkStr(MINOR_VERSION)
#else
#define VERSION_STRING \
    MkStr(MAJOR_VERSION) "." MkStr(MINOR_VERSION) MkStr(BUILD_TYPE) MkStr(BUILD_NUMBER)
#endif

VS_VERSION_INFO VERSIONINFO
#define alpha 1
#define beta 2
#define pl 3
  FILEVERSION     MAJOR_VERSION,MINOR_VERSION,BUILD_TYPE,BUILD_NUMBER
  PRODUCTVERSION  MAJOR_VERSION,MINOR_VERSION,BUILD_TYPE,BUILD_NUMBER
#undef alpha
#undef beta
#undef pl
  FILEFLAGSMASK   VS_FFI_FILEFLAGSMASK
#ifdef _DEBUG
  FILEFLAGS       VS_FF_DEBUG
#else
  FILEFLAGS       0
#endif
  FILEOS          VOS_NT_WINDOWS32
  FILETYPE        VFT_APP
  FILESUBTYPE     VFT2_UNKNOWN
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "0c0904b0"
        BEGIN
            VALUE "CompanyName",      MANUFACTURER_TEXT "\0"
            VALUE "FileDescription",  PRODUCT_NAME_TEXT "\0"
            VALUE "FileVersion",      VERSION_STRING "\0"
            VALUE "InternalName",     EXE_NAME_TEXT "\0"
            VALUE "LegalCopyright",   "Copyright © " COPYRIGHT_HOLDER " 2003\0"
            VALUE "OriginalFilename", EXE_NAME_TEXT ".exe\0"
            VALUE "ProductName",      PRODUCT_NAME_TEXT "\0"
            VALUE "ProductVersion",   VERSION_STRING "\0"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
            VALUE "Translation", 0xc09, 1200
    END
END

#else

PHTTPServiceProcess::Info ProductInfo = {
    PRODUCT_NAME_TEXT,
    MANUFACTURER_TEXT,
    MAJOR_VERSION, MINOR_VERSION, PProcess::BUILD_TYPE, BUILD_NUMBER, __TIME__ __DATE__,

    {{ 0 }}, { NULL }, 0, {{ 0 }},  // Only relevent for commercial apps

    HOME_PAGE,
    EMAIL,
    PRODUCT_NAME_HTML,
    NULL,  // GIF HTML, use calculated from below
    GIF_NAME,
    GIF_WIDTH,
    GIF_HEIGHT
};

#endif

// End of File ///////////////////////////////////////////////////////////////
