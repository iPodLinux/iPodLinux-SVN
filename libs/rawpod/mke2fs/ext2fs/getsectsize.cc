/*
 * getsectsize.c --- get the sector size of a device.
 * 
 * Copyright (C) 1995, 1995 Theodore Ts'o.
 * Copyright (C) 2003 VMware, Inc.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <stdio.h>

#include "ext2.h"
#include "ext2fs.h"

/*
 * Returns the number of blocks in a partition
 */
errcode_t ext2fs_get_device_sectsize(const char *file, int *sectsize)
{
    *sectsize = 512;
    return 0;
}
