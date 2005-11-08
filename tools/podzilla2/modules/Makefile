SUBDIRS = mymodule pong

all:
	for dir in $(SUBDIRS); do \
		cd $$dir; \
		make -f ../../module.mk; \
		cd ..; \
	done
	if find * -name built-in.o | grep '/built-in.o'; then \
		rm -f built-in.o; \
		find . -name built-in.o | xargs ld -r -o built-in.o; \
	fi

xpods:
	for dir in $(SUBDIRS); do \
		cd $$dir; \
		rm -rf ../../xpods/$$dir; \
		mkdir -p ../../xpods/$$dir; \
		make -sf ../../module.mk distfiles | fgrep -v 'make[' | xargs -iX cp X ../../xpods/$$dir; \
		cd ..; \
	done

clean:
	for dir in $(SUBDIRS); do \
		cd $$dir; \
		make -f ../../module.mk clean; \
		cd ..; \
	done

