#
# 5280/Makefile
#
# This file is included by the global makefile so that you can add your own
# platform-specific flags and dependencies.
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1999-2003  Greg Ungerer (gerg@snapgear.com)
# Copyright (C) 1998,1999  D. Jeff Dionne <jeff@lineo.ca>
# Copyright (C) 1998       Kenneth Albanowski <kjahds@kjahds.com>
# Copyright (C) 1994 by Hamish Macdonald
# Copyright (C) 2000  Lineo Inc. (www.lineo.com)

GCC_DIR = $(shell $(CC) -v 2>&1 | grep specs | sed -e 's/.* \(.*\)specs/\1\./')

# Even though we're building for a 5282, we specify 5307 as our processor type.
# The 5307 instruction set is the same as the 5282 (divide unit & MAC) plus
# the 5282 instruction timings are closer to the 5307 than the other 5200
# series processors (specifically the multiply and divide instructions which
# are all this option really alters).

INCGCC = $(GCC_DIR)/include
LIBGCC = $(GCC_DIR)/m5307/libgcc.a

CFLAGS := -fno-builtin -nostdinc $(CFLAGS) -I$(INCGCC) -pipe -DNO_MM -DNO_FPU -m5307 -Wa,-S -Wa,-m5307 -D__ELF__ -DMAGIC_ROM_PTR -DUTS_SYSNAME=\"uClinux\" -D__linux__ -O1
AFLAGS := $(CFLAGS)

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
	rm -f arch/$(ARCH)/platform/$(PLATFORM)/m68k_defs.h

