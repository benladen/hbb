#include "graphics.h"

int sceGxmGetRenderTargetMemSize(const SceGxmRenderTargetParams *params, unsigned int *hostMemSize, unsigned int *driverMemSize);

/* Structs */
struct _displayData {
	void *address;
};

struct _clearVertex {
	float x;
	float y;
};

struct _colorVertex {
	float x;
	float y;
	float z;
	unsigned int color;
};

struct _textureVertex {
	float x;
	float y;
	float z;
	float u;
	float v;
};

/* Defines */
#define PSP2_MAX_TEX_SIZE 4096

/* Extern shaders */
extern const SceGxmProgram clear_v_gxp_start;
extern const SceGxmProgram clear_f_gxp_start;
extern const SceGxmProgram color_v_gxp_start;
extern const SceGxmProgram color_f_gxp_start;
extern const SceGxmProgram texture_v_gxp_start;
extern const SceGxmProgram texture_f_gxp_start;

/* Static functions */
static void _graphicsDebugMessage(char*);
static void _graphicsDebugMessageWithInt(char*, int);
static float graphicsCosf(float);
static float graphicsSinf(float);
static float graphicsFpuSinfCosf(float, int);
static void* patcherHostAlloc(void*, unsigned int);
static void patcherHostFree(void*, void*);
static void displayCallback(const void*);
static void* gpuAlloc(SceKernelMemBlockType, unsigned int, unsigned int, unsigned int, SceUID*);
static void gpuFree(SceUID);
static void* vertexUsseAlloc(unsigned int, SceUID*, unsigned int*);
static void vertexUsseFree(SceUID);
static void* fragmentUsseAlloc(unsigned int, SceUID*, unsigned int*);
static void fragmentUsseFree(SceUID);
/*static void* poolMalloc(unsigned int);*/
static void* poolMemalign(unsigned int, unsigned int);
static int initGxm(void);
static int initCreateContext(void);
static int initCreateRenderTarget(void);
static int initAllocateDisplayBuffers(void);
static int initAllocateDepthBuffer(void);
static int initCreateShaderPatcherAndRegister(void);
static int initCreateProgramsAndDataForClear(void);
static int initCreateProgramsAndDataForDisplay(void);

/* Static variables */
static int graphicsInitialized = 0;
static float orthoMatrix[4*4];
static SceGxmContext *gxmContext = NULL;
static SceGxmContextParams contextParams;
static SceGxmRenderTarget *renderTarget = NULL;
static SceUID displayBufferUid[PSP2_DISPLAY_BUFFER_COUNT];
static void *displayBufferData[PSP2_DISPLAY_BUFFER_COUNT];
static SceGxmColorSurface displaySurface[PSP2_DISPLAY_BUFFER_COUNT];
static SceGxmSyncObject *displayBufferSync[PSP2_DISPLAY_BUFFER_COUNT];
static SceUID depthBufferUid;
static void *depthBufferData;
static SceGxmDepthStencilSurface depthSurface;
static unsigned int backBufferIndex = 0;
static unsigned int frontBufferIndex = 0;
static void *poolAddr = NULL;
static SceUID poolUid;
static unsigned int poolIndex = 0;
static unsigned int poolSize = 0;
static SceULong64 frameNum = 0;

static const SceGxmProgram *const clearVertexProgramGxp = &clear_v_gxp_start;
static const SceGxmProgram *const clearFragmentProgramGxp = &clear_f_gxp_start;
static const SceGxmProgram *const colorVertexProgramGxp = &color_v_gxp_start;
static const SceGxmProgram *const colorFragmentProgramGxp = &color_f_gxp_start;
static const SceGxmProgram *const textureVertexProgramGxp = &texture_v_gxp_start;
static const SceGxmProgram *const textureFragmentProgramGxp = &texture_f_gxp_start;

static SceUID vdmRingBufferUid;
static SceUID vertexRingBufferUid;
static SceUID fragmentRingBufferUid;
static SceUID fragmentUsseRingBufferUid;
static SceUID patcherBufferUid;
static SceUID patcherVertexUsseUid;
static SceUID patcherFragmentUsseUid;
static SceUID clearVerticesUid;
static SceUID clearIndicesUid;
static struct _clearVertex *clearVertices = NULL;
static uint16_t *clearIndices = NULL;

static SceGxmShaderPatcher *shaderPatcher = NULL;
static SceGxmVertexProgram *clearVertexProgram = NULL;
static SceGxmFragmentProgram *clearFragmentProgram = NULL;
static SceGxmShaderPatcherId clearVertexProgramId;
static SceGxmShaderPatcherId clearFragmentProgramId;
static SceGxmShaderPatcherId colorVertexProgramId;
static SceGxmShaderPatcherId colorFragmentProgramId;
static SceGxmShaderPatcherId textureVertexProgramId;
static SceGxmShaderPatcherId textureFragmentProgramId;
static SceGxmVertexProgram *gxmColorVertexProgram = NULL;
static SceGxmFragmentProgram *gxmColorFragmentProgram = NULL;
static SceGxmVertexProgram *gxmTextureVertexProgram = NULL;
static SceGxmFragmentProgram *gxmTextureFragmentProgram = NULL;
static const SceGxmProgramParameter *gxmClearClearColorParam = NULL;
static const SceGxmProgramParameter *gxmColorWvpParam = NULL;
static const SceGxmProgramParameter *gxmTextureWvpParam = NULL;

static void _graphicsDebugMessage(char *text) {
	#ifdef PSP2_DEBUG_GRAPHICS
		#ifdef hasDebugMessage
			debugMessage(text);
		#endif
	#else
		(void)text;
	#endif
}

static void _graphicsDebugMessageWithInt(char *text, int i) {
	#ifdef PSP2_DEBUG_GRAPHICS
		#ifdef hasDebugMessage
			debugMessage(text);
			debugPrintInt(i);
		#endif
	#else
		(void)text;
		(void)i;
	#endif
}

static float graphicsCosf(float f) {
	#ifdef _PSP2_FPU_H_
		return sceFpuCosf(f);
	#endif
	#ifndef __STRICT_ANSI__
		return cosf(f);
	#else
		return graphicsFpuSinfCosf(f, 1);
	#endif
}

static float graphicsSinf(float f) {
	#ifdef _PSP2_FPU_H_
		return sceFpuSinf(f);
	#endif
	#ifndef __STRICT_ANSI__
		return sinf(f);
	#else
		return graphicsFpuSinfCosf(f, 0);
	#endif
}

static float graphicsFpuSinfCosf(float input, int cf) {
	/* d0-d15 (VFP double)
	   s0-s31 (VFP single)
	   r0-r12 (ARM register)  
	*/
	float res;
	int cosf = cf;
	if (cf < 0 || cf > 1) {
		return 0.0;
	}
	__asm__ __volatile__ (
	"vmov.f32 s0, %1"       "\n\t" /* input                */
	"vmov.f32 s4, %3"       "\n\t" /* 1.5707963267948966f  */
	"vmov.f32 s5, %4"       "\n\t" /* 3.14159265f          */
	"vmov.f32 s6, %5"       "\n\t" /* -3.14159265f         */
	"vmov.f32 s7, %6"       "\n\t" /* 6.28318531f          */
	"vmov.f32 s8, %7"       "\n\t" /* 1.27323954f          */
	"vmov.f32 s9, %8"       "\n\t" /* -0.405284735f        */
	"vmov.f32 s10, %10"     "\n\t" /* 0.225f               */
	"vmov.f32 s1, %9"       "\n\t" /* a = 0.0f             */
	"vmov.f32 s2, %9"       "\n\t" /* b = 0.0f             */

	"cmp %2, #1"            "\n\t" /* if cosf == 1         */
	"it eq"                 "\n\t"
	"vaddeq.f32 s0, s4"     "\n\t" /* f = f + 1.5707963... */
	
	"1:" "\n\t"
	"vcmp.f32 s0, s6"       "\n\t" /* if f < -3.14159265   */
	"vmrs APSR_nzcv, FPSCR" "\n\t"
	"itt lt"                "\n\t"
	"vaddlt.f32 s0, s0, s7" "\n\t" /* f = f + 6.28318531   */
	"blt 1b"                "\n\t"

	"2:"                    "\n\t"
	"vcmp.f32 s0, s5"       "\n\t" /* if f > 3.14159265    */
	"vmrs APSR_nzcv, FPSCR" "\n\t"
	"itt gt"                "\n\t"
	"vsubgt.f32 s0, s0, s7" "\n\t" /* f = f - 6.28318531   */
	"bgt 2b"                "\n\t"
	
	"vmul.f32 s1, s9, s0"   "\n\t" /* a = -0.405284735 * f */
	"vabs.f32 s2, s0"       "\n\t" /* b = abs(f)           */
	"vmul.f32 s1, s1, s2"   "\n\t" /* a = a * b            */
	"vmul.f32 s0, s8, s0"   "\n\t" /* f = 1.27323954 * f   */
	"vadd.f32 s0, s0, s1"   "\n\t" /* f = f + a            */ 
	
	"vabs.f32 s1, s0"       "\n\t" /* a = abs(f)           */
	"vmul.f32 s1, s0, s1"   "\n\t" /* a = f * a            */
	"vsub.f32 s1, s1, s0"   "\n\t" /* a = a - f            */
	"vmul.f32 s1, s10, s1"  "\n\t" /* a = 0.225 * a        */
	"vadd.f32 s0, s1, s0"   "\n\t" /* f = a + f            */
	
	"vmov.f32 %0, s0" "\n\t"
	: "=r" (res)
	: "r" (input), "r" (cosf), "r" (1.5707963267948966f), 
	"r" (3.14159265f), "r" (-3.14159265f), "r" (6.28318531f), 
	"r" (1.27323954f), "r" (-0.405284735f),  "r" (0.0f), "r" (0.225f));
	return res;
}

