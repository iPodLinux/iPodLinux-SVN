
# iPod build
CC=arm-elf-gcc
CFLAGS=-DIPOD 
LDFLAGS=-elf2flt
# For JPEG support
# LDFLAGS += -L ../jpeg-6b -ljpeg
MICROWINDOWS = ../microwindows-0.90/src

# Uncomment for building natively
# CC=gcc
# CFLAGS=
# LDFLAGS=-L/usr/X11R6/lib -lX11
# For JPEG support
# LDFLAGS+=-ljpeg
# MICROWINDOWS=../../../src/microwindows-0.90/src

CFLAGS+=-I$(MICROWINDOWS)/include -g
LDFLAGS+=$(MICROWINDOWS)/lib/libnano-X.a $(MICROWINDOWS)/lib/libmwengine.a $(MICROWINDOWS)/lib/libmwdrivers.a $(MICROWINDOWS)/lib/libmwfonts.a

OBJS=\
	pz.o \
	display.o \
	image.o \
	browser.o \
	ipod.o \
	menu.o \
	message.o

all: podzilla

podzilla: $(OBJS)
	$(CC) $(OBJS) -o podzilla  $(CFLAGS) $(LDFLAGS)

clean: 
	$(RM) $(OBJS) *~ podzilla podzilla.gdb
