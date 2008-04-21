include ../config.mk

DEMO = prim-demo

all: $(DEMO)

$(DEMO): main_prim.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) main_prim.o $(DEMO) $(DEMO).gdb $(DEMO).elf $(DEMO).elf2flt
