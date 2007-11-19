# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

# The next line is for SVN. Change if you do a release!
VERSION = SVN-$(shell svnversion .)
export VERSION

DESTDIR ?= /mnt/ipod

ifdef SUBDIRS
do-it: buildmod
endif

ifndef TTKDIR
ifeq ($(shell which ttk-config 2>/dev/null >/dev/null && echo yes),yes)
MYTTKCONF = ttk-config
export TTKCONF := $(MYTTKCONF)
endif
ifneq ($(wildcard ttk/ttk-config-here),)
TTKDIR = ttk
else
ifneq ($(wildcard ../ttk/ttk-config-here),)
TTKDIR = ../ttk
else
ifneq ($(wildcard ../../../libs/ttk/ttk-config-here),)
TTKDIR = ../../../libs/ttk
endif
endif
endif
endif

ifndef TTKCONF
ifndef TTKDIR
$(error Cannot find TTK. Specify TTKDIR,  put it in ttk, ../ttk, ../../../libs/ttk, or install it.)
else
MYTTKCONF = $(TTKDIR)/ttk-config-here
export TTKCONF=${PWD}/$(MYTTKCONF)
endif
endif

ifdef IPOD
CC = $(CROSS)-gcc
LIBS += -Wl,-elf2flt -Wl,-whole-archive -lc `$(MYTTKCONF) --ipod --sdl --libs` contrib/ucdl/libuCdl.a -lintl `$(CC) -print-libgcc-file-name` -lsupc++ -Wl,-no-whole-archive -mapcs
CROSS ?= arm-uclinux-elf
else
ifeq ($(shell uname),Darwin)
EXPSYM =
INTL = -lintl
else
EXPSYM = -Wl,-E
INTL =
LIBS += -L/sw/lib
endif
LIBS += -g `$(MYTTKCONF) --x11 --sdl --libs` -L/usr/local/lib -lstdc++ -ldl $(INTL) $(EXPSYM)
CC ?= cc
endif
POD ?= ../../../podfile/pod # relative to two dirs down

all-msg: all .message

.config:
	@$(MAKE) -s silentdefconfig

Config.in:
	@perl make-config.pl

Kconfig/conf:
	@echo
	@echo Building text-based configuration utility.
	@$(MAKE) -sC Kconfig conf
Kconfig/mconf:
	@echo
	@echo Building graphical configuration utility.
	@$(MAKE) -sC Kconfig mconf

config: Kconfig/conf Config.in
	@echo " CONF"
	@./Kconfig/conf Config.in

menuconfig: Kconfig/mconf Config.in
	@echo " MCONF"
	@./Kconfig/mconf Config.in

defconfig: Kconfig/conf Config.in
	@echo " CONF    [def]"
	@./Kconfig/conf -d Config.in

oldconfig: Kconfig/conf Config.in
	@echo " CONF    [old]"
	@./Kconfig/conf -o Config.in

silentdefconfig: Kconfig/conf Config.in
	@echo " CONF    [def]"
	@./Kconfig/conf -d Config.in >/dev/null

# Everything in one executable
fatconfig: Kconfig/conf Config.in
	@echo " CONF    [fat]"
	@./Kconfig/conf -y Config.in

# Everything as modules
chunkyconfig: defconfig

# No modules
slimconfig: Kconfig/conf Config.in
	@echo " CONF    [slim]"
	@./Kconfig/conf -n Config.in

all: .config checktarget core modules contrib
	@echo
	@echo "Linking podzilla."
	@echo " LD      podzilla"
	@$(CC) -o podzilla core/built-in.o $(wildcard modules/built-in.o) $(LIBS)
ifdef IPOD
# This must be AFTER the elf2flt compilation, because elf2flt deletes podzilla.elf.
	@echo " LD      podzilla.elf"
	@$(CC) -Wl,-r -d -o podzilla.o core/built-in.o $(wildcard modules/built-in.o) $(LIBS)
	@$(CROSS)-ld.real -T elf2flt.ld -Ur -o podzilla.elf podzilla.o
	@echo " SYMS    podzilla"
	@$(CROSS)-nm podzilla.elf | ./contrib/ucdl/symadd podzilla
endif

