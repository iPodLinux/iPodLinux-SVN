ifdef IPOD
LIBS += `ttk-config --ipod --sdl --libs` contrib/ucdl/libuCdl.a
CC ?= arm-elf-gcc
else
LIBS += `ttk-config --x11 --sdl --libs` -ldl
CC ?= cc
endif
POD ?= ../../../podfile/pod # relative to two dirs down

all: core modules contrib deskpods
	$(CC) -o podzilla core/built-in.o modules/built-in.o $(LIBS)

core:
	make -C core

modules:
	touch modules/built-in.o
#	make -C modules

clean:
	rm -f podzilla
	make clean -C core
#	make clean -C modules

dev-env:
	mkdir -p config pods xpods
	-ln -sf ../ttk/fonts
	-ln -sf ../ttk/schemes

pods:
	@for dir in xpods/*; do ( cd $$dir; echo -n "$$dir.. "; $(POD) -c ../../pods/`echo $$dir | sed 's|xpods/||'`.pod *; echo "done" ); done

# Just makes dummy files
deskpods:
	@for dir in xpods/*; do touch `echo $$dir | sed 's|^x||;s|$$|.pod|'`; done

.PHONY: core modules clean pods dev-env
