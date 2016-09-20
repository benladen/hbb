#pragma once

#include "main.h"

struct netInfo {
	char initialized;
	SceUID networkThreadId;
	int serverSocketFD;
    int status; /* 0 = Not connected, 1 = Connected, 2 = Connecting. Negative = Error. */
	char *addr;
	unsigned short port;
	
	char recvWaiting;
	size_t recvtmpSize;
	
	char dlSet;
	size_t dataLength;
	size_t bufpos;
	char recvtmp[65535];
	char buffer[4194304];
};

extern struct netInfo netinf;

int initConnection(char *addr, unsigned short);
int endConnection(void);
int netSendData(char tkn1, char tkn2, char *data, size_t ds);
char* netString(char *input, size_t ds, char ht);