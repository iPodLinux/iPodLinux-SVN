ifneq ($(IPOD),)
PREFIX=arm-elf-
LDFLAGS=-elf2flt
CFLAGS=-DIPOD
else
# Host buld, dev purposes only
PREFIX=
endif

AR= $(PREFIX)ar rc
AR2= $(PREFIX)ranlib
CC= $(PREFIX)gcc

LIB = libipod.a
LIBOBJS= ipod.o disk.o lcd.o

CFLAGS+=-Wall

all: $(LIB) ipodinfo

$(LIB): $(LIBOBJS)
	$(RM) $(LIB)
	$(AR) $(LIB) $(LIBOBJS)
	$(AR2) $(LIB)

ipodinfo: ipodinfo.o
	$(CC) $? -o $@ $(LDFLAGS) $(LIB)
 
clean:
	$(RM) *.o *.a *.gdb ipodinfo
