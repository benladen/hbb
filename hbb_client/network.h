#pragma once

#include "main.h"

#include <stdlib.h>
#include <string.h>
#include <psp2/types.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>

#define NETWORK_INIT_SIZE 16384

int networkInit(void);
int networkEnd(void);

extern char *netMem;
