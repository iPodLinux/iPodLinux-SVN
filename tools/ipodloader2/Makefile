# makefile for iPodLoader2
#
# Note by TT: the option "-mstructure-size-boundary=8" is necessary when compiling macpartitions.cc for the structs to get the correct sizes!

CROSS    ?= arm-uclinux-elf-
CC        = $(CROSS)gcc
MYCFLAGS  = -O2 -Wall -std=gnu99 -ffreestanding -nostdinc -fomit-frame-pointer
# -DDEBUG
MYCPPFLAGS= -O2 -Wall -nostdinc -fomit-frame-pointer -mstructure-size-boundary=8
MYLDFLAGS = -Wl,-Tarm_elf_40.x -nostartfiles -fomit-frame-pointer
OBJCOPY   = $(CROSS)objcopy

OBJFILES = startup.o loader.o fb.o ipodhw.o console.o minilibc.o ata2.o vfs.o fat32.o ext2.o fwfs.o keypad.o menu.o config.o macpartitions.o

all: loader.bin
#	@echo "Building firmware image"
#	@./make_fw -g 4g -o my_sw.bin -i apple_os.bin $<

clean:
	@echo "Cleaning up"
	@rm -f *.o *~ loader.bin loader.elf nohup.out my_sw.bin $(OBJFILES)

loader.bin: loader.elf
	@echo "Converting $< to binary"
	@$(OBJCOPY) -O binary $< $@

loader.elf: $(OBJFILES)
	@echo "Linking $@"
	@$(CC) $(MYLDFLAGS) $(LDFLAGS) -o $@ $^

%.o:%.cc
	@echo "Compiling $<"
	@$(CC) $(MYCPPFLAGS) $(CFLAGS) -c $< -o $@

%.o:%.c
	@echo "Compiling $<"
	@$(CC) $(MYCFLAGS) $(CFLAGS) -c $< -o $@

%.o:%.s
	@echo "Compiling $<"
	@$(CC) $(MYCFLAGS) $(CFLAGS) -c $< -o $@