static void* patcherHostAlloc(void *userData, unsigned int size) {
	(void)userData;
	return malloc(size);
}

static void patcherHostFree(void *userData, void *mem) {
	(void)userData;
	if (mem != NULL) {
		free(mem);
	}
}

static void displayCallback(const void *callback_data) {
	int rtn;
	SceDisplayFrameBuf framebuf;
	const struct _displayData *display_data = (const struct _displayData*)callback_data;

	memset(&framebuf, 0x00, sizeof(SceDisplayFrameBuf));
	framebuf.size = sizeof(SceDisplayFrameBuf);
	framebuf.base = display_data->address;
	framebuf.pitch = PSP2_DISPLAY_STRIDE_IN_PIXELS;
	framebuf.pixelformat = PSP2_DISPLAY_PIXEL_FORMAT;
	framebuf.width = PSP2_DISPLAY_WIDTH;
	framebuf.height = PSP2_DISPLAY_HEIGHT;
	rtn = sceDisplaySetFrameBuf(&framebuf, SCE_DISPLAY_SETBUF_NEXTFRAME);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("displayCallback: sceDisplaySetFrameBuf error:", rtn);
	}
	
	sceDisplayWaitVblankStart();
}

static void* gpuAlloc(SceKernelMemBlockType type, unsigned int size, unsigned int alignment, unsigned int attribs, SceUID *uid) {
	void *mem;
	int rtn;
	(void)alignment;
	if (type == SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW) {
		if (alignment > (256*1024)) {
			_graphicsDebugMessageWithInt("gpuAlloc: Bad alignment value:", alignment);
		}
		size = (((size) + ((256*1024) - 1)) & ~((256*1024) - 1));
	}
	else {
		if (alignment > (4*1024)) {
			_graphicsDebugMessageWithInt("gpuAlloc: Bad alignment value:", alignment);
		}
		size = (((size) + ((4*1024) - 1)) & ~((4*1024) - 1));
	}

	*uid = sceKernelAllocMemBlock("gpu_mem", type, size, NULL);
	if (*uid < 0) {
		_graphicsDebugMessageWithInt("gpuAlloc: sceKernelAllocMemBlock error:", *uid);
		return NULL;
	}
	rtn = sceKernelGetMemBlockBase(*uid, &mem);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("gpuAlloc: sceKernelGetMemBlockBase error:", rtn);
		return NULL;
	}
	rtn = sceGxmMapMemory(mem, size, attribs);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("gpuAlloc: sceGxmMapMemory error:", rtn);
		return NULL;
	}
	return mem;
}

static void gpuFree(SceUID uid) {
	int rtn;
	void *mem = NULL;
	/* sceGxmFinish(gxmContext); */
	rtn = sceKernelGetMemBlockBase(uid, &mem);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("gpuFree: sceKernelGetMemBlockBase error:", rtn);
		return;
	}
	rtn = sceGxmUnmapMemory(mem);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("gpuFree: sceGxmUnmapMemory error:", rtn);
		return;
	}
	rtn = sceKernelFreeMemBlock(uid);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("gpuFree: sceKernelFreeMemBlock error:", rtn);
	}
}

static void* vertexUsseAlloc(unsigned int size, SceUID *uid, unsigned int *usse_offset) {
	int rtn;
	void *mem = NULL;
	size = (((size) + ((4096) - 1)) & ~((4096) - 1));
	*uid = sceKernelAllocMemBlock("vertex_usse", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size, NULL);
	if (*uid < 0) {
		_graphicsDebugMessageWithInt("vertexUsseAlloc: sceKernelAllocMemBlock error:", *uid);
		return NULL;
	}
	rtn = sceKernelGetMemBlockBase(*uid, &mem);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("vertexUsseAlloc: sceKernelGetMemBlockBase error:", rtn);
		return NULL;
	}
	if (sceGxmMapVertexUsseMemory(mem, size, usse_offset) < 0) {
		return NULL;
	}
	return mem;
}

static void vertexUsseFree(SceUID uid) {
	int rtn;
	void *mem = NULL;
	rtn = sceKernelGetMemBlockBase(uid, &mem);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("vertexUsseFree: sceKernelGetMemBlockBase error:", rtn);
		return;
	}
	sceGxmUnmapVertexUsseMemory(mem);
	rtn = sceKernelFreeMemBlock(uid);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("vertexUsseFree: sceKernelFreeMemBlock error:", rtn);
		return;
	}
}

static void* fragmentUsseAlloc(unsigned int size, SceUID *uid, unsigned int *usse_offset) {
	int rtn;
	void *mem = NULL;
	size = (((size) + ((4096) - 1)) & ~((4096) - 1));
	*uid = sceKernelAllocMemBlock("fragment_usse", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size, NULL);
	if (*uid < 0) {
		_graphicsDebugMessageWithInt("fragmentUsseAlloc: sceKernelAllocMemBlock error:", *uid);
		return NULL;
	}
	rtn = sceKernelGetMemBlockBase(*uid, &mem);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("fragmentUsseAlloc: sceKernelGetMemBlockBase error:", rtn);
		return NULL;
	}
	rtn = sceGxmMapFragmentUsseMemory(mem, size, usse_offset);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("fragmentUsseAlloc: sceGxmMapFragmentUsseMemory error:", rtn);
		return NULL;
	}
	return mem;
}

static void fragmentUsseFree(SceUID uid) {
	int rtn;
	void *mem = NULL;
	rtn = sceKernelGetMemBlockBase(uid, &mem);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("fragmentUsseFree: sceKernelGetMemBlockBase error:", rtn);
		return;
	}
	sceGxmUnmapFragmentUsseMemory(mem);
	rtn = sceKernelFreeMemBlock(uid);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("fragmentUsseFree: sceKernelFreeMemBlock error:", rtn);
		return;
	}
}

/*static void* poolMalloc(unsigned int size) {
	void *addr;
	if ((poolIndex + size) < poolSize) {
		addr = (void*)((unsigned int)poolAddr + poolIndex);
		poolIndex += size;
		return addr;
	}
	return NULL;
}*/

static void* poolMemalign(unsigned int size, unsigned int alignment) {
	unsigned int new_index = (poolIndex + alignment - 1) & ~(alignment - 1);
	void *addr;
	if ((new_index + size) < poolSize) {
		addr = (void*)((unsigned int)poolAddr + new_index);
		poolIndex = new_index + size;
		return addr;
	}
	_graphicsDebugMessage("poolMemalign: Reached NULL.");
	return NULL;
}

int textureFormatSize(SceGxmTextureFormat format) {
	switch (format & 0x9F000000U) {
		case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
		case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
		case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
			return 1;
		case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
		case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
		case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
		case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
		case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
		case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
		case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
			return 2;
		case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
		case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
			return 3;
		case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
		case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
		case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
		case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
		case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
		default:
			return 4;
	}
}

static int initGxm(void) {
	int rtn;
	SceGxmInitializeParams initializeParams;
	memset(&initializeParams, 0, sizeof(SceGxmInitializeParams));
	initializeParams.flags = 0;
	initializeParams.displayQueueMaxPendingCount = PSP2_DISPLAY_MAX_PENDING_SWAPS;
	initializeParams.displayQueueCallback = displayCallback;
	initializeParams.displayQueueCallbackDataSize = sizeof(struct _displayData);
	initializeParams.parameterBufferSize = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;
	rtn = sceGxmInitialize(&initializeParams);
	return rtn;
}

