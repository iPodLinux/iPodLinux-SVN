ifdef IPOD
CC ?= arm-elf-gcc
CFLAGS += `ttk-config --ipod --sdl --cflags`
LIBS += `ttk-config --ipod --sdl --libs`
else
CC ?= cc
CFLAGS += `ttk-config --x11 --sdl --cflags`
LIBS += `ttk-config --x11 --sdl --libs`
endif

OBJS= pz.o module.o

podzilla: $(OBJS)
	$(CC) $(CFLAGS) -o podzilla $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
