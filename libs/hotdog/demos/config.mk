HDDIR = ../..
CFLAGS += -Wall -I$(HDDIR)
LDFLAGS += -lhotdog 

ifdef IPOD
CROSS ?= arm-uclinux-elf-
CFLAGS += -O2 -DIPOD -I$(HDDIR)/libs/libpng -I$(HDDIR)/libs/libjpeg -I$(HDDIR)/libs/zlib
LDFLAGS += -Wl,-elf2flt $(HDDIR)/libs/libpng/libpng.a $(HDDIR)/libs/libjpeg/libjpeg.a $(HDDIR)/libs/zlib/libz.a -L$(HDDIR)/ipod/ -lm
else
CFLAGS += -g `libpng-config --cflags` `sdl-config --cflags`
LDFLAGS += `libpng-config --libs` `sdl-config --libs` -ljpeg -L$(HDDIR)/x11/ -lm
endif

CC = $(CROSS)gcc
LD = $(CROSS)ld
