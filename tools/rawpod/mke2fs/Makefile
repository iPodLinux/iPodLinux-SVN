CXXFLAGS := -I. -I.. -I../.. -Ie2p -Iet -Iext2fs -Iuuid -I../e2p -I../et -I../ext2fs -I../uuid
ifdef DEBUG
CXXFLAGS += -g
endif
export CXXFLAGS

all: subdirs mke2fs_app.o
	find . -name '*.o' | sort | xargs ld -r -o mke2fs.o
subdirs:
	for dir in e2p et ext2fs uuid; do make -C $$dir; done
clean:
	for dir in . e2p et ext2fs uuid; do rm -f $$dir/*.o; done
