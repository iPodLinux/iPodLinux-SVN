#! /usr/bin/make -f

CROSS	=arm-elf-

AS	=$(CROSS)as
CC	=$(CROSS)gcc
OBJDUMP =$(CROSS)objdump
CFLAGS	=-Wall -O2 -ffreestanding -fomit-frame-pointer
LDFLAGS =-Wl,-Tarm_elf_40.x -nostartfiles
OBJCOPY	=$(CROSS)objcopy 
HOSTCC	=gcc
HOSTCFLAGS =-Wall -O2

default: make_fw loader.bin

%.s: %.c
	$(CC) $(CFLAGS) -S $<

%.o: %.S %.c

loader: startup.o loader.o tools.o tux.o
	$(CC) $(LDFLAGS) -o $@ startup.o loader.o tools.o tux.o

loader.bin: loader
	$(OBJCOPY) -O binary loader loader.bin

make_fw: make_fw.c
	$(HOSTCC) $(HOSTCFLAGS) -o make_fw make_fw.c

bin_dist: make_fw loader.bin
	mkdir ipodloader-$(VER)
	cp README make_fw loader.bin ipodloader-$(VER)
	tar zcf ../ipodloader-$(VER).tar.gz ipodloader-$(VER)
	rm -rf ipodloader-$(VER)

src_dist:
	mkdir ipodloader-src-$(VER)
	cp README Makefile arm_elf_40.x *.[sch]  ipodloader-src-$(VER)
	tar zcf ../ipodloader-src-$(VER).tar.gz ipodloader-src-$(VER)
	rm -rf ipodloader-src-$(VER)

.PHONY: clean
clean:
	rm -f make_fw loader.bin loader startup.o loader.o make_fw.o \
		tools.o tux.o

# vim:ts=8:sts=8:sw=8:
