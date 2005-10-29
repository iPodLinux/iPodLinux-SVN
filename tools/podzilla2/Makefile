ifdef IPOD
LIBS += `ttk-config --ipod --sdl --libs` contrib/ucdl/libuCdl.a
CC ?= arm-elf-gcc
else
LIBS += `ttk-config --x11 --sdl --libs` -ldl
CC ?= cc
endif

all: core modules contrib
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

.PHONY: core modules clean
