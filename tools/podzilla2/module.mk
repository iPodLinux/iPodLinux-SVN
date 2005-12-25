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

default: all

include Makefile
ifndef MODULE
$(error You must define MODULE.)
endif

obj = $(obj-y) $(obj-m)
ifeq ($(strip $(obj)),)
ifeq ($(strip $(MODLIBS)),)
$(error You must specify some object files or libraries.)
else
WA = -whole-archive
NWA = -no-whole-archive
obj-m = LIBRARY
endif
else
WA =
NWA =
endif

ifdef IPOD
CROSS ?= arm-uclinux-elf
CC = $(CROSS)-gcc
LD = $(CROSS)-ld
TARGET = ipod
PIC =
MYCFLAGS = -mapcs -mcpu=arm7tdmi -DVERSION=\"$(VERSION)\" -Wall
else
CC ?= cc
LD ?= ld
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
onlyso = true
$(MODULE).so: $(obj-m)
	@echo " LD [SO] $(MODULE).so"
	@$(MAKESO) $(LDFLAGS) -o $@ $(obj-m) $(MODLIBS)
else
finalmod = $(MODULE).mod.o
$(MODULE).mod.o: $(obj-m)
	@echo " LD [M]  $(MODULE).mod.o"
	@$(LD) -r -d -o $(MODULE).mod.o $(obj-m) $(WA) $(MODLIBS) $(NWA)
endif

else
## Case for multi-file
ifndef IPOD
finalmod = $(MODULE).so
$(MODULE).so: $(obj-m)
	@echo " LD [SO] $(MODULE).so"
	@$(MAKESO) $(LDFLAGS) -o $@ $(obj-m) $(MODLIBS)
else
finalmod = $(MODULE).mod.o
$(MODULE).mod.o: $(obj-m)
	@echo " LD [M]  $(MODULE).mod.o"
	@$(LD) -r -d -o $@ $(obj-m) $(WA) $(MODLIBS) $(NWA)
endif

endif

ifeq ($(onlyso),true)
all: $(obj-y) $(built-in) $(finalmod)
else
#####
all: $(obj-y) $(built-in) $(obj-m) $(finalmod)
#####
endif

ifdef obj-y
built-in.o: $(obj-y)
	@echo " LD      $(MODULE)"
	@$(LD) -r -o built-in.o $(obj-y)

$(obj-y): %.o: %.c
	@echo " CC     " $@
	@$(CC) $(CFLAGS) $(MYCFLAGS) -c -o $@ $< -I$(PZPATH)/core `$(TTKCONF) --$(TARGET) --sdl --cflags` -D__PZ_BUILTIN_MODULE -D__PZ_MODULE_NAME=\"$(MODULE)\" -DPZ_MOD -I/sw/include -L/sw/lib
endif

#####

ifdef obj-m
$(obj-m): %.o: %.c
	@echo " CC [M] " $@
	@$(CC) $(CFLAGS) $(MYCFLAGS) $(PIC) -c -o $@ $< -I$(PZPATH)/core `$(TTKCONF) --$(TARGET) --sdl --cflags` -D__PZ_MODULE_NAME=\"$(MODULE)\" -DPZ_MOD -I/sw/include -L/sw/lib
endif

LIBRARY:

#####
#####

semiclean:
	@rm -f $(obj)

clean:
	@rm -f $(MODULE).mod.o $(MODULE).so $(obj)

distfiles:
	@echo Module
ifdef IPOD
	@echo $(MODULE).mod.o
else
	@echo $(MODULE).so
endif
ifdef DATA
	@for file in $(DATA); do \
		echo $$file; \
	done
endif
