#pragma once

#include "main.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
/*include <psp2/fpu.h> */
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/types.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/sysmem.h>

#ifndef M_PI
	#define M_PI 3.14159265358979323846264338327950288f
#endif

#ifndef PSP2_DISPLAY_WIDTH
    #define PSP2_DISPLAY_WIDTH 960
#endif
#ifndef PSP2_DISPLAY_HEIGHT
    #define PSP2_DISPLAY_HEIGHT 544
#endif
#ifndef PSP2_DISPLAY_STRIDE_IN_PIXELS
    #define PSP2_DISPLAY_STRIDE_IN_PIXELS 1024
#endif
#ifndef PSP2_DISPLAY_COLOR_FORMAT
    #define PSP2_DISPLAY_COLOR_FORMAT SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR
#endif
#ifndef PSP2_DISPLAY_PIXEL_FORMAT
    #define PSP2_DISPLAY_PIXEL_FORMAT SCE_DISPLAY_PIXELFORMAT_A8B8G8R8
#endif
#ifndef PSP2_DISPLAY_BUFFER_COUNT
    #define PSP2_DISPLAY_BUFFER_COUNT 2
#endif
#ifndef PSP2_DISPLAY_MAX_PENDING_SWAPS
    #define PSP2_DISPLAY_MAX_PENDING_SWAPS 1
#endif
#ifndef PSP2_MSAA_MODE
    #define PSP2_MSAA_MODE SCE_GXM_MULTISAMPLE_NONE
#endif

struct graphicsTexture {
	char flags; /* Bits: 0=Set when drawn */
	float x;
	float y;
	float rot;
	float center_x;
	float center_y;
	float x_scale;
	float y_scale;
	char ignoreCenterXY;
	SceGxmTexture gxm_tex;
	SceUID data_UID;
	SceUID palette_UID;
};

int textureFormatSize(SceGxmTextureFormat);

int graphicsInit(unsigned int);
int graphicsEnd(void);
void graphicsClearScreen(void);
void graphicsClearScreenColor(float a, float b, float c, float d);
void graphicsSwapBuffers(void);
void graphicsStartDrawing(void);
void graphicsEndDrawing(void);
void graphicsWaitFinish(void);

struct graphicsTexture* graphicsCreateTexture(unsigned int, unsigned int);
struct graphicsTexture* graphicsCreateTextureFormat(unsigned int, unsigned int, SceGxmTextureFormat);
struct graphicsTexture* graphicsLoadRawImageFile(char*, unsigned int, unsigned int);
void graphicsFreeTexture(struct graphicsTexture*);
unsigned int graphicsTextureGetStride(struct graphicsTexture*);
void* graphicsTextureGetData(struct graphicsTexture*);
void graphicsDrawTexture(struct graphicsTexture*);
void graphicsDrawTexturePart(struct graphicsTexture*, float, float, float, float);

void graphicsDrawPixel(float, float, unsigned int);
void graphicsDrawLine(float, float, float, float, unsigned int);
void graphicsDrawRectangle(float, float, float, float, unsigned int);

void graphicsDrawDialog(void);
