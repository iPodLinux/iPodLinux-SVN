ifeq ($(OSTYPE),)
        OSTYPE := $(shell uname -s)
endif

ifeq ($(OSTYPE),Darwin)
	PREFIX =
	MYCFLAGS = -DDARWIN -O3 -mcpu=750 -maltivec -mabi=altivec -fomit-frame-pointer -funroll-loops -g
	LDFLAGS = -framework Foundation -framework Cocoa
endif
ifeq ($(OSTYPE),Linux)
ifdef DEBUG
	MYCFLAGS = -g
else
	MYCFLAGS = -O3 -fomit-frame-pointer -funroll-loops
endif
	LDFLAGS = -lc
endif
ifneq ($(IPOD),)
PNGINCLUDE ?= ./libs/libpng
PNGLIB ?= ./libs/libpng
ZINCLUDE ?= ./libs/zlib
ZLIB ?= ./libs/zlib
SDLINCLUDE ?= ../ttk/sdlincludes
SDLLIB ?= ../ttk/libs/SDL
LDFLAGS = -Wl,-elf2flt
MYCFLAGS = -DIPOD -I$(PNGINCLUDE) -I$(ZINCLUDE) -O3 -fomit-frame-pointer -funroll-loops
ifneq ($(PHOTO),)
MYCFLAGS += -DPHOTO
else
ifneq ($(COLOR),)
MYCFLAGS += -DCOLOR
else
ifneq ($(NANO),)
MYCFLAGS += -DNANO
endif
endif
endif
DEMOCFLAGS = -I$(SDLINCLUDE)
DEMOLDFLAGS = $(SDLLIB)/libSDL.a $(PNGLIB)/libpng.a $(ZLIB)/libz.a -lm
else
MYCFLAGS+= `libpng-config --cflags` `freetype-config --cflags`
DEMOLDFLAGS = `sdl-config --libs` `libpng-config --ldflags` `freetype-config --libs`
DEMOCFLAGS = `sdl-config --cflags`
endif

ifdef IPOD
CROSS ?= arm-uclinux-elf
CC = $(CROSS)-gcc
LD = $(CROSS)-ld
AR = $(CROSS)-ar rc
AR2 = $(CROSS)-ranlib
CPPFLAGS = -DIPOD
else
CC ?= gcc
LD ?= ld
AR = ar rc
AR2 = ranlib
AS = as
endif

MYCFLAGS+= -Wall
LIB = libhotdog.a
LIBOBJS=  hotdog.o hotdog_png.o hotdog_font.o hotdog_animation.o hotdog_canvas.o hotdog_surface.o hotdog_ops.o zsort.o
ifdef IPOD
LIBOBJS += hotdog_lcd.o
endif

DEMO = hd-demo

all: $(LIB)

$(LIB): $(LIBOBJS)
	$(RM) $(LIB)
	$(AR) $(LIB) $(LIBOBJS)
	$(AR2) $(LIB)

main_anim.o main_select.o: %.o: %.c
	$(CC) $(CFLAGS) $(MYCFLAGS) $(DEMOCFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) $(MYCFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CPPFLAGS) -c -o $@ $<

$(DEMO): $(LIB) main_anim.o
	$(CC) main_anim.o -o $@ $(LIB) $(LDFLAGS) $(DEMOLDFLAGS)

hd-demo2: $(LIB) main_select.o
	$(CC) main_select.o -o $@ $(LIB) $(LDFLAGS) $(DEMOLDFLAGS)

clean:
	$(RM) *.o *.elf *.elf2flt *.a *.gdb *~ $(DEMO)
