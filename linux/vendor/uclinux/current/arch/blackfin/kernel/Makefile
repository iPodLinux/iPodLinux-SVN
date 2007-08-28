#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

all: kernel.o

O_TARGET := kernel.o
obj-y := process.o traps.o ptrace.o \
	  sys_frio.o time.o semaphore.o
obj-y += setup.o frio_ksyms.o

#Removed by Chang Junxiao 07 01
#ifdef CONFIG_PCI
#obj-y += bios32.o
#endif

obj-$(CONFIG_KGDB)      += nisa-stub.o

head.o: head.S frio_defs.h

entry.o: entry.S frio_defs.h

frio_defs.h: frio_defs.c frio_defs.head
	rm -f frio_defs.d
	SUNPRO_DEPENDENCIES="frio_defs.d frio_defs.h" \
	$(CC) $(filter-out -MD,$(CFLAGS)) -S frio_defs.c
	cp frio_defs.head frio_defs.h
	grep '^#define' frio_defs.s >> frio_defs.h
	rm frio_defs.s
-include frio_defs.d

include $(TOPDIR)/Rules.make

