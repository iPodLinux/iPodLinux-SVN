/*
 * irq.c - irq processing for iPod
 *
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sysdev.h>
                                                                                
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
                                                                                
#include <asm/mach/irq.h>

static void ipod_unmask_irq(unsigned int irq)
{
	/* unmask the interrupt */
	outl((1 << irq), 0xcf001024);
	/* route it to IRQ not FIQ */
	outl(inl(0xcf00102c) & ~(1 << irq), 0xcf00102c);
}

static void ipod_mask_irq(unsigned int irq)
{
	/* mask the interrupt */
	outl((1 << irq), 0xcf001028);
}

static void ipod_ack_irq(unsigned int irq)
{

	/* there is no general IRQ ack, we have to do it at the source */
	switch (irq) {
	case IDE_INT0_IRQ:
		/* clear FIFO interrupt status */
		outl(0xff, 0xc0003020);
		outl(inl(0xc0003024) | (1<<4) | (1<<5), 0xc0003024);
		break;

	case TIMER1_IRQ:
		inl(0xcf001104);
		break;
	}

	ipod_mask_irq(irq);
}

static struct irqchip ipod_chip = {
	.ack    = ipod_ack_irq,
	.mask   = ipod_mask_irq,
	.unmask = ipod_unmask_irq,
};

void __init ipod_init_irq(void)
{
	unsigned int irq;

	/* disable all interrupts */
	outl(-1, 0xcf00101c);
	outl(-1, 0xcf001028);
	outl(-1, 0xcf001038);

	/* clear all interrupts */
	for ( irq = 0; irq < NR_IRQS; irq++ ) {

		if (!VALID_IRQ(irq)) continue;

                set_irq_chip(irq, &ipod_chip);
                set_irq_handler(irq, do_level_IRQ);
                set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}

	/*
	 * since the interrupt vectors at 0x0 are now installed
	 * we can wake up the COP safely
	 */
	outl(0xce, 0xcf004058);
}

