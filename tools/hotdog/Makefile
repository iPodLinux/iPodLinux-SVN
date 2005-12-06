ifeq ($(OSTYPE),)
        OSTYPE := $(shell uname -s)
endif

ifeq ($(OSTYPE),Darwin)
	PREFIX =
	CFLAGS = -g -mcpu=750 -maltivec -mabi=altivec -fomit-frame-pointer -funroll-loops
	LDFLAGS = -framework Foundation -framework Cocoa
endif
ifeq ($(OSTYPE),Linux)
	PREFIX =
	CFLAGS = -g -fomit-frame-pointer -funroll-loops
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

CC=$(PREFIX)gcc
LD=$(PREFIX)ld
AR=$(PREFIX)ar rc
AR2=$(PREFIX)ranlib

LIB = libhotdog.a
LIBOBJS=  hotdog.o hotdog_png.o hotdog_primitive.o hotdog_font.o hotdog_animation.o hotdog_canvas.o

DEMO = hd-demo
DEMOOBJS = main_darwin.o

all: $(LIB)

$(LIB): $(LIBOBJS)
	$(RM) $(LIB)
	$(AR) $(LIB) $(LIBOBJS)
	$(AR2) $(LIB)

$(DEMOOBJS):
	$(CC) $(CFLAGS) $(DEMOCFLAGS) -c -o $@ $*.c

$(DEMO): $(LIB) $(DEMOOBJS) 
	$(CC) $? -o $@ $(LIB) $(LDFLAGS) $(DEMOLDFLAGS)

clean:
	$(RM) *.o *.elf *.elf2flt *.a *.gdb *~ $(DEMO)
