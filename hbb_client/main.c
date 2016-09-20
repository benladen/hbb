#include "main.h"

struct _delTreeDirsFileList {
	char *path;
	int depth;
	struct _delTreeDirsFileList *next;
};

int debugSocketFD;
int dbgNetConnected = 0;
SceUID logFile = -1;
int dbgPTSypos = 0;
struct dbgText *dbgScrn = NULL;
struct _delTreeDirsFileList *_dtdfl = NULL;

static void _deleteDirectoryTreeFolders(char*, int);

void initDebugConnection(void) {
	SceNetSockaddrIn sockAddrIn;
	int i = 0;
    debugSocketFD = sceNetSocket("SOCKET_DEBUG_MESSAGES", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, SCE_NET_IPPROTO_TCP);
    if (debugSocketFD < 0) {
        dbgNetConnected = 0;
    }
    else {
        memset(&sockAddrIn, 0, sizeof(sockAddrIn));
        sockAddrIn.sin_family = SCE_NET_AF_INET;
        sockAddrIn.sin_port = sceNetHtons(9877);
        sceNetInetPton(SCE_NET_AF_INET, "10.0.0.5", &sockAddrIn.sin_addr);
		i = sceNetConnect(debugSocketFD, (SceNetSockaddr*)&sockAddrIn, sizeof(sockAddrIn));
        if (i < 0) {
			debugMessage("sceNetConnect failed:");
			debugPrintInt(i);
            dbgNetConnected = 0;
            sceNetSocketClose(debugSocketFD);
        }
        else {
            dbgNetConnected = 1;
			debugMessage("Debugging connection open.");
        }
    }
}

void debugMessage(char *text) {
	int rtn;
	if (dbgScrn != NULL) {
		debugTextPrint(dbgScrn, /* This could probably be done better. */
						"                                  "
						"                                  "
						"                                  "
						"                                   ", 
			0, dbgPTSypos, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_TINY);
		rtn = debugTextPrint(dbgScrn, text, 0, dbgPTSypos, DBGTXT_C7, DBGTXT_C_CLEAR, DBGTXT_TINY);
		if (rtn == 0 || rtn == 2 || rtn == 3 || rtn == 5) {
			/* rtn 2, 3, 5: Line too wide, text cut. */
			dbgPTSypos+=8;
			if (dbgPTSypos > PSP2_DISPLAY_HEIGHT-6) {
				dbgPTSypos = 0;
			}
		}
	}
	if (logFile >= 0) {
		sceIoWrite(logFile, text, strlen(text));
		sceIoWrite(logFile, "\r\n", 2);
	}
	if (dbgNetConnected == 1) {
		sceNetSend(debugSocketFD, text, strlen(text), SCE_NET_MSG_DONTWAIT);
		sceNetSend(debugSocketFD, "\x00", 1, SCE_NET_MSG_DONTWAIT);
	}
}

void debugPrintInt(int i) {
	char tmpStr[15];
	sprintf(tmpStr, "%d", i);
	debugMessage(tmpStr);
}

void debugPrintHex(char *data, size_t length) {
	char *temp = (char*)calloc((length*2)+1, 1);
	int i = 0;
	for (; i < length; i++) {
		sprintf(temp+i*2, "%02X", data[i]);
	}
	debugMessage(temp);
	free(temp);
}

int sleep(unsigned int sec) {
	sceKernelDelayThread(sec*1000*1000);
	return 0;
}

