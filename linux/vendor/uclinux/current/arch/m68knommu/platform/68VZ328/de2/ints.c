/*
 * linux/arch/m68knommu/platform/MC68VZ328/de2/ints.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 * Copyright 1996 Roman Zippel
 * Copyright 1999 D. Jeff Dionne <jeff@uClinux.org>
 * Copyright 2002 Georges Menie
 *
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/setup.h>
#include <asm/MC68VZ328.h>

#include "traps_proto.h"

/* irq node variables for the 32 (potential) on chip sources */
#define INTERNAL_IRQS 32
static irq_node_t int_irq_list[INTERNAL_IRQS];
static int int_irq_count[INTERNAL_IRQS];

/* The number of spurious interrupts */
volatile unsigned int num_spurious;

/*
 * This function should be called during kernel startup to initialize
 * the IRQ handling routines.
 */

void __init init_IRQ(void)
{
	int i;

	for (i = 0; i < INTERNAL_IRQS; i++) {
		int_irq_list[i].handler = NULL;
		int_irq_count[i] = 0;
	}

	/* turn off all interrupts */
	IMR = ~0;
}

int request_irq(unsigned int irq, void (*handler)(int, void *, struct pt_regs *),
                unsigned long flags, const char *devname, void *dev_id)
{
	if (irq >= INTERNAL_IRQS) {
		printk ("%s: Unknown IRQ %d from %s\n", __FUNCTION__, irq, devname);
		return -ENXIO;
	}

	if (!(int_irq_list[irq].flags & IRQ_FLG_STD)) {
		if (int_irq_list[irq].flags & IRQ_FLG_LOCK) {
			printk("%s: IRQ %d from %s is not replaceable\n",
			       __FUNCTION__, irq, int_irq_list[irq].devname);
			return -EBUSY;
		}
		if (flags & IRQ_FLG_REPLACE) {
			printk("%s: %s can't replace IRQ %d from %s\n",
			       __FUNCTION__, devname, irq, int_irq_list[irq].devname);
			return -EBUSY;
		}
	}

	int_irq_list[irq].handler = handler;
	int_irq_list[irq].flags   = flags;
	int_irq_list[irq].dev_id  = dev_id;
	int_irq_list[irq].devname = devname;

	IMR &= ~(1<<irq);

	return 0;
}

void free_irq(unsigned int irq, void *dev_id)
{
	if (irq >= INTERNAL_IRQS) {
		printk ("%s: Unknown IRQ %d\n", __FUNCTION__, irq);
		return;
	}

	if (int_irq_list[irq].dev_id != dev_id)
		printk("%s: removing probably wrong IRQ %d from %s\n",
		       __FUNCTION__, irq, int_irq_list[irq].devname);

	int_irq_list[irq].handler = (void (*)(int, void *, struct pt_regs *))bad_interrupt;
	int_irq_list[irq].flags   = IRQ_FLG_STD;
	int_irq_list[irq].dev_id  = NULL;
	int_irq_list[irq].devname = NULL;

	IMR |= 1<<irq;
}

int get_irq_list(char *buf)
{
	int i, len = 0;

	len += sprintf(buf+len, "IRQ       count    dev\n");
	for (i = 0; i < INTERNAL_IRQS; i++) {
		if (int_irq_list[i].handler) {
			len += sprintf(buf+len, " %2d: %10u    %s\n", i,
		               int_irq_count[i], int_irq_list[i].devname);
		}
	}
	len += sprintf(buf+len, "     %10u    spurious\n", num_spurious);
	return len;
}

unsigned long probe_irq_on (void)
{
	return 0;
}

int probe_irq_off (unsigned long irqs)
{
	return 0;
}

/* This code is designed to be fast, almost constant time, not clean!
 */
asmlinkage void process_int(struct frame *fp)
{
	register int irq;
	register int mask;
	register unsigned long pend = ISR;

	while (pend) {
		if (pend & (1<<TMR_IRQ_NUM)) {
			mask = 1<<TMR_IRQ_NUM;
			irq = TMR_IRQ_NUM;
		}
		else {
			if (pend & 0x0000ffff) {
				if (pend & 0x000000ff) {
					if (pend & 0x0000000f) {
						mask = 0x00000001;
						irq = 0;
					} else {
						mask = 0x00000010;
						irq = 4;
					}
				} else {
					if (pend & 0x00000f00) {
						mask = 0x00000100;
						irq = 8;
					} else {
						mask = 0x00001000;
						irq = 12;
					}
				}
			} else {
				if (pend & 0x00ff0000) {
					if (pend & 0x000f0000) {
						mask = 0x00010000;
						irq = 16;
					} else {
						mask = 0x00100000;
						irq = 20;
					}
				} else {
					if (pend & 0x0f000000) {
						mask = 0x01000000;
						irq = 24;
					} else {
						mask = 0x10000000;
						irq = 28;
					}
				}
			}

			while (! (mask & pend)) {
				mask <<= 1;
				irq++;
			}
		}
#if NR_IRQS >= INTERNAL_IRQS
		++kstat.irqs[0][irq];
#endif
		if (int_irq_list[irq].handler) {
			int_irq_list[irq].handler(irq, int_irq_list[irq].dev_id, &fp->ptregs);
			int_irq_count[irq]++;
		} else {
			++num_spurious;
			printk("unregistered interrupt %d!\nTurning it off in the IMR...\n", irq);
			IMR |= mask;
		}
		pend &= ~mask;
	}
}
