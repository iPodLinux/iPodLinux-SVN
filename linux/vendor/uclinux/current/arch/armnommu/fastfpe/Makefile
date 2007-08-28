#
# linux/arch/arm/fastfpe/Makefile
#
# Copyright (C) Peter Teichmann
#

O_TARGET := fast-math-emu.o

obj-y	:=
obj-m	:=
obj-n	:=
obj-	:=

fastfpe-objs := module.o entry.o CPDO.o CPRT.o CPDT.o

list-multi := fastfpe.o

obj-$(CONFIG_FPE_FASTFPE) += fastfpe.o

include $(TOPDIR)/Rules.make

fastfpe.o: $(fastfpe-objs)
	 $(LD) -r -o $@ $(fastfpe-objs)
