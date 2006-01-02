APPS = ipodls ipodfind ipodcopy

all: librawpod.a $(APPS)

ipodcopy: librawpod.a ipodcopy.cc dirent.cc
	g++ -g -o $@ ipodcopy.cc dirent.cc librawpod.a

ipodls: librawpod.a ipodls.cc
	g++ -o $@ ipodls.cc librawpod.a

ipodfind: librawpod.a ipodfind.cc
	g++ -o $@ ipodfind.cc librawpod.a

librawpod.a: partition.o ext2.o device.o vfs.o
	ar cru $@ $?

partition.o: partition.h device.h vfs.h partition.cc
vfs.o: vfs.h vfs.cc
device.o: device.h vfs.h device.cc
ext2.o: ext2.h vfs.h ext2.cc

clean:
	rm -f librawpod.a $(APPS) *.o
