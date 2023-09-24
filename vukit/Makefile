EE_LIB = lib/libvukit.a

EE_INCS =  -I$(PS2DEV)/gsKit/include -Iinclude

VIF_OBJS = src/vif.o

EE_OBJS = $(VIF_OBJS)

EE_CFLAGS += -Wno-unused

.PHONY: all clean install

all:
	$(MAKE) $(EE_LIB)

clean:
	rm -f $(EE_LIB) $(EE_OBJS)

install: all

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal