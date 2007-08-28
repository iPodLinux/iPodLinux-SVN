/*
 * arch/v850/kernel/sim.c -- Machine-specific stuff for GDB v850e simulator
 *
 *  Copyright (C) 2001,02  NEC Corporation
 *  Copyright (C) 2001,02  Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Written by Miles Bader <miles@gnu.org>
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/bootmem.h>
#include <linux/irq.h>

#include <asm/atomic.h>
#include <asm/page.h>
#include <asm/machdep.h>
#include <asm/simsyscall.h>

#include "mach.h"

/* The name of a file containing the root filesystem.  */
#define ROOT_FS "rootfs.image"

extern void simcons_setup (void);
extern void simcons_poll_ttys (void);

/* These are where `blkmem' device 0 is allocated.  We put the root
   filesystem there.  */
unsigned long root_fs_image_addr, root_fs_image_len;
/* Make the root filesystem R/W.  */
int root_fs_image_rw = 1;

static int read_file (const char *name,
		      unsigned long *addr, unsigned long *len,
		      const char **err);

void __init mach_setup (char **cmdline)
{
	const char *err;

	simcons_setup ();

	printk (KERN_INFO "Reading root filesystem: %s", ROOT_FS);
	if (read_file (ROOT_FS, &root_fs_image_addr, &root_fs_image_len, &err)) {
		printk (" (size %luK)\n", root_fs_image_len / 1024);
		ROOT_DEV = MKDEV (BLKMEM_MAJOR, 0);
	} else {
		static char bogus_blkmem_array[1];

		printk ("...%s failed!\n", err);

		/* blkmem is going to attempt to read the device even if
		   we don't specify it as the root, so make sure it
		   points to something innocuous.  */
		root_fs_image_addr = (unsigned long)bogus_blkmem_array;
		root_fs_image_len = 0;
	}
}

void mach_get_physical_ram (unsigned long *ram_start, unsigned long *ram_len)
{
	*ram_start = RAM_ADDR;
	*ram_len = RAM_SIZE;
}

void __init mach_sched_init (struct irqaction *timer_action)
{
	/* ...do magic timer initialization?...  */
	mach_tick = simcons_poll_ttys;
	setup_irq (0, timer_action);
}


static void irq_nop (unsigned irq) { }
static unsigned irq_zero (unsigned irq) { return 0; }

static struct hw_interrupt_type sim_irq_type = {
	"IRQ",
	irq_zero,		/* startup */
	irq_nop,		/* shutdown */
	irq_nop,		/* enable */
	irq_nop,		/* disable */
	irq_nop,		/* ack */
	irq_nop,		/* end */
};

void __init mach_init_irqs (void)
{
	init_irq_handlers (0, NUM_MACH_IRQS, 1, &sim_irq_type);
}


void mach_gettimeofday (struct timeval *tv)
{
	long timeval[2], timezone[2];
	int rval = V850_SIM_SYSCALL (gettimeofday, timeval, timezone);
	if (rval == 0) {
		tv->tv_sec = timeval[0];
		tv->tv_usec = timeval[1];
	}
}

void machine_restart (char *__unused)
{
	V850_SIM_SYSCALL (write, 1, "RESTART\n", 8);
	V850_SIM_SYSCALL (exit, 0);
}

void machine_halt (void)
{
	V850_SIM_SYSCALL (write, 1, "HALT\n", 5);
	V850_SIM_SYSCALL (exit, 0);
}

void machine_power_off (void)
{
	V850_SIM_SYSCALL (write, 1, "POWER OFF\n", 10);
	V850_SIM_SYSCALL (exit, 0);
}


/* Load data from a file called NAME into ram.  The address and length
   of the data image are returned in ADDR and LEN.  */
static int __init
read_file (const char *name,
	   unsigned long *addr, unsigned long *len,
	   const char **err)
{
	int rval, fd;
	unsigned long cur, left;
	/* Note this is not a normal stat buffer, it's an ad-hoc
	   structure defined by the simulator.  */
	unsigned long stat_buf[10];

	/* Stat the file to find out the length.  */
	rval = V850_SIM_SYSCALL (stat, name, stat_buf);
	if (rval < 0) {
		if (err) *err = "stat";
		return 0;
	}
	*len = stat_buf[4];

	/* Open the file; `0' is O_RDONLY.  */
	fd = V850_SIM_SYSCALL (open, name, 0);
	if (fd < 0) {
		if (err) *err = "open";
		return 0;
	}

	*addr = (unsigned long)alloc_bootmem(*len);
	if (! *addr) {
		V850_SIM_SYSCALL (close, fd);
		if (err) *err = "alloc_bootmem";
		return 0;
	}

	cur = *addr;
	left = *len;
	while (left > 0) {
		int chunk = V850_SIM_SYSCALL (read, fd, cur, left);
		if (chunk <= 0)
			break;
		cur += chunk;
		left -= chunk;
	}
	V850_SIM_SYSCALL (close, fd);
	if (left > 0) {
		/* Some read failed.  */
		free_bootmem (*addr, *len);
		if (err) *err = "read";
		return 0;
	}

	return 1;
}
