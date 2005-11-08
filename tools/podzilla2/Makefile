ifdef IPOD
LIBS += -Wl,-elf2flt `ttk-config --ipod --sdl --libs` contrib/ucdl/libuCdl.a -lintl
CC = arm-elf-gcc
else
LIBS += `ttk-config --x11 --sdl --libs` -ldl $(shell gcc -o .conftest -lintl 2>/dev/null && rm -f .conftest && echo -- -lintl) -Wl,-E
CC ?= cc
endif
POD ?= ../../../podfile/pod # relative to two dirs down

all: checktarget core modules xpods pods
	@echo " LD      podzilla"
	@$(CC) -o podzilla core/built-in.o $(wildcard modules/built-in.o) $(LIBS)

checktarget: .curtgt
	@cmp .curtgt .target || make clean
	@cp .curtgt .target

.curtgt:
ifdef IPOD
	@echo "iPod" > .curtgt
else
	@echo "X11" > .curtgt
endif

core:
	@make -C core

modules:
	@make -C modules

clean:
	@echo " CLEAN   podzilla"
	@rm -f podzilla
	@echo " CLEAN   core"
	@make clean -C core
	@echo " CLEAN   modules"
	@make clean -C modules

dev-env:
	mkdir -p config pods xpods
	-ln -sf ../ttk/fonts
	-ln -sf ../ttk/schemes

xpods:
	@make xpods -C modules

ifdef IPOD
pods:
	@for dir in xpods/*; do ( cd $$dir; echo " MODPOST $$dir"; $(POD) -c ../../pods/`echo $$dir | sed 's|xpods/||'`.pod * ); done
else
# Just make dummy files
pods:
	@for dir in xpods/*; do echo " MODPOST $$dir"; touch `echo $$dir | sed 's|^x||;s|$$|.pod|'`; done
endif

.PHONY: core modules clean pods xpods dev-env .curtgt
