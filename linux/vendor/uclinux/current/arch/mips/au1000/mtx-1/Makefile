#
#  Copyright 2003 MontaVista Software Inc.
#  Author: MontaVista Software, Inc.
#     	ppopov@mvista.com or source@mvista.com
#       Bruno Randolf <bruno.randolf@4g-systems.de>
#
# Makefile for 4G Systems MTX-1 board.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

.S.s:
	$(CPP) $(CFLAGS) $< -o $*.s
.S.o:
	$(CC) $(CFLAGS) -c $< -o $*.o

O_TARGET := mtx-1.o

obj-y := init.o board_setup.o irqmap.o

include $(TOPDIR)/Rules.make
