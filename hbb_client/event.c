#include "event.h"

struct _categoryList;
struct _itemList;
struct _fileInfoList;
struct _packageInfo;

struct _categoryList {
	char id;
	char *name;
	size_t nameLength;
	struct _categoryList *next;
};

struct _itemList {
	unsigned int id;
	char *titleId;
	size_t titleIdLength;
	unsigned int date;
	char category;
	char *displayName;
	size_t displayNameLength;
	char *author;
	size_t authorLength;
	char *version;
	size_t versionLength;
	char *webUrl;
	size_t webUrlLength;
	char *description;
	size_t descriptionLength;
	unsigned int dlcount;
	unsigned int cachedrating;
	unsigned char isSafeHomebrew;
	char *extra;
	size_t extraLength;
	char iconStatus; /* 0 = Not yet requested, 1 = Waiting on server, 2 = Downloaded, 
						3 = Does not exist on server, 4 = Cached copy in use */
	struct graphicsTexture *icon;
	struct _itemList *next;
};

struct _packageInfo {
	unsigned int itemID;
	char pkgType;
	unsigned int fileCount;
	struct _fileInfoList *files;
};

struct _fileInfoList {
	unsigned int fileID;
	unsigned int dataLength;
	unsigned short nameLength;
	char *name;
	unsigned int crc;
	SceUID file;
	struct _fileInfoList *next;
};

struct SFOHeader {
	uint32_t magic;
	uint32_t version;
	uint32_t keyofs;
	uint32_t valofs;
	uint32_t count;
} __attribute__((packed)) SFOHeader;

struct SFOEntry {
	uint16_t nameofs;
	uint8_t  alignment;
	uint8_t  type;
	uint32_t valsize;
	uint32_t totalsize;
	uint32_t dataofs;
} __attribute__((packed)) SFOEntry;

static char breakLoop = 0;

int showDbgScreen = 1;
int showLoadingTex = 1;
static struct dbgText *textLayer;
static struct dbgText *dialogText;
static struct graphicsTexture *loadingTex;
static struct graphicsTexture *dbgScrnTex;
static void *dbgScreenData;
static struct graphicsTexture *textLayerTex;
static void *textLayerData;
static struct graphicsTexture *dialogTextTex;
static void *dialogTextData;

struct graphicsTexture *loadingIconTex;
struct graphicsTexture *noIconTex;

static char netConnectFinished = 0;
static SceUInt64 awakeTick;

static struct _categoryList *ndCal = NULL;
static struct _itemList *ndItl = NULL;
static char waitingOnData = 0;
static char scrn = 0; /* 0 = Load, 1 = Category View, 2 = Item List View, 3 = Item View, 4 = Install Item */

static char scrnTextNeedRefresh = 0;
static char scrnClSel = 0;
static char scrnClMaxSel = 0;
static unsigned int scrnIlSel = 0;
static unsigned int scrnIlMaxSel = 0;

static char dialogDisplay = 0;
static char dialogType = 0; /* 0 = Information, 1 = Information Yes/No, 2 = Warning, 3 = Warning Yes/No */
static char dialogResult = 0; /* 0 = OK/Yes, 1 = No */
static char *dialogContents = NULL;
static char dialogFreeContents = 0;

static unsigned int instalItemID = 0;
static char installStatus = 0; /* 0 = Downloading, 1 = Installing, 2 = Finished */
static unsigned int installChunkCount = 0;
static unsigned int installExpectedChunkCount = 0;
static char installPercent = 0;
static unsigned int installPercentMax = 4294967295ui;
static int installResult = -1;
static struct _packageInfo *installPkgInfo = NULL;
static struct _fileInfoList *installCurFile = NULL;
static SceUInt64 installFinishedTick;

static void clearNdItl(void);
static void downloadPackage(unsigned int);
static void preparePtmp(void);
static int writeHeadBinFile(void);

static void clearNdItl(void) {
	struct _itemList *cil = ndItl;
	struct _itemList *nx = NULL;
	while (cil != NULL) {
		nx = cil->next;
		if (cil->titleId != NULL) {
			free(cil->titleId);
		}
		if (cil->displayName != NULL) {
			free(cil->displayName);
		}
		if (cil->author != NULL) {
			free(cil->author);
		}
		if (cil->version != NULL) {
			free(cil->version);
		}
		if (cil->webUrl != NULL) {
			free(cil->webUrl);
		}
		if (cil->description != NULL) {
			free(cil->description);
		}
		if (cil->extra != NULL) {
			free(cil->extra);
		}
		if (cil->icon != NULL) {
			graphicsFreeTexture(cil->icon);
		}
		memset(cil, 0, sizeof(struct _itemList));
		free(cil);
		cil = nx;
	}
	ndItl = NULL;
	scrnIlSel = 0;
	scrnIlMaxSel = 0;
}

static void downloadPackage(unsigned int id) {
	unsigned int netInt = 0;
	char netIntChar[4];
	instalItemID = id;
	installStatus = 0;
	installChunkCount = 0;
	installExpectedChunkCount = 0;
	installPercent = 0;
	installPercentMax = 4294967295ui;
	installResult = -1;
	
	waitingOnData = 1;
	debugTextClear(textLayer);
	scrnTextNeedRefresh = 1;
	scrn = 4;

	netInt = sceNetHtonl(id);
	memcpy(&netIntChar[0], &netInt, 4);
	netSendData(4, 2, &netIntChar[0], 4);
}

static void preparePtmp(void) {
	int rtn;
	deleteDirectoryTreeFiles("ux0:/ptmp/", 0);
	deleteDirectoryTreeFolders("ux0:/ptmp/");
	rtn = sceIoMkdir("ux0:/ptmp/pkg/", 0777);
	if (rtn < 0) {
		debugMessage("Error on sceIoMkdir, returned:");
		debugPrintInt(rtn);
	}
}

