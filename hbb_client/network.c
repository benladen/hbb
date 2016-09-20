#include "network.h"

char *netMem;

int networkInit(void) {
	int ret;
	SceNetInitParam netInitParam;
	memset(&netInitParam, 0, sizeof(netInitParam));

	if (sceSysmoduleLoadModule(SCE_SYSMODULE_NET) != 0) {
		debugMessage("An error while loading SCE_SYSMODULE_NET");
	}
	if ((SceUInt32)sceNetShowNetstat() == SCE_NET_ERROR_ENOTINIT) {
		netMem = calloc(1, NETWORK_INIT_SIZE);
		netInitParam.memory = netMem;
		netInitParam.size = NETWORK_INIT_SIZE;
		netInitParam.flags = 0;
		ret = sceNetInit(&netInitParam);
		if (ret != 0) {
            debugMessage("An error on sceNetInit...");
			debugPrintInt(ret);
		}
	}
	ret = sceNetCtlInit();
	if (ret != 0) {
		if ((SceUInt32)ret == 0x80412102) {
			/* Library already initalized? */
		}
		else {
			debugMessage("An error on sceNetCtlInit...");
			debugPrintInt(ret);
		}
	}
	return 0;
}

int networkEnd(void) {
	sceNetCtlTerm();
	sceNetTerm();
	sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
	return 0;
}
