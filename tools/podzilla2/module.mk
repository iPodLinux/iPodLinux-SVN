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
$(error You must specify some object files.)
endif

ifdef IPOD
CROSS ?= arm-elf
CC = $(CROSS)-gcc
LD = $(CROSS)-ld
TARGET = ipod
PIC =
MYCFLAGS = -mapcs -mcpu=arm7tdmi -DVERSION=\"$(VERSION)\"
else
CC ?= cc
LD ?= ld
TARGET = x11
ifeq ($(shell uname),Darwin)
PIC = -dynamic
MAKESO = ld -bundle /usr/lib/bundle1.o -flat_namespace -undefined suppress
else
PIC = -fPIC -DPIC
MAKESO = cc -shared
MYCFLAGS = -DVERSION=\"$(VERSION)\"
endif
endif

ifdef obj-y
built-in = built-in.o
endif

ifdef obj-m
ifeq ($(words $(obj-m)),1)
## Case for single-file
ifndef IPOD
finalmod = $(MODULE).so
onlyso = true
$(MODULE).so: $(obj-m)
	@$(MAKESO) -o $@ $(obj-m)
else
finalmod = $(MODULE).mod.o
$(MODULE).mod.o: $(obj-m)
	@cp $(obj-m) $(MODULE).mod.o
	@cp $(MODULE).mod.o $(MODULE).o
endif

else
## Case for multi-file
ifndef IPOD
finalmod = $(MODULE).so
$(MODULE).so: $(obj-m)
	@echo " LD [SO] $(MODULE).so"
	@$(MAKESO) -o $@ $(obj-m)
else
finalmod = $(MODULE).mod.o
$(MODULE).mod.o: $(obj-m)
	@echo " LD [M]  $(MODULE)"
	@$(LD) -r -o $@ $(obj-m) $(MODLIBS)
	@cp $(MODULE).mod.o $(MODULE).o
endif

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


#####
#####

semiclean:
ifdef IPOD
ifeq ($(findstring $(MODULE).o,$(obj)),)
	@rm -f $(obj)
endif
else
	@rm -f $(obj)
endif

clean:
	@rm -f $(MODULE).o $(MODULE).so $(obj)

distfiles:
	@echo Module
ifdef IPOD
	@echo $(MODULE).o
else
	@echo $(MODULE).so
endif
ifdef DATA
	@for file in $(DATA); do \
		echo $$file; \
	done
endif
