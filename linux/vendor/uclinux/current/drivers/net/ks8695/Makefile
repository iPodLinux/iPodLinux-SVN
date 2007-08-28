#
# Makefile for the Tulip ethernet driver
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := ks8695e.o

obj-y   := ks8695_main.o ks8695_fxhw.o ks8695_cache.o
obj-m   := $(O_TARGET)

#EXTRA_CFLAGS += -I. -DPCI -DMEM_MAPPED_IO -Wno-strict-prototypes 
EXTRA_CFLAGS += -I. -DARM_LINUX -DEXPORT_SYMTAB -Wno-strict-prototypes 

include $(TOPDIR)/Rules.make

clean:
	rm -f core *.o 
