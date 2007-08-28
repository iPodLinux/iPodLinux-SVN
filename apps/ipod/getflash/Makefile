CC = $(CROSS)-gcc
LIBS += -Wl,-elf2flt
CROSS ?= arm-uclinux-elf

all:
	@$(CC) -o getflash getflash.c -Wall $(LIBS)

clean:
	@rm -f getflash getflash.o getflash.gdb
