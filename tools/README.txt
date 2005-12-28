
Hi there, and welcome to the ipodlinux SVN source tree.  In this
document, I hope to explain a little bit about what everything in
here is, and what order it should be built, should you choose to
do so.

This document is meant to be a brief overview of what each of the
subdirectories contain, and not a detailed description of how to
build them.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
NOTE: This file is incomplete, and might be somewhat inaccurate,
	or possibly misleading until it is complete.  
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

Please refer to http://ipodlinux.org/Building_Podzilla for build
instructions.

May the source be with you.

================================================================================

hotdog
	Think of it as a 2D accelerator.  It lets you do all sorts of
	pretty 2D effects with images and such.  If you need to do fast
	2D image manipulations and such, this is the place to go.
	REQUIRES: libsdl, libpng, freetype2

ipodloader2
	The new version of the bootloader.  This one adds new features
	like a better menu-selection for boot kernels, and other 
	niceties.

libipod
	A single library to be used by various subprojects.  It globs
	all of the iPod interactions into one core location.  (LCD,
	hardware info, disk stuff, etc.)

podfile
	A tool for manipulating "pods".  These are the modules that we 
	use for podzilla2, since uclinux doesn't support dynamic libs.
	** build this second for podzilla2 **

podzilla2
	This is the rewrite of the old 'podzilla' proof-of-concept
	application.  This version uses ttk/sdl instead of microwindows,
	and allows for subapps to be modular, rather than being
	monolithic.
	REQUIRES: libsdl, ttk, podfile, ucdl
	** build this last for podzilla2 **

pzconf
	A small utility for dealing with the podzilla configuration
	file.

ttk
	A GUI engine.  It basically wraps libsdl, and allows you
	to have a nice interface for drawing to the screen, rendering
	fonts and images, getting input events, dealing with settings,
	timers, etc.  
	REQUIRES: libsdl
	** build this first for podzilla2 **

ucdl
	Our module library.  This is what lets us have modules in an
	environment that does not support dynamic libraries.  Read the
	README in that directory for more information about how to
	use it.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

iplsign
	generates a crypto key
	(test code)

ipodinfo
	outputs iPod revision and filesystem type
	(test code)

ttkzilla
	A proof of concept version of podzilla, used to see how the TTK
	engine would integrate with podzilla.  This has been abandoned
	in favor of 'podzilla2'.
	(test code)

