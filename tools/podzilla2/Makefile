# The next line is for SVN. Change if you do a release!
VERSION = SVN-$(shell svnversion .)
export VERSION

ifdef SUBDIRS
do-it: buildmod
endif

ifeq ($(shell which ttk-config 2>/dev/null >/dev/null && echo yes),yes)
TTKCONF = ttk-config
else
ifdef TTKDIR
TTKCONF = $(TTKDIR)/ttk-config-here
else
ifneq ($(wildcard ../ttk/ttk-config-here),)
TTKCONF = ../ttk/ttk-config-here
else
$(error Cannot find TTK. Specify TTKDIR,  put it in ../ttk, or install it.)
endif
endif
endif

ifdef IPOD
CC = $(CROSS)-gcc
LIBS += -Wl,-elf2flt -Wl,-whole-archive -lc `$(TTKCONF) --ipod --sdl --libs` contrib/ucdl/libuCdl.a -lintl `$(CC) -print-libgcc-file-name` -Wl,-no-whole-archive -mapcs
CROSS ?= arm-uclinux-elf
else
ifeq ($(shell uname),Darwin)
EXPSYM =
INTL = -lintl
else
EXPSYM = -Wl,-E
INTL =
LIBS += -L/sw/lib
endif
LIBS += `$(TTKCONF) --x11 --sdl --libs` -L/usr/local/lib -ldl $(INTL) $(EXPSYM)
CC ?= cc
endif
POD ?= ../../../podfile/pod # relative to two dirs down

all-msg: all .message

all: checktarget core modules contrib xpods pods
	@echo
	@echo "Linking podzilla."
	@echo " LD      podzilla"
	@$(CC) -o podzilla core/built-in.o $(wildcard modules/built-in.o) $(LIBS)
ifdef IPOD
# This must be AFTER the elf2flt compilation, because elf2flt deletes podzilla.elf.
	@echo " LD      podzilla.elf"
	@$(CC) -Wl,-r -d -o podzilla.o core/built-in.o $(wildcard modules/built-in.o) $(LIBS)
	@$(CROSS)-ld.real -T elf2flt.ld -Ur -o podzilla.elf podzilla.o
	@echo " SYMS    podzilla"
	@$(CROSS)-nm podzilla.elf | ./contrib/ucdl/symadd podzilla
endif

.message:
ifdef IPOD
	@echo
	@echo "-------------------------------------------------"
	@echo "Podzilla and modules have been built. Do"
	@echo "  cp podzilla <IPOD>/bin/podzilla"
	@echo "  cp pods/* <IPOD>/usr/lib/"
	@echo "to install it. If you don't want to use the PODs"
	@echo "for some reason, directly copy the module dirs"
	@echo "instead:"
	@echo "  cp -R xpods/* <IPOD>/usr/lib/"
	@echo "Enjoy."
	@echo "-------------------------------------------------"
else
	@echo
	@echo "---------------------------------------------------------------"
	@echo "Podzilla and modules have been built. First, set up some stuff:"
	@echo "  make dev-env    (assuming you have ttk in ../ttk)"
	@echo "Then, to run it,"
	@echo "  ./podzilla"
	@echo "or, for a different screen size,"
	@echo "  ./podzilla [-nano | -mini | -photo | -video]"
	@echo "Enjoy."
	@echo "---------------------------------------------------------------"
endif
	@touch .message

checktarget: .curtgt
	@if [ -f .target ]; then cmp .curtgt .target >/dev/null || ( echo "Cleaning because of target change."; make -s clean; echo ); fi
	@cp .curtgt .target

.curtgt:
ifdef IPOD
	@echo "iPod" > .curtgt
else
	@echo "X11" > .curtgt
endif

core:
	@echo "Building core."
	@make -sC core

modules:
	@echo
	@echo "Building modules, stage 1."
	@true > modules/.mods
	@make -sC modules

clean:
	@echo " CLEAN  podzilla"
	@rm -f podzilla podzilla.gdb
	@echo " CLEAN  core"
	@make clean -sC core
	@echo " CLEAN  modules:"
	@make clean -sC modules
	@rm -f .message .target modules/.mods

dev-env:
	mkdir -p config pods xpods
	-ln -sf ../ttk/fonts
	-ln -sf ../ttk/schemes
	-ln -sf mono.cs schemes/default.cs

xpods:
	@echo
	@echo "Building modules, stage 2."
	@mkdir -p xpods
	@make xpods -sC modules

ifdef IPOD
pods:
	@echo
	@echo "Building modules, stage 3 (iPod)."
	@mkdir -p pods
	@for dir in `cat modules/.mods`; do ( cd xpods/$$dir; echo " POD     $$dir"; $(POD) -c ../../pods/$$dir.pod * ); done
else
# Just make dummy files
pods:
	@echo
	@mkdir -p pods
	@echo "Building modules, stage 3 (desktop)."
	@for dir in `cat modules/.mods`; do echo " POD [D] $$dir"; touch pods/$$dir.pod; done
endif

ifdef SUBDIRS
buildmod:
	@for dir in $(SUBDIRS); do \
	make -sC $$dir -f `pwd`/module.mk PZPATH=`pwd`; done
endif

contrib:
ifdef IPOD
	@make -sC contrib/ucdl
endif

API.pdf: API.tex
	pdflatex API.tex
	pdflatex API.tex

.PHONY: core modules clean pods xpods dev-env contrib .curtgt
