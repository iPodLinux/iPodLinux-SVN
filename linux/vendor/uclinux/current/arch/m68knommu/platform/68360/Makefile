#
# Makefile for the linux kernel.
#
# Reuse any files we can from the 68328 base
#

VPATH := $(VPATH):../68328

# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
.S.o:
	$(CC) -D__ASSEMBLY__ $(AFLAGS) -c $< -o $*.o

all: $(BOARD)/crt0_$(MODEL).o entry.o platform.o quicc 

QUICC_OBJS :=	quicc/commproc.o

ifeq ($(CONFIG_M68360_SMC_UART),y)
QUICC_OBJS += quicc/uart.o
endif

ifeq ($(CONFIG_M68EN360_ETHERNET),y)
QUICC_OBJS += quicc/enet.o
endif

quicc = $(QUICC_OBJS)


$(BOARD)/crt0_$(MODEL).o: $(BOARD)/crt0_$(MODEL).S 

O_TARGET := platform.o
obj-y := entry.o config.o signal.o traps.o ints.o $(QUICC_OBJS)

entry.o: entry.S m68k_defs.h

m68k_defs.h: ../../kernel/m68k_defs.c ../../kernel/m68k_defs.head
	rm -f m68k_defs.d
	$(CC) $(filter-out -MD,$(CFLAGS)) -S ../../kernel/m68k_defs.c
	cp ../../kernel/m68k_defs.head m68k_defs.h
	grep '^#define' m68k_defs.s >> m68k_defs.h
	rm m68k_defs.s
-include m68k_defs.d


include $(TOPDIR)/Rules.make
