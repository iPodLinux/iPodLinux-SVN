ifdef TTKDIR
TTKCONF = ../$(TTKDIR)/ttk-config-here
else
ifeq ($(shell which ttk-config 2>/dev/null >/dev/null && echo yes),yes)
TTKCONF = ttk-config
else
ifneq ($(wildcard ../../ttk/ttk-config-here),)
TTKCONF = ../../ttk/ttk-config-here
else
$(error Cannot find TTK. Specify TTKDIR,  put it in ../ttk, or install it)
endif
endif
endif

ifdef IPOD
CROSS ?= arm-uclinux-elf
CC = $(CROSS)-gcc
LD = $(CROSS)-ld
MYCFLAGS = -O3 -fomit-frame-pointer -funroll-loops `$(TTKCONF) --ipod --sdl --cflags` -I../contrib/ucdl -mapcs -mcpu=arm7tdmi -DVERSION=\"$(VERSION)\" -Wall
else
CC ?= cc
LD ?= ld
MYCFLAGS = `$(TTKCONF) --x11 --sdl --cflags` -DVERSION=\"$(VERSION)\" -Wall
endif
MYCFLAGS += -I/sw/include

OBJS= pz.o module.o ipod.o dialog.o secrets.o header.o settings.o fonts.o \
      menu.o legacy.o gui.o browser.o vector.o oss.o appearance.o input.o \
      priority.o icons.o
HDRS= pz.h

all: built-in.o

built-in.o: $(OBJS)
	@echo " LD      built-in.o"
	@$(LD) $(LDFLAGS) -r -o built-in.o $(OBJS)

$(OBJS): $(HDRS)

%.o: %.c
	@echo " CC     " $@
	@$(CC) $(CFLAGS) $(MYCFLAGS) -c -o $@ $<

clean:
	@rm -f *.o

.PHONY: clean all
