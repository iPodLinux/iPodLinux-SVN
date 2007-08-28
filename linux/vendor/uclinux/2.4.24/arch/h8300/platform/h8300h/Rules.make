#
# h8300h/Makefile
#
# This file is included by the global makefile so that you can add your own
# platform-specific flags and dependencies.
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (c) 2001		Lineo, Inc, <www.lineo.com>
# Copyright (c) 2000,2001	D. Jeff Dionne <jeff@uClinux.org>
# Copyright (c) 1998,1999	D. Jeff Dionne <jeff@uclinux.org>
# Copyright (C) 1998		Kenneth Albanowski <kjahds@kjahds.com>
# Copyright (C) 1994 		Hamish Macdonald
#
# 68VZ328 Fixes By		Evan Stawnyczy <e@lineo.ca>
# H8/300H Modify By             Yoshinori Sato <ysato@users.sourceforge.jp>

CROSS_COMPILE = h8300-elf-

LIBGCC = $(shell $(CC) --print-libgcc-file-name -mh -mint32)

CFLAGS := -fno-builtin -DNO_CACHE $(CFLAGS) -pipe -DNO_MM -DNO_FPU -DNO_CACHE -mh -mint32 \
           -D__ELF__  -DNO_FORGET -DUTS_SYSNAME=\"uClinux\" -D__linux__ -DTARGET=$(BOARD)
ifndef CONFIG_FULLDEBUG
CFLAGS += -Os
endif
AFLAGS := $(AFLAGS) -pipe -DNO_MM -DNO_FPU -DNO_CACHE -mh -D__ELF__ -DUTS_SYSNAME=\"uClinux\"

LINKFLAGS = -T arch/$(ARCH)/platform/$(PLATFORM)/$(BOARD)/$(MODEL).ld
LDFLAGS := $(LDFLAGS) -mh8300helf

HEAD := arch/$(ARCH)/platform/$(PLATFORM)/$(BOARD)/crt0_$(MODEL).o

SUBDIRS := arch/$(ARCH)/kernel arch/$(ARCH)/mm arch/$(ARCH)/lib \
           arch/$(ARCH)/platform/$(PLATFORM) \
  	   arch/$(ARCH)/platform/$(PLATFORM)/$(BOARD) \
           $(SUBDIRS)

CORE_FILES := arch/$(ARCH)/kernel/kernel.o arch/$(ARCH)/mm/mm.o \
              arch/$(ARCH)/platform/$(PLATFORM)/platform.o \
              arch/$(ARCH)/platform/$(PLATFORM)/$(BOARD)/$(BOARD).o \
	      $(CORE_FILES)

LIBS += arch/$(ARCH)/lib/lib.a $(LIBGCC)

linux.bin: linux
	$(OBJCOPY) -O binary linux linux.bin
	
linux.srec: linux
	$(OBJCOPY) -O srec linux linux.srec

archclean:
	rm -f linux{.bin,.srec,}
