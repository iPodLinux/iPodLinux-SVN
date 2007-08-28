/*
 * arch/microblaze/kernel/procfs.c -- Introspection functions for /proc filesystem
 *
 *  Copyright (C) 2003       John Williams <jwilliams@itee.uq.edu.au>
 *
 *  based heavily on v850 code that was
 *
 *  Copyright (C) 2001,2002  NEC Corporation
 *  Copyright (C) 2001,2002  Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Written by Miles Bader <miles@gnu.org>
 */

#include "mach.h"

static int cpuinfo_print (struct seq_file *m, void *v)
{
	extern unsigned long loops_per_jiffy;
	return seq_printf (m,
			   "CPU-Family:	microblaze\n"
			   "CPU-Arch:	%s\n"
			   "CPU-Model:	%s\n"
			   "CPU-MHz:   %lu.%02lu\n"
			   "BogoMips:	%u.%02u\n",
			   CPU_ARCH,
			   CPU_MODEL,
			   CONFIG_CPU_CLOCK_FREQ/1000000,
			   CONFIG_CPU_CLOCK_FREQ % 1000000,
			   loops_per_jiffy/(500000/HZ),
			   (loops_per_jiffy/(5000/HZ)) % 100);
}

static void *cpuinfo_start (struct seq_file *m, loff_t *pos)
{
	return *pos < NR_CPUS ? ((void *) 0x12345678) : NULL;
}

static void *cpuinfo_next (struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return cpuinfo_start (m, pos);
}

static void cpuinfo_stop (struct seq_file *m, void *v)
{
}

struct seq_operations cpuinfo_op = {
	start:	cpuinfo_start,
	next:	cpuinfo_next,
	stop:	cpuinfo_stop,
	show:	cpuinfo_print
};
