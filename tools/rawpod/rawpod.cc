/*
 * The Rawpod raw device access library.   Copyright (c) 2006 Joshua Oreman.
 * Released under the GPL - see file COPYING.           All rights reserved.
 * rawpod.cc - overall library functions.          Last modified 2006-07-30.
 */

#include "rawpod.h"
#include "device.h"

int rawpod_parse_option (char optopt, const char *optarg) 
{
    switch (optopt) {
    case 'w':
        LocalRawDevice::setCOWFile (strdup (optarg));
        break;
    case 'i':
        LocalRawDevice::setOverride (strdup (optarg));
        break;
    case 'I':
        BlockCache::setCommitInterval (atoi (optarg));
        break;
    case 's':
        LocalRawDevice::setCachedSectors (atoi (optarg));
        break;
    case 'c':
        BlockCache::enable();
        break;
    case 'C':
        BlockCache::disable();
        break;
    default:
        return 0;
    }
    return 1;
}
