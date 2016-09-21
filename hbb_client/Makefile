TITLE_ID = RTST00010
TARGET  = hbb
OBJS    = main.o graphics.o network.o dbgtext.o srvnet.o event.o sha1.o
SHADERS = shader/clear_v_gxp.o shader/clear_f_gxp.o \
          shader/color_v_gxp.o shader/color_f_gxp.o \
          shader/texture_v_gxp.o shader/texture_f_gxp.o

LIBS = -lSceKernel_stub -lSceDisplay_stub -lSceGxm_stub -lSceSysmodule_stub \
       -lSceCtrl_stub -lSceTouch_stub -lSceMotion_stub -lScePgf_stub \
       -lSceNet_stub -lSceNetCtl_stub -lSceAppUtil_stub -lSceCommonDialog_stub \
       -lSceAudio_stub -lSceRtc_stub -lSceCamera_stub -lScePower_stub \
       -lScePromoterUtil_stub -lSceIme_stub -lc -lm

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc
#CFLAGS  = -Wl,-q -Wall -O0
CFLAGS  = -Wl,-q -Wall -O0 -Wextra -std=c89 -pedantic -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition
ASFLAGS = $(CFLAGS)

all: $(TARGET).vpk

%.vpk: eboot.bin
	vita-mksfoex -s TITLE_ID=$(TITLE_ID) "$(TARGET)" output/param.sfo
	python ~/vita-pack-vpk-gen.py vpk_extras output $@
	
eboot.bin: $(TARGET).velf
	vita-make-fself output/$< output/$@

%.velf: %.elf
	$(PREFIX)-strip -g output/$<
	vita-elf-create output/$< output/$@ $(VITAELFDB)

$(TARGET).elf: $(SHADERS) $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@
	mkdir -p output
	mv $(TARGET).elf output
	mv $(OBJS) output

clean:
	@rm -rf output/$(TARGET).vpk output/$(TARGET).velf output/$(TARGET).elf output/$(OBJS) output/eboot.bin output/param.sfo \
			$(TARGET).vpk $(TARGET).velf $(TARGET).elf $(OBJS) eboot.bin param.sfo
