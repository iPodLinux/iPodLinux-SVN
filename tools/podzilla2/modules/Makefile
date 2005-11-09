SUBDIRS = mymodule pong wumpus calc calendar clocks cube dialer invaders ipobble \
          lights mandelpod matrix minesweeper othello poddraw tictactoe tunnel

all:
	@set -e; for dir in $(SUBDIRS); do \
		cd $$dir; \
		make -sf ../../module.mk; \
		cd ..; \
	done
	@if find * -name built-in.o | grep '/built-in.o'; then \
		rm -f built-in.o; \
		echo " LD      built-in.o"; \
		find . -name built-in.o | xargs ld -r -o built-in.o; \
	fi

xpods:
	@set -e; for dir in $(SUBDIRS); do \
		cd $$dir; \
		echo " XPOD    $$dir"; \
		rm -rf ../../xpods/$$dir; \
		mkdir -p ../../xpods/$$dir; \
		make -sf ../../module.mk distfiles | fgrep -v 'make[' | xargs -iX cp X ../../xpods/$$dir; \
		cd ..; \
	done

clean:
	@set -e; for dir in $(SUBDIRS); do \
		cd $$dir; \
		echo " CLEAN   $$dir"; \
		make -sf ../../module.mk clean; \
		cd ..; \
	done

