TARGET = nimesweeper
PACKAGE = Nimesweeper-1.0
CFLAGS = -Wall -Wpointer-arith -Wstrict-prototypes -W -pedantic -ansi -O2 -fomit-frame-pointer
LIBS = -lncurses
OBJS = main.o gameplay.o init.o scores.o windows.o misc.o
bindir = /usr/bin
# Also change HIGHSCORES in mine.h if you alter ScoreDir or ScoreFile
ScoreDir  = /usr/share
ScoreFile = HighScore.dat
CC=cc

all: $(TARGET)
$(TARGET): $(OBJS)
	@rm -f $(TARGET)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)
	strip --strip-all $(TARGET)
	@echo
	@echo "*** `du -h $(TARGET)` made OK. ***"
	@echo

main.o: main.c
	@rm -f main.o
	$(CC) $(CFLAGS) -c main.c 

gameplay.o: gameplay.c
	@rm -f gameplay.o
	$(CC) $(CFLAGS) -c gameplay.c 

init.o: init.c
	@rm -f init.o
	$(CC) $(CFLAGS) -c init.c

misc.o: misc.c
	@rm -f misc.o
	$(CC) $(CFLAGS) -c misc.c

scores.o: scores.c
	@rm -f scores.o
	$(CC) $(CFLAGS) -c scores.c

windows.o: windows.c
	@rm -f windows.o
	$(CC) $(CFLAGS) -c windows.c

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)
	rm -f check

check:
	$(CC) $(CFLAGS) -o check check.c
	strip --strip-all check
	@echo
	@echo "*** `du -h check` made OK ***"
	@echo

tar:
	@echo "*** Tidying source directory up... ***"
	make clean
	rm -f ../$(PACKAGE).tar
	rm -f ../$(PACKAGE).tar.bz2
	rm -f ../$(PACKAGE).tar.gz
	rm -f 00MD5SUMS
	@echo
	@echo "*** Making checksums... ***"
	md5sum * > 00MD5SUMS
	@echo
	@echo "*** Making temp archive... ***"
	ls > file_list.tmp
	mkdir $(PACKAGE)
	cp `cat file_list.tmp | grep -v file_list.tmp` $(PACKAGE)
	@echo
	@echo "*** Making packages... ***"
	tar -cf $(PACKAGE).tar $(PACKAGE)
	bzip2 -k -9 $(PACKAGE).tar
	gzip -9 $(PACKAGE).tar
	@echo
	@echo "*** Testing integrity of tarballs... ***"
	bzip2 -t $(PACKAGE).tar.bz2
	gzip -t  $(PACKAGE).tar.gz
	@echo
	@echo "*** Cleaning up... ***"
	mv $(PACKAGE).tar.bz2 ..
	mv $(PACKAGE).tar.gz ..
	rm -rf $(PACKAGE)
	rm -f $(PACKAGE).tar
	rm -f file_list.tmp
	@echo
	@echo "*** `du -h ../$(PACKAGE).tar.bz2` ***"
	@echo "*** `du -h ../$(PACKAGE).tar.gz`  ***"
	@echo "*** Done OK ***"
info:
	@echo "Info for $(TARGET) source"
	@echo
	@echo "Include Files:"
	@echo "`grep include *.[ch]`"
	@echo
	@echo "Lines of code:"
	@echo "`wc -l *.[ch]`"
	@echo
	@echo "Kilobytes of source code:"
	@echo "`du -ksc *.[ch]`"
	@echo

install:
	cp -i $(TARGET) $(bindir)/$(TARGET)
	mkdir -m 755 $(ScoreDir)/$(TARGET)
	./$(TARGET) --new
	@echo
	@echo "*** $(TARGET) installed ok ***"
	@echo "*** Check permissions of $(bindir)/$(TARGET) ***"
	@echo "*** and $(ScoreDir)/$(TARGET)/$(ScoreFile) ***"
	@echo

uninstall:
	rm -i $(bindir)/$(TARGET)
	rm -ir $(ScoreDir)/$(TARGET)
	@echo
	@echo "*** $(TARGET) uninstalled ok ***"
	@echo
