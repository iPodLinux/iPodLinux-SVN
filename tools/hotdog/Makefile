
all: hotdog

hotdog: main_darwin.o hotdog.o hotdog_png.o hotdog_primitive.o
	gcc -g -o $@ $^ -lSDL -lSDLmain -framework Foundation -framework Cocoa -L/opt/local/lib -lpng

clean:
	rm -f *.o hotdog

%.o:%.c
	gcc -g -c $^ -o $@ -I/opt/local/include