static int initCreateContext(void) {
	int rtn;
	/* Allocate ring buffer memory using default sizes */
	unsigned int fragmentUsseRingBufferOffset;
	void *vdmRingBuffer = gpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE, 4, SCE_GXM_MEMORY_ATTRIB_READ, &vdmRingBufferUid);
	void *vertexRingBuffer = gpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE, 4,	SCE_GXM_MEMORY_ATTRIB_READ,	&vertexRingBufferUid);
	void *fragmentRingBuffer = gpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE, 4, SCE_GXM_MEMORY_ATTRIB_READ, &fragmentRingBufferUid);
	void *fragmentUsseRingBuffer = fragmentUsseAlloc(SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE,
		&fragmentUsseRingBufferUid,	&fragmentUsseRingBufferOffset);
		
	if (vdmRingBuffer == NULL) {
		_graphicsDebugMessage("initCreateContext: vdmRingBuffer is NULL.");
		return -1;
	}
	if (vertexRingBuffer == NULL) {
		_graphicsDebugMessage("initCreateContext: vertexRingBuffer is NULL.");
		return -2;
	}
	if (fragmentRingBuffer == NULL) {
		_graphicsDebugMessage("initCreateContext: fragmentRingBuffer is NULL.");
		return -3;
	}
	if (fragmentUsseRingBuffer == NULL) {
		_graphicsDebugMessage("initCreateContext: fragmentUsseRingBuffer is NULL.");
		return -4;
	}

	memset(&contextParams, 0, sizeof(SceGxmContextParams));
	contextParams.hostMem = malloc(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
	contextParams.hostMemSize = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
	contextParams.vdmRingBufferMem = vdmRingBuffer;
	contextParams.vdmRingBufferMemSize = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
	contextParams.vertexRingBufferMem = vertexRingBuffer;
	contextParams.vertexRingBufferMemSize = SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
	contextParams.fragmentRingBufferMem = fragmentRingBuffer;
	contextParams.fragmentRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
	contextParams.fragmentUsseRingBufferMem	= fragmentUsseRingBuffer;
	contextParams.fragmentUsseRingBufferMemSize	= SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;
	contextParams.fragmentUsseRingBufferOffset = fragmentUsseRingBufferOffset;
	rtn = sceGxmCreateContext(&contextParams, &gxmContext);
	return rtn;
}

static int initCreateRenderTarget(void) {
	int rtn;
	/*unsigned int hostMemSize;
	unsigned int driverMemSize;*/
	SceGxmRenderTargetParams renderTargetParams;
	memset(&renderTargetParams, 0, sizeof(SceGxmRenderTargetParams));
	renderTargetParams.flags = 0;
	renderTargetParams.width = PSP2_DISPLAY_WIDTH;
	renderTargetParams.height = PSP2_DISPLAY_HEIGHT;
	renderTargetParams.scenesPerFrame = 1;
	renderTargetParams.multisampleMode = PSP2_MSAA_MODE;
	renderTargetParams.multisampleLocations	= 0;
	renderTargetParams.driverMemBlock = -1;

	/*
	rtn = sceGxmGetRenderTargetMemSize(&renderTargetParams, &hostMemSize, &driverMemSize);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("initCreateRenderTarget: sceGxmGetRenderTargetMemSizes error:", rtn);
		return -1;
	}
	renderTargetParams.driverMemBlock = sceKernelAllocMemBlock("gpu_driver_mem", 
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, driverMemSize, NULL);
	if (renderTargetParams.driverMemBlock < 0) {
		_graphicsDebugMessageWithInt("initCreateRenderTarget: sceKernelAllocMemBlock error:", rtn);
		return -2;
	}
	*/
	
	rtn = sceGxmCreateRenderTarget(&renderTargetParams, &renderTarget);
	return rtn;
}

static int initAllocateDisplayBuffers(void) {
	unsigned int i, x, y;
	int rtn = 0;
	for (i = 0; i < PSP2_DISPLAY_BUFFER_COUNT; i++) {
		displayBufferData[i] = gpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
			4*PSP2_DISPLAY_STRIDE_IN_PIXELS*PSP2_DISPLAY_HEIGHT, SCE_GXM_COLOR_SURFACE_ALIGNMENT,
			SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE, &displayBufferUid[i]);
		for (y = 0; y < PSP2_DISPLAY_HEIGHT; y++) {
			unsigned int *row = (unsigned int*)displayBufferData[i] + y*PSP2_DISPLAY_STRIDE_IN_PIXELS;
			for (x = 0; x < PSP2_DISPLAY_WIDTH; x++) {
				row[x] = 0xFF000000;
			}
		}
		rtn = sceGxmColorSurfaceInit(&displaySurface[i], PSP2_DISPLAY_COLOR_FORMAT, SCE_GXM_COLOR_SURFACE_LINEAR,
			(PSP2_MSAA_MODE == SCE_GXM_MULTISAMPLE_NONE) ? SCE_GXM_COLOR_SURFACE_SCALE_NONE : SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE,
			SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,	PSP2_DISPLAY_WIDTH, PSP2_DISPLAY_HEIGHT, PSP2_DISPLAY_STRIDE_IN_PIXELS, displayBufferData[i]);
		if (rtn < 0) {
			_graphicsDebugMessageWithInt("initAllocateDisplayBuffers: sceGxmColorSurfaceInit error:", rtn);
		}
		rtn = sceGxmSyncObjectCreate(&displayBufferSync[i]);
	}
	return rtn;
}

static int initAllocateDepthBuffer(void) {
	int rtn;
	/* Compute the memory footprint of the depth buffer */
	unsigned int alignedWidth = (((PSP2_DISPLAY_WIDTH) + ((SCE_GXM_TILE_SIZEX) - 1)) & ~((SCE_GXM_TILE_SIZEX) - 1));
	unsigned int alignedHeight = (((PSP2_DISPLAY_HEIGHT) + ((SCE_GXM_TILE_SIZEY) - 1)) & ~((SCE_GXM_TILE_SIZEY) - 1));
	unsigned int sampleCount = alignedWidth*alignedHeight;
	unsigned int depthStrideInSamples = alignedWidth;
	if (PSP2_MSAA_MODE == SCE_GXM_MULTISAMPLE_4X) {
		/* Samples increase in X and Y */
		sampleCount *= 4;
		depthStrideInSamples *= 2;
	}
	else if (PSP2_MSAA_MODE == SCE_GXM_MULTISAMPLE_2X) {
		/* Samples increase in Y only */
		sampleCount *= 2;
	}
	depthBufferData = gpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		4*sampleCount, SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE, &depthBufferUid);
	rtn = sceGxmDepthStencilSurfaceInit(&depthSurface,
		SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
		SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
		depthStrideInSamples, depthBufferData, NULL);
	return rtn;
}

