#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= ixp425.o

# Object file lists.

obj-y			:= arch.o  mm.o ixp425-irq.o ixp425-pci.o pci.o \
				pci-auto.o gpio.o pci-io.o \
				ixp425-time.o pcibuf.o
obj-m			:=
obj-n			:=
obj-			:=

export-objs		:= pcibuf.o pci-io.o gpio.o ixp425-pci.o

obj-$(CONFIG_ARCH_IXDP425)	+= ixdp425-pci.o
obj-$(CONFIG_ARCH_PRPMC1100)	+= prpmc1100-pci.o
obj-$(CONFIG_ARCH_ADI_COYOTE)	+= coyote-pci.o

obj-$(CONFIG_KGDB) 		+= kgdb.o

include $(TOPDIR)/Rules.make
