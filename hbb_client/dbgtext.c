#include "dbgtext.h"

static char convertCharCode(char code);
static int addLine(char code, int h);

static char convertCharCode(char code) {
	if (code >= 48 && code <= 57) {
		return (code-48)+(2*26);
	}
	if (code >= 65 && code <= 90) {
		return (code-65);
	}
	if (code >= 97 && code <= 122) {
		return (code-97)+26;
	}
	switch (code) {
		case 32: return 119;
		case 33: return 107;
		case 34: return 85;
		case 35: return 87;
		case 36: return 78;
		case 37: return 84;
		case 38: return 89;
		case 39: return 86;
		case 40: return 91;
		case 41: return 92;
		case 42: return 81;
		case 43: return 79;
		case 44: return 93;
		case 45: return 80;
		case 46: return 94;
		case 47: return 82;
		case 58: return 105;
		case 59: return 104;
		case 60: return 112;
		case 61: return 83;
		case 62: return 113;
		case 63: return 106;
		case 64: return 88;
		case 91: return 114;
		case 92: return 108;
		case 93: return 115;
		case 94: return 117;
		case 95: return 90;
		case 96: return 116;
		case 123: return 110;
		case 124: return 109;
		case 125: return 111;
		case 126: return 118;
	}
	return 106;
}

static int addLine(char code, int h) {
	int j = 0;
	while (code >= 26) {
		++j;
		code -= 26;
	}
	return j*h;
}

struct dbgText* debugTextInit(void *buffer) {
	struct dbgText *rtn;
	SceUID file;
	size_t fileSize;
	
	rtn = (struct dbgText*)calloc(1, sizeof(struct dbgText));
	if (rtn == NULL) {
		return NULL;
	}

	file = sceIoOpen("app0:/data/dbgfont0.raw", SCE_O_RDONLY, 0777);
	if (file >= 0) {
		fileSize = sceIoLseek32(file, 0, SCE_SEEK_END);
		sceIoLseek32(file, 0, SCE_SEEK_SET);
		rtn->fontData0 = (char*)calloc(1, fileSize);
		if (rtn->fontData0 == NULL) {
			sceIoClose(file);
			return NULL;
		}
		sceIoRead(file, rtn->fontData0, fileSize);
		sceIoClose(file);
	}
	else {
		return NULL;
	}

	file = sceIoOpen("app0:/data/dbgfont1.raw", SCE_O_RDONLY, 0777);
	if (file >= 0) {
		fileSize = sceIoLseek32(file, 0, SCE_SEEK_END);
		sceIoLseek32(file, 0, SCE_SEEK_SET);
		rtn->fontData1 = (char*)calloc(1, fileSize);
		if (rtn->fontData1 == NULL) {
			free(rtn->fontData0);
			sceIoClose(file);
			return NULL;
		}
		sceIoRead(file, rtn->fontData1, fileSize);
		sceIoClose(file);
	}
	else {
		free(rtn->fontData0);
		return NULL;
	}
	
	file = sceIoOpen("app0:/data/dbgfont2.raw", SCE_O_RDONLY, 0777);
	if (file >= 0) {
		fileSize = sceIoLseek32(file, 0, SCE_SEEK_END);
		sceIoLseek32(file, 0, SCE_SEEK_SET);
		rtn->fontData2 = (char*)calloc(1, fileSize);
		if (rtn->fontData2 == NULL) {
			sceIoClose(file);
			return NULL;
		}
		sceIoRead(file, rtn->fontData2, fileSize);
		sceIoClose(file);
	}
	else {
		free(rtn->fontData0);
		free(rtn->fontData1);
		return NULL;
	}
	
	rtn->screen = buffer;
	return rtn;
}

int debugTextEnd(struct dbgText *dt) {
	if (dt == NULL) {
		return 1;
	}
	free(dt->fontData0);
	free(dt->fontData1);
	free(dt->fontData2);
	free(dt);
	return 0;
}

