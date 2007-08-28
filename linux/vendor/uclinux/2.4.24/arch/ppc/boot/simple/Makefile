# This is far from simple, but I couldn't think of a good name.  This is
# for making the 'zImage' or 'zImage.initrd' on a number of targets.
#
# Author: Tom Rini <trini@mvista.com>
#
# 2001-2002 (c) MontaVista, Software, Inc.  This file is licensed under
# the terms of the GNU General Public License version 2.  This program
# is licensed "as is" without any warranty of any kind, whether express
# or implied.
#
# Notes:
# (1) For machine targets which produce more than one image, define
# ZNETBOOT and ZNETBOOTRD to the image which should be available for
# 'znetboot' and 'znetboot.initrd`
# (2) For machine targets which use the mktree program, define END to be
# the machine name you want in the image, and you can optionally set
# ENTRYPOINT which the image should be loaded at.  The optimal setting
# for ENTRYPOINT is the link address.

USE_STANDARD_AS_RULE		:= true

# Normally, we use the 'misc-simple.c' file for decompress_kernel and
# whatnot.  Sometimes we need to override this however.
MISC				:= ../common/misc-simple.o
ifeq ($(CONFIG_EMBEDDEDBOOT),y)
ZIMAGE				:= zImage-EMBEDDED
ZIMAGEINITRD			:= zImage.initrd-EMBEDDED
TFTPIMAGE			:= /tftpboot/zImage.embedded
MISC				:= misc-embedded.o
endif
ifeq ($(CONFIG_IBM_OPENBIOS),y)
ZIMAGE				:= zImage-TREE
ZIMAGEINITRD			:= zImage.initrd-TREE
END				:= treeboot
ZNETBOOT			:= zImage.treeboot
ZNETBOOTRD			:= zImage.initrd.treeboot
TFTPIMAGE			:= /tftpboot/zImage.embedded
MISC				:= misc-embedded.o
endif
ifeq ($(CONFIG_EBONY),y)
ZIMAGE				:= zImage-TREE
ZIMAGEINITRD			:= zImage.initrd-TREE
END				:= ebony
ENTRYPOINT			:= 0x00800000
ZNETBOOT			:= zImage.treeboot
ZNETBOOTRD			:= zImage.initrd.treeboot
TFTPIMAGE			:= /tftpboot/zImage.$(END)
EXTRA				:= misc-44x.o
endif
ifeq ($(CONFIG_OCOTEA),y)
ZIMAGE				:= zImage-TREE
ZIMAGEINITRD			:= zImage.initrd-TREE
END				:= ocotea
ENTRYPOINT			:= 0x00800000
ZNETBOOT			:= zImage.treeboot
ZNETBOOTRD			:= zImage.initrd.treeboot
TFTPIMAGE			:= /tftpboot/zImage.$(END)
EXTRA				:= misc-44x.o
endif
ifeq ($(CONFIG_GEMINI),y)
ZIMAGE				:= zImage-SMON
ZIMAGEINITRD			:= zImage.initrd-SMON
TFTPIMAGE			:= /tftpboot/zImage.gemini
endif
# kbuild-2.4 'feature', only one of these will ever by 'y' at a time.
# The rest will be unset.
ifeq ($(CONFIG_LOPEC)$(CONFIG_PPLUS),y)
ZIMAGE				:= zImage-PPLUS
ZIMAGEINITRD			:= zImage.initrd-PPLUS
EXTRA				:= ../common/mpc10x_memory.o
TFTPIMAGE			:= /tftpboot/zImage.pplus
ZNETBOOT			:= zImage.pplus
ZNETBOOTRD			:= zImage.initrd.pplus
endif
ifeq ($(CONFIG_PPLUS),y)
EXTRA				:= legacy.o
endif
ifeq ($(CONFIG_PAL4),y)
ZIMAGE				:= zImage-PAL4
ZIMAGEINITRD			:= zImage.initrd-PAL4
TFTPIMAGE			:= /tftpboot/zImage.pal4
endif
ifeq ($(CONFIG_SPRUCE),y)
ZIMAGE				:= zImage-TREE
ZIMAGEINITRD			:= zImage.initrd-TREE
ENTRYPOINT			:= 0x00800000
MISC				:= misc-spruce.o
END				:= spruce
TFTPIMAGE			:= /tftpboot/zImage.$(END)
endif
ifeq ($(CONFIG_SMP),y)
TFTPIMAGE			+= .smp
endif

# Setup a default address to put ourselves, change it as needed.
LD_ARGS				= -T ../ld.script -Ttext 0x00800000 -Bstatic
ifdef CONFIG_8xx
LD_ARGS				:= -T ../ld.script -Ttext 0x00180000 -Bstatic
endif
ifeq ($(CONFIG_8260)$(CONFIG_40x),y)
LD_ARGS				:= -T ../ld.script -Ttext 0x00400000 -Bstatic
endif
OBJCOPY_ARGS			:= -O elf32-powerpc

