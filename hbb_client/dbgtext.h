#pragma once

#include "main.h"

#define DBGTXT_C0 0xFF000000 /* Black */
#define DBGTXT_C1 0xFF800000 /* Blue */
#define DBGTXT_C2 0xFF008000 /* Green */
#define DBGTXT_C3 0xFF808000 /* Aqua */
#define DBGTXT_C4 0xFF000080 /* Red */
#define DBGTXT_C5 0xFF800080 /* Purple */
#define DBGTXT_C6 0xFF008080 /* Yellow */
#define DBGTXT_C7 0xFFC0C0C0 /* White */
#define DBGTXT_C8 0xFF808080 /* Gray */
#define DBGTXT_C9 0xFFFF0000 /* Light Blue */
#define DBGTXT_CA 0xFF00FF00 /* Light Green */
#define DBGTXT_CB 0xFFFFFF00 /* Light Aqua */
#define DBGTXT_CC 0xFF0000FF /* Light Red */
#define DBGTXT_CD 0xFFFF00FF /* Light Purple */
#define DBGTXT_CE 0xFF00FFFF /* Light Yellow */
#define DBGTXT_CF 0xFFFFFFFF /* Bright White */
#define DBGTXT_C_CLEAR 0x00000000

#define DBGTXT_SMALL 0
#define DBGTXT_LARGE 1
#define DBGTXT_TINY  2

/* Usage:
struct graphicsTexture *dbgScrnTex;
void *dbgScreenData;
struct dbgText *dbgScrn;

dbgScrnTex = graphicsCreateTexture(PSP2_DISPLAY_WIDTH, PSP2_DISPLAY_HEIGHT);
dbgScreenData = graphicsTextureGetData(dbgScrnTex);
memset(dbgScreenData, 0x00, PSP2_DISPLAY_WIDTH*PSP2_DISPLAY_HEIGHT*4);
dbgScrnTex->x = PSP2_DISPLAY_WIDTH/2;
dbgScrnTex->y = PSP2_DISPLAY_HEIGHT/2;

dbgScrn = debugTextInit(dbgScreenData);
debugTextPrint(dbgScrn, "Hello!", 10, 50, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_LARGE);
graphicsDrawTexture(dbgScrnTex);
debugTextEnd(dbgScrn);
*/

struct dbgText {
	char *fontData0; /* DBGTXT_SMALL */
	char *fontData1; /* DBGTXT_LARGE */
	char *fontData2; /* DBGTXT_TINY */
	void *screen;
};

struct dbgText* debugTextInit(void *buffer);
int debugTextEnd(struct dbgText *dt);
int debugTextClear(struct dbgText *dt);
int debugTextPrint(struct dbgText *dt, char *text, int scrn_x, int scrn_y, int color, int bg_color, char font);
int debugTextPrintInt(struct dbgText *dt, int i, int x, int y, int color, int bg_color, char font);
int debugTextPrintFloat(struct dbgText *dt, int f, int x, int y, int color, int bg_color, char font);