CC ?= gcc
CFLAGS ?= -g
LDFLAGS ?= -lncurses
ARMAS ?= arm-uclinux-elf-as
ARMLD ?= arm-uclinux-elf-ld
ARMOC ?= arm-uclinux-elf-objcopy

OBJS = armemu.o execute.o disasm.o

all: armemu app.bin

armemu: $(OBJS)
	$(CC) $(CFLAGS) -o armemu $(OBJS) $(LDFLAGS)

$(OBJS): %.o: %.c armemu.h

app.bin: app.s
	$(ARMAS) -o app.o app.s
	$(ARMLD) -e IVEC_rst -o app app.o
	$(ARMOC) -O binary app app.bin

clean:
	rm -f armemu app app.bin app.o $(OBJS)