static int writeHeadBinFile(void) {
	unsigned char *headData;
	SHA1_CTX ctx;
	unsigned char sha1[20];
	unsigned char buf[64];
	unsigned int off;
	unsigned int len;
	unsigned int out;
	
	SceUID templateHead;
	size_t headSize;
	
	char *titleID = NULL;
	char fullTitleID[128];
	SceUID file = 0;
	size_t fileSize = 0;
	char *fileData = NULL;
	char *name;
	char *value;
	int i;
	struct SFOHeader *header;
	struct SFOEntry *entry;

	sceIoRemove("ux0:/ptmp/pkg/sce_sys/package/head.bin");

	file = sceIoOpen("ux0:/ptmp/pkg/sce_sys/param.sfo", SCE_O_RDONLY, 0777);
	if (!file) {
		debugMessage("writeHeadBinFile Error 1");
	}
	fileSize = sceIoLseek32(file, 0, SCE_SEEK_END);
	sceIoLseek32(file, 0, SCE_SEEK_SET);
	fileData = (char*)calloc(1, fileSize + 1);
	if (!fileData) {
		debugMessage("writeHeadBinFile Error 2");
	}
	sceIoRead(file, fileData, fileSize);

	header = (struct SFOHeader*)fileData;
	entry = (struct SFOEntry*)(fileData + sizeof(struct SFOHeader));
	for (i = 0; i < header->count; ++i) {
		name = fileData + header->keyofs + entry->nameofs;
		value = fileData + header->valofs + entry->dataofs;
		if (name >= fileData + fileSize || value >= fileData + fileSize) {
			break;
		}
		if (strcmp(name, "TITLE_ID") == 0) {
			titleID = (char*)calloc(1, strlen(value+1));
			memcpy(titleID, value, strlen(value));
		}
		++entry;
	}
	
	if (fileData != NULL) {
		free(fileData);
	}
	if (file != 0) {
		sceIoClose(file);
	}
	if (titleID == NULL) {
		debugMessage("writeHeadBinFile Error 3");
	}

	templateHead = sceIoOpen("app0:/data/h.bin", SCE_O_RDONLY, 0777);
	if (templateHead < 0) {
		debugMessage("writeHeadBinFile Error 4");
	}
	headSize = sceIoLseek32(templateHead, 0, SCE_SEEK_END);
	sceIoLseek32(templateHead, 0, SCE_SEEK_SET);
	headData = (unsigned char*)calloc(1, headSize);
	sceIoRead(templateHead, headData, headSize);
	sceIoClose(templateHead);

	if (strlen(titleID) > 9) {
		debugMessage("writeHeadBinFile Error 5");
	}
	sprintf(fullTitleID, "EP9000-%s_00-XXXXXXXXXXXXXXXX", titleID);
	memcpy(&headData[0x30], &fullTitleID[0], 48);

	len = sceNetNtohl(*(unsigned int*)&headData[0xD0]);
	sha1_init(&ctx);
	sha1_update(&ctx, &headData[0], len);
	sha1_final(&ctx, sha1);
	memset(buf, 0, 64);
	memcpy(&buf[0], &sha1[4], 8);
	memcpy(&buf[8], &sha1[4], 8);
	memcpy(&buf[16], &sha1[12], 4);
	buf[20] = sha1[16];
	buf[21] = sha1[1];
	buf[22] = sha1[2];
	buf[23] = sha1[3];
	memcpy(&buf[24], &buf[16], 8);
	sha1_init(&ctx);
	sha1_update(&ctx, buf, 64);
	sha1_final(&ctx, sha1);
	memcpy(&headData[len], sha1, 16);

	off = sceNetNtohl(*(unsigned int*)&headData[0x08]);
	len = sceNetNtohl(*(unsigned int*)&headData[0x10]);
	out = sceNetNtohl(*(unsigned int*)&headData[0xD4]);
	sha1_init(&ctx);
	sha1_update(&ctx, &headData[off], len - 64);
	sha1_final(&ctx, sha1);
	memset(buf, 0, 64);
	memcpy(&buf[0], &sha1[4], 8);
	memcpy(&buf[8], &sha1[4], 8);
	memcpy(&buf[16], &sha1[12], 4);
	buf[20] = sha1[16];
	buf[21] = sha1[1];
	buf[22] = sha1[2];
	buf[23] = sha1[3];
	memcpy(&buf[24], &buf[16], 8);
	sha1_init(&ctx);
	sha1_update(&ctx, buf, 64);
	sha1_final(&ctx, sha1);
	memcpy(&headData[out], sha1, 16);

	len = sceNetNtohl(*(unsigned int*)&headData[0xE8]);
	sha1_init(&ctx);
	sha1_update(&ctx, &headData[0], len);
	sha1_final(&ctx, sha1);
	memset(buf, 0, 64);
	memcpy(&buf[0], &sha1[4], 8);
	memcpy(&buf[8], &sha1[4], 8);
	memcpy(&buf[16], &sha1[12], 4);
	buf[20] = sha1[16];
	buf[21] = sha1[1];
	buf[22] = sha1[2];
	buf[23] = sha1[3];
	memcpy(&buf[24], &buf[16], 8);
	sha1_init(&ctx);
	sha1_update(&ctx, buf, 64);
	sha1_final(&ctx, sha1);
	memcpy(&headData[len], sha1, 16);

	sceIoMkdir("ux0:/ptmp/pkg/sce_sys/package", 0777);
	file = sceIoOpen("ux0:/ptmp/pkg/sce_sys/package/head.bin", SCE_O_WRONLY|SCE_O_CREAT, 0777);
	if (file >= 0) {
		sceIoWrite(file, headData, headSize);
		sceIoClose(file);
	}
	else {
		debugMessage("Error on sceIoOpen, returned:");
		debugPrintInt(file);
	}
	free(headData);
	free(titleID);
	return 0;
}

int eventInit(void) {
	dbgScrnTex = graphicsCreateTexture(PSP2_DISPLAY_WIDTH, PSP2_DISPLAY_HEIGHT);
	dbgScreenData = graphicsTextureGetData(dbgScrnTex);
	memset(dbgScreenData, 0x00, PSP2_DISPLAY_WIDTH*PSP2_DISPLAY_HEIGHT*4);
	dbgScrnTex->x = PSP2_DISPLAY_WIDTH/2;
	dbgScrnTex->y = PSP2_DISPLAY_HEIGHT/2;
	dbgScrn = debugTextInit(dbgScreenData);
	
	textLayerTex = graphicsCreateTexture(PSP2_DISPLAY_WIDTH, PSP2_DISPLAY_HEIGHT);
	textLayerData = graphicsTextureGetData(textLayerTex);
	memset(textLayerData, 0x00, PSP2_DISPLAY_WIDTH*PSP2_DISPLAY_HEIGHT*4);
	textLayerTex->x = PSP2_DISPLAY_WIDTH/2;
	textLayerTex->y = PSP2_DISPLAY_HEIGHT/2;
	textLayer = debugTextInit(textLayerData);

	dialogTextTex = graphicsCreateTexture(PSP2_DISPLAY_WIDTH, PSP2_DISPLAY_HEIGHT);
	dialogTextData = graphicsTextureGetData(dialogTextTex);
	memset(dialogTextData, 0x00, PSP2_DISPLAY_WIDTH*PSP2_DISPLAY_HEIGHT*4);
	dialogTextTex->x = PSP2_DISPLAY_WIDTH/2;
	dialogTextTex->y = PSP2_DISPLAY_HEIGHT/2;
	dialogText = debugTextInit(dialogTextData);
	
	loadingIconTex = graphicsLoadRawImageFile("app0:/data/loadicon0.raw", 128, 128);
	noIconTex = graphicsLoadRawImageFile("app0:/data/noicon0.raw", 128, 128);
	
	loadingTex = graphicsLoadRawImageFile("app0:/data/loading.raw", 24, 23);
	loadingTex->x = PSP2_DISPLAY_WIDTH/2;
	loadingTex->y = PSP2_DISPLAY_HEIGHT/2;
	debugTextPrint(textLayer, "Connecting...", 400, 300, DBGTXT_C7, DBGTXT_C0, DBGTXT_SMALL);
	debugMessage("Press LTRIGGER to show/hide debug text.");
	initConnection("10.0.0.5", 40111);
	return 0;
}

int eventEnd(void) {
	endConnection();
	debugTextEnd(dialogText);
	graphicsFreeTexture(dialogTextTex);
	debugTextEnd(textLayer);
	graphicsFreeTexture(textLayerTex);
	debugTextEnd(dbgScrn);
	graphicsFreeTexture(dbgScrnTex);
	return 0;
}

