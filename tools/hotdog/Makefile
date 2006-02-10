ifeq ($(OSTYPE),)
        OSTYPE := $(shell uname -s)
endif

ifeq ($(OSTYPE),Darwin)
	PREFIX =
	CFLAGS = -DDARWIN -g -mcpu=750 -maltivec -mabi=altivec -fomit-frame-pointer -funroll-loops
	LDFLAGS = -framework Foundation -framework Cocoa
endif
ifeq ($(OSTYPE),Linux)
	PREFIX =
#	CFLAGS = -O3 -fomit-frame-pointer -funroll-loops
	CFLAGS = -g
	LDFLAGS = -lc
endif
ifneq ($(IPOD),)
	PREFIX = arm-uclinux-elf-
	LDFLAGS = -elf2flt
	CFLAGS = -DIPOD -I./xc/include -fomit-frame-pointer -funroll-loops
	DEMOCFLAGS = -I./xc/include/SDL
	DEMOLDFLAGS = ./xc/lib/libSDL.a ./xc/lib/libpng.a ./xc/lib/libz.a ./xc/lib/libfreetype.a -lm
else
	CFLAGS+= `libpng-config --cflags` `freetype-config --cflags`
	DEMOLDFLAGS = `sdl-config --libs` `libpng-config --ldflags` `freetype-config --libs`
	DEMOCFLAGS = `sdl-config --cflags`
endif

ifdef IPOD
CROSS ?= arm-uclinux-elf
CC = $(CROSS)-gcc
LD = $(CROSS)-ld
AR = $(CROSS)-ar rc
AR2 = $(CROSS)-ranlib
AS = $(CROSS)-as -DIPOD
else
CC ?= gcc
LD ?= ld
AR = ar rc
AR2 = ranlib
AS = as
endif

CFLAGS+= -Wall
LIB = libhotdog.a
LIBOBJS=  hotdog.o hotdog_png.o hotdog_font.o hotdog_animation.o hotdog_canvas.o hotdog_surface.o hotdog_ops.o zsort.o

DEMO = hd-demo

all: $(LIB)

$(LIB): $(LIBOBJS)
	$(RM) $(LIB)
	$(AR) $(LIB) $(LIBOBJS)
	$(AR2) $(LIB)

main_anim.o: main_anim.c
	$(CC) $(CFLAGS) $(DEMOCFLAGS) -c -o $@ $<

$(DEMO): $(LIB) main_anim.o
	$(CC) main_anim.o -o $@ $(LIB) $(LDFLAGS) $(DEMOLDFLAGS)

clean:
	$(RM) *.o *.elf *.elf2flt *.a *.gdb *~ $(DEMO)
