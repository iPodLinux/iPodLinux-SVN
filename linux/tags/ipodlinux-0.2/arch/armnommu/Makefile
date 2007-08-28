#
# arch/armnommu/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1995-2001 by Russell King
# Addition of S3C4530 by Arcturus Networks Inc.

LINKFLAGS	:=-p -X -T arch/armnommu/vmlinux.lds
GZFLAGS		:=-9
CFLAGS		+=-fno-common -pipe -fno-builtin -D__linux__

ifneq ($(CONFIG_NO_FRAME_POINTER),y)
CFLAGS		:=$(CFLAGS:-fomit-frame-pointer=)
endif

ifeq ($(CONFIG_DEBUG_INFO),y)
CFLAGS		+=-g
endif

CFLAGS += -DNO_MM
AFLAGS += -DNO_MM

# Select CPU dependent flags.  Note that order of declaration is important;
# the options further down the list override previous items.
#
apcs-$(CONFIG_CPU_26)		:=-mapcs-26 -mcpu=arm3 -Os
apcs-$(CONFIG_CPU_32)		:=-mapcs-32

arch-$(CONFIG_CPU_32v3)		:=-march=armv3
arch-$(CONFIG_CPU_32v4)		:=-march=armv4
arch-$(CONFIG_CPU_32v5)		:=-march=armv5

proc-$(CONFIG_CPU_32v3)		:=-marmv3m
proc-$(CONFIG_CPU_32v4)		:=-marmv4
proc-$(CONFIG_CPU_32v5)		:=-marmv5

tune-$(CONFIG_CPU_ARM7V3)	:=-mtune=arm610
tune-$(CONFIG_CPU_ARM610)	:=-mtune=arm610
#tune-$(CONFIG_CPU_ARM710)	:=-mtune=arm710
tune-$(CONFIG_CPU_ARM710)	:=-mtune=arm7tdmi
tune-$(CONFIG_CPU_ARM720T)	:=-mtune=arm7tdmi
tune-$(CONFIG_CPU_ARM740T)	:=-mtune=arm7tdmi
tune-$(CONFIG_CPU_ARM920T)	:=-mtune=arm9tdmi
tune-$(CONFIG_CPU_SA110)	:=-mtune=strongarm110
tune-$(CONFIG_CPU_SA1100)	:=-mtune=strongarm1100

ifeq ($(CONFIG_CPU_BIG_ENDIAN),y)
CFLAGS    	+= -mbig-endian
AFLAGS		+= -mbig-endian
LINKFLAGS	+= -EB
LDFLAGS		+= -EB
endif

CFLAGS		+= $(apcs-y) $(arch-y) $(tune-y) -mshort-load-bytes -msoft-float
AFLAGS		+= $(apcs-y) $(proc-y) -msoft-float -mno-fpu

LIBGCC		:= $(shell $(CC) $(CFLAGS) --print-libgcc-file-name)

ifeq ($(CONFIG_CPU_26),y)
PROCESSOR	 = armo
TEXTADDR	 = 0x02080000
endif

ifeq ($(CONFIG_CPU_32),y)
PROCESSOR	 = armv
TEXTADDR	 = 0xC0008000
endif

ifeq ($(CONFIG_ARCH_ARCA5K),y)
MACHINE		 = arc
endif

ifeq ($(CONFIG_ARCH_RPC),y)
MACHINE		 = rpc
endif

ifeq ($(CONFIG_ARCH_EBSA110),y)
MACHINE		 = ebsa110
endif

ifeq ($(CONFIG_ARCH_CLPS7500),y)
MACHINE		 = clps7500
INCDIR		 = cl7500
endif

ifeq ($(CONFIG_FOOTBRIDGE),y)
MACHINE		 = footbridge
INCDIR		 = ebsa285
endif

ifeq ($(CONFIG_ARCH_CO285),y)
TEXTADDR	 = 0x60008000
MACHINE		 = footbridge
INCDIR		 = ebsa285
endif

ifeq ($(CONFIG_ARCH_NEXUSPCI),y)
MACHINE		 = nexuspci
endif

ifeq ($(CONFIG_ARCH_SHARK),y)
MACHINE		 = shark
endif