int eventUpdate(void) {
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_OLED_OFF);
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_OLED_DIMMING);
	if (breakLoop) {
		return -1;
	}
	if (netinf.status < 0) {
		if (scrn != 0) {
			scrn = 0;
			debugTextClear(textLayer);
		}
		debugTextPrint(textLayer, "Connection failed.", 360, 300, DBGTXT_C7, DBGTXT_C0, DBGTXT_SMALL);
		showLoadingTex = 0;
	}
	if (netinf.status == 1 && netConnectFinished == 1) {
		if (sceKernelGetProcessTimeWide() - awakeTick >= (10000000)) {
			netSendData(2, 2, NULL, 0);
			awakeTick = sceKernelGetProcessTimeWide();
		}
		if (ndCal == NULL && waitingOnData == 0) {
			waitingOnData = 1;
			showLoadingTex = 1;
			netSendData(3, 2, "\x00", 1);
		}
	}
	if (scrn == 2 && waitingOnData == 0) {
		struct _itemList *nil = ndItl;
		int skipSel = scrnIlSel;
		int skip = 0;
		int s = 0;
		int netInt = 0;
		char netIntChar[4];
		while (skipSel >= 4) {
			++skip;
			skipSel -= 1;
		}
		while (nil != NULL) {
			if (skip > 0) {
				if (nil->iconStatus == 2 || nil->iconStatus == 4) {
					graphicsFreeTexture(nil->icon);
					nil->icon = NULL;
					nil->iconStatus = 0;
				}
				--skip;
			}
			else {
				++s;
				if (s <= 4) {
					if (nil->iconStatus == 0) {
						netInt = sceNetHtonl(nil->id);
						memcpy(&netIntChar[0], &netInt, 4);
						netSendData(3, 4, &netIntChar[0], 4);
						nil->iconStatus = 1;
					}
				}
				else {
					if (nil->iconStatus == 2 || nil->iconStatus == 4) {
						graphicsFreeTexture(nil->icon);
						nil->icon = NULL;
						nil->iconStatus = 0;
					}
				}
			}
			nil = nil->next;
		}
	}
	if (scrn == 3 && waitingOnData == 0) {
		struct _itemList *nil = ndItl;
		int netInt = 0;
		char netIntChar[4];
		int i = 0;
		while (nil != NULL) {
			if (i == scrnIlSel) {
				if (nil->iconStatus == 0) {
					netInt = sceNetHtonl(nil->id);
					memcpy(&netIntChar[0], &netInt, 4);
					netSendData(3, 4, &netIntChar[0], 4);
					nil->iconStatus = 1;
				}
			}
			else {
				if (nil->iconStatus == 2 || nil->iconStatus == 4) {
					graphicsFreeTexture(nil->icon);
					nil->icon = NULL;
					nil->iconStatus = 0;
				}
			}
			nil = nil->next;
			++i;
		}
	}
	if (scrn == 4 && installCurFile != NULL && installStatus == 0 && waitingOnData == 0) {
		int netInt = 0;
		char netIntChar[8];
		netInt = sceNetHtonl(instalItemID);
		memcpy(&netIntChar[0], &netInt, 4);
		netInt = sceNetHtonl(installCurFile->fileID);
		memcpy(&netIntChar[4], &netInt, 4);
		netSendData(4, 3, &netIntChar[0], 8);
		waitingOnData = 1;
	}
	else if (scrn == 4 && installPkgInfo != NULL && installStatus == 1 && waitingOnData == 0) {
		int state = 1;
		int result = 0;
		uint32_t ptr[256] = {0};
		ptr[0] = 0;
		ptr[1] = (uint32_t)&ptr[0];
		uint32_t scepaf_argp[] = {0x400000, 60000, 0x40000, 0, 0};
		sceSysmoduleLoadModuleInternalWithArg(0x80000008, sizeof(scepaf_argp), scepaf_argp, ptr);
		
		writeHeadBinFile();
		
		sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_PROMOTER_UTIL);
		scePromoterUtilityInit();

		scePromoterUtilityPromotePkg("ux0:/ptmp/pkg/", 0);
		while (state) {
			scePromoterUtilityGetState(&state);
			sleep(1);
		} 
		scePromoterUtilityGetResult(&result);

		scePromoterUtilityExit();
		sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_PROMOTER_UTIL);
		installPercent = 1;
		installStatus = 2;
		scrnTextNeedRefresh = 1;
		installFinishedTick = sceKernelGetProcessTimeWide();
	}
	else if (scrn == 4 && installStatus == 2) {
		if (sceKernelGetProcessTimeWide() - installFinishedTick >= (3000000)) {
			struct _itemList *nil = ndItl;
			int netInt = 0;
			char netIntChar[4];
			int i = 0;
			while (nil != NULL) {
				if (i == scrnIlSel) {
					netInt = sceNetHtonl(nil->id);
					memcpy(&netIntChar[0], &netInt, 4);
					netSendData(3, 5, &netIntChar[0], 4);
					waitingOnData = 1;
					showLoadingTex = 1;
					break;
				}
				nil = nil->next;
				++i;
			}
				
			debugTextClear(textLayer);
			scrnTextNeedRefresh = 1;
			scrn = 3;
		}
	}
	return 0;
}

