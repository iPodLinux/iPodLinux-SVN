#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= ixp1200.o

# Object file lists.

obj-y			:= arch.o mm.o ixp-pci.o irq.o pci.o pci-auto.o
obj-m			:=
obj-n			:=
obj-			:=

export-objs		:= 

include $(TOPDIR)/Rules.make
