include ../config.mk

DEMO = anim-demo

all: $(DEMO)

$(DEMO): main_anim.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) main_anim.o $(DEMO) $(DEMO).gdb $(DEMO).elf $(DEMO).elf2flt
