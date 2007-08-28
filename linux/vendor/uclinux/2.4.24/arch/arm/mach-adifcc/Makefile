#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= adifcc.o

# Object file lists.

obj-y			:= arch.o xs80200-irq.o mm.o
obj-m			:= csum.o
obj-n			:=
obj-			:=

export-objs		:=

obj-$(CONFIG_ARCH_BRH)	+= brh-irq.o brh-pci.o pci.o pci-auto.o brh-time.o

include $(TOPDIR)/Rules.make
