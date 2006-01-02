obj-m = terminal.o
MODULE = terminal
ifdef IPOD
MODLIBS = -lutil
else
ifdef HAVE_UTIL
MODLIBS = -lutil
endif
endif
DATA = minix-sh
