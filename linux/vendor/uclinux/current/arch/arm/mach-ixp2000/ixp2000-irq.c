/*
 * arch/arm/mach-ixp2000/ixp2000-irq.c
 *
 * Interrupt code for IXDP2400 board
 *
 * Original Author: Naeem Afzal <naeem.m.afzal@intel.com>
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright (c) 2002 Intel Corp.
 * Copyright (c) 2003 MontaVista Software, Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>

#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/bitops.h>

#include <asm/mach/irq.h>

extern void do_IRQ(int, struct pt_regs *);
extern int setup_arm_irq(int, struct irqaction *);
extern void ixmb2400_init_irq(void);

static void ixp_irq_mask(unsigned int irq)
{
	*(IXP2000_IRQ_ENABLE_CLR) = (1 << irq);
}

static void ixp_irq_unmask(unsigned int irq)
{
	*(IXP2000_IRQ_ENABLE_SET) = (1 << irq);
}

void __init ixp2000_init_irq(void)
{
	int i = 0;

	/*
	 * Mask all sources
	 */
	*(IXP2000_IRQ_ENABLE_CLR) = 0xffffffff;
	*(IXP2000_FIQ_ENABLE_CLR) = 0xffffffff;

	for(i = 0; i < NR_IXP2000_IRQS-1; i++) {
			irq_desc[i].valid 	= 1;
			irq_desc[i].probe_ok	= 0;
			irq_desc[i].mask_ack	= ixp_irq_mask;
			irq_desc[i].mask	= ixp_irq_mask;
			irq_desc[i].unmask	= ixp_irq_unmask;
	}

}
