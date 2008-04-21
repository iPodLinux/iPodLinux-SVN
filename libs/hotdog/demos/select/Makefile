include ../config.mk

DEMO = select-demo

all: $(DEMO)

$(DEMO): main_select.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) main_select.o $(DEMO) $(DEMO).gdb $(DEMO).elf $(DEMO).elf2flt
