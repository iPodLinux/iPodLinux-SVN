#
# E132XS/Makefile
#
# This file is included by the global makefile so that you can add your own
# platform-specific flags and dependencies.
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1999 by Greg Ungerer (gerg@lineo.com)
# Copyright (C) 2000 Lineo Inc.
# Copyright (C) 2002 Yiannis Mitsos <yannis.mitsos@gdt.gr>

# CROSS_COMPILE = $(GCC_EXEC_PREFIX)

GCC_DIR = $(shell $(CC) -v 2>&1 | grep specs | sed -e 's/.* \(.*\)specs/\1\./')

INCGCC = $(GCC_DIR)/include
LIBGCC = $(GCC_DIR)/libgcc.a

# INCGCC = $(TOOLS)/gcc-include
# LIBGCC = $(TOOLS)/gcc-lib/libgcc.a

CFLAGS := -fno-builtin -nostdinc $(CFLAGS) -I$(INCGCC) -pipe  \
	-DNO_MM -DNO_FPU -D__ELF__ -DMAGIC_ROM_PTR -DUTS_SYSNAME=\"uClinux\" \
	-D__linux__ -mgnu-param #-DDEBUG

##AFLAGS := $(CFLAGS)
AFLAGS :=  --gstabs

LINKFLAGS = -T arch/$(ARCH)/platform/$(PLATFORM)/$(BOARD)/$(MODEL).ld

HEAD := arch/$(ARCH)/platform/$(PLATFORM)/$(BOARD)/crt0_$(MODEL).o

SUBDIRS := arch/$(ARCH)/kernel arch/$(ARCH)/mm arch/$(ARCH)/lib \
           arch/$(ARCH)/platform/$(PLATFORM) $(SUBDIRS) 

CORE_FILES := arch/$(ARCH)/kernel/kernel.o arch/$(ARCH)/mm/mm.o \
              arch/$(ARCH)/platform/$(PLATFORM)/platform.o $(CORE_FILES)

LIBS += arch/$(ARCH)/lib/lib.a $(LIBGCC)

MAKEBOOT = $(MAKE) -C arch/$(ARCH)/boot

archclean:
	@$(MAKEBOOT) clean