void deleteDirectoryTreeFiles(char *path, int depth) {
	char *subPath;
	char *tmpText;
	SceUID dirFd = sceIoDopen(path);
	if (dirFd >= 0) {
		SceIoDirent dirent;
		memset(&dirent, 0, sizeof(dirent));
		while (sceIoDread(dirFd, &dirent) > 0) {
			tmpText = (char*)calloc(strlen(path)+strlen(dirent.d_name)+7, 1);
			if (tmpText == NULL) {
				/* Error */
			}
			if (((dirent.d_stat.st_mode) & SCE_S_IFMT) != SCE_S_IFDIR) {
				sprintf(tmpText, "%s/%s", path, dirent.d_name);
				sceIoRemove(tmpText);
			}
			free(tmpText);
			
			if (((dirent.d_stat.st_mode) & SCE_S_IFMT) == SCE_S_IFDIR) {
				subPath = (char*)calloc(strlen(path)+strlen(dirent.d_name)+2, 1);
				if (subPath == NULL) {
					/* Error */
				}
				sprintf(subPath, "%s/%s", path, dirent.d_name);
				deleteDirectoryTreeFiles(subPath, depth+1);
				free(subPath);
			}
			memset(&dirent, 0, sizeof(dirent));
		}
		sceIoDclose(dirFd);
	}
	else {
		debugMessage("Error on sceIoDopen, returned:");
		debugPrintInt(dirFd);
	}
}

void deleteDirectoryTreeFolders(char *path) {
	struct _delTreeDirsFileList *dtdfl;
	struct _delTreeDirsFileList *prv = NULL;
	int highestDepth = 0;
	_deleteDirectoryTreeFolders(path, 0);
	dtdfl = _dtdfl;
	while (dtdfl != NULL) {
		if (dtdfl->depth > highestDepth) {
			highestDepth = dtdfl->depth;
		}
		dtdfl = dtdfl->next;
	}
	while (highestDepth >= 0) {
		dtdfl = _dtdfl;
		while (dtdfl != NULL) {
			if (dtdfl->depth == highestDepth) {
				sceIoRmdir(dtdfl->path);
			}
			dtdfl = dtdfl->next;
		}
		--highestDepth;
	}
	dtdfl = _dtdfl;
	while (dtdfl != NULL) {
		if (prv != NULL) {
			free(prv->path);
			free(prv);
		}
		prv = dtdfl;
		dtdfl = dtdfl->next;
	}
	if (prv != NULL) {
		free(prv->path);
		free(prv);
	}
	_dtdfl = NULL;
}

static void _deleteDirectoryTreeFolders(char *path, int depth) {
	struct _delTreeDirsFileList *dtdfl = _dtdfl; 
	struct _delTreeDirsFileList *ndtdfl = NULL; 
	char *subPath;
	char *tmpText;
	SceUID dirFd = sceIoDopen(path);
	if (dirFd >= 0) {
		SceIoDirent dirent;
		memset(&dirent, 0, sizeof(dirent));
		while (sceIoDread(dirFd, &dirent) > 0) {
			tmpText = (char*)calloc(strlen(path)+strlen(dirent.d_name)+7, 1);
			if (tmpText == NULL) {
				/* Error */
			}
			if (((dirent.d_stat.st_mode) & SCE_S_IFMT) == SCE_S_IFDIR) {
				sprintf(tmpText, "%s/%s", path, dirent.d_name);
				dtdfl = _dtdfl;
				if (dtdfl == NULL) {
					ndtdfl = (struct _delTreeDirsFileList*)calloc(1, sizeof(struct _delTreeDirsFileList));
					ndtdfl->path = tmpText;
					ndtdfl->depth = depth;
					_dtdfl = ndtdfl;
				}
				else {
					while (dtdfl != NULL) {
						if (dtdfl->next == NULL) {
							ndtdfl = (struct _delTreeDirsFileList*)calloc(1, sizeof(struct _delTreeDirsFileList));
							ndtdfl->path = tmpText;
							ndtdfl->depth = depth;
							dtdfl->next = ndtdfl;
							break;
						}
						dtdfl = dtdfl->next;
					}
				}
			}
			else {
				free(tmpText);
			}
			
			if (((dirent.d_stat.st_mode) & SCE_S_IFMT) == SCE_S_IFDIR) {
				subPath = (char*)calloc(strlen(path)+strlen(dirent.d_name)+2, 1);
				if (subPath == NULL) {
					/* Error */
				}
				sprintf(subPath, "%s/%s", path, dirent.d_name);
				_deleteDirectoryTreeFolders(subPath, depth+1);
				free(subPath);
			}
			memset(&dirent, 0, sizeof(dirent));
		}
		sceIoDclose(dirFd);
	}
	else {
		debugMessage("Error on sceIoDopen, returned:");
		debugPrintInt(dirFd);
	}
}

