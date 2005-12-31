OBJS = ttk.o menu.o icons.o slider.o imgview.o textarea.o lex.yy.o mwin-emu.o gradient.o ttkmm.o
HDR = ttk.h menu.h icons.h slider.h imgview.h textarea.h appearance.h mwin-emu.h gradient.h ttkmm.h

EXAMPLES = exscroll exmenu eximage exti
EXOBJS = exscroll.o exmenu.o eximage.o exti.o

MYCFLAGS = -g -I. -Wall -Wno-unused -Wno-implicit -Wno-char-subscripts
PREFIX ?= /usr
ifdef IPOD
CROSS ?= arm-uclinux-elf
CC = $(CROSS)-gcc
CXX = $(CROSS)-g++
AR = $(CROSS)-ar
RANLIB = $(CROSS)-ranlib
MYCFLAGS += -DIPOD
E2F = -Wl,-elf2flt
GS = -Wl,--start-group
GE = -Wl,--end-group
else
CC ?= gcc
CXX ?= g++
AR ?= ar
RANLIB ?= ranlib
MYCFLAGS += $(NATIVE_CFLAGS)
ifdef SDL_FRAMEWORK
MYCFLAGS += -framework SDL -framework SDL_image -I/Library/Frameworks/SDL.framework/Headers/ -I/Library/Frameworks/SDL_image.framework/Headers/
endif
endif

FLEX ?= flex

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

ifdef TTF
OBJS += SDL_ttf.o
else
MYCFLAGS += -DNO_TF
endif

ifdef IPOD
LIBSA = ../libs/common/*.a
LIBS += -lintl
else
LIBSA =
LIBS += -L/sw/lib -L/usr/local/lib
ifneq ($(shell uname),Linux)
LIBS += -lintl
endif
endif

ifeq ($(GFXLIB),SDL)
MYCFLAGS += -DSDL
OBJS += sdl.o SDL_gfxPrimitives.o SDL_gfxPrimitives_Byte.o SDL_rotozoom.o SFont.o
HDR += SDL_gfxPrimitives.h SDL_rotozoom.h SFont.h
ifdef IPOD
MYCFLAGS += -I../sdlincludes
LIBSA += ../libs/SDL/*.a
LIBS += -lpthread -lm
else
MYCFLAGS += -I/usr/include/SDL -I/usr/local/include/SDL `sdl-config --cflags`
LIBS += `sdl-config --libs` -lSDL_image -lpng -ljpeg
endif

else
ifeq ($(GFXLIB),mwin)
MYCFLAGS += -DMWIN -I../mwincludes
OBJS += mwin.o
ifdef IPOD
LIBSA += ../libs/mwin/*.a
else
LIBSA += $(shell find ../microwindows* ../../microwindows* -name 'lib*.a' 2>/dev/null)
ifeq ($(strip $(LIBSA)),)
$(error Unable to find microwindows. Put it in ../microwindows-SOMETHING and make sure it is compiled.)
endif
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
	$(CC) $(CFLAGS) $(MYCFLAGS) -c -o $@ $<
%.o: %.cc
	$(CXX) $(CFLAGS) $(CXXFLAGS) $(MYCFLAGS) -c -o $@ $<
lex.yy.c: appearance.l
	$(FLEX) appearance.l

# Examples:
%: %.o libttk.a
	$(CC) $(CFLAGS) $(MYCFLAGS) $(E2F) -o $@ $< $(GS) libttk.a $(LIBS) $(GE)

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
