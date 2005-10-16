CROSS= arm-uclinux-elf-
CC= $(CROSS)gcc

all: libuCdl.a modtest testmod.o

libuCdl.a: ucdl.o flat.o
	$(CROSS)ar cru libuCdl.a ucdl.o flat.o

clean:
	$(RM) *.o libuCdl.a modtest symadd modtest.gdb

symadd:
	gcc -o symadd flat.c symadd.c

modtest: symadd
	$(CROSS)gcc -Wl,-elf2flt -o modtest modtest.c libuCdl.a
	$(CROSS)nm modtest.gdb | grep ' T ' | ./symadd modtest

testmod.o:
	$(CROSS)gcc -c testmod.c