ifeq ($(CONFIG_ARCH_SA1100),y)
ifeq ($(CONFIG_SA1111),y)
# SA1111 DMA bug: we don't want the kernel to live in precious DMA-able memory
TEXTADDR	 = 0xc0208000
endif
MACHINE		 = sa1100
endif

ifeq ($(CONFIG_ARCH_L7200),y)
MACHINE		 = l7200
endif

ifeq ($(CONFIG_ARCH_INTEGRATOR),y)
MACHINE		 = integrator
TEXTADDR	 = 0x00008000
endif

ifeq ($(CONFIG_ARCH_CLPS711X),y)
TEXTADDR	 = 0xc0018000
MACHINE		 = clps711x
endif

ifeq ($(CONFIG_ARCH_DSC21),y)
MACHINE 	 = dsc21
TEXTADDR	 = 0x02000000
DATAADDR   = 0x08000000
INCDIR     = dsc21
#LINKFLAGS	 = -X -T arch/armnommu/vmlinux.lds
endif

ifeq ($(CONFIG_ARCH_P52),y)
TEXTADDR	 = 0x900000
MACHINE		 = p52
CFLAGS		+= -DNO_MM -DMAGIC_ROM_PTR
endif

ifeq ($(CONFIG_ARCH_SPIPE),y)
TEXTADDR	 = 0x808000
MACHINE		 = spipe
endif

ifeq ($(CONFIG_ARCH_ATMEL),y)
TEXTADDR	= 0x1000000
MACHINE		= atmel
endif

ifeq ($(CONFIG_ARCH_NETARM),y)
MACHINE 	 = netarm
TEXTADDR	 = 0x00008000
INCDIR  	 = netarm
CFLAGS		+= -mcpu=arm7tdmi
#LINKFLAGS	 = -X -T arch/armnommu/vmlinux.lds
endif

ifeq ($(CONFIG_ARCH_SWARM),y)
MACHINE    = swarm
TEXTADDR   = 0x00008000
endif

ifeq ($(CONFIG_BOARD_SNDS100),y)
TEXTADDR   	 = 0x00008000
MACHINE    	 = snds100
endif

ifeq ($(CONFIG_BOARD_EVS3C4530HEI),y)
TEXTADDR         = 0x00020000
MACHINE          = evS3C4530HEI
INCDIR           = $(MACHINE)
endif

ifeq ($(CONFIG_ARCH_IPOD),y)
MACHINE		 = ipod
TEXTADDR	 = 0x28008000
endif

export	LDFLAGS LIBGCC MACHINE PROCESSOR TEXTADDR GZFLAGS

# Only set INCDIR if its not already defined above
# Grr, ?= doesn't work as all the other assignment operators do.  Make bug?
ifeq ($(origin INCDIR), undefined)
INCDIR		:= $(MACHINE)
endif

# If we have a machine-specific directory, then include it in the build.
MACHDIR		:= arch/armnommu/mach-$(MACHINE)
ifeq ($(MACHDIR),$(wildcard $(MACHDIR)))
SUBDIRS		+= $(MACHDIR)
CORE_FILES	:= $(MACHDIR)/$(MACHINE).o $(CORE_FILES)
endif

ifeq ($(CONFIG_ARCH_DSC21),y)
HEAD		:= arch/armnommu/kernel/head-arm-dsc21.o \
		   arch/armnommu/kernel/init_task.o
else
HEAD		:= arch/armnommu/kernel/head-$(PROCESSOR).o \
		   arch/armnommu/kernel/init_task.o
endif
SUBDIRS		+= arch/armnommu/kernel arch/armnommu/mm arch/armnommu/lib arch/armnommu/nwfpe
CORE_FILES	:= arch/armnommu/kernel/kernel.o arch/armnommu/mm/mm.o $(CORE_FILES)
LIBS		:= arch/armnommu/lib/lib.a $(LIBS) $(LIBGCC)

ifeq ($(CONFIG_NWFPE),y)
LIBS		:= arch/armnommu/nwfpe/math-emu.o $(LIBS)
endif