install:
	@if [ "$(DESTDIR)" = "/mnt/ipod" -a -z "$(wildcard $(DESTDIR)/bin)" ]; then echo "*** $(DESTDIR)/bin does not exist. Please verify that you have correctly specified your iPod location. Stop."; false; fi
	@mkdir -p $(DESTDIR)/bin $(DESTDIR)/usr/lib
	@mkdir -p $(DESTDIR)/usr/share/schemes $(DESTDIR)/usr/share/fonts
	@echo "Installing to $(DESTDIR)."
	@echo " INST   podzilla"
	@install -m 755 podzilla $(DESTDIR)/bin/podzilla
	@echo " INST   modules:"
	@$(MAKE) install -sC modules DESTDIR=$(DESTDIR) IPOD=1
	@if [ ! -d $(TTKDIR)/schemes ]; then echo "*** No Schemes directory found. Not installed." ; false; fi
	@echo " INST   schemes:"
	@install $(TTKDIR)/schemes/* $(DESTDIR)/usr/share/schemes/
	@echo " INST   fonts:"
	@if [ ! -d $(TTKDIR)/fonts ]; then echo "*** No Fonts directory found. Not installed." ; false; fi
	@install $(TTKDIR)/fonts/* $(DESTDIR)/usr/share/fonts/

.message:
ifdef IPOD
	@echo
	@echo "-------------------------------------------------"
	@echo "Podzilla and modules have been built. Do"
	@echo "  make install DESTDIR=/mnt/ipod IPOD=1"
	@echo "to install it. Enjoy."
	@echo "-------------------------------------------------"
else
	@echo
	@echo "---------------------------------------------------------------"
	@echo "Podzilla and modules have been built. First, set up some stuff:"
	@echo "  make dev-env    (assuming you have ttk in ttk, ../ttk or ../../../libs/ttk)"
	@echo "Then, to run it,"
	@echo "  ./podzilla"
	@echo "or, for a different screen size,"
	@echo "  ./podzilla [-g nano | -g mini | -g photo | -g video]"
	@echo "Enjoy."
	@echo "---------------------------------------------------------------"
endif
	@touch .message

checktarget: .curtgt
	@if [ -f .target ]; then cmp .curtgt .target >/dev/null || ( echo "Cleaning because of target change."; make -s clean; echo ); fi
	@cp .curtgt .target

.curtgt:
ifdef IPOD
	@echo "iPod" > .curtgt
else
	@echo "X11" > .curtgt
endif

core:
	@echo
	@echo "Building core."
	@make -sC core

modules:
	@echo
	@echo "Building modules."
	@true > modules/.mods
	@make -sC modules

clean:
	@echo " CLEAN  podzilla"
	@rm -f podzilla podzilla.gdb
	@echo " CLEAN  config"
	@make clean -sC Kconfig
	@echo " CLEAN  core"
	@make clean -sC core
	@echo " CLEAN  contrib/ucdl"
	@make clean -sC contrib/ucdl
	@echo " CLEAN  modules:"
	@make clean -sC modules
	@rm -f .message .target modules/.mods Config.in menu-strings.c
	@rm -f podzilla.o podzilla.elf
	@make clean -sC Kconfig

mrproper: clean
	@rm -f .config config.h
	@rm -f podzilla2.pot

dev-env:
ifndef TTKDIR
	$(error Cannot find TTK. Specify TTKDIR,  put it in ttk, ../ttk, ../../../libs/ttk, or install it.)
else
	mkdir -p config pods xpods
	-ln -sf $(TTKDIR)/fonts
	-ln -sf $(TTKDIR)/schemes
	-ln -s mono.cs schemes/default.cs
endif

xpods:
	@echo
	@echo "Building modules, stage 2."
	@mkdir -p xpods
	@make xpods -sC modules

ifdef IPOD
pods: xpods
	@echo
	@echo "Building modules, stage 3 (iPod)."
	@mkdir -p pods
	@for dir in `cat modules/.mods`; do ( cd xpods/$$dir; echo " POD     $$dir"; $(POD) -c ../../pods/$$dir.pod * ); done
else
# Just make dummy files
pods:
	@echo
	@mkdir -p pods
	@echo "Building modules, stage 3 (desktop)."
	@for dir in `cat modules/.mods`; do echo " POD [D] $$dir"; touch pods/$$dir.pod; done
endif

ifdef SUBDIRS
buildmod:
	@for dir in $(SUBDIRS); do \
	make -sC $$dir -f `pwd`/module.mk PZPATH=`pwd`; done
endif

contrib:
ifdef IPOD
	@make -sC contrib/ucdl
endif

API.pdf: API.tex
	pdflatex API.tex
	pdflatex API.tex

translate:
	@echo " Extracting menu strings..."
	@find . -type f -name '*.c' | perl xgettext-menu.pl > menu-strings.c
	@echo " Extracting other strings..."
	@xgettext -kN_ -k_ -o pz2.pot.tmp `find . -type f -name '*.c' -print`
	@echo " Generating template file..."
	@echo "# podzilla2 LANGUAGE translation" > podzilla2.pot
	@echo "# Copyright (C) 2003-2006 Bernard Leach" >> podzilla2.pot
	@echo "# This file is distributed under the same license as the podzilla2 package." >> podzilla2.pot
	@tail -n +4 pz2.pot.tmp >> podzilla2.pot
	@rm -f pz2.pot.tmp
	@echo
	@echo "---------------------------------------------------------------"
	@echo "Translation template file is in podzilla2.pot"
	@echo "If you haven't already, please read the internationalization"
	@echo " documentation at http://ipodlinux.org/Internationalization"
	@echo "---------------------------------------------------------------"

.PHONY: core modules clean pods xpods dev-env contrib translate .curtgt Config.in

help:
	@echo ""
	@echo === Useful makefile targets ===
	@echo ""
	@echo == general targets ==
	@echo all .......... build the entire thing
	@echo install ...... build and install to your ipod
	@echo clean ........ remove all generated binaries
	@echo dev-env ...... links ttk directories over for desktop dev
	@echo ""
	@echo == language ==
	@echo translate .... make the translation strings files
	@echo ""
	@echo == module config ==
	@echo defconfig .... generate a default configuration file
	@echo mrproper ..... a more complete 'clean' which wipes config stuff
	@echo config ....... text-based module configuration
	@echo menuconfig ... curses-based module configuration
	@echo ""
	@echo == developer ==
	@echo modulesourcedist MODULE=modulename MODVERS=r24 ... makes a source zip
	@echo ""

.PHONY: help


modulesourcedist:
	@if [[ "$(MODULE)" == "" ]]; then echo "make modulesourcedist MODULE=YourModuleName MODVERS=1.0"; false ; fi
	@if [[ "$(MODVERS)" == "" ]]; then echo "make modulesourcedist MODULE=YourModuleName MODVERS=1.0"; false ; fi
	@if [ ! -d modules/$(MODULE) ]; then echo "Module $(MODULE) does not exist." ; false; fi
	@if [ ! -f modules/$(MODULE)/Makefile ]; then echo "No Makefile."; false; fi
	@if [ ! -f modules/$(MODULE)/Module ]; then echo "No Module file."; false; fi
	@echo -- Creating directory...
	@-rm -rf $(MODULE)_$(MODVERS)_src
	@mkdir -p $(MODULE)_$(MODVERS)_src/$(MODULE)
	@echo -- Copying files...
	@cp -r modules/$(MODULE)/* $(MODULE)_$(MODVERS)_src/$(MODULE)
	@echo -- Removing extraneous files...
	@-rm -rf $(MODULE)_$(MODVERS)_src/$(MODULE)/*.o
	@-rm -rf $(MODULE)_$(MODVERS)_src/$(MODULE)/*.so
	@-rm -rf $(MODULE)_$(MODVERS)_src/$(MODULE)/.svn
	@-rm -rf $(MODULE)_$(MODVERS)_src/$(MODULE)/CVS
	@-rm -rf $(MODULE)_$(MODVERS)_src/$(MODULE)/*~
	@echo -- Building ZIP file...
	@zip -rp $(MODULE)_$(MODVERS)_src.zip $(MODULE)_$(MODVERS)_src
	@echo -n -- Cleaning up...
	@-rm -rf $(MODULE)_$(MODVERS)_src
	@echo ...done.
	@echo
	@echo Your module source archive is:   $(MODULE)_$(MODVERS)_src.zip

.PHONY: moduledist
