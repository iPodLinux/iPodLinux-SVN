include ../config.mk

DEMO = prim-demo

all: $(DEMO)

FONT = Aiken14.png 6x13.fff

$(FONT):
	svn export https://ipodlinux.svn.sourceforge.net/svnroot/ipodlinux/libs/ttk/fonts/$@
	

$(DEMO): main_prim.o $(FONT)
	$(CC) -o $@ $< $(LDFLAGS)

clean:
	$(RM) main_prim.o $(DEMO) $(DEMO).gdb $(DEMO).elf $(DEMO).elf2flt