# head.o and ../common/relocate.o must be at the start.
obj-y				:= head.o ../common/relocate.o $(EXTRA) \
				$(MISC) ../common/misc-common.o \
				../common/string.o ../common/util.o \
				../common/serial_stub.o
obj-$(CONFIG_40x)		+= embed_config.o
obj-$(CONFIG_8xx)		+= embed_config.o
obj-$(CONFIG_8260)		+= embed_config.o
obj-$(CONFIG_BSEIP)		+= iic.o
obj-$(CONFIG_MBX)		+= iic.o
obj-$(CONFIG_RPXCLASSIC)	+= iic.o
obj-$(CONFIG_RPXLITE)		+= iic.o
# Different boards need different serial implementations.
ifeq ($(CONFIG_SERIAL_CONSOLE),y)
obj-$(CONFIG_8xx)		+= m8xx_tty.o
obj-$(CONFIG_8260)		+= m8260_tty.o
obj-$(CONFIG_SERIAL)		+= ../common/ns16550.o
endif

LIBS				:= ../lib/zlib.a

# Tools
MKBUGBOOT			:= ../utils/mkbugboot
MKPREP				:= ../utils/mkprep
MKTREE				:= ../utils/mktree

zvmlinux: $(obj-y) $(LIBS) ../ld.script ../images/vmlinux.gz ../common/dummy.o
	$(OBJCOPY) $(OBJCOPY_ARGS) \
		--add-section=.image=../images/vmlinux.gz \
		--set-section-flags=.image=contents,alloc,load,readonly,data \
		../common/dummy.o image.o
	$(LD) $(LD_ARGS) -o $@ $(obj-y) image.o $(LIBS)
	$(OBJCOPY) $(OBJCOPY_ARGS) $@ $@ -R .comment -R .stab -R .stabstr \
		-R .ramdisk -R .sysmap

zvmlinux.initrd: $(obj-y) $(LIBS) ../ld.script ../images/vmlinux.gz \
		../common/dummy.o
	$(OBJCOPY) $(OBJCOPY_ARGS) \
		--add-section=.ramdisk=../images/ramdisk.image.gz \
		--set-section-flags=.ramdisk=contents,alloc,load,readonly,data \
		--add-section=.image=../images/vmlinux.gz \
		--set-section-flags=.image=contents,alloc,load,readonly,data \
		../common/dummy.o image.o
	$(LD) $(LD_ARGS) -o $@ $(obj-y) image.o $(LIBS)
	$(OBJCOPY) $(OBJCOPY_ARGS) $@ $@ -R .comment -R .stab -R .stabstr \
		-R .sysmap

# Sort-of dummy rules, that let us format the image we want.
zImage: $(ZIMAGE)
	rm -f zvmlinux
zImage.initrd: $(ZIMAGEINITRD)
	rm -f zvmlinux.initrd

znetboot: zImage
ifneq ($(ZNETBOOT),)
	cp ../images/$(ZNETBOOT) $(TFTPIMAGE)
else
	cp ../images/zImage.* $(TFTPIMAGE)
endif

znetboot.initrd: zImage.initrd
ifneq ($(ZNETBOOTRD),)
	cp ../images/$(ZNETBOOTRD) $(TFTPIMAGE)
else
	cp ../images/zImage.* $(TFTPIMAGE)
endif

zImage-EMBEDDED: zvmlinux
	mv zvmlinux ../images/zImage.embedded

zImage.initrd-EMBEDDED: zvmlinux.initrd
	mv zvmlinux.initrd ../images/zImage.initrd.embedded

zImage-PAL4: zvmlinux
	mv zvmlinux ../images/zImage.pal4

zImage.initrd-PAL4: zvmlinux.initrd
	mv zvmlinux.initrd ../images/zImage.initrd.pal4

zImage-PPLUS: zvmlinux $(MKPREP) $(MKBUGBOOT)
	$(MKPREP) -pbp zvmlinux ../images/zImage.pplus
	$(MKBUGBOOT) zvmlinux ../images/zImage.bugboot

zImage.initrd-PPLUS: zvmlinux.initrd $(MKPREP) $(MKBUGBOOT)
	$(MKPREP) -pbp zvmlinux.initrd ../images/zImage.initrd.pplus
	$(MKBUGBOOT) zvmlinux.initrd ../images/zImage.initrd.bugboot

zImage-SMON: zvmlinux
	dd if=zvmlinux of=../images/zImage.gemini skip=64 bs=1k

zImage.initrd-SMON: zvmlinux.initrd
	dd if=zvmlinux.initrd of=../images/zImage.initrd.gemini skip=64 bs=1k

zImage-TREE: zvmlinux
	$(MKTREE) zvmlinux ../images/zImage.$(END) $(ENTRYPOINT)

zImage.initrd-TREE: zvmlinux.initrd
	$(MKTREE) zvmlinux.initrd ../images/zImage.initrd.$(END) $(ENTRYPOINT)

include $(TOPDIR)/Rules.make
