/*
 * linux/arch/arm/mach-iop3xx/iop321-irq.c
 *
 * Generic IOP321 IRQ handling functionality
 * 
 * Author: Rory Bolt <rorybolt@pacbell.net>
 * Copyright (C) 2002 Rory Bolt
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Added IOP3XX chipset and IQ80321 board masking code.
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <asm/mach/irq.h>
#include <asm/irq.h>
#include <asm/hardware.h>

#include <asm/mach-types.h>

static u32 iop321_mask = 0;

static void 
iop321_irq_mask (unsigned int irq)
{
	
	iop321_mask &= ~(1 << (irq - IOP321_IRQ_OFS));

	intctl_write(iop321_mask);
}

static void
iop321_irq_unmask (unsigned int irq)
{
	iop321_mask |= (1 << (irq - IOP321_IRQ_OFS));

	intctl_write(iop321_mask);
}

void __init iop321_init_irq(void)
{
	int i;

	intctl_write(0);		// disable all interrupts
	intstr_write(0);		// treat all as IRQ
	if(machine_is_iq80321()) 	// all interrupts are inputs to chip
		*IOP321_PCIIRSR = 0x0f;	

	for(i = IOP321_IRQ_OFS; i < NR_IOP321_IRQS; i++)
	{
		irq_desc[i].valid	= 1;
		irq_desc[i].probe_ok	= 1;
		irq_desc[i].mask_ack	= iop321_irq_mask;
		irq_desc[i].mask	= iop321_irq_mask;
		irq_desc[i].unmask	= iop321_irq_unmask;
	}
}

