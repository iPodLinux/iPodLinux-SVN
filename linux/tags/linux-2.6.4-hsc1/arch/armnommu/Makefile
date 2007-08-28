#
# arch/armnommu/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2004 by Hyok S. Choi (for uClinux/ARM 2.6)
# Copyright (C) 1995-2001 by Russell King

LDFLAGS_vmlinux	:=-p -X
LDFLAGS_BLOB	:=--format binary
AFLAGS_vmlinux.lds.o = -DTEXTADDR=$(TEXTADDR) -DDATAADDR=$(DATAADDR)
OBJCOPYFLAGS	:=-O binary -R .note -R .comment -S
GZFLAGS		:=-9
CFLAGS		+=-fno-builtin -D__linux__

CFLAGS		:=$(CFLAGS:-O2=-Os)

ifeq ($(CONFIG_FRAME_POINTER),y)
CFLAGS		+=-fno-omit-frame-pointer -mapcs -mno-sched-prolog
endif

ifeq ($(CONFIG_CPU_BIG_ENDIAN),y)
CFLAGS		+= -mbig-endian
AS		+= -EB
LD		+= -EB
AFLAGS		+= -mbig-endian
else
CFLAGS		+= -mlittle-endian
AS		+= -EL
LD		+= -EL
AFLAGS		+= -mlittle-endian
endif

comma = ,

# This selects which instruction set is used.
# Note that GCC does not numerically define an architecture version
# macro, but instead defines a whole series of macros which makes
# testing for a specific architecture or later rather impossible.
arch-$(CONFIG_CPU_32v6)		:=-D__LINUX_ARM_ARCH__=6 -march=armv5t -Wa,-march=armv6
arch-$(CONFIG_CPU_32v5)		:=-D__LINUX_ARM_ARCH__=5 $(call check_gcc,-march=armv5te,-march=armv4)
arch-$(CONFIG_CPU_32v4)		:=-D__LINUX_ARM_ARCH__=4 -march=armv4
arch-$(CONFIG_CPU_32v3)		:=-D__LINUX_ARM_ARCH__=3 -march=armv3

# This selects how we optimise for the processor.
tune-$(CONFIG_CPU_ARM7V3)	:=-mtune=arm610
tune-$(CONFIG_CPU_ARM610)	:=-mtune=arm610
tune-$(CONFIG_CPU_ARM710)	:=-mtune=arm7tdmi
tune-$(CONFIG_CPU_ARM720T)	:=-mtune=arm7tdmi
tune-$(CONFIG_CPU_ARM740T)	:=-mtune=arm7tdmi
tune-$(CONFIG_CPU_ARM920T)	:=-mtune=arm9tdmi
tune-$(CONFIG_CPU_ARM922T)	:=-mtune=arm9tdmi
tune-$(CONFIG_CPU_ARM926T)	:=-mtune=arm9tdmi
tune-$(CONFIG_CPU_ARM940T)	:=-mtune=arm9tdmi
tune-$(CONFIG_CPU_SA110)	:=-mtune=strongarm110
tune-$(CONFIG_CPU_SA1100)	:=-mtune=strongarm1100
tune-$(CONFIG_CPU_XSCALE)	:=$(call check_gcc,-mtune=xscale,-mtune=strongarm110) -Wa,-mcpu=xscale
tune-$(CONFIG_CPU_V6)		:=-mtune=strongarm

# Need -Uarm for gcc < 3.x
CFLAGS_BOOT	:=-mapcs-32 $(arch-y) $(tune-y) -mshort-load-bytes -msoft-float -Wa,-mno-fpu -Uarm
CFLAGS		+=-mapcs-32 $(arch-y) $(tune-y) -mshort-load-bytes -msoft-float -Wa,-mno-fpu -Uarm
AFLAGS		+=-mapcs-32 $(arch-y) $(tune-y) -msoft-float -Wa,-mno-fpu

# settings for endian
ifeq ($(CONFIG_CPU_BIG_ENDIAN),y)
  LDFLAGS_BLOB  += --oformat elf32-bigarm
else
  LDFLAGS_BLOB  += --oformat elf32-littlearm
endif

# These are the default values for independt
DATAADDR	:= .
PROCESSOR	:= armv
head-y		:= arch/armnommu/kernel/head.o 	# default head
textaddr-y	:= 0x00008000			# default text address

# setup the machine name and the machine dependent settings
#   in alphabetical order.
machine-$(CONFIG_ARCH_ATMEL)		:= atmel
	textaddr-$(CONFIG_ARCH_ATMEL)		:= 0x01000000
machine-$(CONFIG_ARCH_S3C3410)		:= s3c3410
	textaddr-$(CONFIG_ARCH_S3C3410)		:= 0x01020000
machine-$(CONFIG_ARCH_S5C7375) 		:= s5c7375
	textaddr-$(CONFIG_ARCH_S5C7375) 	:= 0x00008000

