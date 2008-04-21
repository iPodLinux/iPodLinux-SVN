ifeq ($(OSTYPE),)
        OSTYPE := $(shell uname -s)
endif

ifeq ($(HOST_CPU),)
        HOST_CPU := $(shell uname -m)
endif
ifeq ($(OSTYPE),Darwin)
	PREFIX =
ifdef DEBUG
	MYCFLAGS = -DDARWIN -g -pg
else
ifeq ($(HOST_CPU),i386)
	MYCFLAGS = -DDARWIN -O3 -march=i386 -fomit-frame-pointer -funroll-loops
else
	MYCFLAGS = -DDARWIN -O3 -mcpu=750 -maltivec -mabi=altivec -fomit-frame-pointer -funroll-loops
endif
endif
	LDFLAGS = -framework Foundation -framework Cocoa
endif
ifeq ($(OSTYPE),Linux)
ifdef DEBUG
	MYCFLAGS = -g -pg
else
	MYCFLAGS = -O3 -fomit-frame-pointer -funroll-loops
endif
	LDFLAGS = -lc
endif
ifneq ($(IPOD),)
PNGINCLUDE ?= ./libs/libpng
PNGLIB ?= ./libs/libpng
JPEGINCLUDE ?= ./libs/libjpeg
JPEGLIB ?= ./libs/libjpeg
ZINCLUDE ?= ./libs/zlib
ZLIB ?= ./libs/zlib
LDFLAGS = -Wl,-elf2flt
MYCFLAGS = -DIPOD -I$(PNGINCLUDE) -I$(JPEGINCLUDE) -I$(ZINCLUDE) -O3 -funroll-loops
ifdef DEBUG
LDFLAGS = -pg
endif
else
MYCFLAGS+= `libpng-config --cflags`
endif

ifdef IPOD
CROSS ?= arm-uclinux-elf
CC = $(CROSS)-gcc
LD = $(CROSS)-ld
AR = $(CROSS)-ar rc
AR2 = $(CROSS)-ranlib
CPPFLAGS = -DIPOD
OBJDIR = ipod
else
CC ?= gcc
LD ?= ld
AR = ar rc
AR2 = ranlib
AS = as
OBJDIR = x11
endif

MYCFLAGS+= -Wall
LIB = $(OBJDIR)/libhotdog.a
LIBOBJS = $(OBJDIR)/hotdog.o $(OBJDIR)/hotdog_png.o $(OBJDIR)/hotdog_jpeg.o \
          $(OBJDIR)/hotdog_ipg.o $(OBJDIR)/hotdog_font.o $(OBJDIR)/hotdog_animation.o \
          $(OBJDIR)/hotdog_canvas.o $(OBJDIR)/hotdog_surface.o \
          $(OBJDIR)/hotdog_ops.o $(OBJDIR)/hotdog_primitive.o $(OBJDIR)/zsort.o
ifdef IPOD
LIBOBJS += $(OBJDIR)/hotdog_lcd.o
endif

all: $(LIB)

$(LIB): $(OBJDIR) $(LIBOBJS)
	$(RM) $(LIB)
	$(AR) $(LIB) $(LIBOBJS)
	$(AR2) $(LIB)

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(MYCFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: %.S
	$(CC) $(CPPFLAGS) -c -o $@ $<

clean:
	$(RM) *.o *.elf *.elf2flt *.a *.gdb *~
	$(RM) -rf ipod x11
