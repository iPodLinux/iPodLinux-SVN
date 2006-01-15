SUBDIRS := $(shell for dir in */; do if test -f $$dir/Module; then echo $$dir | sed 's|/$$||'; fi; done)

DESTDIR ?= /mnt/ipod

#SUBDIRS = mymodule pong wumpus calc calendar clocks cube dialer invaders ipobble \
#          lights mandelpod matrix minesweeper othello poddraw tictactoe tunnel

ifeq ($(shell uname),Darwin)
XARGS_I = -IX
else
XARGS_I = -iX
endif

all:
	@set -e; for dir in $(SUBDIRS); do \
		cd $$dir; \
		make -sqf ../../module.mk || echo $$dir >> ../.mods; \
		make -sf ../../module.mk; \
		cd ..; \
	done
	@if find * -name built-in.o | grep '/built-in.o'; then \
		rm -f built-in.o; \
		echo " LD      built-in.o"; \
		find . -name built-in.o | xargs ld -r -o built-in.o; \
	fi

install: all
	@set -e; for dir in $(SUBDIRS); do \
		cd $$dir; \
		make -sf ../../module.mk install DESTDIR=$(DESTDIR); \
		cd ..; \
	done

xpods:
	@set -e; for dir in `cat .mods`; do \
		cd $$dir; \
		echo " XPOD    $$dir"; \
		rm -rf ../../xpods/$$dir; \
		mkdir -p ../../xpods/$$dir; \
		make -sf ../../module.mk distfiles | fgrep -v 'make[' | xargs $(XARGS_I) cp X ../../xpods/$$dir; \
		cd ..; \
	done

clean:
	@set -e; for dir in $(SUBDIRS); do \
		cd $$dir; \
		echo " CLEAN   $$dir"; \
		make -sf ../../module.mk clean; \
		cd ..; \
	done

