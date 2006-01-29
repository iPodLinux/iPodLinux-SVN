PZPATH ?= ../..

ifeq ($(shell which ttk-config 2>/dev/null >/dev/null && echo yes),yes)
TTKCONF = ttk-config
else
ifdef TTKDIR
TTKCONF = $(TTKDIR)/ttk-config-here
else
ifneq ($(wildcard $(PZPATH)/../ttk),)
TTKCONF = $(PZPATH)/../ttk/ttk-config-here
else
$(error Cannot find TTK. Specify TTKDIR, put it in ../ttk, or install it.)
endif
endif
endif

default: all-check

include Makefile
-include $(PZPATH)/.config
ifndef MODULE
$(error You must define MODULE.)
endif

ifeq ($(MODULE_$(MODULE)),)
all-check:
	@echo " (Skipping $(MODULE).)"
	@rm -f built-in.o
distfiles:
install:
else
all-check: all
distfiles: real-distfiles
install: real-install
endif

ifeq ($(MODULE_$(MODULE)),y)
STATIC := 1
endif

ifdef STATIC
obj-y := $(obj-y) $(obj-m)
obj-m =
endif

cobj-y  := $(patsubst %.c,%.o,$(wildcard $(patsubst %.o,%.c,$(obj-y))))
ccobj-y := $(patsubst %.cc,%.o,$(wildcard $(patsubst %.o,%.cc,$(obj-y))))
cobj-m  := $(patsubst %.c,%.o,$(wildcard $(patsubst %.o,%.c,$(obj-m))))
ccobj-m := $(patsubst %.cc,%.o,$(wildcard $(patsubst %.o,%.cc,$(obj-m))))

obj = $(obj-y) $(obj-m)
ifeq ($(strip $(obj)),)
ifeq ($(strip $(MODLIBS)),)
$(error You must specify some object files or libraries.)
else
ifneq ($(shell uname),Darwin)
WA = -whole-archive
NWA = -no-whole-archive
endif
ifndef STATIC
obj-m = LIBRARY
endif
endif
else
WA =
NWA =
endif

ifdef IPOD
CROSS ?= arm-uclinux-elf
CC = $(CROSS)-gcc
CXX = $(CROSS)-g++
LD = $(CROSS)-ld
OBJCOPY = $(CROSS)-objcopy
TARGET = ipod
PIC =
MYCFLAGS = -mapcs -mcpu=arm7tdmi -DVERSION=\"$(VERSION)\" -Wall
else
CC ?= cc
CXX ?= c++
LD ?= ld
OBJCOPY ?= objcopy
TARGET = x11
ifeq ($(shell uname),Darwin)
PIC = -dynamic
MAKESO = ld -bundle /usr/lib/bundle1.o -flat_namespace -undefined suppress `gcc -print-libgcc-file-name`
else
PIC = -fPIC -DPIC
MAKESO = cc -shared
endif
MYCFLAGS = -DVERSION=\"$(VERSION)\"
endif

ifdef obj-y
built-in = built-in.o
endif

ifdef obj-m
ifeq ($(obj-m),LIBRARY)
obj-m =
endif
## Case for single-file
ifndef IPOD
finalmod = $(MODULE).so
$(MODULE).so: $(obj-m)
	@echo " LD [SO] $(MODULE).so"
	@$(MAKESO) $(LDFLAGS) -o $@ $(obj-m) $(MODLIBS)
else
finalmod = $(MODULE).mod.o
$(MODULE).mod.o: $(obj-m)
	@echo " LD [M]  $(MODULE).mod.o"
	@$(LD) -Ur -d -o $(MODULE).mod.o $(obj-m) $(WA) $(MODLIBS) $(NWA)
endif

else
finalmod =
endif

#####
all: $(obj-y) $(built-in) $(obj-m) $(finalmod)
#####

ifdef obj-y
built-in.o: $(obj-y)
	@echo " LD      $(MODULE)"
ifdef IPOD
	@$(LD) -r -o built-in.o.strong $(obj-y) -L$(dir $(shell $(CC) -print-file-name=libc.a)) $(WA) $(MODLIBS) $(NWA)
	@$(OBJCOPY) --weaken built-in.o.strong built-in.o
	@$(RM) -f built-in.o.strong
else
	@$(LD) -r -o built-in.o $(obj-y) -L$(dir $(shell $(CC) -print-file-name=libc.a)) $(WA) $(MODLIBS) $(NWA)
endif

$(cobj-y): %.o: %.c $(PZPATH)/config.h
	@echo " CC     " $@
	@$(CC) $(CFLAGS) $(MYCFLAGS) -c -o $@ $< -I$(PZPATH)/core -I$(PZPATH) `$(TTKCONF) --$(TARGET) --sdl --cflags` -D__PZ_BUILTIN_MODULE -D__PZ_MODULE_NAME=\"$(MODULE)\" -DPZ_MOD -I/sw/include -L/sw/lib

$(ccobj-y): %.o: %.cc $(PZPATH)/config.h
	@echo " CXX    " $@
	@$(CXX) $(CFLAGS) $(MYCFLAGS) -c -o $@ $< -I$(PZPATH)/core `$(TTKCONF) --$(TARGET) --sdl --cflags` -D__PZ_BUILTIN_MODULE -D__PZ_MODULE_NAME=\"$(MODULE)\" -DPZ_MOD -I/sw/include -L/sw/lib
endif

#####

ifdef obj-m
$(cobj-m): %.o: %.c $(PZPATH)/config.h
	@echo " CC [M] " $@
	@$(CC) $(CFLAGS) $(MYCFLAGS) $(PIC) -c -o $@ $< -I$(PZPATH) -I$(PZPATH)/core `$(TTKCONF) --$(TARGET) --sdl --cflags` -D__PZ_MODULE_NAME=\"$(MODULE)\" -DPZ_MOD -I/sw/include -L/sw/lib
$(ccobj-m): %.o: %.cc $(PZPATH)/config.h
	@echo " CXX [M]" $@
	@$(CXX) $(CFLAGS) $(MYCFLAGS) $(PIC) -c -o $@ $< -I$(PZPATH) -I$(PZPATH)/core `$(TTKCONF) --$(TARGET) --sdl --cflags` -D__PZ_MODULE_NAME=\"$(MODULE)\" -DPZ_MOD -I/sw/include -L/sw/lib
endif

LIBRARY:

#####
#####

semiclean:
	@rm -f $(obj) built-in.o

clean:
	@rm -f $(MODULE).mod.o $(MODULE).so $(obj) built-in.o

real-distfiles:
	@echo Module
ifneq ($(MODULE_$(MODULE)),y)
ifdef IPOD
	@echo $(MODULE).mod.o
else
	@echo $(MODULE).so
endif
endif
ifdef DATA
	@for file in $(DATA); do \
		echo $$file; \
	done
endif

DESTDIR ?= /mnt/ipod

real-install:
	@echo " INST    $(MODULE)"
	@rm -rf $(DESTDIR)/usr/lib/$(MODULE)
	@install -d -m 755 $(DESTDIR)/usr/lib/$(MODULE)
	@install -m 644 Module $(DESTDIR)/usr/lib/$(MODULE)/Module
ifneq ($(MODULE_$(MODULE)),y)
	@install -m 755 $(MODULE).mod.o $(DESTDIR)/usr/lib/$(MODULE)/$(MODULE).mod.o
endif
	@for file in $(DATA); do \
		install -m 755 $$file $(DESTDIR)/usr/lib/$(MODULE)/; \
	done
