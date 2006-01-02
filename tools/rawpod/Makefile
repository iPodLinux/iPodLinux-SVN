findipod: librawipod.a findipod.cc
	g++ -o $@ findipod.cc librawipod.a -DFIND_IPOD_APP

librawipod.a: partition.o ext2.o device.o vfs.o
	ar cru $@ $?

partition.o: partition.h partition.cc
vfs.o: vfs.h vfs.cc
device.o: device.h vfs.h device.cc
ext2.o: ext2.h vfs.h ext2.cc

clean:
	rm -f librawipod.a findipod *.o
