APPS = ipodls ipodfind ipodcopy ipodshell ipodrm

all: librawpod.a $(APPS)

ipodcopy: librawpod.a ipodcopy.cc dirent.cc
	g++ $(CXXFLAGS) -o $@ ipodcopy.cc dirent.cc librawpod.a $(LDFLAGS)

ipodls: librawpod.a ipodls.cc
	g++ $(CXXFLAGS) -o $@ ipodls.cc librawpod.a $(LDFLAGS)

ipodfind: librawpod.a ipodfind.cc
	g++ $(CXXFLAGS) -o $@ ipodfind.cc librawpod.a $(LDFLAGS)

ipodshell: librawpod.a ipodshell.cc
	g++ $(CXXFLAGS) -o $@ ipodshell.cc librawpod.a $(LDFLAGS)

ipodrm: librawpod.a ipodrm.cc
	g++ $(CXXFLAGS) -o $@ ipodrm.cc librawpod.a $(LDFLAGS)

librawpod.a: partition.o ext2.o fat32.o device.o vfs.o
	ar cru $@ $?

partition.o: partition.h device.h vfs.h partition.cc
vfs.o: vfs.h vfs.cc
device.o: device.h vfs.h device.cc
ext2.o: ext2.h vfs.h ext2.cc
fat32.o: fat32.h vfs.h fat32.cc

clean:
	rm -f librawpod.a $(APPS) *.o
