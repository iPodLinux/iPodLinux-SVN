#
# use 'make IPOD=1' to build for iPod, or 'make' for host
# 

ifneq ($(IPOD),)

# iPod build
CC=arm-elf-gcc
CFLAGS=-DIPOD -D__linux__
LDFLAGS=-elf2flt
MICROWINDOWS = ../microwindows/src
LIBITUNESDB=../libitunesdb.ipod/src
IPP=/usr/local/ipp/ippsa11

# enable JPEG support
LDFLAGS+=-L ../jpeg-6b

CFLAGS+=-I$(IPP)/include
LDFLAGS+=$(IPP)/lib/ippAC_SA11LNX_r.a $(IPP)/lib/ippSP_SA11LNX_r.a

else

CC=gcc
CFLAGS=
LDFLAGS=-L/usr/X11R6/lib -lX11
MICROWINDOWS = ../../../src/microwindows-0.90/src
LIBITUNESDB=../libitunesdb/src

endif

CFLAGS+=-Wall -g -I$(MICROWINDOWS)/include -g -I$(LIBITUNESDB) -DPZ_VER="\"podzilla v0.2beta\""
LDFLAGS+=$(MICROWINDOWS)/lib/libnano-X.a $(MICROWINDOWS)/lib/libmwengine.a $(MICROWINDOWS)/lib/libmwdrivers.a $(MICROWINDOWS)/lib/libmwfonts.a $(LIBITUNESDB)/.libs/libitunesdb.a

# enable JPEG support
LDFLAGS += -ljpeg

LDFLAGS += -lm

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
	about.o \
	othello.o \
	pong.o \
	calendar.o \
	btree.o \
	itunes_db.o \
	itunesmenu.o \
	calc.o \
	Steroids/asteroid.o \
	Steroids/main.o \
	Steroids/object.o \
	Steroids/polygon.o \
	Steroids/ship.o \
	Steroids/shot.o \
	Steroids/vector.o \
	bluecube/box.o \
	bluecube/main.o \
	bluecube/pieces.o \
	Nimesweeper/gameplay.o \
	Nimesweeper/init.o \
	Nimesweeper/main.o \
	Nimesweeper/misc.o \
	Nimesweeper/scores.o

ifneq ($(IPOD),)
OBJS+=\
	mp3decoder.o
endif

all: podzilla

podzilla: $(OBJS)
	$(CC) $(OBJS) -o podzilla  $(CFLAGS) $(LDFLAGS)

clean: 
	$(RM) $(OBJS) *~ podzilla podzilla.gdb
