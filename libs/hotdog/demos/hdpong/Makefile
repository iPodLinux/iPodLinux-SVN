include ../config.mk

APP = hd-pong
OBJS = hdpong.o

all: $(APP)

$(APP): $(OBJS) 
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	$(RM) $(OBJS) $(APP).elf $(APP).elf2flt $(APP).gdb $(APP)
