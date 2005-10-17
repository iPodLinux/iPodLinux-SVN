PREFIX ?= /usr

all: build/ipod-sdl
	@echo ">>> Building TTK..."
	make -C build/ipod-sdl IPOD=1 SDL=1 all
	make -C build/x11-sdl SDL=1 all
	make -C build/ipod-mwin IPOD=1 MWIN=1 all
	make -C build/x11-mwin MWIN=1 all
	@echo "<<< Done."
	@echo

examples:
	@echo ">>> Compiling examples..."
	make -C build/ipod-sdl IPOD=1 SDL=1 examples
	make -C build/x11-sdl SDL=1 examples
	make -C build/ipod-mwin IPOD=1 MWIN=1 examples
	make -C build/x11-mwin MWIN=1 examples
	@echo "<<< Done."
	@echo

build/ipod-sdl:
	@echo ">>> Making build directories..."
	./make-build-dirs.sh
	ln -s ../../fonts ../../schemes build/x11-mwin/
	ln -s ../../fonts ../../schemes build/x11-sdl/
	ln -s ../libs ../mwincludes build/
	@echo "<<< Done."
	@echo

install: all
	@echo ">>> Installing TTK..."
	cp ttk-config.in ttk-config.tmp
	make -C build/ipod-sdl IPOD=1 SDL=1 install
	make -C build/x11-sdl SDL=1 install
	make -C build/ipod-mwin IPOD=1 MWIN=1 install
	make -C build/x11-mwin MWIN=1 install
	install -d $(PREFIX)/include/ttk
	install -d $(PREFIX)/include/ttk/ttk
	install -m 644 src/*.h $(PREFIX)/include/ttk/ttk/
	install -m 644 src/all-ttk.h $(PREFIX)/include/ttk/ttk.h
	sed 's:@PREFIX@:$(PREFIX):g' < ttk-config.tmp > ttk-config
	install -m 755 ttk-config $(PREFIX)/bin/
	cp -pR mwincludes $(PREFIX)/include/ttk/
	rm -f ttk-config.tmp
	@echo "<<< Done."
	@echo

clean:
	@echo ">>> Cleaning..."
	make -C build/ipod-sdl IPOD=1 SDL=1 clean
	make -C build/x11-sdl SDL=1 clean
	make -C build/ipod-mwin IPOD=1 MWIN=1 clean
	make -C build/x11-mwin MWIN=1 clean
	@echo "<<< Done."
	@echo

distclean:
	@echo ">>> Cleaning thoroughly..."
	rm -rf build
	rm -f ttk-config
	rm -f API/API.aux API/API.log API/API.out API/API.toc
	@echo "<<< Done."
	@echo

dist:
	@if [ x$(VERSION) == x ]; then echo '***' You need something like: make dist VERSION=0.3; false; fi
	@echo ">>> Making distribution tarball..."
	@rm -f ttk-*.tar.gz
	@cd ..; \
	mkdir ttk-$(VERSION); \
	cp -a ttk/* ttk-$(VERSION); \
	make -C ttk-$(VERSION) distclean >/dev/null 2>&1; \
	find ttk-$(VERSION) -name '.#*' -o -name '#*#' -o -name '*~' | xargs rm -f; \
	find ttk-$(VERSION) -name 'core*' | xargs rm -f; \
	tar -cf- ttk-$(VERSION) | gzip -9 > ttk/ttk-$(VERSION).tar.gz; \
	echo "--- Tarball made."; \
	make -C ttk-$(VERSION) >/dev/null 2>&1 || make -C ttk-$(VERSION) || false; \
	echo "--- Test compilation successful."; \
	rm -rf ttk-$(VERSION)
	@echo "<<< Done.";
	@echo

docs:
	cd API && \
	pdflatex API.tex && \
	pdflatex API.tex && \
	cd ..
