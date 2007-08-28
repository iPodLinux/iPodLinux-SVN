#
# Makefile for frio-specific library files..
#

.S.o:
	$(CC) $(AFLAGS) -D__ASSEMBLY__ -traditional -c $< -o $@

L_TARGET = lib.a
obj-y  = ashrdi3.o checksum.o memcpy.o memset.o memcmp.o muldi3.o divsi3.o

include $(TOPDIR)/Rules.make
