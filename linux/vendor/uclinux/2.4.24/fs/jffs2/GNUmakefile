# GNUmakefile for JFFS2 build outside kernel tree
# $Id: GNUmakefile,v 1.1 2003/08/28 11:55:10 davidm Exp $

LINUXDIR=/lib/modules/$(shell uname -r)/build

ifndef VERSION
# Someone just typed 'make' in this directory.
# Invoke a kernel build process, with only this dir in SUBDIRS.

modules:
	$(MAKE) -C $(LINUXDIR) CONFIG_JFFS2_FS_NAND=y SUBDIRS=`pwd` modules

nonand:
	make -C $(LINUXDIR) SUBDIRS=`pwd` modules

dep:
	make -C $(LINUXDIR) SUBDIRS=`pwd` dep

clean:
	rm -f *.o

else

ifndef CONFIG_JFFS2_FS
# Invoked from a kernel build tree, but CONFIG_JFFS2_FS isn't set. 
# That means we must have got here from the hack above - pretend
# CONFIG_JFFS2_FS _was_ set so that we actually build it, and also
# pull in the local MTD include files in case they're also newer than
# the ones in the kernel tree. And turn on debugging. 

CC += -I$(shell pwd)/../../include

CONFIG_JFFS2_FS := m
EXTRA_CFLAGS += -DCONFIG_JFFS2_FS_DEBUG=1 -g -Werror

ifeq ($(CONFIG_JFFS2_FS_NAND),y)
EXTRA_CFLAGS += -DCONFIG_JFFS2_FS_NAND=1
endif
endif

# Check whether we've put the JFFS2 stuff in the superblock and inode unions
OUT_OF_KERNEL_CFLAGS := $(shell grep -q jffs2 $(TOPDIR)/include/linux/fs.h || echo "-DJFFS2_OUT_OF_KERNEL")
EXTRA_CFLAGS += $(OUT_OF_KERNEL_CFLAGS)


include Makefile


endif
