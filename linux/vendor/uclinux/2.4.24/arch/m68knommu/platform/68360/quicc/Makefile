#
# Makefile for the linux MC68360 CPM access routines
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := quicc360.a
O_OBJS	= commproc.o

ifdef CONFIG_M68360_UART
O_OBJS += uart.o
endif

ifdef CONFIG_M68EN360_ETHERNET
O_OBJS += enet.o
endif

ifdef CONFIG_M68EN360_HDLC
O_OBJS += hdlc.o
endif

include $(TOPDIR)/Rules.make
