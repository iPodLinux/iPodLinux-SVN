#! /usr/bin/make -f

ifneq ($(KERNELRELEASE),)
obj-m	:= ipodeth1394.o

else
KDIR	:= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

modules_install:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules_install

clean:
	rm -f ipodeth1394.ko ipodeth1394.o ipodeth1394.mod.c ipodeth1394.mod.o \
		built-in.o

endif

