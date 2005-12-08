ifeq ($(OSTYPE),)
        OSTYPE := $(shell uname -s)
endif

ifeq ($(OSTYPE),Darwin)
	PREFIX =
	CFLAGS = -g -mcpu=750 -maltivec -mabi=altivec -fomit-frame-pointer -funroll-loops
	LDFLAGS = -framework Foundation -framework Cocoa ../libhotdog.a
endif
ifeq ($(OSTYPE),Linux)
	PREFIX =
	CFLAGS = -g -fomit-frame-pointer -funroll-loops
	LDFLAGS = ../libhotdog.a -lc
endif

ifneq ($(IPOD),)
	PREFIX = arm-uclinux-elf-
	LDFLAGS = -elf2flt ../libhotdog.a ../xc/lib/libSDL.a ../xc/lib/libpng.a ../xc/lib/libz.a ../xc/lib/libfreetype.a -lm
	CFLAGS = -DIPOD -I../xc/include -fomit-frame-pointer -funroll-loops -I../xc/include/SDL
else
	CFLAGS+= `sdl-config --cflags`
	LDFLAGS += `sdl-config --libs` `libpng-config --ldflags` `freetype-config --libs`
endif

CFLAGS+=-I../

CC=$(PREFIX)gcc

APP = hd-pong
OBJS = hdpong.o

all: $(APP)

$(OBJS):
	$(CC) $(CFLAGS) -c -o $@ $*.c

$(APP): $(OBJS) 
	$(CC) $? -o $@ $(LIB) $(LDFLAGS)

clean:
	$(RM) *.o *.elf *.elf2flt *.gdb *~ $(APP)