static int initCreateShaderPatcherAndRegister(void) {
	int rtn;
	const unsigned int patcherBufferSize = 64*1024;
	const unsigned int patcherVertexUsseSize = 64*1024;
	const unsigned int patcherFragmentUsseSize = 64*1024;
	void *patcherBuffer = gpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, patcherBufferSize,
		4, SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE, &patcherBufferUid);
	unsigned int patcherVertexUsseOffset;
	void *patcherVertexUsse = vertexUsseAlloc(patcherVertexUsseSize,
		&patcherVertexUsseUid, &patcherVertexUsseOffset);
	unsigned int patcherFragmentUsseOffset;
	void *patcherFragmentUsse = fragmentUsseAlloc(patcherFragmentUsseSize,
		&patcherFragmentUsseUid, &patcherFragmentUsseOffset);
	SceGxmShaderPatcherParams patcherParams;
	
	if (patcherBuffer == NULL) {
		_graphicsDebugMessage("initCreateShaderPatcherAndRegister: patcherBuffer is NULL.");
	}
	if (patcherVertexUsse == NULL) {
		_graphicsDebugMessage("initCreateShaderPatcherAndRegister: patcherVertexUsse is NULL.");
	}
	if (patcherFragmentUsse == NULL) {
		_graphicsDebugMessage("initCreateShaderPatcherAndRegister: patcherFragmentUsse is NULL.");
	}
	memset(&patcherParams, 0, sizeof(SceGxmShaderPatcherParams));
	patcherParams.userData = NULL;
	patcherParams.hostAllocCallback = &patcherHostAlloc;
	patcherParams.hostFreeCallback = &patcherHostFree;
	patcherParams.bufferAllocCallback = NULL;
	patcherParams.bufferFreeCallback = NULL;
	patcherParams.bufferMem = patcherBuffer;
	patcherParams.bufferMemSize = patcherBufferSize;
	patcherParams.vertexUsseAllocCallback = NULL;
	patcherParams.vertexUsseFreeCallback = NULL;
	patcherParams.vertexUsseMem = patcherVertexUsse;
	patcherParams.vertexUsseMemSize = patcherVertexUsseSize;
	patcherParams.vertexUsseOffset = patcherVertexUsseOffset;
	patcherParams.fragmentUsseAllocCallback	= NULL;
	patcherParams.fragmentUsseFreeCallback = NULL;
	patcherParams.fragmentUsseMem = patcherFragmentUsse;
	patcherParams.fragmentUsseMemSize = patcherFragmentUsseSize;
	patcherParams.fragmentUsseOffset = patcherFragmentUsseOffset;
	rtn = sceGxmShaderPatcherCreate(&patcherParams, &shaderPatcher);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("initCreateShaderPatcherAndRegister: sceGxmShaderPatcherCreate error:", rtn);
	}

	/* Check the shaders */
	rtn = sceGxmProgramCheck(clearVertexProgramGxp);
	if (rtn < 0) {_graphicsDebugMessageWithInt("initCreateShaderPatcherAndRegister: sceGxmProgramCheck error:", rtn);}
	rtn = sceGxmProgramCheck(clearFragmentProgramGxp);
	if (rtn < 0) {_graphicsDebugMessageWithInt("initCreateShaderPatcherAndRegister: sceGxmProgramCheck error:", rtn);}
	rtn = sceGxmProgramCheck(colorVertexProgramGxp);
	if (rtn < 0) {_graphicsDebugMessageWithInt("initCreateShaderPatcherAndRegister: sceGxmProgramCheck error:", rtn);}
	rtn = sceGxmProgramCheck(colorFragmentProgramGxp);
	if (rtn < 0) {_graphicsDebugMessageWithInt("initCreateShaderPatcherAndRegister: sceGxmProgramCheck error:", rtn);}
	rtn = sceGxmProgramCheck(textureVertexProgramGxp);
	if (rtn < 0) {_graphicsDebugMessageWithInt("initCreateShaderPatcherAndRegister: sceGxmProgramCheck error:", rtn);}
	rtn = sceGxmProgramCheck(textureFragmentProgramGxp);
	if (rtn < 0) {_graphicsDebugMessageWithInt("initCreateShaderPatcherAndRegister: sceGxmProgramCheck error:", rtn);}

	/* Register programs with the patcher */
	rtn = sceGxmShaderPatcherRegisterProgram(shaderPatcher, clearVertexProgramGxp, &clearVertexProgramId);
	if (rtn < 0) {_graphicsDebugMessageWithInt("initCreateShaderPatcherAndRegister: sceGxmShaderPatcherRegisterProgram error:", rtn);}
	rtn = sceGxmShaderPatcherRegisterProgram(shaderPatcher, clearFragmentProgramGxp, &clearFragmentProgramId);
	if (rtn < 0) {_graphicsDebugMessageWithInt("initCreateShaderPatcherAndRegister: sceGxmShaderPatcherRegisterProgram error:", rtn);}
	rtn = sceGxmShaderPatcherRegisterProgram(shaderPatcher, colorVertexProgramGxp, &colorVertexProgramId);
	if (rtn < 0) {_graphicsDebugMessageWithInt("initCreateShaderPatcherAndRegister: sceGxmShaderPatcherRegisterProgram error:", rtn);}
	rtn = sceGxmShaderPatcherRegisterProgram(shaderPatcher, colorFragmentProgramGxp, &colorFragmentProgramId);
	if (rtn < 0) {_graphicsDebugMessageWithInt("initCreateShaderPatcherAndRegister: sceGxmShaderPatcherRegisterProgram error:", rtn);}
	rtn = sceGxmShaderPatcherRegisterProgram(shaderPatcher, textureVertexProgramGxp, &textureVertexProgramId);
	if (rtn < 0) {_graphicsDebugMessageWithInt("initCreateShaderPatcherAndRegister: sceGxmShaderPatcherRegisterProgram error:", rtn);}
	rtn = sceGxmShaderPatcherRegisterProgram(shaderPatcher, textureFragmentProgramGxp, &textureFragmentProgramId);
	if (rtn < 0) {_graphicsDebugMessageWithInt("initCreateShaderPatcherAndRegister: sceGxmShaderPatcherRegisterProgram error:", rtn);}
	return rtn;
}

static int initCreateProgramsAndDataForClear(void) {
	int rtn;
	const SceGxmProgramParameter *paramClearPositionAttribute = sceGxmProgramFindParameterByName(clearVertexProgramGxp, "aPosition");
	SceGxmVertexAttribute clearVertexAttributes[1];
	SceGxmVertexStream clearVertexStreams[1];
	if (paramClearPositionAttribute == NULL) {
		_graphicsDebugMessage("initCreateProgramsAndDataForClear: paramClearPositionAttribute is NULL");
	}
	clearVertexAttributes[0].streamIndex = 0;
	clearVertexAttributes[0].offset = 0;
	clearVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	clearVertexAttributes[0].componentCount	= 2;
	clearVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramClearPositionAttribute);
	clearVertexStreams[0].stride = sizeof(struct _clearVertex);
	clearVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	rtn = sceGxmShaderPatcherCreateVertexProgram(shaderPatcher, clearVertexProgramId,
		clearVertexAttributes, 1, clearVertexStreams, 1, &clearVertexProgram);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("initCreateProgramsAndDataForClear: sceGxmShaderPatcherCreateVertexProgram error:", rtn);
	}
	rtn = sceGxmShaderPatcherCreateFragmentProgram(shaderPatcher,
		clearFragmentProgramId, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		PSP2_MSAA_MODE, NULL, clearVertexProgramGxp, &clearFragmentProgram);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("initCreateProgramsAndDataForClear: sceGxmShaderPatcherCreateFragmentProgram error:", rtn);
	}

	clearVertices = (struct _clearVertex*)gpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		3*sizeof(struct _clearVertex), 4, SCE_GXM_MEMORY_ATTRIB_READ, &clearVerticesUid);
	if (clearVertices == NULL) {
		_graphicsDebugMessage("initCreateProgramsAndDataForClear: clearVertices is NULL.");
	}
	clearIndices = (uint16_t*)gpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		3*sizeof(uint16_t), 2, SCE_GXM_MEMORY_ATTRIB_READ, &clearIndicesUid);
	if (clearIndices == NULL) {
		_graphicsDebugMessage("initCreateProgramsAndDataForClear: clearIndices is NULL.");
	}
	clearVertices[0].x = -1.0f;clearVertices[0].y = -1.0f;
	clearVertices[1].x =  3.0f;clearVertices[1].y = -1.0f;
	clearVertices[2].x = -1.0f;clearVertices[2].y =  3.0f;
	clearIndices[0] = 0;
	clearIndices[1] = 1;
	clearIndices[2] = 2;
	return rtn;
}