# set the environment variables and export
MACHINE  := $(machine-y)
TEXTADDR := $(textaddr-y)
ifeq ($(incdir-y),)
incdir-y := $(MACHINE)
endif
INCDIR   := $(incdir-y)
  
export	MACHINE PROCESSOR TEXTADDR GZFLAGS CFLAGS_BOOT

# Does the machine has its own head.S ?
HEADMACH 	:= arch/armnommu/mach-$(MACHINE)/head.S
ifeq ($(HEADMACH), $(wildcard $(HEADMACH)))
head-y		:= arch/armnommu/mach-$(MACHINE)/head.o
HEADMACH	:= ../mach-$(MACHINE)/head.o
else
HEADMACH 	:=
endif
export HEADMACH

# add the init_task to head
head-y		+= arch/armnommu/kernel/init_task.o

# Do we have FASTFPE?
FASTFPE		:=arch/armnommu/fastfpe
ifeq ($(FASTFPE),$(wildcard $(FASTFPE)))
FASTFPE_OBJ	:=$(FASTFPE)/
endif

# If we have a machine-specific directory, then include it in the build.
core-y				+= arch/armnommu/kernel/ arch/armnommu/mm/ arch/armnommu/common/
ifneq ($(MACHINE),)
core-y				+= arch/armnommu/mach-$(MACHINE)/
endif
core-$(CONFIG_FPE_NWFPE)	+= arch/armnommu/nwfpe/
core-$(CONFIG_FPE_FASTFPE)	+= $(FASTFPE_OBJ)

libs-y				+= arch/armnommu/lib/

# Default target when executing plain make
all: uCImage

boot := arch/armnommu/boot

#	Update machine arch and proc symlinks if something which affects
#	them changed.  We use .arch and .proc to indicate when they were
#	updated last, otherwise make uses the target directory mtime.

include/asm-armnommu/.arch: $(wildcard include/config/arch/*.h)
	@echo '  Making asm-armnommu/arch -> asm-armnommu/arch-$(INCDIR) symlink'
	@rm -f include/asm-armnommu/arch
	@ln -sf arch-$(INCDIR) include/asm-armnommu/arch
	@touch $@

prepare: maketools

.PHONY: maketools FORCE
maketools: include/asm-armnommu/.arch \
	   include/asm-armnommu/constants.h include/linux/version.h FORCE
	$(Q)$(MAKE) $(build)=arch/armnommu/tools include/asm-armnommu/mach-types.h

# Convert bzImage to zImage
bzImage: vmlinux
	$(Q)$(MAKE) $(build)=$(boot) $(boot)/zImage

zImage Image bootpImage uImage: vmlinux
	$(Q)$(MAKE) $(build)=$(boot) $(boot)/$@

uCImage: vmlinux linux.bin

linux.bin: vmlinux FORCE
	@$(OBJCOPY) $(OBJCOPYFLAGS) vmlinux $@
	@echo '  Kernel: $@ is ready'
	

zinstall install: vmlinux
	$(Q)$(MAKE) $(build)=$(boot) $@

CLEAN_FILES += include/asm-armnommu/constants.h* include/asm-armnommu/mach-types.h
CLEAN_FILES += linux.bin linux arch/armnommu/kernel/entry-macro.S
MRPROPER_FILES += include/asm-armnommu/arch include/asm-armnommu/.arch

# We use MRPROPER_FILES and CLEAN_FILES now
archclean:
	$(Q)$(MAKE) $(clean)=$(boot)

# My testing targets (that short circuit a few dependencies)
zImg:;	$(Q)$(MAKE) $(build)=$(boot) $(boot)/zImage
Img:;	$(Q)$(MAKE) $(build)=$(boot) $(boot)/Image
bp:;	$(Q)$(MAKE) $(build)=$(boot) $(boot)/bootpImage
i:;	$(Q)$(MAKE) $(build)=$(boot) install
zi:;	$(Q)$(MAKE) $(build)=$(boot) zinstall

arch/$(ARCH)/kernel/asm-offsets.s: include/asm include/linux/version.h \
				   include/asm-armnommu/.arch \
				   include/config/MARKER

include/asm-$(ARCH)/constants.h: arch/$(ARCH)/kernel/asm-offsets.s
	$(call filechk,gen-asm-offsets)

define archhelp
  echo  '* zImage        - Compressed kernel image (arch/$(ARCH)/boot/zImage)'
  echo  '  Image         - Uncompressed kernel image (arch/$(ARCH)/boot/Image)'
  echo  '  uCImage	 - Uncompressed kernel and the stripped binary (linux.bin)'
  echo  '  bootpImage    - Combined zImage and initial RAM disk' 
  echo  '  install       - Install uncompressed kernel'
  echo  '  zinstall      - Install compressed kernel'
  echo  '                  Install using (your) ~/bin/installkernel or'
  echo  '                  (distribution) /sbin/installkernel or'
  echo  '                  install to $$(INSTALL_PATH) and run lilo'
endef