int eventDraw(void) {
	if (scrn == 0) {
		if (netConnectFinished == 1) {
			debugTextPrint(textLayer, "  Loading... ", 400, 300, DBGTXT_C7, DBGTXT_C0, DBGTXT_SMALL);
		}
	}
	else if (scrn == 1) {
		struct _categoryList *ncl = ndCal;
		int y = 42;
		if (scrnTextNeedRefresh == 1) {
			debugTextPrint(textLayer, "Select Category:", 10, 10, DBGTXT_C7, DBGTXT_C0, DBGTXT_LARGE);
		}
		while (ncl != NULL) {
			if (scrnTextNeedRefresh == 1) {
				debugTextPrint(textLayer, " ", 15, y, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_LARGE);
				debugTextPrint(textLayer, ncl->name, 33, y, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_LARGE);
			}
			ncl = ncl->next;
			y+=32;
		}
		graphicsDrawRectangle(0, 512, PSP2_DISPLAY_WIDTH, 32, 0xFF2F2F2F);
		graphicsDrawRectangle(15, (scrnClSel*32)+42, 930, 32, 0xFF2F2F2F);
		if (scrnTextNeedRefresh == 1) {
			/*debugTextPrint(textLayer, ">", 15, (scrnClSel*32)+42, DBGTXT_C2, DBGTXT_C0, DBGTXT_LARGE);*/
			debugTextPrint(textLayer, "Press X to select", 5, 522, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_SMALL);
		}
		scrnTextNeedRefresh = 0;
	}
	else if (scrn == 2) {
		struct _itemList *nil = ndItl;
		int y = 0;
		int s = 0;
		int skipSel = scrnIlSel;
		int skip = 0;
		while (skipSel >= 4) {
			++skip;
			skipSel -= 1;
		}
		if (scrnTextNeedRefresh == 1) {
			debugTextClear(textLayer);
		}
		while (nil != NULL) {
			if (skip > 0) {
				--skip;
			}
			else {
				if (s == scrnIlSel) {
					graphicsDrawRectangle(0, y, PSP2_DISPLAY_WIDTH, 128, 0xFF4F4F4F);
				}
				loadingIconTex->x = 64;
				loadingIconTex->y = y+64;
				noIconTex->x = 64;
				noIconTex->y = y+64;
				if (nil->iconStatus == 0 || nil->iconStatus == 1) {
					graphicsDrawTexture(loadingIconTex);
				}
				if (nil->iconStatus == 3) {
					graphicsDrawTexture(noIconTex);
				}
				if (nil->iconStatus == 2 || nil->iconStatus == 4) {
					if (nil->icon != NULL) {
						nil->icon->x = 64;
						nil->icon->y = y+64;
						graphicsDrawTexture(nil->icon);
					}
					else {
						graphicsDrawTexture(noIconTex);
						graphicsDrawRectangle(0, y, 128, 128, 0xFF000080);
						debugTextPrint(textLayer, "NULL", 28, y+48, DBGTXT_CC, DBGTXT_C0, DBGTXT_LARGE);
					}
				}
				if (scrnTextNeedRefresh == 1) {
					if (nil->displayName != NULL) {
						debugTextPrint(textLayer, nil->displayName, 130, y+2, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_LARGE);
					}
					else {
						debugTextPrint(textLayer, "Error: displayName is NULL.", 130, y+2, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_LARGE);
					}
					if (nil->author != NULL) {
						debugTextPrint(textLayer, nil->author, 130, y+42, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_LARGE);
					}
				}
				y+=128;
				if (y+128 > PSP2_DISPLAY_HEIGHT) {
					break;
				}
			}
			nil = nil->next;
			++s;
		}
		graphicsDrawRectangle(0, 512, PSP2_DISPLAY_WIDTH, 32, 0xFF2F2F2F);
		if (scrnTextNeedRefresh == 1) {
			debugTextPrint(textLayer, "Press X to select, O to go back", 5, 522, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_SMALL);
			debugTextPrintInt(textLayer, scrnIlSel, 850, 522, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_SMALL);
			debugTextPrintInt(textLayer, scrnIlMaxSel-1, 900, 522, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_SMALL);
		}
		scrnTextNeedRefresh = 0;
	}
	else if (scrn == 3) {
		struct _itemList *nil = ndItl;
		int i = 0;
		float r = 0.0f;
		while (nil != NULL) {
			if (i == scrnIlSel) {
				loadingIconTex->x = 64;
				loadingIconTex->y = 64;
				noIconTex->x = 64;
				noIconTex->y = 64;
				if (nil->iconStatus == 0 || nil->iconStatus == 1) {
					graphicsDrawTexture(loadingIconTex);
				}
				if (nil->iconStatus == 3) {
					graphicsDrawTexture(noIconTex);
				}
				if (nil->iconStatus == 2 || nil->iconStatus == 4) {
					if (nil->icon != NULL) {
						nil->icon->x = 64;
						nil->icon->y = 64;
						graphicsDrawTexture(nil->icon);
					}
					else {
						graphicsDrawTexture(noIconTex);
						graphicsDrawRectangle(0, 0, 128, 128, 0xFF000080);
						debugTextPrint(textLayer, "NULL", 28, 48, DBGTXT_CC, DBGTXT_C0, DBGTXT_LARGE);
					}
				}
				graphicsDrawRectangle(0, 128, PSP2_DISPLAY_WIDTH, 2, 0xFF808080);
				if (scrnTextNeedRefresh == 1) {
					debugTextPrint(textLayer, nil->displayName, 130, 2, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_LARGE);
					debugTextPrint(textLayer, nil->version, ((nil->displayNameLength+2)*18)+130, 2, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_LARGE);
					debugTextPrint(textLayer, nil->author, 130, 34, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_LARGE);
					if (nil->webUrl != NULL && nil->webUrlLength != 0) {
						debugTextPrint(textLayer, nil->webUrl, 130, 74, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_SMALL);
					}
					debugTextPrint(textLayer, "Downloads:", 130, 90, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_SMALL);
					debugTextPrintInt(textLayer, nil->dlcount, 284, 90, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_SMALL);
					debugTextPrint(textLayer, "Rating:", 130, 106, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_SMALL);
					if (nil->cachedrating == 0) {
						debugTextPrint(textLayer, "Not Available", 242, 106, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_SMALL);
					}
					if (nil->description == NULL || nil->descriptionLength == 0) {
						debugTextPrint(textLayer, "No description.", 2, 132, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_SMALL);
					}
					else {
						debugTextPrint(textLayer, nil->description, 2, 132, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_SMALL);
					}
					
					debugTextPrint(textLayer, "Press X to install, O to go back, Left/Right for next/previous", 5, 522, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_SMALL);
				}
				if (nil->cachedrating != 0) {
					/* Rating is on a scale of 1 to 2147483647 */
					r = (float)nil->cachedrating;
					r = (r/2147483647.0f)*98.0f;
					graphicsDrawRectangle(242, 106, 100, 16, 0xFF808080);
					graphicsDrawRectangle(243, 107, 98, 14, 0xFFC0C0C0);
					graphicsDrawRectangle(243, 107, (int)r, 14, 0xFF008000);
				}
				graphicsDrawRectangle(0, 512, PSP2_DISPLAY_WIDTH, 32, 0xFF2F2F2F);
				scrnTextNeedRefresh = 0;
				break;
			}
			nil = nil->next;
			++i;
		}
	}
	else if (scrn == 4) {
		float r = 0.0f;
		int b = 0xFF000000;
		int y = 542;
		if (installStatus == 0 && scrnTextNeedRefresh == 1) {
			debugTextClear(textLayer);
			debugTextPrint(textLayer, "Downloading", 381, 264, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_LARGE);
		}
		else if (installStatus == 1 && scrnTextNeedRefresh == 1) {
			debugTextClear(textLayer);
			debugTextPrint(textLayer, "Installing", 390, 264, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_LARGE);
		}
		else if (installStatus == 2 && scrnTextNeedRefresh == 1) {
			debugTextClear(textLayer);
			debugTextPrint(textLayer, "Finished", 408, 264, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_LARGE);
		}
		graphicsDrawRectangle(0, 0, 960, 544, 0xFFFF0000);
		while (y > 34) {
			graphicsDrawRectangle(0, y, 960, 2, b);
			b += 0x00010000;
			y -= 2;
		}
		r = (float)installPercent;
		r = (r/(float)installPercentMax)*198.0f;
		graphicsDrawRectangle(380, 298, 200, 16, 0xFF808080);
		graphicsDrawRectangle(381, 299, 198, 14, 0xFFC0C0C0);
		graphicsDrawRectangle(381, 299, (int)r, 14, 0xFF008000);
		scrnTextNeedRefresh = 0;
	}
	graphicsDrawTexture(textLayerTex);
	if (dialogDisplay) {
		int i = 0;
		size_t lc = 0;
		char l = 0;
		char dialogLine[33];
		memset(&dialogLine, 0, 33);
		if (dialogType == 2 || dialogType == 3) {
			graphicsDrawRectangle(180, 122, 600, 300, 0xFF000080);
			graphicsDrawRectangle(185, 127, 590, 290, 0xFF4F4F4F);
			debugTextPrint(dialogText, "WARNING", 188, 128, DBGTXT_CC, DBGTXT_C_CLEAR, DBGTXT_LARGE);
		}
		else {
			graphicsDrawRectangle(180, 122, 600, 300, 0xFF808080);
			graphicsDrawRectangle(185, 127, 590, 290, 0xFF4F4F4F);
			debugTextPrint(dialogText, "MESSAGE", 188, 128, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_LARGE);
		}
		while (i < strlen(dialogContents)+1) {
			if (dialogContents[i] == '\n' || lc >= 32 || i == strlen(dialogContents)) {
				debugTextPrint(dialogText, dialogLine, 188, (l*32)+160, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_LARGE);
				memset(&dialogLine, 0, 33);
				lc = 0;
				++l;
				if (l > 6) {
					break;
				}
				if (dialogContents[i] == '\n') {
					++i;
				}
			}
			else {
				dialogLine[lc] = dialogContents[i];
				++lc;
				++i;
			}
		}
		if (dialogType == 1 || dialogType == 3) {
			debugTextPrint(dialogText, "X: Yes", 185, 389, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_LARGE);
			debugTextPrint(dialogText, "O: No", 675, 389, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_LARGE);
		}
		else {
			debugTextPrint(dialogText, "X: OK", 185, 389, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_LARGE);
		}
		graphicsDrawTexture(dialogTextTex);
	}
	if (showLoadingTex) {
		graphicsDrawTexture(loadingTex);
	}
	if (showDbgScreen) {
		graphicsDrawTexture(dbgScrnTex);
	}
	return 0;
}

