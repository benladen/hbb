#pragma once

#include "main.h"

int eventInit(void);
int eventEnd(void);
int eventUpdate(void);
int eventDraw(void);
int eventButtonDown(int button);
int eventButtonUp(int button);
int eventAnalog(char lr, unsigned char x, unsigned char y);
int eventNetworkConnect(void);
int eventNetworkRecv(char *data, size_t len);
int eventNetworkMsg(char ev1, char ev2, char *data, size_t len);
int eventNetworkMisc(int ev);
int eventNetworkDisconnect(void);