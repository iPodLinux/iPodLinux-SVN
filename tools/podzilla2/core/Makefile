ifdef IPOD
CROSS ?= arm-elf
CC = $(CROSS)-gcc
LD = $(CROSS)-ld
MYCFLAGS = `ttk-config --ipod --sdl --cflags` -I../contrib/ucdl
else
CC ?= cc
LD ?= ld
MYCFLAGS = `ttk-config --x11 --sdl --cflags`
endif

OBJS= pz.o module.o ipod.o dialog.o secrets.o header.o settings.o fonts.o \
      menu.o legacy.o gui.o browser.o vector.o oss.o
HDRS= pz.h

all: built-in.o

built-in.o: $(OBJS)
	@echo " LD      built-in.o"
	@$(LD) -r -o built-in.o $(OBJS)

$(OBJS): $(HDRS)

%.o: %.c
	@echo " CC     " $@
	@$(CC) $(CFLAGS) $(MYCFLAGS) -c -o $@ $<

clean:
	@rm -f *.o

.PHONY: clean all
