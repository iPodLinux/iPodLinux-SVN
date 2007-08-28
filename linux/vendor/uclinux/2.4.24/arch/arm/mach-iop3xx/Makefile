#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= iop3xx.o

# Object file lists.

obj-y			:= arch.o pci.o pci-auto.o

obj-n			:=
obj-			:=

export-objs		:=

obj-m			:=

obj-$(CONFIG_ARCH_IOP310)  += xs80200-irq.o iop310-irq.o iop310-pci.o mm.o

obj-$(CONFIG_ARCH_IQ80310) += iq80310-pci.o iq80310-irq.o 

obj-$(CONFIG_ARCH_IOP321)  += iop321-irq.o iop321-pci.o mm-321.o iop321-time.o

obj-$(CONFIG_ARCH_IQ80321) += iq80321-pci.o 

ifeq ($(CONFIG_ARCH_IQ80310),y)
   ifneq ($(CONFIG_XSCALE_PMU_TIMER),y)
      obj-y			+= iq80310-time.o
   endif
endif

obj-$(CONFIG_IOP3XX_AAU) += aau.o
obj-$(CONFIG_IOP3XX_DMA) += dma.o
obj-$(CONFIG_IOP3XX_MU) += message.o
obj-$(CONFIG_IOP3XX_PMON) += pmon.o

include $(TOPDIR)/Rules.make
