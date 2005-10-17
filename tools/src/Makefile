OBJS = ttk.o menu.o icons.o slider.o imgview.o textarea.o appearance.o mwin-emu.o
HDR = ttk.h menu.h icons.h slider.h imgview.h textarea.h appearance.h mwin-emu.h

EXAMPLES = exscroll exmenu eximage exti
EXOBJS = exscroll.o exmenu.o eximage.o exti.o

CFLAGS = -g -I. -Wall -Wno-unused -Wno-implicit -Wno-char-subscripts
PREFIX ?= /usr
ifdef IPOD
CROSS ?= arm-uclinux-elf
CC = $(CROSS)-gcc
AR = $(CROSS)-ar
RANLIB = $(CROSS)-ranlib
CFLAGS += -DIPOD
E2F = -Wl,-elf2flt
GS = -Wl,--start-group
GE = -Wl,--end-group
else
CC ?= gcc
AR ?= ar
RANLIB ?= ranlib
CFLAGS += $(NATIVE_CFLAGS)
endif

ifdef SDL
GFXLIB = SDL
else
ifdef MWIN
GFXLIB = mwin
else
ifndef GFXLIB
default-two:
	make -C../..
clean:
	make -C../.. clean
distclean:
	make -C../.. distclean
examples:
	make -C../.. examples
docs:
	make -C../.. docs
dist:
	make -C../.. dist
endif
endif
endif

ifdef PHOTO
OBJS += SFont.o
else
CFLAGS += -DNO_SF
endif

ifdef TTF
OBJS += SDL_ttf.o
else
CFLAGS += -DNO_TF
endif

ifdef IPOD
LIBSA = ../libs/common/*.a
else
LIBSA =
endif

ifeq ($(GFXLIB),SDL)
CFLAGS += -DSDL -I/usr/include/SDL -I/usr/local/include/SDL `sdl-config --cflags`
OBJS += sdl.o SDL_gfxPrimitives.o SDL_gfxPrimitives_Byte.o SDL_rotozoom.o
HDR += SDL_gfxPrimitives.h SDL_rotozoom.h
ifdef IPOD
LIBSA += ../libs/SDL/*.a
LIBS += -lpthread -lm
else
LIBS += `sdl-config --libs` -lSDL_image -lpng -ljpeg
endif

else
ifeq ($(GFXLIB),mwin)
CFLAGS += -DMWIN -I../mwincludes
OBJS += mwin.o
ifdef IPOD
LIBSA += ../libs/mwin/ipod/*.a
else
LIBSA += ../libs/mwin/x11/$(shell uname -s)/*.a
LIBS += -L/usr/X11R6/lib -lX11 -lpng -ljpeg
endif
LIBS += -lm

else
default:
	make -C..
endif
endif

ifdef IPOD
LIBDIR=$(shell $(CC) -v /dev/null 2>&1 | grep -- -L | perl -pe 's/.*-L(\S+).*/$$1/g')
else
LIBDIR=$(PREFIX)/lib
endif

ifdef GFXLIB
all: libttk.a

examples: libttk.a $(EXOBJS) $(EXAMPLES)

install: libttk.a munge-config
	install -m 644 libttk.a $(LIBDIR)/libttk-$(GFXLIB).a
	$(RANLIB) $(LIBDIR)/libttk-$(GFXLIB).a

ifdef IPOD
munge-config:
	mv ../../ttk-config.tmp ../../ttk-config.tmpp
	sed 's:@IPOD_LIBS@:$(LIBDIR):g' < ../../ttk-config.tmpp > ../../ttk-config.tmp
	rm -f ../../ttk-config.tmpp
else
munge-config:
	mv ../../ttk-config.tmp ../../ttk-config.tmpp
	sed 's:@X11_LIBS@:$(LIBDIR):g' < ../../ttk-config.tmpp > ../../ttk-config.tmp
	rm -f ../../ttk-config.tmpp
endif

# Source:
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Examples:
%: %.o libttk.a
	$(CC) $(CFLAGS) $(E2F) -o $@ $< $(GS) libttk.a -L/sw/lib -L/usr/local/lib $(LIBS) $(GE)

ifneq ($(LIBSA),)
libttk.a: $(OBJS) $(LIBSA)
	for lib in $(LIBSA); do lfixed=`echo $$lib | tr / _`; mkdir -p .x-$$lfixed; cd .x-$$lfixed; $(AR) x ../$$lib; cd ..; done
	$(AR) cru libttk.a $(OBJS) .x-*/*.o
	$(RANLIB) libttk.a
	rm -rf .x-*
else
libttk.a: $(OBJS)
	$(AR) cru libttk.a $(OBJS)
	$(RANLIB) libttk.a
endif

clean:
	rm -f *.o libttk.a $(EXAMPLES) *.gdb
endif

.PHONY: clean
