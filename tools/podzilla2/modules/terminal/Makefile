obj-m = terminal.o
MODULE = terminal
ifdef __linux__
MODLIBS = -lutil
endif
ifdef __FreeBSD__
MODLIBS = -lutil
endif
DATA = minix-sh
