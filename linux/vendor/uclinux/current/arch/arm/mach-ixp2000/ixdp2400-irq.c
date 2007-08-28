/*
 * arch/arm/mach-ixp2000/ixdp2400-irq.c
 *
 * Interrupt code for IXDP2400 board
 *
 * Author: Naeem Afzal <naeem.m.afzal@intel.com>
 * Copyright 2002 Intel Corp.
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
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
#include <asm/mach/irq.h>
#include <asm/mach-types.h>
#include <asm/arch/hardware.h>

extern void do_IRQ(int, struct pt_regs *);
extern int setup_arm_irq(int, struct irqaction *);
extern void  ixp2000_init_irq(void);

/*
 * Slowport configuration for accessing CPLD registers
 */
static struct slowport_cfg slowport_cpld_cfg = {
	CCR:	SLOWPORT_CCR_DIV_1,
	WTC:	0x00000068,
	RTC:	0x00000068,
	PCR:	SLOWPORT_MODE_FLASH,
	ADC:	SLOWPORT_ADDR_WIDTH_24 | SLOWPORT_DATA_WIDTH_8
};

static void ext_irq_mask(unsigned int irq)
{
	acquire_slowport(&slowport_cpld_cfg);
	*(IXP2000_IRQ_ENABLE_CLR) = IRQ_MASK_PCI;
	release_slowport();
}

static void ext_irq_unmask(unsigned int irq)
{
	acquire_slowport(&slowport_cpld_cfg);
	*(IXP2000_IRQ_ENABLE_SET) = IRQ_MASK_PCI;
	release_slowport();
}

static void ext_irq_demux(int irq, void *dev_id, struct pt_regs *regs)
{
        volatile u32 ex_interrupt = 0;
    	int irqno = 0;

	acquire_slowport(&slowport_cpld_cfg);
        ex_interrupt = *(IXDP2400_CPLD_INT);
	release_slowport();

	if (npu_is_master()) {
               if (ex_interrupt & IXDP2400_MASK_INGRESS)
                       irqno=IRQ_IXDP2400_INGRESS_NPU;
               else if (ex_interrupt & IXDP2400_MASK_EGRESS_NIC)
                       irqno = IRQ_IXDP2400_METHERNET;/* Master Ethernet IRQ*/
               else if (ex_interrupt & IXDP2400_MASK_MEDIA_PCI)
                       irqno = IRQ_IXDP2400_MEDIA_PCI;
               else if (ex_interrupt & IXDP2400_MASK_MEDIA_SP)
                       irqno = IRQ_IXDP2400_MEDIA_SP;
               else if (ex_interrupt & IXDP2400_MASK_SF_PCI)
                       irqno = IRQ_IXDP2400_SF_PCI;
               else if (ex_interrupt & IXDP2400_MASK_SF_SP)
                       irqno = IRQ_IXDP2400_SF_SP;
               else if (ex_interrupt & IXDP2400_MASK_PMC)
                       irqno = IRQ_IXDP2400_PMC;
               else if (ex_interrupt & IXDP2400_MASK_TVM)
                       irqno = IRQ_IXDP2400_TVM;
 	} else {   /* Slave_NPU */
                       /* INTB is connected to NIC only, INTA is output*/
                	irqno = IRQ_IXDP2400_SETHERNET;/* slave Ethernet IRQ*/
	}

   	if((irqno< 0) || (irq > NR_IRQS))
   		printk(KERN_ERR "Error: Spurious IXDP2400 interrupt\n");

   	do_IRQ(irqno, regs);
}

static struct irqaction ext_irq = {
	name:	"IXDP2400 CPLD",
	handler: ext_irq_demux,
	flags: SA_INTERRUPT
};

void __init ixdp2400_init_irq(void)
{
	int i = 0;

	/* enable PCI interrupts */
	*IXP2000_PCI_XSCALE_INT_ENABLE |= (1<<27) | (1<<26);

	acquire_slowport(&slowport_cpld_cfg);
        *(IXDP2400_CPLD_INT_MASK) = 0x0; /* enable all INT on CPLD reg */
	release_slowport();

	/* initialize chip specific interrupts */
	ixp2000_init_irq();

	for(i = NR_IXP2000_IRQS; i <IRQ_IXP2000_INTERRUPT; i++) {
		irq_desc[i].valid 	= 1;
		irq_desc[i].probe_ok	= 0;
		irq_desc[i].mask_ack	= ext_irq_mask;
		irq_desc[i].mask	= ext_irq_mask;
		irq_desc[i].unmask	= ext_irq_unmask;
	}

	/* init PCI interrupts */
	setup_arm_irq(IRQ_IXP2000_PCI, &ext_irq);
}