static int initCreateProgramsAndDataForDisplay(void) {
	int rtn;
	SceGxmBlendInfo blend_info;
	const SceGxmProgramParameter *paramColorPositionAttribute;
	const SceGxmProgramParameter *paramColorColorAttribute;
	const SceGxmProgramParameter *paramTexturePositionAttribute;
	const SceGxmProgramParameter *paramTextureTexcoordAttribute;
	SceGxmVertexAttribute colorVertexAttributes[2];
	SceGxmVertexStream colorVertexStreams[1];
	SceGxmVertexAttribute textureVertexAttributes[2];
	SceGxmVertexStream textureVertexStreams[1];
	
	blend_info.colorFunc = SCE_GXM_BLEND_FUNC_ADD;
	blend_info.alphaFunc = SCE_GXM_BLEND_FUNC_ADD;
	blend_info.colorSrc = SCE_GXM_BLEND_FACTOR_SRC_ALPHA;
	blend_info.colorDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend_info.alphaSrc = SCE_GXM_BLEND_FACTOR_ONE;
	blend_info.alphaDst = SCE_GXM_BLEND_FACTOR_ZERO;
	blend_info.colorMask = SCE_GXM_COLOR_MASK_ALL;
	
	paramColorPositionAttribute = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "aPosition");
	paramColorColorAttribute = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "aColor");
	paramTexturePositionAttribute = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "aPosition");
	paramTextureTexcoordAttribute = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "aTexcoord");
	if (paramColorPositionAttribute == NULL) {
		_graphicsDebugMessage("initCreateProgramsAndDataForDisplay: paramColorPositionAttribute is NULL.");
	}
	if (paramColorColorAttribute == NULL) {
		_graphicsDebugMessage("initCreateProgramsAndDataForDisplay: paramColorColorAttribute is NULL.");
	}
	if (paramTexturePositionAttribute == NULL) {
		_graphicsDebugMessage("initCreateProgramsAndDataForDisplay: paramTexturePositionAttribute is NULL.");
	}
	if (paramTextureTexcoordAttribute == NULL) {
		_graphicsDebugMessage("initCreateProgramsAndDataForDisplay: paramTextureTexcoordAttribute is NULL.");
	}

	colorVertexAttributes[0].streamIndex = 0;
	colorVertexAttributes[0].offset = 0;
	colorVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	colorVertexAttributes[0].componentCount = 3; /* x,y,z */
	colorVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramColorPositionAttribute);
	colorVertexAttributes[1].streamIndex = 0;
	colorVertexAttributes[1].offset = 12; /* x,y,z * 4 */
	colorVertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_U8N;
	colorVertexAttributes[1].componentCount = 4; /* Color */
	colorVertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(paramColorColorAttribute);
	colorVertexStreams[0].stride = sizeof(struct _colorVertex);
	colorVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
	rtn = sceGxmShaderPatcherCreateVertexProgram(shaderPatcher,
		colorVertexProgramId, colorVertexAttributes, 2,
		colorVertexStreams, 1, &gxmColorVertexProgram);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("initCreateProgramsAndDataForDisplay: sceGxmShaderPatcherCreateVertexProgram:1 error:", rtn);
	}
	rtn = sceGxmShaderPatcherCreateFragmentProgram(shaderPatcher,
		colorFragmentProgramId, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		PSP2_MSAA_MODE, &blend_info, colorVertexProgramGxp, &gxmColorFragmentProgram);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("initCreateProgramsAndDataForDisplay: sceGxmShaderPatcherCreateFragmentProgram:1 error:", rtn);
	}

	textureVertexAttributes[0].streamIndex = 0;
	textureVertexAttributes[0].offset = 0;
	textureVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	textureVertexAttributes[0].componentCount = 3; /* x,y,z */
	textureVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramTexturePositionAttribute);
	textureVertexAttributes[1].streamIndex = 0;
	textureVertexAttributes[1].offset = 12; /* x,y,z * 4 */
	textureVertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	textureVertexAttributes[1].componentCount = 2; /* u, v */
	textureVertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(paramTextureTexcoordAttribute);
	textureVertexStreams[0].stride = sizeof(struct _textureVertex);
	textureVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
	rtn = sceGxmShaderPatcherCreateVertexProgram(shaderPatcher,
		textureVertexProgramId, textureVertexAttributes, 2,
		textureVertexStreams, 1, &gxmTextureVertexProgram);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("initCreateProgramsAndDataForDisplay: sceGxmShaderPatcherCreateVertexProgram:2 error:", rtn);
	}
	rtn = sceGxmShaderPatcherCreateFragmentProgram(shaderPatcher,
		textureFragmentProgramId, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		PSP2_MSAA_MODE, &blend_info, textureVertexProgramGxp, &gxmTextureFragmentProgram);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("initCreateProgramsAndDataForDisplay: sceGxmShaderPatcherCreateFragmentProgram:2 error:", rtn);
	}
	return rtn;
}

int graphicsInit(unsigned int tempPoolSize) {
	int rtn;
	if (graphicsInitialized) {
		return 1;
	}
	if (tempPoolSize == 0) {
		tempPoolSize = (1 * 1024 * 1024);
	}
	
	graphicsFpuSinfCosf(0.0, 0); /* Remove unused function warning. */

	_graphicsDebugMessage("Graphics debugging enabled.");
	
	#ifdef _PSP2_FPU_H_
		_graphicsDebugMessage("sinf/cosf is using sceFpu.");
	#else
		#ifndef __STRICT_ANSI__
			_graphicsDebugMessage("sinf/cosf is using libc.");
		#else
			_graphicsDebugMessage("sinf/cosf is using inline assembly with VFP.");
		#endif
	#endif

	rtn = initGxm();
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsInit: initGxm error:", rtn);
	}
	rtn = initCreateContext();
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsInit: initCreateContext error:", rtn);
	}
	rtn = initCreateRenderTarget();
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsInit: initCreateRenderTarget error:", rtn);
	}
	rtn = initAllocateDisplayBuffers();
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsInit: initAllocateDisplayBuffers error:", rtn);
	}
	rtn = initAllocateDepthBuffer();
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsInit: initAllocateDepthBuffer error:", rtn);
	}
	rtn = initCreateShaderPatcherAndRegister();
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsInit: initCreateShaderPatcherAndRegister error:", rtn);
	}
	rtn = initCreateProgramsAndDataForClear();
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsInit: initCreateProgramsAndDataForClear error:", rtn);
	}
	rtn = initCreateProgramsAndDataForDisplay();
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsInit: initCreateProgramsAndDataForDisplay error:", rtn);
	}

	/* Find vertex uniforms by name and cache parameter information */
	gxmClearClearColorParam = sceGxmProgramFindParameterByName(clearFragmentProgramGxp, "uClearColor");
	gxmColorWvpParam = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "wvp");
	gxmTextureWvpParam = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "wvp");
	if (gxmClearClearColorParam == NULL) {
		_graphicsDebugMessage("graphicsInit: gxmClearClearColorParam is NULL.");
	}
	if (gxmColorWvpParam == NULL) {
		_graphicsDebugMessage("graphicsInit: gxmColorWvpParam is NULL.");
	}
	if (gxmTextureWvpParam == NULL) {
		_graphicsDebugMessage("graphicsInit: gxmTextureWvpParam is NULL.");
	}

	/* Allocate memory for the memory pool */
	poolSize = tempPoolSize;
	poolAddr = gpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		poolSize, sizeof(void*), SCE_GXM_MEMORY_ATTRIB_READ, &poolUid);
	if (poolAddr == NULL) {
		_graphicsDebugMessage("graphicsInit: poolAddr is NULL.");
	}

	orthoMatrix[0] = 2.0f/(PSP2_DISPLAY_WIDTH);orthoMatrix[4] = 0.0f;
	orthoMatrix[8] = 0.0f;orthoMatrix[12] = -1.0f;
	orthoMatrix[1] = 0.0f;orthoMatrix[5] = -2.0f/(PSP2_DISPLAY_HEIGHT);
	orthoMatrix[9] = 0.0f;orthoMatrix[13] = 1.0f;
	orthoMatrix[2] = 0.0f;orthoMatrix[6] = 0.0f;
	orthoMatrix[10] = -2.0f;orthoMatrix[14] = 1.0f;
	orthoMatrix[3] = 0.0f;orthoMatrix[7] = 0.0f;
	orthoMatrix[11] = 0.0f;orthoMatrix[15] = 1.0f;

	backBufferIndex = 0;
	frontBufferIndex = 0;
	graphicsInitialized = 1;
	return 0;
}

