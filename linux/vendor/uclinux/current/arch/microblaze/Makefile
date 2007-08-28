#
# arch/microblaze/Makefile
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


CFLAGS += -fno-builtin
CFLAGS += -DNO_MM -DNO_FPU -D__ELF__
CFLAGS += $(UTS_SYSNAME) -D__linux__

ifdef CONFIG_MICROBLAZE_HARD_MULT
	CFLAGS += -mno-xl-soft-mul
endif

ifdef CONFIG_MICROBLAZE_HARD_DIV
	CFLAGS += -mno-xl-soft-div
endif

ifdef CONFIG_MICROBLAZE_HARD_BARREL
	CFLAGS += -mxl-barrel-shift
endif

LDFLAGS += --defsym _STACK_SIZE=0x00

ARCH_DIR = arch/$(ARCH)


HEAD := $(ARCH_DIR)/kernel/head.o

SUBDIRS += $(ARCH_DIR)/kernel $(ARCH_DIR)/lib $(ARCH_DIR)/xilinx_ocp

CORE_FILES := $(ARCH_DIR)/kernel/kernel.o $(CORE_FILES)
CORE_FILES += $(ARCH_DIR)/xilinx_ocp/xilinx_ocp.o

LIBS += $(ARCH_DIR)/lib/lib.a 


# Deal with the initial contents of the blkmem device
ifdef ROOT_FS_IMAGE 
# Embed it in the kernel

ROOT_FS_IMAGE_OBJ = root_fs_image.o

CORE_FILES += $(ROOT_FS_IMAGE_OBJ)

$(LINUX) : $(ROOT_FS_IMAGE_OBJ)

# Note that we use the build-system's objcopy, as the microblaze tools 
# are fairly old, and don't have the --rename-section option.
$(ROOT_FS_IMAGE_OBJ): $(ROOT_FS_IMAGE)
	objcopy -I binary -O elf32-big --rename-section .data=.root,alloc,load,readonly,data,contents $< $@

endif # ROOT_FS_IMAGE


#kernel linker script
LINKFLAGS = -T linux.ld --defsym _STACK_SIZE=0x00 
$(LINUX) : linux.ld
linux.ld: $(ARCH_DIR)/linux.ld.in
	$(CPP) -P -x assembler-with-cpp $(AFLAGS) $< > $@


#
# If you want the kernel build to build modules outside of the tree
# then define this and pass it to the main linux makefile
#
ifdef EXTRA_MODULE_DIRS
SUBDIRS += $(EXTRA_MODULE_DIRS)
endif

bootstrap:
	@$(MAKEBOOT) bootstrap

archmrproper:

archdep:

archclean:
	rm -f $(LINUX)
	rm -f $(ARCH_DIR)/kernel/microblaze_defs.h $(ARCH_DIR)/kernel/microblaze_defs.d

include $(TOPDIR)/Rules.make
