obj-m = terminal.o
MODULE = terminal
ifdef IPOD
MODLIBS = -lutil
else
ifneq ($(shell uname),Darwin)
MODLIBS = -lutil
endif
endif
DATA = minix-sh
