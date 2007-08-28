#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
.S.o:
	$(CC) -D__ASSEMBLY__ $(AFLAGS) -c $< -o $*.o

all: $(BOARD)/crt0_$(MODEL).o entry.o platform.o
O_TARGET := platform.o
obj-y := entry.o config.o signal.o traps.o ints.o interrupt.o

$(BOARD)/crt0_$(MODEL).o: $(BOARD)/crt0_$(MODEL).S $(BOARD)/bootlogo.rh

entry.o: entry.S frio_defs.h

frio_defs.h: ../../kernel/frio_defs.c ../../kernel/frio_defs.head
	rm -f frio_defs.d
	$(CC) $(filter-out -MD,$(CFLAGS)) -S ../../kernel/frio_defs.c
	cp ../../kernel/frio_defs.head frio_defs.h
	grep '^#define' frio_defs.s >> frio_defs.h
	rm frio_defs.s
-include frio_defs.d

$(BOARD)/bootlogo.rh: $(BOARD)/bootlogo.h
	perl tools/bootlogo.pl < $(BOARD)/bootlogo.h > $(BOARD)/bootlogo.rh

include $(TOPDIR)/Rules.make