int main(void) {
	int i;
	int netStatusPrev = 0;
	SceCtrlData ctrlData;
	unsigned int ctrlButtonsPrev;
	unsigned char ctrlLxPrev;
	unsigned char ctrlLyPrev;
	unsigned char ctrlRxPrev;
	unsigned char ctrlRyPrev;
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
	sceCtrlPeekBufferPositive(0, &ctrlData, 1);
	ctrlButtonsPrev = ctrlData.buttons;
	ctrlLxPrev = ctrlData.lx;
	ctrlLyPrev = ctrlData.ly;
	ctrlRxPrev = ctrlData.rx;
	ctrlRyPrev = ctrlData.ry;
	memset(&netinf, 0, sizeof(netinf));
	
	#ifdef PSP2_DEBUG_ALL
		logFile = sceIoOpen("ux0:/data/RTST-DEBUG.LOG", SCE_O_WRONLY|SCE_O_CREAT, 0777);
		if (logFile < 0) {
			debugMessage("Error on sceIoOpen, returned:");
			debugPrintInt(logFile);
		}
	#endif

	networkInit();
	#ifdef PSP2_DEBUG_ALL
		initDebugConnection();
	#endif
	graphicsInit(0);
	eventInit();
	
	while (1) {
		sceCtrlPeekBufferPositive(0, &ctrlData, 1);
		
		for (i = 1; i < 65536; i = i * 2) {
			if (ctrlData.buttons & i) {
				if (!(ctrlButtonsPrev & i)) {
					eventButtonDown(i);
				}
			}
			else if (!(ctrlData.buttons & i)) {
				if (ctrlButtonsPrev & i) {
					eventButtonUp(i);
				}
			}
		}
		
		if (ctrlData.lx != ctrlLxPrev || ctrlData.ly != ctrlLyPrev) {
			eventAnalog(0, ctrlData.lx, ctrlData.ly);
		}
		if (ctrlData.rx != ctrlRxPrev || ctrlData.ry != ctrlRyPrev) {
			eventAnalog(1, ctrlData.rx, ctrlData.ry);
		}
		ctrlButtonsPrev = ctrlData.buttons;
		ctrlLxPrev = ctrlData.lx;
		ctrlLyPrev = ctrlData.ly;
		ctrlRxPrev = ctrlData.rx;
		ctrlRyPrev = ctrlData.ry;
		
		if (netinf.status != netStatusPrev) {
			if (netStatusPrev == 0 && netinf.status == 1) {
				eventNetworkConnect();
				netStatusPrev = netinf.status;
			}
			if (netStatusPrev == 1 && netinf.status == 0) {
				eventNetworkDisconnect();
				netStatusPrev = netinf.status;
			}
		}
		if (netinf.recvtmpSize != 0 && netinf.status == 1) {
			if (netinf.recvWaiting == 1) {
				eventNetworkRecv(netinf.recvtmp, netinf.recvtmpSize);
				netinf.recvtmpSize = 0;
				netinf.recvWaiting = 0;
			}
		}
		
		if (eventUpdate() == -1) {
			break;
		}
		
		graphicsStartDrawing();
		graphicsClearScreen();
		eventDraw();
		graphicsEndDrawing();
		graphicsSwapBuffers();
	}
	
	eventEnd();
	
	if (dbgNetConnected == 1) {
        sceNetShutdown(debugSocketFD, SCE_NET_SHUT_RDWR);
        sceNetSocketClose(debugSocketFD);
	}

	networkEnd();
	graphicsEnd();
	
	if (logFile >= 0) {
		sceIoClose(logFile);
	}
	
	sceKernelExitProcess(0);
	return 0;
}
