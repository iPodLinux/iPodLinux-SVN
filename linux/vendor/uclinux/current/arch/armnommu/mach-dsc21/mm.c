#ifdef CONFIG_REVISIT
#error Move this to mach-dsc21?
#endif
/*
 *  linux/arch/arm/mm/mm-dsc21.c
 *
 *  Copyright (C) 1998-1999 Russell King
 *  Copyright (C) 1999 Hugo Fiennes
 *  Copyright (C) 2001 RidgeRun, Inc
 *
 *  Extra MM routines for the SA1100 architecture
 *
 *  1999/12/04 Nicolas Pitre <nico@cam.org>
 *	Converted memory definition for struct meminfo initialisations.
 *	Memory is listed physically now.
 *
 *  2000/04/07 Nicolas Pitre <nico@cam.org>
 *	Reworked for run-time selection of memory definitions
 *
 *  2001/02/19 Gordon McNutt <gmcnutt@ridgerun.com>
 *      Modified mm-sa1100.c to make this, mm-dsc21.c.
 *
 */
#include <linux/config.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/bootmem.h>

#include <asm/hardware.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/mach-types.h>

#include <asm/mach/map.h>
 
#ifdef CONFIG_DISCONTIGMEM

/*
 * Our node_data structure for discontigous memory.
 * There is 4 possible nodes i.e. the 4 SA1100 RAM banks.
 */

static bootmem_data_t node_bootmem_data[4];

pg_data_t dsc21_node_data[4] =
{ { bdata: &node_bootmem_data[0] },
  { bdata: &node_bootmem_data[1] },
  { bdata: &node_bootmem_data[2] },
  { bdata: &node_bootmem_data[3] } };

#endif
