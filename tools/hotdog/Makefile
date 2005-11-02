ifeq ($(OSTYPE),)
        OSTYPE := $(shell uname -s)
endif

ifeq ($(OSTYPE),Darwin)
        CC = gcc
        LD = ld
        CFLAGS = -g -mcpu=750 -maltivec -mabi=altivec -fomit-frame-pointer -funroll-loops -I /usr/X11R6/include/freetype2
        LDFLAGS = -framework Foundation -framework Cocoa -L/usr/X11R6/lib
endif
ifeq ($(OSTYPE),Linux)
        CC = gcc
        LD = ld
        CFLAGS = -g -fomit-frame-pointer -funroll-loops
        LDFLAGS = -lc
endif

PROG=hotdog
CFLAGS  +=`sdl-config --cflags` -g `libpng-config --cflags` `freetype-config --cflags`
LDFLAGS +=`sdl-config --libs` `libpng-config --ldflags` -lfreetype

OBJS=  main_darwin.o hotdog.o hotdog_png.o hotdog_primitive.o hotdog_font.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(OBJS) $(PROG) *~