int eventButtonDown(int button) {
	if (button == SCE_CTRL_START && scrn != 4) {
		breakLoop = 1;
	}
	if (button == SCE_CTRL_LTRIGGER) {
		showDbgScreen ^= 1;
	}
	if (dialogDisplay) {
		if (button == SCE_CTRL_CROSS) {
			if (dialogFreeContents == 1 && dialogContents != NULL) {
				free(dialogContents);
			}
			dialogResult = 0;
			dialogDisplay = 0;
			dialogFreeContents = 0;
			dialogContents = NULL;
			debugTextClear(dialogText);
			if (scrn == 3) {
				struct _itemList *nil = ndItl;
				int i = 0;
				while (nil != NULL) {
					if (i == scrnIlSel) {
						downloadPackage(nil->id);
					}
					nil = nil->next;
					++i;
				}
			}
		}
		if (button == SCE_CTRL_CIRCLE) {
			if (dialogType == 1 || dialogType == 3) {
				if (dialogFreeContents == 1 && dialogContents != NULL) {
					free(dialogContents);
				}
				dialogResult = 1;
				dialogDisplay = 0;
				dialogFreeContents = 0;
				dialogContents = NULL;
				debugTextClear(dialogText);
			}
		}
	}
	else {
		if (button == SCE_CTRL_UP) {
			if (scrn == 1 && waitingOnData == 0) {
				if (scrnClSel > 0) {
					--scrnClSel;
					scrnTextNeedRefresh = 1;
				}
			}
			else if (scrn == 2 && waitingOnData == 0) {
				if (scrnIlSel > 0) {
					--scrnIlSel;
					scrnTextNeedRefresh = 1;
				}
			}
		}
		if (button == SCE_CTRL_DOWN) {
			if (scrn == 1 && waitingOnData == 0) {
				if (scrnClSel < scrnClMaxSel-1) {
					++scrnClSel;
					scrnTextNeedRefresh = 1;
				}
			}
			else if (scrn == 2 && waitingOnData == 0) {
				if (scrnIlSel < scrnIlMaxSel-1) {
					++scrnIlSel;
					scrnTextNeedRefresh = 1;
				}
			}
		}
		if (button == SCE_CTRL_CROSS) {
			if (scrn == 1 && waitingOnData == 0) {
				struct _categoryList *ncl = ndCal;
				char i = 0;
				char id[2];
				while (ncl != NULL) {
					if (i == scrnClSel) {
						waitingOnData = 1;
						showLoadingTex = 1;
						id[0] = ncl->id;
						netSendData(3, 3, &id[0], 1);
						break;
					}
					ncl = ncl->next;
					++i;
				}
			}
			if (scrn == 2 && waitingOnData == 0) {
				struct _itemList *nil = ndItl;
				int netInt = 0;
				char netIntChar[4];
				int i = 0;
				while (nil != NULL) {
					if (i == scrnIlSel) {
						netInt = sceNetHtonl(nil->id);
						memcpy(&netIntChar[0], &netInt, 4);
						netSendData(3, 5, &netIntChar[0], 4);
						waitingOnData = 1;
						showLoadingTex = 1;
						break;
					}
					nil = nil->next;
					++i;
				}
			}
			if (scrn == 3 && waitingOnData == 0) {
				struct _itemList *nil = ndItl;
				int i = 0;
				while (nil != NULL) {
					if (i == scrnIlSel) {
						if (nil->isSafeHomebrew == 0) {
							if (dialogDisplay != 0 && dialogFreeContents == 1 && dialogContents != NULL) {
								free(dialogContents);
							}
							dialogDisplay = 1;
							dialogType = 3;
							dialogFreeContents = 0;
							dialogContents = "It is unknown if this file\n"
								"requires extended permissions.\n\n"
								"Continue with install?";
						}
						else if (nil->isSafeHomebrew == 1) {
							if (dialogDisplay != 0 && dialogFreeContents == 1 && dialogContents != NULL) {
								free(dialogContents);
							}
							dialogDisplay = 1;
							dialogType = 3;
							dialogFreeContents = 0;
							dialogContents = "This file requires extended\npermissions.\n\n"
								"Continue with install?";
						}
						else {
							downloadPackage(nil->id);
						}
						break;
					}
					nil = nil->next;
					++i;
				}
			}
		}
		if (button == SCE_CTRL_CIRCLE) {
			if (scrn == 2 && waitingOnData == 0) {
				debugTextClear(textLayer);
				scrn = 1;
				scrnTextNeedRefresh = 1;
			}
			else if (scrn == 3 && waitingOnData == 0) {
				debugTextClear(textLayer);
				scrn = 2;
				scrnTextNeedRefresh = 1;
			}
		}
		if (button == SCE_CTRL_LEFT) {
			if (scrn == 3 && waitingOnData == 0) {
				struct _itemList *nil = ndItl;
				int netInt = 0;
				char netIntChar[4];
				int i = 0;
				if (scrnIlSel > 0) {
					--scrnIlSel;
					while (nil != NULL) {
						if (i == scrnIlSel) {
							netInt = sceNetHtonl(nil->id);
							memcpy(&netIntChar[0], &netInt, 4);
							netSendData(3, 5, &netIntChar[0], 4);
							waitingOnData = 1;
							showLoadingTex = 1;
							break;
						}
						nil = nil->next;
						++i;
					}
				}
			}
		}
		if (button == SCE_CTRL_RIGHT) {
			if (scrn == 3 && waitingOnData == 0) {
				struct _itemList *nil = ndItl;
				int netInt = 0;
				char netIntChar[4];
				int i = 0;
				if (scrnIlSel < scrnIlMaxSel-1) {
					++scrnIlSel;
					while (nil != NULL) {
						if (i == scrnIlSel) {
							netInt = sceNetHtonl(nil->id);
							memcpy(&netIntChar[0], &netInt, 4);
							netSendData(3, 5, &netIntChar[0], 4);
							waitingOnData = 1;
							showLoadingTex = 1;
							break;
						}
						nil = nil->next;
						++i;
					}
				}
			}
		}
	}
	return 0;
}

int eventButtonUp(int button) {
	return 0;
}

int eventAnalog(char lr, unsigned char x, unsigned char y) {
	return 0;
}

