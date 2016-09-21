#include "srvnet.h"

static int networkRunThread(SceSize, void*);

static char version[3] = "0.2";
static int ms15 = 15000000;
struct netInfo netinf;

int initConnection(char *addr, unsigned short port) {
	struct netInfo *ni = NULL;
	
	netinf.addr = addr;
	netinf.port = port;
	netinf.networkThreadId = sceKernelCreateThread("networkThread", &networkRunThread, 0x10000100, 0x10000, 0, 0, NULL);
	if (netinf.networkThreadId < 0) {
		debugMessage("error");
		return netinf.networkThreadId;
	}
	ni = &netinf;
	sceKernelStartThread(netinf.networkThreadId, sizeof(ni), &ni);
	netinf.initialized = 1;
	return 3;
}

int endConnection(void) {
	if (netinf.initialized == 1) {
		if (netinf.status == 1) {
			netinf.status = -1;
			netinf.recvWaiting = 0;
			sceNetSocketAbort(netinf.serverSocketFD, 0);
			sceKernelWaitThreadEnd(netinf.networkThreadId, NULL, NULL);
			sceNetShutdown(netinf.serverSocketFD, SCE_NET_SHUT_RDWR);
			sceNetSocketClose(netinf.serverSocketFD);
		}
		if (netinf.networkThreadId >= 0) {
			sceKernelDeleteThread(netinf.networkThreadId);
		}
		netinf.initialized = 0;
	}
	return 0;
}

static int networkRunThread(SceSize args, void *argp) {
	unsigned int rtn;
	int i = 0;
	SceNetSockaddrIn sockAddrIn;
	char *sendVer;
	struct netInfo *netinfp;
	netinfp = *(struct netInfo**)(argp);
	(void)args;
	while (1) {
		if (netinfp->status == 0) {
			debugMessage("initConnection connecting...");
			netinfp->dataLength = 0;
			netinfp->bufpos = 0;
			netinfp->status = 2;
			netinfp->recvWaiting = 0;
			netinfp->serverSocketFD = sceNetSocket("SOCKET_SERVER", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, SCE_NET_IPPROTO_TCP);
			if (netinfp->serverSocketFD < 0) {
				netinfp->status = netinfp->serverSocketFD;
			}
			else {
				sceNetSetsockopt(netinfp->serverSocketFD, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO, &ms15, 4);
				sceNetSetsockopt(netinfp->serverSocketFD, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &ms15, 4);
				memset(&sockAddrIn, 0, sizeof(sockAddrIn));
				sockAddrIn.sin_family = SCE_NET_AF_INET;
				sockAddrIn.sin_port = sceNetHtons(netinfp->port);
				sceNetInetPton(SCE_NET_AF_INET, netinfp->addr, &sockAddrIn.sin_addr);
				i = sceNetConnect(netinfp->serverSocketFD, (SceNetSockaddr*)&sockAddrIn, sizeof(sockAddrIn));
				if (i < 0) {
					debugMessage("sceNetConnect failed:");
					debugPrintInt(i);
					netinfp->status = i;
					sceNetSocketClose(netinfp->serverSocketFD);
				}
				else {
					debugMessage("initConnection success.");
					netinfp->initialized = 1;
					sendVer = netString(version, sizeof(version), 1);
					netSendData(2, 3, sendVer, sizeof(version)+1);
					netSendData(2, 2, NULL, 0);
					free(sendVer);
					netinfp->status = 1;
				}
			}
		}
		else if (netinfp->status < 0) {
			break;
		}
		else if (netinfp->status == 1) {
			rtn = sceNetRecv(netinfp->serverSocketFD, &(netinfp->recvtmp), 65535, 0);
			if ((signed int)rtn < 0) {
				if (netinfp->status < 0) {
					break;
				}
				if (rtn == SCE_NET_ERROR_ECONNABORTED) {
					netinfp->status = 0;
				}
				else if (rtn == SCE_NET_ERROR_EINTR) {
					netinfp->status = 0;
				}
			}
			else if (rtn == 0) {
				sceNetShutdown(netinfp->serverSocketFD, SCE_NET_SHUT_RDWR);
				sceNetSocketClose(netinfp->serverSocketFD);
				netinfp->status = -1;
			}
			else {
				netinfp->recvtmpSize = rtn;
				netinfp->recvWaiting = 1;
				while (netinfp->recvWaiting == 1) {}
			}
		}
	}
	return 0;
}

int netSendData(char tkn1, char tkn2, char *data, size_t ds) {
	int len = sceNetHtonl(ds+6);
	unsigned int rtn;
	if (netinf.initialized == 1) {
		rtn = sceNetSend(netinf.serverSocketFD, &len, 4, 0);
		if (rtn == SCE_NET_ERROR_EPIPE) {
			netinf.status = 0;
		}
		sceNetSend(netinf.serverSocketFD, &tkn1, 1, 0);
		sceNetSend(netinf.serverSocketFD, &tkn2, 1, 0);
		if (ds != 0) {
			sceNetSend(netinf.serverSocketFD, data, ds, 0);
		}
	}
	return 0;
}

char* netString(char *input, size_t ds, char ht) {
	int l;
	short s;
	char *res = NULL;
	if (ht == 1) {
		res = (char*)calloc(1, ds+1);
		if (res == NULL) {
			return NULL;
		}
		res[0] = (char)ds;
		memcpy(res+1, input, ds);
	}
	else if (ht == 2) {
		s = sceNetHtons(ds+2);
		res = (char*)calloc(1, ds+2);
		if (res == NULL) {
			return NULL;
		}
		memcpy(res, &s, 2);
		memcpy(res+2, input, ds);
	}
	else if (ht == 4) {
		l = sceNetHtonl(ds+4);
		res = (char*)calloc(1, ds+4);
		if (res == NULL) {
			return NULL;
		}
		memcpy(res, &l, 4);
		memcpy(res+4, input, ds);
	}
	return res;
}
