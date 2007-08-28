#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= SMDK2500.o

# Object file lists.

obj-y			:= $(patsubst %.c, %.o, $(wildcard *.c))
obj-m			:=
obj-n			:=
obj-			:=

export-objs		:= 

include $(TOPDIR)/Rules.make