int graphicsEnd(void) {
	int rtn;
	unsigned int i;
	if (graphicsInitialized == 0) {
		return 1;
	}
	sceGxmFinish(gxmContext);

	/* Clean up allocations */
	rtn = sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, clearFragmentProgram);
	if (rtn < 0) {_graphicsDebugMessageWithInt("graphicsEnd: sceGxmShaderPatcherReleaseFragmentProgram:1 error:", rtn);}
	rtn = sceGxmShaderPatcherReleaseVertexProgram(shaderPatcher, clearVertexProgram);
	if (rtn < 0) {_graphicsDebugMessageWithInt("graphicsEnd: sceGxmShaderPatcherReleaseVertexProgram:1 error:", rtn);}
	rtn = sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, gxmColorFragmentProgram);
	if (rtn < 0) {_graphicsDebugMessageWithInt("graphicsEnd: sceGxmShaderPatcherReleaseFragmentProgram:2 error:", rtn);}
	rtn = sceGxmShaderPatcherReleaseVertexProgram(shaderPatcher, gxmColorVertexProgram);
	if (rtn < 0) {_graphicsDebugMessageWithInt("graphicsEnd: sceGxmShaderPatcherReleaseVertexProgram:2 error:", rtn);}
	rtn = sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, gxmTextureFragmentProgram);
	if (rtn < 0) {_graphicsDebugMessageWithInt("graphicsEnd: sceGxmShaderPatcherReleaseFragmentProgram:3 error:", rtn);}
	rtn = sceGxmShaderPatcherReleaseVertexProgram(shaderPatcher, gxmTextureVertexProgram);
	if (rtn < 0) {_graphicsDebugMessageWithInt("graphicsEnd: sceGxmShaderPatcherReleaseVertexProgram:3 error:", rtn);}
	gpuFree(clearIndicesUid);
	gpuFree(clearVerticesUid);

	/* Clean up display queue */
	rtn = sceGxmDisplayQueueFinish();
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsEnd: sceGxmDisplayQueueFinish error:", rtn);
	}
	gpuFree(depthBufferUid);
	for (i = 0; i < PSP2_DISPLAY_BUFFER_COUNT; i++) {
		memset(displayBufferData[i], 0, PSP2_DISPLAY_HEIGHT*PSP2_DISPLAY_STRIDE_IN_PIXELS*4);
		gpuFree(displayBufferUid[i]);
		rtn = sceGxmSyncObjectDestroy(displayBufferSync[i]);
		if (rtn < 0) {
			_graphicsDebugMessageWithInt("graphicsEnd: sceGxmSyncObjectDestroy error:", rtn);
		}
	}

	/* Unregister programs and destroy shader patcher */
	rtn = sceGxmShaderPatcherUnregisterProgram(shaderPatcher, clearFragmentProgramId);
	if (rtn < 0) {_graphicsDebugMessageWithInt("graphicsEnd: sceGxmShaderPatcherUnregisterProgram:1 error:", rtn);}
	rtn = sceGxmShaderPatcherUnregisterProgram(shaderPatcher, clearVertexProgramId);
	if (rtn < 0) {_graphicsDebugMessageWithInt("graphicsEnd: sceGxmShaderPatcherUnregisterProgram:2 error:", rtn);}
	rtn = sceGxmShaderPatcherUnregisterProgram(shaderPatcher, colorFragmentProgramId);
	if (rtn < 0) {_graphicsDebugMessageWithInt("graphicsEnd: sceGxmShaderPatcherUnregisterProgram:3 error:", rtn);}
	rtn = sceGxmShaderPatcherUnregisterProgram(shaderPatcher, colorVertexProgramId);
	if (rtn < 0) {_graphicsDebugMessageWithInt("graphicsEnd: sceGxmShaderPatcherUnregisterProgram:4 error:", rtn);}
	rtn = sceGxmShaderPatcherUnregisterProgram(shaderPatcher, textureFragmentProgramId);
	if (rtn < 0) {_graphicsDebugMessageWithInt("graphicsEnd: sceGxmShaderPatcherUnregisterProgram:5 error:", rtn);}
	rtn = sceGxmShaderPatcherUnregisterProgram(shaderPatcher, textureVertexProgramId);
	if (rtn < 0) {_graphicsDebugMessageWithInt("graphicsEnd: sceGxmShaderPatcherUnregisterProgram:6 error:", rtn);}
	rtn = sceGxmShaderPatcherDestroy(shaderPatcher);
	fragmentUsseFree(patcherFragmentUsseUid);
	vertexUsseFree(patcherVertexUsseUid);
	gpuFree(patcherBufferUid);

	/* Destroy the render target and context */
	sceGxmDestroyRenderTarget(renderTarget);
	sceGxmDestroyContext(gxmContext);
	fragmentUsseFree(fragmentUsseRingBufferUid);
	gpuFree(fragmentRingBufferUid);
	gpuFree(vertexRingBufferUid);
	gpuFree(vdmRingBufferUid);
	free(contextParams.hostMem);

	gpuFree(poolUid);
	rtn = sceGxmTerminate();
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsEnd: sceGxmTerminate error:", rtn);
	}
	graphicsInitialized = 0;
	return 1;
}

void graphicsClearScreen(void) {
	graphicsClearScreenColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void graphicsClearScreenColor(float a, float b, float c, float d) {
	int rtn;
	float clear_color[4];
	void *color_buffer;
	clear_color[0] = a;
	clear_color[1] = b;
	clear_color[2] = c;
	clear_color[3] = d;
	sceGxmSetVertexProgram(gxmContext, clearVertexProgram);
	sceGxmSetFragmentProgram(gxmContext, clearFragmentProgram);

	rtn = sceGxmReserveFragmentDefaultUniformBuffer(gxmContext, &color_buffer);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsClearScreenColor: sceGxmReserveFragmentDefaultUniformBuffer error:", rtn);
	}
	rtn = sceGxmSetUniformDataF(color_buffer, gxmClearClearColorParam, 0, 4, clear_color);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsClearScreenColor: sceGxmSetUniformDataF error:", rtn);
	}

	rtn = sceGxmSetVertexStream(gxmContext, 0, clearVertices);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsClearScreenColor: sceGxmSetVertexStream error:", rtn);
	}
	rtn = sceGxmDraw(gxmContext, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, clearIndices, 3);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsClearScreenColor: sceGxmDraw error:", rtn);
	}
}

void graphicsSwapBuffers(void) {
	int rtn;
	struct _displayData displayData;
	sceGxmPadHeartbeat(&displaySurface[backBufferIndex], displayBufferSync[backBufferIndex]);

	displayData.address = displayBufferData[backBufferIndex];
	rtn = sceGxmDisplayQueueAddEntry(displayBufferSync[frontBufferIndex],
		displayBufferSync[backBufferIndex], &displayData);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsSwapBuffers: sceGxmDisplayQueueAddEntry error:", rtn);
	}

	frontBufferIndex = backBufferIndex;
	backBufferIndex = (backBufferIndex + 1) % PSP2_DISPLAY_BUFFER_COUNT;
}

void graphicsStartDrawing(void) {
	int rtn;
	poolIndex = 0;
	rtn = sceGxmBeginScene(gxmContext, 0, renderTarget, NULL, NULL,
		displayBufferSync[backBufferIndex],	&displaySurface[backBufferIndex],
		&depthSurface);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsStartDrawing: sceGxmBeginScene error:", rtn);
	}
}

void graphicsEndDrawing(void) {
	int rtn;
	rtn = sceGxmEndScene(gxmContext, NULL, NULL);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsEndDrawing: sceGxmEndScene error:", rtn);
	}
	++frameNum;
}

void graphicsWaitFinish(void) {
	sceGxmFinish(gxmContext);
}

struct graphicsTexture* graphicsCreateTexture(unsigned int w, unsigned int h) {
	return graphicsCreateTextureFormat(w, h, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);
}

struct graphicsTexture* graphicsCreateTextureFormat(unsigned int w, unsigned int h, SceGxmTextureFormat format) {
	struct graphicsTexture *texture = calloc(1, sizeof(struct graphicsTexture));
	const int tex_size =  w * h * textureFormatSize(format);
	void *texture_data;
	void *texture_palette;
	int rtn;

	if (texture == NULL) {
		_graphicsDebugMessage("graphicsCreateTextureFormat: texture is NULL.");
		return NULL;
	}
	if (w > PSP2_MAX_TEX_SIZE || h > PSP2_MAX_TEX_SIZE) {
		_graphicsDebugMessage("graphicsCreateTextureFormat: texture is too big.");
		_graphicsDebugMessageWithInt("w:", w);
		_graphicsDebugMessageWithInt("h:", h);
		free(texture);
		return NULL;
	}
	texture->flags = 0;
	texture->x = 0.0;
	texture->y = 0.0;
	texture->rot = 0.0;
	texture->center_x = 0.0;
	texture->center_y = 0.0;
	texture->x_scale = 1.0;
	texture->y_scale = 1.0;
	texture->ignoreCenterXY = 1;

	texture_data = gpuAlloc(SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
		tex_size, SCE_GXM_TEXTURE_ALIGNMENT, SCE_GXM_MEMORY_ATTRIB_READ,
		&texture->data_UID);
	if (texture_data == NULL) {
		_graphicsDebugMessage("graphicsCreateTextureFormat: texture_data is NULL.");
		free(texture);
		return NULL;
	}
	memset(texture_data, 0, tex_size);
	rtn = sceGxmTextureInitLinear(&texture->gxm_tex, texture_data, format, w, h, 0);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsCreateTextureFormat: sceGxmTextureInitLinear error:", rtn);
	}
	if ((format & 0x9F000000U) == SCE_GXM_TEXTURE_BASE_FORMAT_P8) {
		texture_palette = gpuAlloc(
			SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
			256 * sizeof(uint32_t),
			SCE_GXM_PALETTE_ALIGNMENT,
			SCE_GXM_MEMORY_ATTRIB_READ,
			&texture->palette_UID);
		if (texture_palette == NULL) {
			_graphicsDebugMessage("graphicsCreateTextureFormat: texture_palette is NULL.");
			graphicsFreeTexture(texture);
			return NULL;
		}
		memset(texture_palette, 0, 256 * sizeof(uint32_t));
		rtn = sceGxmTextureSetPalette(&texture->gxm_tex, texture_palette);
		if (rtn < 0) {
			_graphicsDebugMessageWithInt("graphicsCreateTextureFormat: sceGxmTextureSetPalette error:", rtn);
		}
	}
	else {
		texture->palette_UID = 0;
	}
	return texture;
}

