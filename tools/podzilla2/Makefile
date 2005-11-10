ifeq ($(shell which ttk-config 2>/dev/null >/dev/null && echo yes),yes)
TTKCONF = ttk-config
else
ifdef TTKDIR
TTKCONF = $(TTKDIR)/ttk-config-here
else
ifneq ($(wildcard ../ttk/ttk-config-here),)
TTKCONF = ../ttk/ttk-config-here
else
$(error Can't find TTK. Specify TTKDIR,  put it in ../ttk, or install it.)
endif
endif
endif

ifdef IPOD
LIBS += -Wl,-elf2flt `$(TTKCONF) --ipod --sdl --libs` contrib/ucdl/libuCdl.a -lintl
CC = arm-elf-gcc
else
ifeq ($(shell uname),Darwin)
EXPSYM =
INTL = -lintl
else
EXPSYM = -Wl,-E
INTL =
endif
LIBS += `$(TTKCONF) --x11 --sdl --libs` -L/usr/local/lib -L/sw/lib -ldl $(INTL) $(EXPSYM)
CC ?= cc
endif
POD ?= ../../../podfile/pod # relative to two dirs down

all: checktarget core modules xpods pods
	@echo
	@echo "Linking podzilla."
	@echo " LD      podzilla"
	@$(CC) -o podzilla core/built-in.o $(wildcard modules/built-in.o) $(LIBS)

checktarget: .curtgt
	@cmp .curtgt .target >/dev/null || ( echo "Cleaning because of target change."; make -s clean; echo )
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
	@make -sC modules

clean:
	@echo " CLEAN   podzilla"
	@rm -f podzilla podzilla.gdb
	@echo " CLEAN   core"
	@make clean -sC core
	@echo " CLEAN   modules"
	@make clean -sC modules

dev-env:
	mkdir -p config pods xpods
	-ln -sf ../ttk/fonts
	-ln -sf ../ttk/schemes

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
	@for dir in xpods/*; do ( cd $$dir; echo " POD     $$dir"; $(POD) -c ../../pods/`echo $$dir | sed 's|xpods/||'`.pod * ); done
else
# Just make dummy files
pods:
	@echo
	@mkdir -p pods
	@echo "Building modules, stage 3 (desktop)."
	@for dir in xpods/*; do echo " POD [D] $$dir"; touch `echo $$dir | sed 's|^x||;s|$$|.pod|'`; done
endif

ifdef SUBDIRS
buildmod:
	@for dir in $(SUBDIRS); do \
	make -sC $$dir -f `pwd`/module.mk PZPATH=`pwd`; done
endif

.PHONY: core modules clean pods xpods dev-env .curtgt
