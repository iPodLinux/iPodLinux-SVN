#
# Makefile for the Xilinx GPIO driver
#

EXTRA_CFLAGS		+= -I$(TOPDIR)/arch/microblaze/xilinx_ocp

list-multi		:= xilinx_gpio.o

# The Linux adapter for the Xilinx driver code.
xilinx_gpio-objs	+= adapter.o

# The Xilinx OS independent code.
xilinx_gpio-objs	+= xgpio.o xgpio_g.o

obj-$(CONFIG_XILINX_GPIO) := xilinx_gpio.o

xilinx_gpio.o: $(xilinx_gpio-objs)
	$(LD) -r -o $@ $(xilinx_gpio-objs)

include $(TOPDIR)/Rules.make
