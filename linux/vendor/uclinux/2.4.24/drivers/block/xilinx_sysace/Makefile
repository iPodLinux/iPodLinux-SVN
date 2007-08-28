#
# Makefile for the Xilinx System ACE driver
#

EXTRA_CFLAGS		+= -I$(TOPDIR)/arch/microblaze/xilinx_ocp

list-multi		:= xilinx_sysace.o

# The Linux adapter for the Xilinx driver code.
xilinx_sysace-objs	+= adapter.o

# The Xilinx OS independent code.
xilinx_sysace-objs	+= xsysace.o xsysace_g.o xsysace_intr.o xsysace_l.o
xilinx_sysace-objs	+= xsysace_compactflash.o xsysace_jtagcfg.o

obj-$(CONFIG_XILINX_SYSACE) := xilinx_sysace.o

xilinx_sysace.o: $(xilinx_sysace-objs)
	$(LD) -r -o $@ $(xilinx_sysace-objs)

include $(TOPDIR)/Rules.make
