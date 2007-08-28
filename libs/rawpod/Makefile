ifdef DEBUG
CXXFLAGS += -g
endif
APPS = ipodls ipodfind ipodcopy ipodshell ipodrm imoo

all: librawpod.a $(APPS)

$(APPS): %: %.cc librawpod.a
	g++ $(CXXFLAGS) -o $@ $< librawpod.a $(LDFLAGS)

librawpod.a: partition.o ext2.o fat32.o device.o vfs.o mkdosfs.o rawpod.o mke2fs/mke2fs.o
	ar cru $@ $?

partition.o: partition.h device.h vfs.h partition.cc
vfs.o: vfs.h vfs.cc
device.o: device.h vfs.h device.cc
ext2.o: ext2.h vfs.h ext2.cc
fat32.o: fat32.h vfs.h fat32.cc
mkdosfs.o: vfs.h
rawpod.o: rawpod.h
mke2fs/mke2fs.o:
	make -C mke2fs

clean:
	rm -f librawpod.a $(APPS) *.o
	make clean -C mke2fs

