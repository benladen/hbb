#pragma once

#ifndef __arm__
	#ifndef __vita__
        #error __arm__ or __vita__ is not defined.
	#endif
#endif

#ifdef __STRICT_ANSI__
	#define inline 
#endif

/*define PSP2_DEBUG_ALL*/
#ifdef PSP2_DEBUG_ALL
	#define PSP2_DEBUG 5
	#define PSP2_DEBUG_GRAPHICS
	#define PSP2_DEBUG_AUDIO
	#define PSP2_DEBUG_NETWORK
#endif

#define hasDebugMessage

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <psp2/types.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/power.h>
#include <psp2/common_dialog.h>
#include <psp2/ime_dialog.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/moduleinfo.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/apputil.h>
#include <psp2/appmgr.h>
#include "sysmodule_internal.h"
#include "promoterutil.h"
#include "sha1.h"
#include "network.h"
#include "graphics.h"
#include "srvnet.h"
#include "dbgtext.h"
#include "event.h"

extern struct dbgText *dbgScrn;

void initDebugConnection(void);
void debugMessage(char*);
void debugPrintInt(int);
void debugPrintHex(char*, size_t);
int sleep(unsigned int);
float posAdj(SceInt16, SceInt16, SceInt16, SceInt32);
void deleteDirectoryTreeFiles(char*, int);
void deleteDirectoryTreeFolders(char*);
