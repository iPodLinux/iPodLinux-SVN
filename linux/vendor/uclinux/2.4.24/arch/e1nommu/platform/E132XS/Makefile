#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

#.S.o:
#	$(CC) -E -D__ASSEMBLY__ $(AFLAGS) -c $< -o $*.o

all: $(BOARD)/crt0_$(MODEL).o platform.o

#E132XS_OBJS := #

#e132xs = $(E132XS_OBJS) 

$(BOARD)/crt0_$(MODEL).o: $(BOARD)/crt0_$(MODEL).s

O_TARGET := platform.o
obj-y := config.o entry.o initstack.o syscalls.o syscall_entry.o traps.o

ifeq ($(CONFIG_EMBEDDED_SERIAL_CONSOLE),y)
obj-y += st16550.o
endif

ifeq ($(CONFIG_BLK_DEV_INITRD),y)
my_initrd.o: my_initrd.s
obj-y += my_initrd.o
endif


# entry.o: entry.S m68k_defs.h
# 
# m68k_defs.h: ../../kernel/m68k_defs.c ../../kernel/m68k_defs.head
# 	rm -f m68k_defs.d
# 	$(CC) $(filter-out -MD,$(CFLAGS)) -S ../../kernel/m68k_defs.c
# 	cp ../../kernel/m68k_defs.head m68k_defs.h
# 	grep '^#define' m68k_defs.s >> m68k_defs.h
# 	rm m68k_defs.s
# -include m68k_defs.d

include $(TOPDIR)/Rules.make
