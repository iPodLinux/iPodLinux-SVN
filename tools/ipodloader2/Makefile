CC      = arm-elf-gcc
CFLAGS  = -O2 -Wall -pedantic -std=c99 -ffreestanding -nostdinc -fomit-frame-pointer
LDFLAGS = -Wl,-Tarm_elf_40.x -nostartfiles -fomit-frame-pointer
OBJCOPY = arm-elf-objcopy

all: loader.bin
#	@echo "Building firmware image"
	@./make_fw -g 4g -o my_sw.bin -i apple_os.bin $<

clean:
	@echo "Cleaning up"
	@rm -f *.o *~ loader.bin loader.elf nohup.out my_sw.bin ipodhw.o ata.o ata2.o

loader.bin: loader.elf
	@echo "Converting $< to binary"
	@$(OBJCOPY) -O binary $< $@

loader.elf: startup.o loader.o fb.o ipodhw.o console.o minilibc.o ata2.o vfs.o fat32.o ext2.o fwfs.o keypad.o menu.o config.o
	@echo "Linking $@"
	@$(CC) $(LDFLAGS) -o $@ $^

%.o:%.c
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) -c $< -o $@

%.o:%.s
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) -c $< -o $@