#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= ipod.o

# Object file lists.

obj-y += arch.o cop.o dma.o hardware.o irq.o time.o

obj-$(CONFIG_AUDIO_IPOD)	+= audio.o
obj-$(CONFIG_FB_IPOD)		+= fb.o
obj-$(CONFIG_IEEE1394_IPOD)	+= tsb43aa82.o
obj-$(CONFIG_KB_IPOD)		+= keyboard.o
obj-$(CONFIG_REMOTE_IPOD)	+= ipodremote.o

export-objs		:= audio.o fb.o tsb43aa82.o keyboard.o ipodremote.o hardware.o

include $(TOPDIR)/Rules.make
