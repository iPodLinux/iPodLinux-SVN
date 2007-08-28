# 
# Makefile for Nios library files..
#

.S.o:
	$(CC) $(AFLAGS) -D__ASSEMBLY__ -traditional -c $< -o $@

L_TARGET := lib.a
obj-y  =checksum.o string.o

include $(TOPDIR)/Rules.make
