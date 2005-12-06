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
	PREFIX = arm-elf-
	LDFLAGS = -elf2flt
	CFLAGS += -DIPOD -fomit-frame-pointer -funroll-loops
else
	CFLAGS+= `libpng-config --cflags` `freetype-config --cflags`
	DEMOCFLAGS = `sdl-config --cflags`
	DEMOLDFLAGS = `sdl-config --libs` `libpng-config --ldflags` `freetype-config --libs`
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
	$(CC) $(DEMOCFLAGS) -c -o $@ $*.c

$(DEMO): $(DEMOOBJS) 
	$(CC) $? -o $@ $(LDFLAGS) $(DEMOLDFLAGS) $(LIB)

clean:
	$(RM) *.o *.a *.gdb *~ $(DEMO)
