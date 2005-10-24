ifdef MOD_2_6

obj-m := podfs.o

else

CFLAGS += -Wall -pedantic -g
OBJS = pod.o

all: $(OBJS)
	$(CC) -o pod $(OBJS) $(LDFLAGS)

pod.o: pod.h

module-2.6:
	make -C /usr/src/linux SUBDIRS=${PWD} MOD_2_6=1 modules

module-2.4:
	gcc -D__KERNEL__ -DMODULE -I/usr/src/linux/include -O2 -c podfs.c

clean:
	$(RM) pod $(OBJS) podfs.mod.c podfs.mod.o podfs.o podfs.ko

endif
