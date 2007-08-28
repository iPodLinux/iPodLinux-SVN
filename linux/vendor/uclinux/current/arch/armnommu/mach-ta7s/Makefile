#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= ta7s.o


# Object file lists.

obj-y			:= $(patsubst %.c, %.o, $(wildcard *.c)) \
			   $(patsubst %.S, %.o, $(wildcard *.S))
obj-m			:=
obj-n			:=
obj-			:=

export-objs		:= 

ifeq ($(CONFIG_USE_A7HAL),y)
obj-y			+= \
			   a7hal/clock.o \
			   a7hal/dma.o \
			   a7hal/icu.o \
			   a7hal/led.o \
			   a7hal/mpu.o \
			   a7hal/sdram.o \
			   a7hal/timer.o \
			   a7hal/uart.o \
			   a7hal/watchdog.o \
			   a7hal/soft/lancs.o
endif

#obj-$(CONFIG_LEDS)	+=

include $(TOPDIR)/Rules.make
