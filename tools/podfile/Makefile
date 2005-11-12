ifdef MOD_2_6

obj-m := podfs.o

else

CFLAGS += -Wall -pedantic -g
OBJS = pod.o
IPLKERNEL ?= ../ipl/linux

all: $(OBJS)
	$(CC) -o pod $(OBJS) $(LDFLAGS)

pod.o: pod.h

module-2.6:
	make -C /usr/src/linux SUBDIRS=${PWD} MOD_2_6=1 CFLAGS=-DMODULE_2_6 modules

module-2.4:
	gcc -D__KERNEL__ -DMODULE -I/usr/src/linux/include -O2 -c podfs.c

module-pod:
	arm-elf-gcc -D__linux__ -D__KERNEL__ -DMODULE -I$(IPLKERNEL)/include -O2 -fno-strict-aliasing -fno-common -DNO_MM -mapcs-32 -march=armv4 -mtune=arm7tdmi -mshort-load-bytes -msoft-float -c -o podfs.o podfs.c

clean:
	$(RM) pod $(OBJS) podfs.mod.c podfs.mod.o podfs.o podfs.ko

endif
