#
# use 'make IPOD=1' to build for iPod, 'make MINI=1' for x11-mini, 
# 'make PHOTO=1' for x11-photo, and 'make' for x11
# 
# directory structure is set up as follows:
# 	../microwindows
# 		ipod/src	- build for ipod
# 		ipod-x11/src	- build for x11
# 		mini-x11/src	- build for mini on x11
# 		photo-x11/src	- build for photo on x11
# 	../libjpeg
# 		ipod		- jpeg library built for ipod
# 		ipod-x11	- jpeg library built for host(optional)
# 	../libitunesdb
# 		ipod/src	- build for ipod
# 		ipod-x11/src	- build for x11
# 	../ipp		- Intel Performance Primitives
#	../helix-aacdec	- Helix AAC decoder library
# 		ipod		- jpeg library built for ipod
# 		ipod-x11	- jpeg library built for host (optional, x86 only)
#	../mp4ff	- MP4 file format library
# 		ipod		- jpeg library built for ipod
# 		ipod-x11	- jpeg library built for host (optional, x86 only)

ifneq ($(IPOD),)

# iPod build
CC= arm-elf-gcc
CFLAGS= -DIPOD -D__linux__
LDFLAGS= -elf2flt
MICROWINDOWS= ../microwindows/ipod/src
LIBITUNESDB= ../libitunesdb/ipod/src
IPP= ../ipp
AACDEC= ../helix-aacdec/ipod
MP4FF=../mp4ff/ipod
LDFLAGS+= -L ../libjpeg/ipod

CFLAGS+= -I$(IPP)/include -I$(AACDEC)/pub -I$(MP4FF)
CFLAGS+= -DUSE_HELIXAACDEC
LDFLAGS+=\
	$(IPP)/lib/ippAC_SA11LNX_r.a \
	$(IPP)/lib/ippSP_SA11LNX_r.a \
	$(AACDEC)/libaacdec.a \
	$(MP4FF)/libmp4ff.a

else
ifneq ($(MINI),)
 # mini on x11
 MICROWINDOWS= ../microwindows/mini-x11/src
else
 ifneq ($(PHOTO),)
  # photo on x11
  MICROWINDOWS= ../microwindows/photo-x11/src
 else
  # x11 build
  MICROWINDOWS= ../microwindows/ipod-x11/src
 endif
endif
CC=gcc
CFLAGS= -I$(IPP)/include 
LDFLAGS= -L/usr/X11R6/lib -lX11
LIBITUNESDB= ../libitunesdb/ipod-x11/src
ifneq ($(shell "arch"),ppc)
  # if not ppc or x86, change ppc with output from 'arch'
  AACDEC= ../helix-aacdec/ipod-x11
  MP4FF=../mp4ff/ipod-x11
  CFLAGS+= -DUSE_HELIXAACDEC -I$(AACDEC)/pub -I$(MP4FF)
  LDFLAGS+= $(AACDEC)/libaacdec.a \
            $(MP4FF)/libmp4ff.a
endif

# for libjpeg on x11
#LDFLAGS+= -L../libjpeg/ipod-x11

endif

PZ_VER=\"podzilla `date -u +"%Y-%m-%d"`CVS\"

CFLAGS+=\
	-Wall -g \
	-I$(MICROWINDOWS)/include \
	-I$(LIBITUNESDB) \
	-DPZ_VER="$(PZ_VER)"

LDFLAGS+=\
	$(MICROWINDOWS)/lib/libnano-X.a \
	$(MICROWINDOWS)/lib/libmwengine.a \
	$(MICROWINDOWS)/lib/libmwdrivers.a \
	$(MICROWINDOWS)/lib/libmwfonts.a \
	$(LIBITUNESDB)/.libs/libitunesdb.a

LDFLAGS+=\
	-ljpeg \
	-lm

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
	aac.o \
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
	tictactoe.o \
	tunnel.o \
	calc.o \
	poddraw.o \
	playlist.o \
	cube.o \
	matrix.o \
	lights.o \
	mlist.o \
	minesweeper.o \
	ipobble.o \
	invaders.o \
	vectorfont.o \
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
	tuxchess/board.o \
	tuxchess/data.o \
	tuxchess/eval.o \
	tuxchess/main.o \
	tuxchess/search.o

ifneq ($(IPOD),)
OBJS+=\
	mp3decoder.o
endif

all: podzilla

podzilla: $(OBJS) Makefile
	$(CC) $(OBJS) -o podzilla $(CFLAGS) $(LDFLAGS)

clean: 
	$(RM) $(OBJS) *~ podzilla podzilla.gdb
