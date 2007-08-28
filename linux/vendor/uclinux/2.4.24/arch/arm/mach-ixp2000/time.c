/*
 * arch/arm/mach-ixp2000//time.c
 *
 * Original Author: Naeem M Afzal <naeem.m.afzal@intel.com>
 *
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
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/timex.h>
#include <asm/hardware.h>

#include <asm/mach-types.h>


static inline unsigned long get_ext_oscillator(void)
{
	int numerator, denominator;
	int denom_array[] = {2, 4, 8, 16, 1, 2, 4, 8};

	if(machine_is_ixdp2400()) {
		numerator = (*(IXDP2400_CPLD_SYS_CLK_M) & 0xFF) *2;
		denominator = denom_array[(*(IXDP2400_CPLD_SYS_CLK_N) & 0x7)];

		return   ((3125000 * numerator) / (denominator));
	}
}

static unsigned long ixp2400_gettimeoffset (void)
{
	unsigned long offset, usec;

	/* Get ticks since last timer service */
	offset= (((get_ext_oscillator()/2) + HZ/2) / HZ) - *IXP2000_T1_CSR;

	usec = (unsigned long)(offset*tick)/LATCH;
	return usec;
}
static void ixp2400_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	/* clear timer 1 */
	*IXP2000_T1_CLR = 1;
	/* Call the kernel's do_timer function so jiffies may be */
	/* incremented and bottom half's scheduled */
	do_timer(regs);
}

static struct irqaction ixp2000_timer_irq = {

	handler:ixp2400_timer_interrupt,
	name:"timer",
//	flags: SA_INTERRUPT
};

extern unsigned long (*gettimeoffset)(void);
extern int setup_arm_irq(int, struct irqaction *);

void setup_timer (void)
{
	gettimeoffset = ixp2400_gettimeoffset;
	*IXP2000_T1_CLR = 0;

	/*
	 * Load timer 1 with latch value which is 
	 *  (tick rate + hz/2)/ hz  
	 *
	 * tick rate is half of the external oscillator in our case
	 */
	*IXP2000_T1_CLD = ((get_ext_oscillator()/2) + HZ/2) / HZ;

	/* Enable Timer 1 in periodic mode */
	*IXP2000_T1_CTL = (1 << 7) ;

	/* register for interrupt */
	setup_arm_irq(IRQ_IXP2000_TIMER1, &ixp2000_timer_irq);
}

