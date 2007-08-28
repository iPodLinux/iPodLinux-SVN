#
# arch/microblaze/kernel/Makefile
#
#  Copyright (C) 2003       John Williams <jwilliams@itee.uq.edu.au>
#  Copyright (C) 2001,2002  NEC Corporation
#  Copyright (C) 2001,2002  Miles Bader <miles@gnu.org>
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

EXTRA_CFLAGS		+= -I$(TOPDIR)/arch/microblaze/xilinx_ocp

all: head.o kernel.o

USE_STANDARD_AS_RULE	:= true

O_TARGET := kernel.o
obj-y := intv.o entry.o process.o syscalls.o time.o semaphore.o setup.o \
	 signal.o irq.o mach.o ptrace.o bug.o
export-objs := microblaze_ksyms.o 

obj-$(CONFIG_MODULES)		+= microblaze_ksyms.o
# chip-specific code
obj-$(CONFIG_MICROBLAZE)	+= microblaze_intc.o 
obj-$(CONFIG_MICROBLAZE)	+= microblaze_timer.o 
# obj-$(CONFIG_MICROBLAZE)	+= memcons.o
obj-$(CONFIG_MICROBLAZE)	+= xmbserial.o
# platform-specific code
obj-$(CONFIG_MBVANILLA)		+= mbvanilla.o 
obj-$(CONFIG_MBEGRET01)		+= mbegret.o
# feature-specific code
obj-$(CONFIG_PROC_FS)		+= procfs.o

head.o: head.S microblaze_defs.h
entry.o: entry.S microblaze_defs.h

microblaze_defs.h: microblaze_defs.c microblaze_defs.head
	rm -f microblaze_defs.d
	SUNPRO_DEPENDENCIES="microblaze_defs.d microblaze_defs.h" \
	$(CC) $(filter-out -MD,$(CFLAGS)) -S microblaze_defs.c
	cp microblaze_defs.head microblaze_defs.h
	grep '^#define' microblaze_defs.s >> microblaze_defs.h
	rm microblaze_defs.s
-include microblaze_defs.d

include $(TOPDIR)/Rules.make