struct graphicsTexture* graphicsLoadRawImageFile(char *loc, unsigned int w, unsigned int h) {
	struct graphicsTexture *rtn;
	unsigned char *tex_data;
	SceUID rawFile;
	char *fileContents;
	size_t fileLen = -1;
	size_t i = 0;

	rawFile = sceIoOpen(loc, SCE_O_RDONLY, 0777);
	if (rawFile >= 0) {
		fileLen = sceIoLseek32(rawFile, 0, SCE_SEEK_END);
		if ((long)fileLen == -1) {
			sceIoClose(rawFile);
			_graphicsDebugMessage("graphicsLoadRawImageFile: sceIoLseek32 error.");
			return NULL;
		}
		sceIoLseek32(rawFile, 0, SCE_SEEK_SET);
		fileContents = (char*)malloc(fileLen);
		if (fileContents == NULL) {
			sceIoClose(rawFile);
			_graphicsDebugMessage("graphicsLoadRawImageFile: malloc error.");
			return NULL;
		}
		sceIoRead(rawFile, fileContents, fileLen);
		rtn = graphicsCreateTexture(w, h);
		if (rtn == NULL) {
			_graphicsDebugMessage("graphicsLoadRawImageFile: graphicsCreateTexture error.");
			return NULL;
		}
		tex_data = graphicsTextureGetData(rtn);
		if (tex_data == NULL) {
			_graphicsDebugMessage("graphicsLoadRawImageFile: graphicsTextureGetData error.");
			return NULL;
		}
		i = 0;
		while (i < fileLen) {
			tex_data[i] = fileContents[i];
			++i;
		}
		free(fileContents);
		sceIoClose(rawFile);
		return rtn;
	}
	_graphicsDebugMessage("graphicsLoadRawImageFile: sceIoOpen error.");
	return NULL;
}

void graphicsFreeTexture(struct graphicsTexture *texture) {
	if (texture != NULL) {
		if (texture->flags & (1 << 0)) {
			sceGxmFinish(gxmContext);
		}
		if (texture->palette_UID != 0) {
			gpuFree(texture->palette_UID);
		}
		gpuFree(texture->data_UID);
		free(texture);
	}
}

unsigned int graphicsTextureGetStride(struct graphicsTexture *texture) {
	return ((sceGxmTextureGetWidth(&texture->gxm_tex) + 7) & ~7)
		* textureFormatSize(sceGxmTextureGetFormat(&texture->gxm_tex));
}

void* graphicsTextureGetData(struct graphicsTexture *texture) {
	return sceGxmTextureGetData(&texture->gxm_tex);
}

void __attribute__((optimize("O2"))) graphicsDrawTexture(struct graphicsTexture *texture) {
	float x;float y;
	float center_x;
	float center_y;
	float c;float s;
	int i;int rtn;
	void *vertex_wvp_buffer;
	struct _textureVertex *vertices = (struct _textureVertex*)poolMemalign(
		4 * sizeof(struct _textureVertex), sizeof(struct _textureVertex));
	uint16_t *indices = (uint16_t*)poolMemalign(4 * sizeof(uint16_t), sizeof(uint16_t));
	const float w = texture->x_scale * sceGxmTextureGetWidth(&texture->gxm_tex);
	const float h = texture->y_scale * sceGxmTextureGetHeight(&texture->gxm_tex);
	
	texture->flags |= 1 << 0;

	sceGxmSetVertexProgram(gxmContext, gxmTextureVertexProgram);
	sceGxmSetFragmentProgram(gxmContext, gxmTextureFragmentProgram);
	rtn = sceGxmReserveVertexDefaultUniformBuffer(gxmContext, &vertex_wvp_buffer);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawTexture: sceGxmReserveVertexDefaultUniformBuffer error:", rtn);
	}
	rtn = sceGxmSetUniformDataF(vertex_wvp_buffer, gxmTextureWvpParam, 0, 16, orthoMatrix);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawTexture: sceGxmSetUniformDataF error:", rtn);
	}

	if (texture->ignoreCenterXY) {
		center_x = sceGxmTextureGetWidth(&texture->gxm_tex)/2.0f;
		center_y = sceGxmTextureGetHeight(&texture->gxm_tex)/2.0f;
	}
	else {
		center_x = texture->center_x;
		center_y = texture->center_y;
	}
	x = texture->x;
	y = texture->y;

	if (texture->rot != 0.0) {
		vertices[0].x = -center_x;vertices[0].y = -center_y;
		vertices[0].z = +0.5f;vertices[0].u = 0.0f;vertices[0].v = 0.0f;
		vertices[1].x = w - center_x;vertices[1].y = -center_y;
		vertices[1].z = +0.5f;vertices[1].u = 1.0f;vertices[1].v = 0.0f;
		vertices[2].x = -center_x;vertices[2].y = h - center_y;
		vertices[2].z = +0.5f;vertices[2].u = 0.0f;vertices[2].v = 1.0f;
		vertices[3].x = w - center_x;vertices[3].y = h - center_y;
		vertices[3].z = +0.5f;vertices[3].u = 1.0f;vertices[3].v = 1.0f;

		c = graphicsCosf(texture->rot);
		s = graphicsSinf(texture->rot);
		for (i = 0; i < 4; ++i) {
			float _x = vertices[i].x;
			float _y = vertices[i].y;
			vertices[i].x = _x*c - _y*s + x;
			vertices[i].y = _x*s + _y*c + y;
		}
	}
	else {
		x -= center_x;
		y -= center_y;
		vertices[0].x = x;vertices[0].y = y;
		vertices[0].z = +0.5f;vertices[0].u = 0.0f;vertices[0].v = 0.0f;
		vertices[1].x = x + w;vertices[1].y = y;
		vertices[1].z = +0.5f;vertices[1].u = 1.0f;vertices[1].v = 0.0f;
		vertices[2].x = x;vertices[2].y = y + h;
		vertices[2].z = +0.5f;vertices[2].u = 0.0f;vertices[2].v = 1.0f;
		vertices[3].x = x + w;vertices[3].y = y + h;
		vertices[3].z = +0.5f;vertices[3].u = 1.0f;vertices[3].v = 1.0f;
	}

	indices[0] = 0;indices[1] = 1;
	indices[2] = 2;indices[3] = 3;

	rtn = sceGxmSetFragmentTexture(gxmContext, 0, &texture->gxm_tex);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawTexture: sceGxmSetFragmentTexture error:", rtn);
	}
	rtn = sceGxmSetVertexStream(gxmContext, 0, vertices);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawTexture: sceGxmSetVertexStream error:", rtn);
	}
	rtn = sceGxmDraw(gxmContext, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, indices, 4);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawTexture: sceGxmDraw error:", rtn);
	}
}

