#!/bin/sh

PATH=$PATH:/usr/X11R6/bin

builddirs="ipod-sdl x11-sdl ipod-mwin x11-mwin"

if test x$MBD = x; then
    echo "*** Don't call me directly; I should be called"
    echo "*** when you type \`make'."
    exit 1
fi


cc -o lndir lndir.c

mkdir -p build
cd build

for dir in $builddirs; do
	mkdir -p $dir
	cd $dir
	../../lndir ../../src
	cd ..
done

cd ..
rm lndir
