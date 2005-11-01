ifeq ($(OSTYPE),)
        OSTYPE := $(shell uname -s)
endif

ifeq ($(OSTYPE),Darwin)
        CC = gcc
        LD = ld
        CFLAGS = -g -mcpu=750 -maltivec -mabi=altivec -fomit-frame-pointer -funroll-loops
        LDFLAGS = -framework Foundation -framework Cocoa
endif
ifeq ($(OSTYPE),Linux)
        CC = gcc
        LD = ld
        CFLAGS = -g -fomit-frame-pointer -funroll-loops
        LDFLAGS = -lc
endif

PROG=hotdog
CFLAGS  +=`sdl-config --cflags` -g `pkg-config --cflags libpng`
LDFLAGS +=`sdl-config --libs` `pkg-config --libs libpng`

OBJS=  main_darwin.o hotdog.o hotdog_png.o hotdog_primitive.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(OBJS) $(PROG) *~
