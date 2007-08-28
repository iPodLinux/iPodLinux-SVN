#
# Makefile for the Xilinx On Chip Peripheral support code
#

EXTRA_CFLAGS		+=

all: xilinx_ocp.o

USE_STANDARD_AS_RULE		:= TRUE

O_TARGET := xilinx_ocp.o
obj-y := xbasic_types.o xdma_channel.o xdma_channel_sg.o \
	 xipif_v1_23_b.o xpacket_fifo_v1_00_b.o xversion.o

# Linux file to EXPORT_SYMBOL all of the Xilinx entries.
export-objs		+= xilinx_syms.o

include $(TOPDIR)/Rules.make
