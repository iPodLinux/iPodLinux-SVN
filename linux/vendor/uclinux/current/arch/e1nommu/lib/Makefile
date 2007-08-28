#
# arch/e1nommu/lib/Makefile
#

.S.o:
	$(CC) $(AFLAGS) -D__ASSEMBLY__ -c -o $@ $<

L_TARGET := lib.a
obj-y    := memchr.o memcmp.o memcpy.o memset.o strcmp.o strcpy.o checksum.o delay.o

include $(TOPDIR)/Rules.make
