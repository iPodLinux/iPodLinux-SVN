#
# ADI/Makefile
#
# This file is included by the global makefile so that you can add your own
# platform-specific flags and dependencies.
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (c) 2002  Metrowerks, Inc. <mwaddel@metrowerks.ca> 
# Copyright (c) 2000,2001  Lineo, Inc. <jeff@lineo.ca> 
# Copyright (c) 1998,1999  D. Jeff Dionne <jeff@uclinux.org>
# Copyright (C) 1998       Kenneth Albanowski <kjahds@kjahds.com>
# Copyright (C) 1994 by Hamish Macdonald
#

GCC_DIR = $(shell $(CC) -v 2>&1 | grep specs | sed -e 's/.* \(.*\)specs/\1\./')

INCGCC = $(GCC_DIR)/include
#LIBGCC = $(GCC_DIR)/000/libgcc.a

CFLAGS := -fno-builtin -DNO_CACHE $(CFLAGS) -pipe -DNO_MM -DNO_FPU -DNO_CACHE -D__ELF__ -DMAGIC_ROM_PTR -DNO_FORGET -DUTS_SYSNAME='"uClinux"' -D__linux__ 
AFLAGS := $(AFLAGS) -pipe -DNO_MM -DNO_FPU -DNO_CACHE -D__ELF__ -DMAGIC_ROM_PTR -DUTS_SYSNAME='"uClinux"' -Wa

ifndef CONFIG_FULLDEBUG
LINKFLAGS = -T arch/$(ARCH)/platform/$(PLATFORM)/$(BOARD)/$(MODEL).ld
else
LINKFLAGS = -S -T arch/$(ARCH)/platform/$(PLATFORM)/$(BOARD)/$(MODEL).ld
endif
HEAD := arch/$(ARCH)/platform/$(PLATFORM)/$(BOARD)/crt0_$(MODEL).o

SUBDIRS := arch/$(ARCH)/kernel arch/$(ARCH)/mm arch/$(ARCH)/lib \
           arch/$(ARCH)/platform/$(PLATFORM) $(SUBDIRS)




#########   Temp modification: Tonyko   ############
#CORE_FILES := arch/$(ARCH)/kernel/kernel.o arch/$(ARCH)/mm/mm.o \
              arch/$(ARCH)/platform/$(PLATFORM)/platform.o $(CORE_FILES)

CORE_FILES := arch/$(ARCH)/kernel/process.o arch/$(ARCH)/kernel/traps.o \
	      arch/$(ARCH)/kernel/ptrace.o  arch/$(ARCH)/kernel/sys_frio.o \
              arch/$(ARCH)/kernel/time.o    arch/$(ARCH)/kernel/semaphore.o \
              arch/$(ARCH)/kernel/setup.o   arch/$(ARCH)/kernel/frio_ksyms.o \
              arch/$(ARCH)/mm/init.o        arch/$(ARCH)/mm/fault.o \
              arch/$(ARCH)/mm/memory.o      arch/$(ARCH)/mm/kmap.o \
              arch/$(ARCH)/platform/$(PLATFORM)/entry.o \
              arch/$(ARCH)/platform/$(PLATFORM)/config.o \
              arch/$(ARCH)/platform/$(PLATFORM)/signal.o \
              arch/$(ARCH)/platform/$(PLATFORM)/interrupt.o \
              arch/$(ARCH)/platform/$(PLATFORM)/traps.o \
              arch/$(ARCH)/platform/$(PLATFORM)/ints.o $(CORE_FILES)


LIBS += arch/$(ARCH)/lib/lib.a

ifdef CONFIG_FRAMEBUFFER
SUBDIRS := $(SUBDIRS) arch/$(ARCH)/console
ARCHIVES := $(ARCHIVES) arch/$(ARCH)/console/console.a
endif

MAKEBOOT = $(MAKE) -C arch/$(ARCH)/boot

romfs.s19: romfs.img arch/$(ARCH)/empty.o
	$(CROSS_COMPILE)objcopy -v -R .text -R .data -R .bss --add-section=.fs=romfs.img --adjust-section-vma=.fs=0xa0000 arch/$(ARCH)/empty.o romfs.s19
	$(CROSS_COMPILE)objcopy -O srec romfs.s19

linux.data: linux
	$(CROSS_COMPILE)objcopy -O binary --remove-section=.romvec --remove-section=.text --remove-section=.ramvec --remove-section=.bss --remove-section=.eram linux linux.data

linux.text: linux
	$(CROSS_COMPILE)objcopy -O binary --remove-section=.ramvec --remove-section=.bss --remove-section=.data --remove-section=.eram --set-section-flags=.romvec=CONTENTS,ALLOC,LOAD,READONLY,CODE linux linux.text

romfs.img:
	echo creating a vmlinux rom image without root filesystem!

linux.bin: linux.text linux.data romfs.img
	if [ -f romfs.img ]; then\
		cat linux.text linux.data romfs.img > linux.bin;\
	else\
		cat linux.text linux.data > linux.bin;\
	fi

linux.trg linux.rom: linux.bin
	perl arch/$(ARCH)/platform/$(PLATFORM)/tools/fixup.pl

archclean:
	@$(MAKEBOOT) clean
	rm -f arch/$(ARCH)/platform/$(PLATFORM)/m68k_defs.h
	rm -f linux.text linux.data linux.bin linux.rom linux.trg
	rm -f linux.s19 romfs.s19
	rm -f linux.img romdisk.img
