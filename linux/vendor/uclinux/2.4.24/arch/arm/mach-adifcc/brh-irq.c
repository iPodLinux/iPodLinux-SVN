/*
 * arch/arm/mach-adifcc/brh.h
 *
 * Interrupt code for ADI BRH board
 *
 * Author: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright 2001 MontaVista Software Inc.
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

extern void xs80200_init_irq(void);

static void brh_irq_mask(unsigned int irq)
{
	long intmask = *(BRH_INT_MASK);
	
	intmask |= (1 << (irq - NR_XS80200_IRQS));

	*(BRH_INT_MASK) = intmask;
}

static void brh_irq_unmask(unsigned int irq)
{
	long intmask = *(BRH_INT_MASK);

	intmask &= ~(1 << (irq - NR_XS80200_IRQS));

	*(BRH_INT_MASK) = intmask;
}

extern void do_IRQ(int, struct pt_regs *);

static void brh_irq_demux(int irq, void *dev_id, struct pt_regs *regs)
{
	u32 irq_stat = *(BRH_INT_STAT);
	u32 irq_mask = *(BRH_INT_MASK);
	int irqno = 0;

	irq_stat &= ~irq_mask;

	if(irq_stat & 0x00000001)
		irqno = IRQ_BRH_SWI;
	else if(irq_stat & 0x00000002)
		irqno = IRQ_BRH_TIMERA;
	else if(irq_stat & 0x00000004)
		irqno = IRQ_BRH_TIMERB;
	else if(irq_stat & 0x00000008)
		irqno = IRQ_BRH_ERROR;
	else if(irq_stat & 0x00000010)
		irqno = IRQ_BRH_DMA_EOT;
	else if(irq_stat & 0x00000020)
		irqno = IRQ_BRH_DMA_PARITY;
	else if(irq_stat & 0x00000040)
		irqno = IRQ_BRH_DMA_TABORT;
	else if(irq_stat & 0x00000080)
		irqno = IRQ_BRH_DMA_MABORT;
	else if(irq_stat & 0x00010000)
		irqno = IRQ_BRH_PCI_PERR;
	else if(irq_stat & 0x00080000)
		irqno = IRQ_BRH_PCI_SERR;
	else if(irq_stat & 0x00100000)
		irqno = IRQ_BRH_ATU_PERR;
	else if(irq_stat & 0x00200000)
		irqno = IRQ_BRH_ATU_TABORT;
	else if(irq_stat & 0x00400000)
		irqno = IRQ_BRH_ATU_MABORT;
	else if(irq_stat & 0x01000000)
		irqno = IRQ_BRH_UART_A;
	else if(irq_stat & 0x02000000)
		irqno = IRQ_BRH_UART_B;
	else if(irq_stat & 0x04000000)
		irqno = IRQ_BRH_PCI_INT_A;
	else if(irq_stat & 0x08000000)
		irqno = IRQ_BRH_PCI_INT_B;
	else if(irq_stat & 0x10000000)
		irqno = IRQ_BRH_PCI_INT_C;
	else if(irq_stat & 0x20000000)
		irqno = IRQ_BRH_PCI_INT_D;
	else if(irq_stat & 0x40000000)
		irqno = IRQ_BRH_SOFT_RESET;

	if(!irqno)
		printk(KERN_ERR "Error: Spurious BRH interrupt\n");

//	printk("brh_irq_demux called: %d\n", irqno);
	do_IRQ(irqno, regs);
}

static struct irqaction brh_irq = {
	name:	"BRH IRQ",
	handler: brh_irq_demux,
	flags: SA_INTERRUPT
};

extern int setup_arm_irq(int, struct irqaction *);

void __init brh_init_irq(void)
{
	int i = 0;

	/*
	 * Route all sources to IRQ instead of FIQ
	 * Mask all sources
	 */
	*(BRH_INT_STEERING) = 0;
	*(BRH_INT_MASK) = 0xffffffff;

	for(i = NR_XS80200_IRQS; i < NR_IRQS; i++)
	{
		irq_desc[i].valid 	= 1;
		irq_desc[i].probe_ok	= 0;
		irq_desc[i].mask_ack	= brh_irq_mask;
		irq_desc[i].mask	= brh_irq_mask;
		irq_desc[i].unmask	= brh_irq_unmask;
	}

	xs80200_init_irq();

	setup_arm_irq(IRQ_XS80200_EXTIRQ, &brh_irq);
}
