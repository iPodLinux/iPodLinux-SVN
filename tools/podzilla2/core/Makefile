ifdef IPOD
CC ?= arm-elf-gcc
CFLAGS += `ttk-config --ipod --sdl --cflags` -I../contrib/ucdl
LIBS += `ttk-config --ipod --sdl --libs` ../contrib/ucdl/libuCdl.a
else
CC ?= cc
CFLAGS += `ttk-config --x11 --sdl --cflags`
LIBS += `ttk-config --x11 --sdl --libs` -ldl
endif

OBJS= pz.o module.o ipod.o dialog.o secrets.o header.o
HDRS= pz.h ipod.h

all: podzilla

podzilla: $(OBJS)
	$(CC) $(CFLAGS) -o podzilla $(OBJS) $(LIBS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f podzilla *.o

.PHONY: clean all
