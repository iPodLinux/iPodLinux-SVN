CROSS ?= arm-uclinux-elf-
CC = $(CROSS)gcc
CFLAGS = -O3 -fomit-frame-pointer -funroll-loops

all: libuCdl.a libdl.a modtest testmod.o

libdl.a: ucdl.o flat.o dlfcn.o
	$(CROSS)ar cru $@ $^
	$(CROSS)ranlib $@

libuCdl.a: ucdl.o flat.o
	$(CROSS)ar cru $@ $^
	$(CROSS)ranlib $@

clean:
	$(RM) *.o libdl.a libuCdl.a modtest symadd modtest.gdb

symadd: symadd.c flat.c
	gcc -o $@ $^

modtest: symadd
	$(CROSS)gcc -Wl,-elf2flt -o $@ modtest.c libuCdl.a
	$(CROSS)nm modtest.gdb | grep ' T ' | ./symadd $@
