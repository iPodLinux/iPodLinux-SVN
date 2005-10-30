ifdef IPOD
CC ?= arm-elf-gcc
LD ?= arm-elf-ld
MYCFLAGS = `ttk-config --ipod --sdl --cflags` -I../contrib/ucdl
else
CC ?= cc
LD ?= ld
MYCFLAGS = `ttk-config --x11 --sdl --cflags`
endif

OBJS= pz.o module.o ipod.o dialog.o secrets.o header.o settings.o fonts.o
HDRS= pz.h

all: built-in.o

built-in.o: $(OBJS)
	$(LD) -r -o built-in.o $(OBJS)

$(OBJS): $(HDRS)

%.o: %.c
	$(CC) $(CFLAGS) $(MYCFLAGS) -c -o $@ $<

clean:
	rm -f *.o

.PHONY: clean all
