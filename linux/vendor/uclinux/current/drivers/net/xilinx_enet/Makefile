#
# Makefile for the Xilinx ethernet driver
#

EXTRA_CFLAGS		+= -I$(TOPDIR)/arch/microblaze/xilinx_ocp

list-multi		:= xilinx_enet.o

# The Linux adapter for the Xilinx driver code.
xilinx_enet-objs	+= adapter.o

# The Xilinx OS independent code.
xilinx_enet-objs	+= xemac.o xemac_g.o xemac_intr.o xemac_intr_dma.o \
			   xemac_intr_fifo.o xemac_options.o xemac_phy.o

obj-$(CONFIG_XILINX_ENET) := xilinx_enet.o

xilinx_enet.o: $(xilinx_enet-objs)
	$(LD) -r -o $@ $(xilinx_enet-objs)

include $(TOPDIR)/Rules.make