int eventTouch(char side, int x, int y) {
	if (side == 0 && scrn == 1 && waitingOnData == 0) {
		int ychk = 42;
		int i = 0;
		for (; i < scrnClMaxSel; i++) {
			if ((y > ychk && y < ychk+32) && scrnClSel != i) {
				scrnClSel = i;
				scrnTextNeedRefresh = 1;
			}
			ychk += 32;
		}
	}
	if (side == 1 && scrn == 3 && waitingOnData == 0) {
		if (x < PSP2_DISPLAY_WIDTH/2) {
			eventButtonDown(SCE_CTRL_LEFT);
		}
		else if (x >= PSP2_DISPLAY_WIDTH/2) {
			eventButtonDown(SCE_CTRL_RIGHT);
		}
	}
	return 0;
}

int eventNetworkConnect(void) {
	awakeTick = sceKernelGetProcessTimeWide();
	netConnectFinished = 1;
	return 0;
}

int eventNetworkRecv(char *data, size_t len) {
	size_t i = 0;
	unsigned int tmpint = 0;
	
	while (i < len) {
		if (netinf.dlSet == 0) {
			memcpy(&tmpint, data+i, sizeof(tmpint));
			netinf.dataLength = sceNetNtohl(tmpint);
			if (netinf.dataLength > sizeof(netinf.buffer)) {
				debugMessage("Data too big!");
				debugPrintInt(sizeof(netinf.buffer));
				debugPrintInt(netinf.dataLength);
				debugPrintInt(tmpint);
				breakLoop = 1;
				break;
			}
			
			if (i+netinf.dataLength > len) {
				memcpy(netinf.buffer, data+i, len-i);
				netinf.dlSet = 1;
				netinf.bufpos = len-i;
				break;
			}
			else {
				memcpy(netinf.buffer, data+i, netinf.dataLength);
				eventNetworkMsg(netinf.buffer[4], netinf.buffer[5], &netinf.buffer[6], netinf.dataLength-6);
				i += netinf.dataLength;
			}
		}
		else {
			if (netinf.dataLength-netinf.bufpos > len) {
				memcpy(netinf.buffer+netinf.bufpos, data+i, len-i);
				netinf.bufpos += len-i;
				break;
			}
			else {
				memcpy(netinf.buffer+netinf.bufpos, data+i, netinf.dataLength-netinf.bufpos);
				eventNetworkMsg(netinf.buffer[4], netinf.buffer[5], &netinf.buffer[6], netinf.dataLength-6);
				i += netinf.dataLength-netinf.bufpos;
				netinf.bufpos = 0;
				netinf.dlSet = 0;
			}
		}
	}
	return 0;
}

