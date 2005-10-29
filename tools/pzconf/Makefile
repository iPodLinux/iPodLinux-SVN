pzconf: pzconf.o settings.o
	cc -o pzconf pzconf.o settings.o
pzconf.o: pzconf.c
settings.o: settings.c settings.h
clean:
	rm -f pzconf *.o
.PHONY: clean
