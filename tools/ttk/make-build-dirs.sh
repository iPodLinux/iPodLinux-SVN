#!/bin/sh

PATH=$PATH:/usr/X11R6/bin

builddirs="ipod-sdl x11-sdl ipod-mwin x11-mwin"

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