int debugTextClear(struct dbgText *dt) {
	if (dt == NULL) {
		return 1;
	}
	memset(dt->screen, 0x00, PSP2_DISPLAY_WIDTH*PSP2_DISPLAY_HEIGHT*4);
	return 0;
}

int debugTextPrint(struct dbgText *dt, char *text, int scrn_x, int scrn_y, int color, int bg_color, char font) {
	int *ifd;
	int *isc;
	size_t textlen = strlen(text);
	size_t i, j;
	char c;
	int y_pos;
	int src_x = 0;
	
	if (dt == NULL) {
		return 1;
	}
	
	if (font == 0) {
		if (scrn_y > PSP2_DISPLAY_HEIGHT-16) {
			return 6;
		}
		for (i = 0; i < textlen; i++) {
			c = convertCharCode(text[i]);
			src_x = (c*14);
			for (y_pos = 0; y_pos < 16; y_pos++) {
				for (j = 0; j < 14; j++) {
					ifd = (int*)(dt->fontData0);
					ifd += (y_pos+addLine(c, 16))*(364);
					ifd += src_x+j;
					isc = (int*)(dt->screen);
					isc += (scrn_y+y_pos)*(PSP2_DISPLAY_WIDTH);
					isc += scrn_x+j;
					if (ifd[0] == (int)0xFFFFFFFF) {
						isc[0] = bg_color;
					}
					else {
						isc[0] = color;
					}
				}
			}
			scrn_x += 14;
			if (scrn_x+14 > PSP2_DISPLAY_WIDTH) {
				return 2;
			}
		}
	}
	else if (font == 1) {
		if (scrn_y > PSP2_DISPLAY_HEIGHT-32) {
			return 7;
		}
		for (i = 0; i < textlen; i++) {
			c = convertCharCode(text[i]);
			src_x = (c*18);
			for (y_pos = 0; y_pos < 32; y_pos++) {
				for (j = 0; j < 18; j++) {
					ifd = (int*)(dt->fontData1);
					ifd += (y_pos+addLine(c, 32))*(468);
					ifd += src_x+j;
					isc = (int*)(dt->screen);
					isc += (scrn_y+y_pos)*(PSP2_DISPLAY_WIDTH);
					isc += scrn_x+j;
					if (ifd[0] == (int)0xFFFFFFFF) {
						isc[0] = bg_color;
					}
					else {
						isc[0] = color;
					}
				}
			}
			scrn_x += 18;
			if (scrn_x+18 > PSP2_DISPLAY_WIDTH) {
				return 3;
			}
		}
	}
	else if (font == 2) {
		if (scrn_y > PSP2_DISPLAY_HEIGHT-8) {
			return 8;
		}
		for (i = 0; i < textlen; i++) {
			c = convertCharCode(text[i]);
			src_x = (c*7);
			for (y_pos = 0; y_pos < 8; y_pos++) {
				for (j = 0; j < 7; j++) {
					ifd = (int*)(dt->fontData2);
					ifd += (y_pos+addLine(c, 8))*(182);
					ifd += src_x+j;
					isc = (int*)(dt->screen);
					isc += (scrn_y+y_pos)*(PSP2_DISPLAY_WIDTH);
					isc += scrn_x+j;
					if (ifd[0] == (int)0xFFFFFFFF) {
						isc[0] = bg_color;
					}
					else {
						isc[0] = color;
					}
				}
			}
			scrn_x += 7;
			if (scrn_x+7 > PSP2_DISPLAY_WIDTH) {
				return 5;
			}
		}
	}
	else {
		return 4;
	}
	
	return 0;
}

int debugTextPrintInt(struct dbgText *dt, int i, int x, int y, int color, int bg_color, char font) {
	char tmpStr[15];
	sprintf(tmpStr, "%d", i);
	return debugTextPrint(dt, tmpStr, x, y, color, bg_color, font);
}

int debugTextPrintFloat(struct dbgText *dt, int f, int x, int y, int color, int bg_color, char font) {
	char tmpStr[20];
	sprintf(tmpStr, "%.4f", (double)f);
	return debugTextPrint(dt, tmpStr, x, y, color, bg_color, font);
}