int eventNetworkMsg(char ev1, char ev2, char *data, size_t len) {
	size_t pos = 0;
	
	if (ev1 == 2) {
		if (ev2 == 2) { /* Ping Reply */
			/* Do nothing */
		}
		else if (ev2 == 3) { /* Server Debug Message */
			unsigned short strLen = 0;
			char *str;
			memcpy(&strLen, data, sizeof(strLen));
			strLen = sceNetNtohs(strLen);
			str = (char*)calloc(1, strLen+1);
			memcpy(str, data+2, strLen);
			debugMessage(str);
			if (dialogDisplay != 0 && dialogFreeContents == 1 && dialogContents != NULL) {
				free(dialogContents);
			}
			dialogDisplay = 1;
			dialogType = 0;
			dialogFreeContents = 1;
			dialogContents = str;
		}
		else if (ev2 == 4) { /* Server Error Message */
			unsigned short strLen = 0;
			char *str;
			memcpy(&strLen, data, sizeof(strLen));
			strLen = sceNetNtohs(strLen);
			str = (char*)calloc(1, strLen+1);
			memcpy(str, data+2, strLen);
			debugMessage(str);
			if (dialogDisplay != 0 && dialogFreeContents == 1 && dialogContents != NULL) {
				free(dialogContents);
			}
			dialogDisplay = 1;
			dialogType = 2;
			dialogFreeContents = 1;
			dialogContents = str;
		}
		else if (ev2 == 5) { /* Server Message */
			unsigned short strLen = 0;
			char *str;
			memcpy(&strLen, data, sizeof(strLen));
			strLen = sceNetNtohs(strLen);
			str = (char*)calloc(1, strLen+1);
			memcpy(str, data+2, strLen);
			debugMessage(str);
			if (dialogDisplay != 0 && dialogFreeContents == 1 && dialogContents != NULL) {
				free(dialogContents);
			}
			dialogDisplay = 1;
			dialogType = 0;
			dialogFreeContents = 1;
			dialogContents = str;
		}
		else if (ev2 == 6) { /* Server Shutdown/Reboot Soon */
		}
		else if (ev2 == 7) { /* IP address Is Banned */
		}
		else if (ev2 == 8) { /* Update Required */
		}
		else {
			debugMessage("Unknown network data:");
			debugPrintInt(ev1);
			debugPrintInt(ev2);
			debugPrintHex(data, len);
		}
	}
	else if (ev1 == 3) { /* Menu */
		if (ev2 == 2) { /* Category List */
			int count;
			char category;
			unsigned short strLen;
			char *str;
			struct _categoryList *prv = NULL;
			struct _categoryList *ncl;
			if (ndCal == NULL) {
				memcpy(&count, data+pos, sizeof(count));
				count = sceNetNtohl(count);
				pos += 4;
				while (count > 0) {
					memcpy(&category, data+pos, sizeof(category));
					++pos;
					memcpy(&strLen, data+pos, sizeof(strLen));
					strLen = sceNetNtohs(strLen);
					str = (char*)calloc(1, strLen+1);
					if (str == NULL) {
						debugMessage("ERROR: calloc returned NULL.");
					}
					memcpy(str, data+pos+2, strLen);
					pos += strLen+2;
					--count;
					
					ncl = (struct _categoryList*)calloc(1, sizeof(struct _categoryList));
					if (ncl == NULL) {
						debugMessage("ERROR: ncl is NULL.");
					}
					else {
						++scrnClMaxSel;
						ncl->id = category;
						ncl->name = str;
						ncl->nameLength = strLen;
						ncl->next = NULL;
						
						if (ndCal == NULL) {
							ndCal = ncl;
						}
						if (prv != NULL) {
							prv->next = ncl;
						}
						prv = ncl;
					}
				}
			}
			waitingOnData = 0;
			showLoadingTex = 0;
			if (scrn == 0) {
				debugTextClear(textLayer);
				scrnTextNeedRefresh = 1;
				scrn = 1;
			}
		}
		else if (ev2 == 3) { /* Items List */
			int count;
			unsigned int id;
			unsigned short titleIdLen;
			char *titleId;
			unsigned int date;
			unsigned short displayNameLen;
			char *displayName;
			unsigned short authorLen;
			char *author;
			unsigned short versionLen;
			char *version;
			unsigned int cachedRating;
			unsigned short extraLen;
			char *extra;
			struct _itemList *prv = NULL;
			struct _itemList *nil;
			
			clearNdItl();
			
			memcpy(&count, data+pos, sizeof(count));
			count = sceNetNtohl(count);
			pos += 4;
			while (count > 0) {
				memcpy(&id, data+pos, sizeof(id));
				id = sceNetNtohl(id);
				pos+=4;
				
				memcpy(&titleIdLen, data+pos, sizeof(titleIdLen));
				titleIdLen = sceNetNtohs(titleIdLen);
				titleId = (char*)calloc(1, titleIdLen+1);
				if (titleId == NULL) {
					debugMessage("ERROR: calloc returned NULL [1].");
				}
				memcpy(titleId, data+pos+2, titleIdLen);
				pos += titleIdLen+2;
				
				memcpy(&date, data+pos, sizeof(date));
				date = sceNetNtohl(date);
				pos+=4;
				
				memcpy(&displayNameLen, data+pos, sizeof(displayNameLen));
				displayNameLen = sceNetNtohs(displayNameLen);
				displayName = (char*)calloc(1, displayNameLen+1);
				if (displayName == NULL) {
					debugMessage("ERROR: calloc returned NULL [2].");
				}
				memcpy(displayName, data+pos+2, displayNameLen);
				pos += displayNameLen+2;
				
				memcpy(&authorLen, data+pos, sizeof(authorLen));
				authorLen = sceNetNtohs(authorLen);
				author = (char*)calloc(1, authorLen+1);
				if (author == NULL) {
					debugMessage("ERROR: calloc returned NULL [3].");
				}
				memcpy(author, data+pos+2, authorLen);
				pos += authorLen+2;
				
				memcpy(&versionLen, data+pos, sizeof(versionLen));
				versionLen = sceNetNtohs(versionLen);
				version = (char*)calloc(1, versionLen+1);
				if (version == NULL) {
					debugMessage("ERROR: calloc returned NULL [4].");
				}
				memcpy(version, data+pos+2, versionLen);
				pos += versionLen+2;
				
				memcpy(&cachedRating, data+pos, sizeof(cachedRating));
				cachedRating = sceNetNtohl(cachedRating);
				pos+=4;
				
				memcpy(&extraLen, data+pos, sizeof(extraLen));
				extraLen = sceNetNtohs(extraLen);
				extra = (char*)calloc(1, extraLen+1);
				if (extra == NULL) {
					debugMessage("ERROR: calloc returned NULL [5].");
				}
				memcpy(extra, data+pos+2, extraLen);
				pos += extraLen+2;
				
				--count;
				
				nil = (struct _itemList*)calloc(1, sizeof(struct _itemList));
				if (nil == NULL) {
					debugMessage("ERROR: nil is NULL.");
				}
				else {
					++scrnIlMaxSel;

					nil->id = id;
					nil->titleId = titleId;
					nil->titleIdLength = titleIdLen;
					nil->date = date;
					nil->category = -1;
					nil->displayName = displayName;
					nil->displayNameLength = displayNameLen;
					nil->author = author;
					nil->authorLength = authorLen;
					nil->version = version;
					nil->versionLength = versionLen;
					nil->webUrl = NULL;
					nil->webUrlLength = 0;
					nil->description = NULL;
					nil->descriptionLength = 0;
					nil->dlcount = 0;
					nil->cachedrating = cachedRating;
					nil->isSafeHomebrew = 0;
					nil->extra = extra;
					nil->extraLength = extraLen;
					nil->iconStatus = 0;
					nil->icon = NULL;
					
					if (ndItl == NULL) {
						ndItl = nil;
					}
					if (prv != NULL) {
						prv->next = nil;
					}
					prv = nil;
				}
			}
			
			waitingOnData = 0;
			showLoadingTex = 0;
			if (scrn == 1) {
				debugTextClear(textLayer);
				scrnTextNeedRefresh = 1;
				scrn = 2;
			}
		}
		else if (ev2 == 4) { /* Item Icon */
			struct _itemList *nil = ndItl;
			void *iconData;
			unsigned int id;
			unsigned int dataLen;
			memcpy(&id, data+pos, sizeof(id));
			id = sceNetNtohl(id);
			pos += 4;
			memcpy(&dataLen, data+pos, sizeof(dataLen));
			dataLen = sceNetNtohl(dataLen);
			pos += 4;
			if (dataLen == 65536) {
				while (nil != NULL) {
					if (nil->id == id) {
						if (nil->icon != NULL) {
							graphicsFreeTexture(nil->icon);
							nil->icon = NULL;
						}
						nil->icon = graphicsCreateTexture(128, 128);
						iconData = graphicsTextureGetData(nil->icon);
						memset(iconData, 0xFF, 128*128*4);
						memcpy(iconData, data+pos, 128*128*4);
						nil->iconStatus = 2;
						break;
					}
					nil = nil->next;
				}
			}
			else if (dataLen == 0) {
				while (nil != NULL) {
					if (nil->id == id) {
						if (nil->icon != NULL) {
							graphicsFreeTexture(nil->icon);
							nil->icon = NULL;
						}
						nil->iconStatus = 3;
						break;
					}
					nil = nil->next;
				}
			}
			else {
				debugMessage("Icon data length wrong.");
			}
		}
		else if (ev2 == 5) { /* Item Details */
			struct _itemList *nil = ndItl;
			unsigned int id = 0;
			char category = 0;
			unsigned short webUrlLength = 0;
			char *webUrl = NULL;
			unsigned int descriptionLength = 0;
			char *description = NULL;
			unsigned int dlCount = 0;
			unsigned char isSafeHomebrew = 0;
			
			memcpy(&id, data+pos, sizeof(id));
			id = sceNetNtohl(id);
			pos += 4;
			memcpy(&category, data+pos, sizeof(category));
			pos += 1;
			
			memcpy(&webUrlLength, data+pos, sizeof(webUrlLength));
			webUrlLength = sceNetNtohs(webUrlLength);
			webUrl = (char*)calloc(1, webUrlLength+1);
			if (webUrl == NULL) {
				debugMessage("ERROR: calloc returned NULL.");
			}
			memcpy(webUrl, data+pos+2, webUrlLength);
			pos += webUrlLength+2;
			
			memcpy(&descriptionLength, data+pos, sizeof(descriptionLength));
			descriptionLength = sceNetNtohl(descriptionLength);
			description = (char*)calloc(1, descriptionLength+1);
			if (description == NULL) {
				debugMessage("ERROR: calloc returned NULL.");
			}
			memcpy(description, data+pos+4, descriptionLength);
			pos += descriptionLength+4;
			
			memcpy(&dlCount, data+pos, sizeof(dlCount));
			dlCount = sceNetNtohl(dlCount);
			pos += 4;
			memcpy(&isSafeHomebrew, data+pos, sizeof(isSafeHomebrew));
			pos += 1;
			
			while (nil != NULL) {
				if (nil->id == id) {
					nil->category = category;
					nil->webUrlLength = webUrlLength;
					nil->webUrl = webUrl;
					nil->descriptionLength = descriptionLength;
					nil->description = description;
					nil->dlcount = dlCount;
					nil->isSafeHomebrew = isSafeHomebrew;
				}
				nil = nil->next;
			}
			
			waitingOnData = 0;
			showLoadingTex = 0;
			debugTextClear(textLayer);
			scrnTextNeedRefresh = 1;
			scrn = 3;		
		}
		else {
			debugMessage("Unknown network data:");
			debugPrintInt(ev1);
			debugPrintInt(ev2);
			debugPrintHex(data, len);
		}
	}
	else if (ev1 == 4) { /* File */
		if (ev2 == 2) { /* Package Information */
			struct _packageInfo *pi;
			struct _fileInfoList *fil;
			struct _fileInfoList *fprv = NULL;
			unsigned int itemID;
			char pkgType;
			unsigned int count;
			
			unsigned int fileID;
			unsigned int fileDataLength;
			unsigned short fileNameLength;
			char *fileName;
			unsigned int fileCRC;
			char fileIsDir;
			
			SceUID mkfile;
			
			preparePtmp();
			
			memcpy(&itemID, data+pos, sizeof(itemID));
			itemID = sceNetNtohl(itemID);
			pos += 4;
			memcpy(&pkgType, data+pos, sizeof(pkgType));
			pos += 1;
			memcpy(&count, data+pos, sizeof(count));
			count = sceNetNtohl(count);
			pos += 4;
			
			if (itemID != instalItemID) {
				debugMessage("ID DOES NOT MATCH!");
			}
			
			pi = (struct _packageInfo*)calloc(1, sizeof(struct _packageInfo));
			if (pi == NULL) {
				debugMessage("ERROR: pi is NULL.");
			}
			else {
				pi->itemID = itemID;
				pi->pkgType = pkgType;
				pi->fileCount = 0;
				pi->files = NULL;
			}
			installPercentMax = 0;
			
			while (count > 0) {
				memcpy(&fileID, data+pos, sizeof(fileID));
				fileID = sceNetNtohl(fileID);
				pos += 4;
				memcpy(&fileDataLength, data+pos, sizeof(fileDataLength));
				fileDataLength = sceNetNtohl(fileDataLength);
				pos += 4;
				
				memcpy(&fileNameLength, data+pos, sizeof(fileNameLength));
				fileNameLength = sceNetNtohs(fileNameLength);
				fileName = (char*)calloc(1, fileNameLength+14+1);
				if (fileName == NULL) {
					debugMessage("ERROR: calloc returned NULL.");
				}
				memcpy(fileName, "ux0:/ptmp/pkg/", 14);
				memcpy(fileName+14, data+pos+2, fileNameLength);
				pos += fileNameLength+2;
				
				memcpy(&fileCRC, data+pos, sizeof(fileCRC));
				fileCRC = sceNetNtohl(fileCRC);
				pos += 4;
				
				memcpy(&fileIsDir, data+pos, sizeof(fileIsDir));
				pos += 1;

				if (fileIsDir == 0) {
					fil = (struct _fileInfoList*)calloc(1, sizeof(struct _fileInfoList));
					if (fil == NULL) {
						debugMessage("ERROR: fil is NULL.");
					}
					else {
						++installPercentMax;
						fil->fileID = fileID;
						fil->dataLength = fileDataLength;
						fil->nameLength = fileNameLength;
						fil->name = fileName;
						fil->crc = fileCRC;
						fil->file = 0;
						if (pi->files == NULL) {
							pi->files = fil;
						}
						if (fprv != NULL) {
							fprv->next = fil;
						}
						fprv = fil;
					}

				}
				else {
					int i = 15;
					char prvc;
					while (i <= fileNameLength+14) {
						if (fileName[i] == '/' || fileName[i] == '\\') {
							prvc = fileName[i+1];
							fileName[i+1] = 0;
							mkfile = sceIoMkdir(fileName, 0777);
							if (mkfile < 0 && mkfile != 0x80010011) {
								debugMessage("Error on sceIoMkdir, returned:");
								debugPrintInt(mkfile);
							}
							fileName[i+1] = prvc;
						}
						++i;
					}
					free(fileName);
				}
				--count;
			}
			
			installPkgInfo = pi;
			installCurFile = pi->files;
			waitingOnData = 0;
		}
		else if (ev2 == 3) { /* File Start */
			unsigned int itemID;
			unsigned int fileID;
			unsigned int chunkCount;
			memcpy(&itemID, data+pos, sizeof(itemID));
			itemID = sceNetNtohl(itemID);
			pos += 4;
			memcpy(&fileID, data+pos, sizeof(fileID));
			fileID = sceNetNtohl(fileID);
			pos += 4;
			memcpy(&chunkCount, data+pos, sizeof(chunkCount));
			chunkCount = sceNetNtohl(chunkCount);
			pos += 4;
			if (instalItemID != itemID) {
				debugMessage("itemID DOES NOT MATCH! [File Start]");
			}
			if (installCurFile->fileID != fileID) {
				debugMessage("fileID DOES NOT MATCH! [File Start]");
			}
			installCurFile->file = sceIoOpen(installCurFile->name, SCE_O_WRONLY|SCE_O_CREAT, 0777);
			if (installCurFile->file < 0) {
				debugMessage("Error on sceIoOpen, returned:");
				debugPrintInt(installCurFile->file);
			}
			installExpectedChunkCount = chunkCount;
			installChunkCount = 0;
		}
		else if (ev2 == 4) { /* File Data */
			unsigned int itemID;
			unsigned int fileID;
			unsigned int chunkNum;
			unsigned int dataLen;
			++installChunkCount;
			memcpy(&itemID, data+pos, sizeof(itemID));
			itemID = sceNetNtohl(itemID);
			pos += 4;
			memcpy(&fileID, data+pos, sizeof(fileID));
			fileID = sceNetNtohl(fileID);
			pos += 4;
			memcpy(&chunkNum, data+pos, sizeof(chunkNum));
			chunkNum = sceNetNtohl(chunkNum);
			pos += 4;
			memcpy(&dataLen, data+pos, sizeof(dataLen));
			dataLen = sceNetNtohl(dataLen);
			pos += 4;
			if (instalItemID != itemID) {
				debugMessage("itemID DOES NOT MATCH! [File Data]");
			}
			if (installCurFile->fileID != fileID) {
				debugMessage("fileID DOES NOT MATCH! [File Data]");
			}
			if (installChunkCount != chunkNum) {
				debugMessage("chunkNum DOES NOT MATCH! [File Data]");
			}
			sceIoWrite(installCurFile->file, data+pos, dataLen);
		}
		else if (ev2 == 5) { /* File End */
			unsigned int itemID;
			unsigned int fileID;
			++installPercent;
			memcpy(&itemID, data+pos, sizeof(itemID));
			itemID = sceNetNtohl(itemID);
			pos += 4;
			memcpy(&fileID, data+pos, sizeof(fileID));
			fileID = sceNetNtohl(fileID);
			pos += 4;
			if (instalItemID != itemID) {
				debugMessage("itemID DOES NOT MATCH! [File End]");
			}
			if (installCurFile->fileID != fileID) {
				debugMessage("fileID DOES NOT MATCH! [File End]");
			}
			if (installChunkCount != installExpectedChunkCount) {
				debugMessage("installChunkCount DOES NOT MATCH! [File End]");
			}
			sceIoClose(installCurFile->file);
			installCurFile = installCurFile->next;
			if (installCurFile == NULL) {
				installPercent = 0;
				installPercentMax = 1;
				installStatus = 1;
				scrnTextNeedRefresh = 1;
			}
			waitingOnData = 0;
		}
		else {
			debugMessage("Unknown network data:");
			debugPrintInt(ev1);
			debugPrintInt(ev2);
			debugPrintHex(data, len);
		}
	}
	else {
		debugMessage("Unknown network data:");
		debugPrintInt(ev1);
		debugPrintInt(ev2);
		debugPrintHex(data, len);
	}
	return 0;
}

int eventNetworkMisc(int ev) {
	return 0;
}

int eventNetworkDisconnect(void) {
	return 0;
}
