/*
 * inline.c --- Includes the inlined functions defined in the header
 * 	files as standalone functions, in case the application program
 * 	is compiled with inlining turned off.
 * 
 * Copyright (C) 1993, 1994 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */


#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ext2.h"
#define INCLUDE_INLINE_FUNCS
#include "ext2fs.h"

