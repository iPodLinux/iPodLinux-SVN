# arch/niosnommu/Makefile
#
# Makefile for the architecture dependent flags and dependencies on the
# nios.
#
# Copyright (C) 2001 Vic Phillips (vic@microtronix.com)
#
# based on sparcnommu/Makefile:
#
# Copyright (C) 1994 David S. Miller (davem@caip.rutgers.edu)
#
KERNELLOAD = ${shell echo `grep "nasys_program_mem " include/asm/nios.h | sed 's/^.*\*)//' | sed 's/)//'`}

CFLAGS := $(CFLAGS) -m32 -DNO_MM -pipe -D__linux__ -D__ELF__ -g
CFLAGS := $(CFLAGS) -funaligned-struct-hack -mno-zero-extend

# Uncomment this if you are doing gdb source level
# debugging of the kernel to get the proper debugging information.
#
#CFLAGS := $(CFLAGS) -DDEBUG

# This undefines the "__init" type used in defining initialization
# procedures.  When defined, the procedures are put into an 'init' data
# section that GDB doesn't recognize as source.
#
CFLAGS := $(CFLAGS) -DNO_TEXT_SECTIONS

AFLAGS := $(AFLAGS) -m32 -DNO_MM -funaligned-struct-hack -g

LINKFLAGS = -T arch/niosnommu/ram.ld -Ttext $(KERNELLOAD)

# add this to avoid multiple '_stack' and '_vecbase' definition errors
#
ifdef niosgnu
# Include the path to the lib directory where the ldscripts are found to fix
# a problem with the cygwin/bash environment.
LDSCRIPTS:=$(shell nios-elf-gcc -print-file-name=ldscripts)
EXTRA_LDFLAGS := -melfnios32 -L $(LDSCRIPTS)/..
else
EXTRA_LDFLAGS = -melfnios32
endif
export EXTRA_LDFLAGS

# vic - add this to get name of nios gcc library
GCCLIB	:= `$(CC) -m32 --print-libgcc-file-name`

HEAD := arch/niosnommu/kernel/head.o

SUBDIRS := $(SUBDIRS) arch/niosnommu/kernel arch/niosnommu/lib arch/niosnommu/mm

CORE_FILES := arch/niosnommu/kernel/kernel.o arch/niosnommu/mm/mm.o $(CORE_FILES)

LIBS := $(LIBS) $(GCCLIB) \
	$(TOPDIR)/arch/niosnommu/lib/lib.a

archclean:
	rm -f $(TOPDIR)/arch/niosnommu/kernel/nios_defs.h
	rm -f $(TOPDIR)/arch/niosnommu/kernel/nios_defs.d
	rm -f $(TOPDIR)/arch/niosnommu/boot/boot
	rm -f $(TOPDIR)/linux.srec
	rm -f $(TOPDIR)/linux.flash
	rm -f $(TOPDIR)/linux.bin
	rm -f $(TOPDIR)/linux.bin.srec

linuxsrec: linux
	$(OBJCOPY) -O srec $(LINUX) linux.srec

archmrproper:
	rm -f $(TOPDIR)/include/asm/nios.h
	rm -f $(TOPDIR)/include/asm/niosasm.h

archdep:
