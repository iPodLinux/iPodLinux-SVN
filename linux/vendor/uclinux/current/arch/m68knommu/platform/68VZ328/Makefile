#
# Makefile for the linux kernel.
#
# Reuse any files we can from the 68328 base
#

VPATH := $(VPATH):$(BOARD):../68328

# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
.S.o:
	$(CC) -D__ASSEMBLY__ $(AFLAGS) -I. -c $< -o $*.o

all: $(BOARD)/crt0_$(MODEL).o entry.o platform.o
O_TARGET := platform.o
obj-y := entry.o config.o signal.o traps.o ints.o

obj-$(CONFIG_PM) += pm.o

ifdef CONFIG_DRAGONIXVZ
ifdef CONFIG_DRAGONIXVZ_JTAGFPGA
obj-y += $(BOARD)/jblaster.o $(BOARD)/jb_jtag.o $(BOARD)/jb_io.o $(BOARD)/hardware.o
endif
endif

ifdef CONFIG_DRAGONIXVZ
$(BOARD)/crt0_$(MODEL).o: $(BOARD)/crt0_$(MODEL).S $(BOARD)/bootlogo.rh
else
ifeq ($(CONFIG_DRAGEN2),y)
$(BOARD)/crt0_$(MODEL).o: $(BOARD)/crt0_$(MODEL).S $(BOARD)/crt0_fixed.S
else
ifeq ($(CONFIG_CWVZ328),y)
$(BOARD)/crt0_$(MODEL).o: $(BOARD)/crt0_$(MODEL).S $(BOARD)/bootlogo.rh
else
$(BOARD)/crt0_$(MODEL).o: $(BOARD)/crt0_$(MODEL).S $(BOARD)/crt0_fixed.S $(BOARD)/bootlogo.rh
endif
endif
endif

entry.o: entry.S m68k_defs.h

m68k_defs.h: ../../kernel/m68k_defs.c ../../kernel/m68k_defs.head
	rm -f m68k_defs.d
	$(CC) $(filter-out -MD,$(CFLAGS)) -S ../../kernel/m68k_defs.c
	cp ../../kernel/m68k_defs.head m68k_defs.h
	chmod +w m68k_defs.h
	grep '^#define' m68k_defs.s >> m68k_defs.h
	rm m68k_defs.s
-include m68k_defs.d

ifeq ($(CONFIG_DRAGEN2),y)
ifeq ($(CONFIG_INIT_LCD),y)
obj-y += ccfl.o
config.o: $(BOARD)/screen.h

$(BOARD)/screen.h: $(BOARD)/screen.xbm
	perl $(BOARD)/xbm2lcd.pl < $(BOARD)/screen.xbm > $(BOARD)/screen.h
endif
else
$(BOARD)/bootlogo.rh: $(BOARD)/bootlogo.h
	perl ../68328/tools/bootlogo.pl < $(BOARD)/bootlogo.h > $(BOARD)/bootlogo.rh
endif

include $(TOPDIR)/Rules.make
