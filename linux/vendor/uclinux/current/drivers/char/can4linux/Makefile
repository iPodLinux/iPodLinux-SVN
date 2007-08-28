#
# Makefile for the linux CAN drivers.
#
# (C) H.-J. Oertel oe@port.de
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
#
# $Source: /var/cvs/uClinux-2.4.x/drivers/char/can4linux/Makefile,v $
# $Revision: 1.2 $
# $Date: 2003/08/28 00:38:31 $



# Used release tag for this software version
VERSION=3
REL=0
RELEASE=CAN4LINUX-$(VERSION)_$(REL)
DVERSION=$(VERSION).$(REL)


#
# The CAN driver major device number
# The new linux/Documentation/devices.txt defines major=91
CAN_MAJOR=	91

EXTRA_CFLAGS +=  -DVERSION=\"$(DVERSION)_$(TARGET)\" -DCan_MAJOR=$(CAN_MAJOR) \
	# -DDEBUG=1 -DDEFAULT_DEBUG

O_TARGET := Can.o

obj-y   := can_core.o can_open.o can_select.o can_sysctl.o can_write.o \
	   can_close.o can_ioctl.o can_read.o can_util.o \
	   can_debug.o can_error.o

##########################################################################
ifeq ($(CONFIG_CAN_ATCANMINI),y)
obj-y += can_sja1000funcs.o
EXTRA_CFLAGS += -DATCANMINI_PELICAN -DMAX_CHANNELS=2
TARGET=ATCANMINI_PELICAN
endif
##########################################################################
ifeq ($(CONFIG_CAN_CCPC104),y)
obj-y += can_sja1000funcs.o
EXTRA_CFLAGS += -DCCPC104 -DMAX_CHANNELS=2
TARGET=CTRLink_PC104
endif
##########################################################################
ifeq ($(CONFIG_CAN_MCF5282),y)
obj-y += can_mcf5282funcs.o
EXTRA_CFLAGS += -DMCF5282 -DMAX_CHANNELS=1
TARGET=ColdFire_FlexCAN
endif
##########################################################################
ifeq ($(CONFIG_CAN_MCF5282_MODULE),y)
obj-y += can_mcf5282funcs.o
EXTRA_CFLAGS += -DMCF5282 -DMAX_CHANNELS=1
TARGET=ColdFire_FlexCAN
endif

obj-y += can_mcf5282funcs.o
EXTRA_CFLAGS += -DMCF5282 -DMAX_CHANNELS=1
TARGET=ColdFire_FlexCAN

##########################################################################
           
obj-m   := $(O_TARGET)



include $(TOPDIR)/Rules.make


