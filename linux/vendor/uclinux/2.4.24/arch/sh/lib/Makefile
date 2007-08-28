#
# Makefile for SuperH-specific library files..
#

L_TARGET = lib.a
obj-y  = delay.o memset.o memmove.o memchr.o old-checksum.o \
	 checksum.o strcasecmp.o strlen.o udivdi3.o div64.o

ifeq ($(findstring y, \
		$(CONFIG_CPU_SUBTYPE_SH7750) \
		$(CONFIG_CPU_SUBTYPE_SH7751) \
		),y)
	obj-y += sh4-memcpy.o
else
	obj-y += memcpy.o
endif

USE_STANDARD_AS_RULE := true

include $(TOPDIR)/Rules.make
