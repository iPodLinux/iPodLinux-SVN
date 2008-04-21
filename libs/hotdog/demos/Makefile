DEMOS= $(dir $(wildcard */Makefile))

all:
	@set -e; for i in $(DEMOS); do make -C $$i; done

clean:
	@set -e; for i in $(DEMOS); do make clean -C $$i; done
