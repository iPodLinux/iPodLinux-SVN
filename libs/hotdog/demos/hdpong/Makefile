include ../config.mk

APP = hd-pong
OBJ = hdpong.o

all: $(APP)

FONT = Aiken14.png

$(FONT):
	svn export https://ipodlinux.svn.sourceforge.net/svnroot/ipodlinux/libs/ttk/fonts/$@
	


$(APP): $(OBJ) $(FONT)
	$(CC) $< -o $@ $(LDFLAGS)

clean:
	$(RM) $(OBJ) $(APP).elf $(APP).elf2flt $(APP).gdb $(APP)