void graphicsDrawTexturePart(struct graphicsTexture *texture, float tex_x, float tex_y, float tex_w, float tex_h) {
	void *vertex_wvp_buffer;
	float x;float y;
	int rtn;
	struct _textureVertex *vertices = (struct _textureVertex*)poolMemalign(
		4 * sizeof(struct _textureVertex), sizeof(struct _textureVertex));
	uint16_t *indices = (uint16_t*)poolMemalign(
		4 * sizeof(uint16_t), sizeof(uint16_t));
	const float w = sceGxmTextureGetWidth(&texture->gxm_tex);
	const float h = sceGxmTextureGetHeight(&texture->gxm_tex);
	const float u0 = tex_x/w;
	const float v0 = tex_y/h;
	const float u1 = (tex_x+tex_w)/w;
	const float v1 = (tex_y+tex_h)/h;

	sceGxmSetVertexProgram(gxmContext, gxmTextureVertexProgram);
	sceGxmSetFragmentProgram(gxmContext, gxmTextureFragmentProgram);
	rtn = sceGxmReserveVertexDefaultUniformBuffer(gxmContext, &vertex_wvp_buffer);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawTexturePart: sceGxmReserveVertexDefaultUniformBuffer error:", rtn);
	}
	rtn = sceGxmSetUniformDataF(vertex_wvp_buffer, gxmTextureWvpParam, 0, 16, orthoMatrix);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawTexturePart: sceGxmSetUniformDataF error:", rtn);
	}

	x = texture->x;
	y = texture->y;
	tex_w *= texture->x_scale;
	tex_h *= texture->y_scale;

	vertices[0].x = x;vertices[0].y = y;
	vertices[0].z = +0.5f;vertices[0].u = u0;vertices[0].v = v0;
	vertices[1].x = x + tex_w;vertices[1].y = y;
	vertices[1].z = +0.5f;vertices[1].u = u1;vertices[1].v = v0;
	vertices[2].x = x;vertices[2].y = y + tex_h;
	vertices[2].z = +0.5f;vertices[2].u = u0;vertices[2].v = v1;
	vertices[3].x = x + tex_w;vertices[3].y = y + tex_h;
	vertices[3].z = +0.5f;vertices[3].u = u1;vertices[3].v = v1;
	indices[0] = 0;indices[1] = 1;
	indices[2] = 2;indices[3] = 3;

	rtn = sceGxmSetFragmentTexture(gxmContext, 0, &texture->gxm_tex);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawTexturePart: sceGxmSetFragmentTexture error:", rtn);
	}
	rtn = sceGxmSetVertexStream(gxmContext, 0, vertices);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawTexturePart: sceGxmSetVertexStream error:", rtn);
	}
	rtn = sceGxmDraw(gxmContext, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, indices, 4);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawTexturePart: sceGxmDraw error:", rtn);
	}
}

void graphicsDrawPixel(float x, float y, unsigned int color) {
	int rtn;
	void *vertexDefaultBuffer;
	struct _colorVertex *vertex = (struct _colorVertex*)poolMemalign(
		1 * sizeof(struct _colorVertex), sizeof(struct _colorVertex));
	uint16_t *index = (uint16_t*)poolMemalign(
		1 * sizeof(uint16_t), sizeof(uint16_t));

	vertex->x = x;
	vertex->y = y;
	vertex->z = +0.5f;
	vertex->color = color;
	*index = 0; /* TODO: CHECK THIS */

	sceGxmSetVertexProgram(gxmContext, gxmColorVertexProgram);
	sceGxmSetFragmentProgram(gxmContext, gxmColorFragmentProgram);
	rtn = sceGxmReserveVertexDefaultUniformBuffer(gxmContext, &vertexDefaultBuffer);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawPixel: sceGxmReserveVertexDefaultUniformBuffer error:", rtn);
	}
	rtn = sceGxmSetUniformDataF(vertexDefaultBuffer, gxmColorWvpParam, 0, 16, orthoMatrix);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawPixel: sceGxmSetUniformDataF error:", rtn);
	}
	rtn = sceGxmSetVertexStream(gxmContext, 0, vertex);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawPixel: sceGxmSetVertexStream error:", rtn);
	}
	sceGxmSetFrontPolygonMode(gxmContext, SCE_GXM_POLYGON_MODE_POINT);
	rtn = sceGxmDraw(gxmContext, SCE_GXM_PRIMITIVE_POINTS, SCE_GXM_INDEX_FORMAT_U16, index, 1);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawPixel: sceGxmDraw error:", rtn);
	}
	sceGxmSetFrontPolygonMode(gxmContext, SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);
}

void graphicsDrawLine(float x0, float y0, float x1, float y1, unsigned int color) {
	int rtn;
	void *vertexDefaultBuffer;
	struct _colorVertex *vertices = (struct _colorVertex*)poolMemalign(
		2 * sizeof(struct _colorVertex), sizeof(struct _colorVertex));
	uint16_t *indices = (uint16_t*)poolMemalign(
		2 * sizeof(uint16_t), sizeof(uint16_t));

	vertices[0].x = x0;vertices[0].y = y0;
	vertices[0].z = +0.5f;vertices[0].color = color;
	vertices[1].x = x1;vertices[1].y = y1;
	vertices[1].z = +0.5f;vertices[1].color = color;
	indices[0] = 0;indices[1] = 1;

	sceGxmSetVertexProgram(gxmContext, gxmColorVertexProgram);
	sceGxmSetFragmentProgram(gxmContext, gxmColorFragmentProgram);
	rtn = sceGxmReserveVertexDefaultUniformBuffer(gxmContext, &vertexDefaultBuffer);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawLine: sceGxmReserveVertexDefaultUniformBuffer error:", rtn);
	}
	rtn = sceGxmSetUniformDataF(vertexDefaultBuffer, gxmColorWvpParam, 0, 16, orthoMatrix);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawLine: sceGxmSetUniformDataF error:", rtn);
	}
	rtn = sceGxmSetVertexStream(gxmContext, 0, vertices);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawLine: sceGxmSetVertexStream error:", rtn);
	}
	sceGxmSetFrontPolygonMode(gxmContext, SCE_GXM_POLYGON_MODE_LINE);
	rtn = sceGxmDraw(gxmContext, SCE_GXM_PRIMITIVE_LINES, SCE_GXM_INDEX_FORMAT_U16, indices, 2);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawLine: sceGxmDraw error:", rtn);
	}
	sceGxmSetFrontPolygonMode(gxmContext, SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);
}

void graphicsDrawRectangle(float x, float y, float w, float h, unsigned int color) {
	int rtn;
	void *vertexDefaultBuffer;
	struct _colorVertex *vertices = (struct _colorVertex*)poolMemalign(
		4 * sizeof(struct _colorVertex), sizeof(struct _colorVertex));
	uint16_t *indices = (uint16_t*)poolMemalign(
		4 * sizeof(uint16_t), sizeof(uint16_t));

	vertices[0].x = x;vertices[0].y = y;
	vertices[0].z = +0.5f;vertices[0].color = color;
	vertices[1].x = x + w;vertices[1].y = y;
	vertices[1].z = +0.5f;vertices[1].color = color;
	vertices[2].x = x;vertices[2].y = y + h;
	vertices[2].z = +0.5f;vertices[2].color = color;
	vertices[3].x = x + w;vertices[3].y = y + h;
	vertices[3].z = +0.5f;vertices[3].color = color;
	indices[0] = 0;indices[1] = 1;
	indices[2] = 2;indices[3] = 3;

	sceGxmSetVertexProgram(gxmContext, gxmColorVertexProgram);
	sceGxmSetFragmentProgram(gxmContext, gxmColorFragmentProgram);
	rtn = sceGxmReserveVertexDefaultUniformBuffer(gxmContext, &vertexDefaultBuffer);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawRectangle: sceGxmReserveVertexDefaultUniformBuffer error:", rtn);
	}
	rtn = sceGxmSetUniformDataF(vertexDefaultBuffer, gxmColorWvpParam, 0, 16, orthoMatrix);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawRectangle: sceGxmSetUniformDataF error:", rtn);
	}
	rtn = sceGxmSetVertexStream(gxmContext, 0, vertices);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawRectangle: sceGxmSetVertexStream error:", rtn);
	}
	rtn = sceGxmDraw(gxmContext, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, indices, 4);
	if (rtn < 0) {
		_graphicsDebugMessageWithInt("graphicsDrawRectangle: sceGxmDraw error:", rtn);
	}
}

void graphicsDrawDialog(void) {
	#ifdef _PSP2_COMMON_API_H_
		int rtn;
		SceCommonDialogUpdateParam dialogUpdateParam;
		memset(&dialogUpdateParam, 0, sizeof(dialogUpdateParam));
			dialogUpdateParam.renderTarget.depthSurfaceData = depthBufferData;
		dialogUpdateParam.renderTarget.colorSurfaceData = displayBufferData[backBufferIndex];
		dialogUpdateParam.renderTarget.surfaceType = SCE_GXM_COLOR_SURFACE_LINEAR;
		dialogUpdateParam.renderTarget.colorFormat = PSP2_DISPLAY_COLOR_FORMAT;
		dialogUpdateParam.renderTarget.width = PSP2_DISPLAY_WIDTH;
		dialogUpdateParam.renderTarget.height = PSP2_DISPLAY_HEIGHT;
		dialogUpdateParam.renderTarget.strideInPixels = PSP2_DISPLAY_STRIDE_IN_PIXELS;
		dialogUpdateParam.displaySyncObject = (SceGxmSyncObject*)displayBufferSync[backBufferIndex];
		rtn = sceCommonDialogUpdate(&dialogUpdateParam);
		if (rtn < 0) {
			_graphicsDebugMessageWithInt("graphicsDrawDialog: sceCommonDialogUpdate error:", rtn);
		}
	#endif
}
