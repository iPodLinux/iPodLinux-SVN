#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

.S.s:
	$(CPP) -D__ASSEMBLY__ $(AFLAGS) -ansi $< -o $*.s

.S.o:
	$(CC) -D__ASSEMBLY__ $(AFLAGS) -ansi -c $< -o $*.o

all: kernel.o head.o

O_TARGET := kernel.o

obj-y   := entry.o switch.o traps.o irq.o \
	    process.o signal.o setup.o sys_nios.o \
	    semaphore.o nios_flat.o io.o \
	    time.o ptrace.o start.o nios_module.o nios_ksyms.o

export-objs := nios_ksyms.o

obj-$(CONFIG_KGDB) += nios_gdb_stub_isr.o nios_gdb_stub_io.o nios_gdb_stub.o

obj-$(CONFIG_CONSOLE) += console.o

entry.o: entry.S nios_defs.h
head.o: head.S nios_defs.h
switch.o: switch.S nios_defs.h

nios_defs.h: nios_defs.c nios_defs.head
	rm -f nios_defs.d
	SUNPRO_DEPENDENCIES="nios_defs.d nios_defs.h" \
	$(CC) $(filter-out -MD,$(CFLAGS)) -S nios_defs.c
	cp nios_defs.head nios_defs.h
	grep '^#define' nios_defs.s >> nios_defs.h
	rm nios_defs.s
-include nios_defs.d

include $(TOPDIR)/Rules.make