ifeq ($(CONFIG_ARCH_CLPS7500),y)
SUBDIRS		+= drivers/acorn/char
DRIVERS		+= drivers/acorn/char/acorn-char.o
endif

ifeq ($(CONFIG_ARCH_NETARM),y)
HEAD		:= arch/armnommu/kernel/head-arm-netarm.o \
		   arch/armnommu/kernel/init_task.o
endif

ifeq ($(CONFIG_ARCH_IPOD),y)
HEAD		:= arch/armnommu/kernel/head-arm-ipod.o \
		   arch/armnommu/kernel/init_task.o
endif

MAKEBOOT	 = $(MAKE) -C arch/$(ARCH)/boot LINUX=$(LINUX)
MAKETOOLS	 = $(MAKE) -C arch/$(ARCH)/tools LINUX=$(LINUX)

# The following is a hack to get 'constants.h' up
# to date before starting compilation

$(patsubst %,_dir_%, $(SUBDIRS)): maketools
$(patsubst %,_modsubdir_%,$(MOD_DIRS)): maketools

symlinks: archsymlinks

archsymlinks:
	$(RM) include/asm-armnommu/arch include/asm-armnommu/proc
	(cd include/asm-armnommu; ln -sf arch-$(INCDIR) arch; ln -sf proc-$(PROCESSOR) proc)

$(LINUX): arch/armnommu/vmlinux.lds

arch/armnommu/vmlinux.lds: arch/armnommu/vmlinux-$(PROCESSOR).lds.in dummy
ifeq ($(CONFIG_ARCH_DSC21),y)
	@sed 's/TEXTADDR/$(TEXTADDR)/' <$< >tmp.ld
	@sed 's/DATAADDR/$(DATAADDR)/' <tmp.ld >$@
	$(RM) tmp.ld
else
	@sed 's/TEXTADDR/$(TEXTADDR)/' <$< >$@	
endif

arch/armnommu/kernel arch/armnommu/mm arch/armnommu/lib: dummy
	$(MAKE) CFLAGS="$(CFLAGS) $(CFLAGS_KERNEL)" $(subst $@, _dir_$@, $@)

bzImage zImage zinstall Image bootpImage install: $(LINUX)
	@$(MAKEBOOT) $@

CLEAN_FILES	+= \
	arch/armnommu/vmlinux.lds \
	include/asm-armnommu/constants.h* \
	include/asm-armnommu/mach-types.h

MRPROPER_FILES	+= \
	include/asm-armnommu/arch \
	include/asm-armnommu/proc

# We use MRPROPER_FILES and CLEAN_FILES now
archmrproper:
	@/bin/true

archclean:
	@$(MAKEBOOT) clean

archdep: scripts/mkdep archsymlinks
	@$(MAKETOOLS) dep
	@$(MAKEBOOT) dep

maketools: checkbin
	@$(MAKETOOLS) all

# Ensure this is ld "2.9.4" or later
NEW_LINKER	:= $(shell $(LD) --gc-sections --version >/dev/null 2>&1; echo $$?)

ifneq ($(NEW_LINKER),0)
checkbin:
	@echo '*** ${VERSION}.${PATCHLEVEL} kernels no longer build correctly with old versions of binutils.'
	@echo '*** Please upgrade your binutils to 2.9.5.'
	@false
else
checkbin:
	@true
endif

# My testing targets (that short circuit a few dependencies)
zImg:;	@$(MAKEBOOT) zImage
Img:;	@$(MAKEBOOT) Image
i:;	@$(MAKEBOOT) install
zi:;	@$(MAKEBOOT) zinstall
bp:;	@$(MAKEBOOT) bootpImage

#
# Configuration targets.  Use these to select a
# configuration for your architecture
%_config:
	@( \
	CFG=$(@:_config=); \
	if [ -f arch/armnommu/def-configs/$$CFG ]; then \
	  $(RM) arch/armnommu/defconfig; \
	  cp arch/armnommu/def-configs/$$CFG arch/armnommu/defconfig; \
	  echo "*** Default configuration for $$CFG installed"; \
	  echo "*** Next, you may run 'make oldconfig'"; \
	else \
	  echo "$$CFG does not exist"; \
	fi; \
	)
