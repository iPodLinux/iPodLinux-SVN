#
# use 'make IPOD=1' to build for iPod, or 'make' for host
# 

ifneq ($(IPOD),)

# iPod build
CC=arm-elf-gcc
CFLAGS=-DIPOD -D_REENTRANT
LDFLAGS=-elf2flt
MICROWINDOWS = ../microwindows/src

# enable JPEG support
LDFLAGS+=-L ../jpeg-6b
else

CC=gcc
CFLAGS=
LDFLAGS=-L/usr/X11R6/lib -lX11
MICROWINDOWS = ../../../src/microwindows-0.90/src

endif

CFLAGS+=-I$(MICROWINDOWS)/include -g
LDFLAGS+=$(MICROWINDOWS)/lib/libnano-X.a $(MICROWINDOWS)/lib/libmwengine.a $(MICROWINDOWS)/lib/libmwdrivers.a $(MICROWINDOWS)/lib/libmwfonts.a

# enable JPEG support
LDFLAGS += -ljpeg

LDFLAGS += -lpthread

OBJS=\
	pz.o \
	display.o \
	image.o \
	browser.o \
	ipod.o \
	menu.o \
	piezo.o \
	textview.o \
	mp3.o \
	message.o \
	slider.o \
	audio.o \
	othello.o \
	bluecube/box.o \
	bluecube/main.o \
	bluecube/pieces.o

all: podzilla

podzilla: $(OBJS)
	$(CC) $(OBJS) -o podzilla  $(CFLAGS) $(LDFLAGS)

clean: 
	$(RM) $(OBJS) *~ podzilla podzilla.gdb
