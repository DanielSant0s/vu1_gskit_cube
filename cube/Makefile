# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

EE_BIN = cube.elf
EE_OBJS = draw_3D.o main.o
EE_LIBS = -L$(PS2DEV)/gsKit/lib/ -L../vukit/lib/ -lmath3d -ldmakit -lgskit -lvukit
EE_INCS += -I$(PS2DEV)/gsKit/include -I../vukit/include

EE_DVP = dvp-as
EE_VCL = vcl
EE_VCLPP = vclpp

all: cube.c $(EE_BIN)
#	$(EE_STRIP) --strip-all $(EE_BIN)

# Original VCL tool preferred. 
# It can be runned on WSL, but with some tricky commands: 
# https://github.com/microsoft/wsl/issues/2468#issuecomment-374904520
%.vcl: %.vclpp
	$(EE_VCLPP) $< $@.vcl
	
%.vsm: %.vcl
	$(EE_VCL) $< >> $@

%.o: %.vsm
	$(EE_DVP) $< -o $@

cube.c:
	bin2c cube.raw cube.c cube

clean:
	rm -f $(EE_BIN) $(EE_OBJS) cube.c

run: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

reset:
	ps2client reset

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
