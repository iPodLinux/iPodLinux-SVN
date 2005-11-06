PZPATH := $(dir $(filter %/module.mk,$(MAKEFILE_LIST)))

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
CC = arm-elf-gcc
LD = arm-elf-ld
TARGET = ipod
PIC =
else
CC ?= cc
LD ?= ld
TARGET = x11
PIC = -fPIC
endif

ifdef obj-y
built-in = built-in.o
endif

ifdef obj-m
ifeq ($(words $(obj-m)),1)
ifeq ($(findstring $(MODULE).o,$(obj-m)),)
$(error If you have only one object file, it must be named after your module.)
endif
## Case for single-file
ifndef IPOD
finalmod = $(MODULE).so
$(MODULE).so: $(obj-m)
	$(CC) -shared -o $@ $<
else
finalmod = $(MODULE).o
# already will be made, don't need a rule
endif

else
ifneq ($(findstring $(MODULE).o,$(obj-m)),)
$(error If you have multiple object files, you cannot have one named after your module.)
endif
## Case for multi-file
ifndef IPOD
finalmod = $(MODULE).so
$(MODULE).so: $(obj-m)
	$(CC) -shared -o $@ $<
else
finalmod = $(MODULE).o
$(MODULE).o: $(obj-m)
	$(LD) -r -o $@ $<
endif

endif
endif

#####
all: $(obj-y) $(built-in) $(obj-m) $(finalmod)
#####

ifdef obj-y
built-in.o: $(obj-y)
	$(LD) -r -o built-in.o $(obj-y)

$(obj-y): %.o: %.c
	$(CC) -c -o $@ $< -I$(PZPATH)/core `ttk-config --$(TARGET) --sdl --cflags` -D__PZ_BUILT_IN
endif

#####

ifdef obj-m
$(obj-m): %.o: %.c
	$(CC) $(PIC) -c -o $@ $< -I$(PZPATH)/core `ttk-config --$(TARGET) --sdl --cflags`
endif


#####
#####

semiclean:
ifeq ($(findstring $(MODULE).o,$(obj)),)
	rm -f $(obj)
endif

clean:
	rm -f $(MODULE).o $(MODULE).so $(obj)

