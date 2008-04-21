ifeq ($(OSTYPE),)
        OSTYPE := $(shell uname -s)
endif

ifeq ($(OSTYPE),Darwin)
	PREFIX =
	MYCFLAGS = -DDARWIN -O3 -mcpu=750 -maltivec -mabi=altivec -fomit-frame-pointer -funroll-loops
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
PNGINCLUDE ?= ../libs/libpng
PNGLIB ?= ../libs/libpng
ZINCLUDE ?= ../libs/zlib
ZLIB ?= ../libs/zlib
LDFLAGS = -Wl,-elf2flt
MYCFLAGS = -DIPOD -I$(PNGINCLUDE) -I$(ZINCLUDE) -I.. -O3 -fomit-frame-pointer -funroll-loops
LDFLAGS += $(PNGLIB)/libpng.a $(ZLIB)/libz.a -lm
HDLIB = ../ipod/libhotdog.a
else
MYCFLAGS+= `libpng-config --cflags` `freetype-config --cflags` -I..
LDFLAGS = `sdl-config --libs` `libpng-config --ldflags` `freetype-config --libs`
MYCFLAGS += `sdl-config --cflags`
HDLIB = ../x11/libhotdog.a
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

APP = hd-pong
OBJS = hdpong.o

all: $(APP)

$(OBJS):
	$(CC) $(CFLAGS) $(MYCFLAGS) -c -o $@ $*.c

$(APP): $(OBJS) 
	$(CC) $? -o $@ $(LIB) $(HDLIB) $(LDFLAGS) $(MYLDFLAGS)

clean:
	$(RM) *.o *.elf *.elf2flt *.gdb *~ $(APP)
