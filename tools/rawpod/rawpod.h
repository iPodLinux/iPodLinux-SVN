/*
 * The Rawpod raw device access library.   Copyright (c) 2006 Joshua Oreman.
 * Released under the GPL - see file COPYING.           All rights reserved.
 * rawpod.h - main header file for the library.    Last modified 2006-07-30.
 */

#ifndef _RAWPOD_H_
#define _RAWPOD_H_

#include "vfs.h"
#include "device.h"
#include "ext2.h"
#include "fat32.h"
#include "partition.h"

#define RAWPOD_OPTIONS_STR "Ccw:i:I:s:"
#ifdef WIN32
#define RAWPOD_OPTIONS_USAGE_CACHE \
"       -C  Do not use a block cache. Expect MASSIVE SLOWNESS, but no problems\n" \
"           in case of a hastily unplugged iPod.\n" \
"       -c  Use a block cache. [default]\n"
#else
#define RAWPOD_OPTIONS_USAGE_CACHE \
"       -C  Do not use an additional block cache. [default]\n" \
"       -c  Use a block cache. On Linux, this is not really necessary, since the\n" \
"           kernel does a much better job at caching than we ever could.\n"
#endif
#define RAWPOD_OPTIONS_USAGE \
" Rawpod options:\n" \
RAWPOD_OPTIONS_USAGE_CACHE \
"  -i IPOD  Look for the iPod at IPOD instead of probing for it.\n" \
"   -I NUM  Allow writes to rest in cache no longer than NUM seconds before\n" \
"           being flushed to disk (the `commit interval'). [default 5]\n" \
"   -w COW  Perform all writes to the file COW instead of to the iPod. Useful\n" \
"           for testing. COW must exist, but it can be empty.\n" \
"   -s NUM  Set the cache size to NUM sectors. This will use NUM*515 bytes of\n" \
"           memory. [default 16384]\n\n"

/* Returns 1 if the option was handled, 0 if not. */
int rawpod_parse_option (char optopt, const char *optarg);

#endif
