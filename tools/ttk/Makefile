PREFIX ?= /usr

export MBD = yes

all: build-dirs
	@echo ">>> Building TTK..."
ifndef NOHDOG
ifndef NOIPOD
	make -C build/ipod-hdog IPOD=1 HDOG=1 all
endif
ifndef NOX11
	make -C build/x11-hdog HDOG=1 all
endif
endif
ifndef NOSDL
ifndef NOIPOD
	make -C build/ipod-sdl IPOD=1 SDL=1 all
endif
ifndef NOX11
	make -C build/x11-sdl SDL=1 all
endif
endif
ifdef DOMWIN
ifndef NOIPOD
	make -C build/ipod-mwin IPOD=1 MWIN=1 all
endif
ifndef NOX11
	make -C build/x11-mwin MWIN=1 all
endif
endif
	@echo "<<< Done."
	@echo

examples:
	@echo ">>> Compiling examples..."
ifndef NOHDOG
ifndef NOIPOD
	make -C build/ipod-hdog IPOD=1 HDOG=1 examples
endif
ifndef NOX11
	make -C build/x11-hdog HDOG=1 examples
endif
endif
ifndef NOSDL
ifndef NOIPOD
	make -C build/ipod-sdl IPOD=1 SDL=1 examples
endif
ifndef NOX11
	make -C build/x11-sdl SDL=1 examples
endif
endif
ifdef DOMWIN
ifndef NOIPOD
	make -C build/ipod-mwin IPOD=1 MWIN=1 examples
endif
ifndef NOX11
	make -C build/x11-mwin MWIN=1 examples
endif
endif
	@echo "<<< Done."
	@echo

build-dirs:
	@if ! test -d build; then \
	echo ">>> Making build directories..."; \
	./make-build-dirs.sh; \
	ln -s ../../fonts ../../schemes build/x11-mwin/; \
	ln -s ../../fonts ../../schemes build/x11-sdl/; \
	ln -s ../../fonts ../../schemes build/x11-hdog/; \
	ln -s ../libs ../mwincludes ../sdlincludes build/; \
	echo "<<< Done."; \
	echo; \
	fi

install: all
	@echo ">>> Installing TTK..."
	cp ttk-config.in ttk-config.tmp
ifndef NOHDOG
ifndef NOIPOD
	make -C build/ipod-hdog IPOD=1 HDOG=1 install
endif
ifndef NOX11
	make -C build/x11-hdog HDOG=1 install
endif
endif
ifndef NOSDL
ifndef NOIPOD
	make -C build/ipod-sdl IPOD=1 SDL=1 install
endif
ifndef NOX11
	make -C build/x11-sdl SDL=1 install
endif
endif
ifdef DOMWIN
ifndef NOIPOD
	make -C build/ipod-mwin IPOD=1 MWIN=1 install
endif
ifndef NOX11
	make -C build/x11-mwin MWIN=1 install
endif
endif
	rm -rf $(PREFIX)/include/ttk
	install -d $(PREFIX)/include/ttk
	install -d $(PREFIX)/include/ttk/ttk
	install -m 644 src/include/*.h $(PREFIX)/include/ttk/ttk/
	install -m 644 src/ttk.h $(PREFIX)/include/ttk/
	-install -m 644 ../hotdog/hotdog.h $(PREFIX)/include/ttk/
	sed 's:@PREFIX@:$(PREFIX):g' < ttk-config.tmp > ttk-config
	install -m 755 ttk-config $(PREFIX)/bin/
	cp -pR mwincludes sdlincludes $(PREFIX)/include/ttk/
	rm -f ttk-config.tmp
	@echo "<<< Done."
	@echo

clean:
	@echo ">>> Cleaning..."
ifndef NOHDOG
	make -C build/ipod-hdog IPOD=1 HDOG=1 clean
	make -C build/x11-hdog HDOG=1 clean
endif
ifndef NOSDL
	make -C build/ipod-sdl IPOD=1 SDL=1 clean
	make -C build/x11-sdl SDL=1 clean
endif
ifdef DOMWIN
	make -C build/ipod-mwin IPOD=1 MWIN=1 clean
	make -C build/x11-mwin MWIN=1 clean
endif
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

.PHONY: all build-dirs examples install docs dist
