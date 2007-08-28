#
# arch/v850/Makefile
#
#  Copyright (C) 2001,02,03  NEC Corporation
#  Copyright (C) 2001,02,03  Miles Bader <miles@gnu.org>
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies. Remember to do have actions
# for "archclean" and "archdep" for cleaning up and making dependencies for
# this architecture
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#

UTS_SYSNAME = -DUTS_SYSNAME=\"uClinux\"


CFLAGS += -mv850e
# r16 is a fixed pointer to the current task
CFLAGS += -ffixed-r16 -mno-prolog-function
CFLAGS += -fno-builtin
CFLAGS += -DNO_MM
CFLAGS += $(UTS_SYSNAME) -D__linux__


ARCH_DIR = arch/$(ARCH)


HEAD := $(ARCH_DIR)/kernel/head.o

SUBDIRS += $(ARCH_DIR)/kernel $(ARCH_DIR)/lib
CORE_FILES := $(ARCH_DIR)/kernel/kernel.o $(CORE_FILES)
LIBS += $(ARCH_DIR)/lib/lib.a


# Deal with the initial contents of the blkmem device
ifdef ROOT_FS_IMAGE
ifndef CONFIG_SIM
# Embed it in the kernel

ROOT_FS_IMAGE_OBJ = root_fs_image.o

CORE_FILES += $(ROOT_FS_IMAGE_OBJ)

$(LINUX) : $(ROOT_FS_IMAGE_OBJ)

# Note that we use the build-system's objcopy, as the v850 tools are fairly
# old, and don't have the --rename-section option.
$(ROOT_FS_IMAGE_OBJ): $(ROOT_FS_IMAGE)
	objcopy -I binary -O elf32-little -B i386 --rename-section .data=.root,alloc,load,readonly,data,contents $< $@

endif # !CONFIG_SIM
endif # ROOT_FS_IMAGE


# kernel linker script
LINKFLAGS = -T linux.ld
$(LINUX) : linux.ld
linux.ld: $(ARCH_DIR)/kernel/linux.ld.in
	$(CPP) -P -x assembler-with-cpp $(AFLAGS) $< > $@


# If you want the kernel build to build modules outside of the tree
# then define this and pass it to the main linux makefile
ifdef EXTRA_MODULE_DIRS
SUBDIRS += $(EXTRA_MODULE_DIRS)
endif

bootstrap:
	@$(MAKEBOOT) bootstrap

archmrproper:

archdep:

archclean:
	rm -f $(LINUX) linux.ld
	rm -f $(ARCH_DIR)/kernel/v850_defs.h $(ARCH_DIR)/kernel/v850_defs.d

include $(TOPDIR)/Rules.make
