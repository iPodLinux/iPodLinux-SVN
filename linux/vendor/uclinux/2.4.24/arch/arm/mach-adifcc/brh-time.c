/*
 * arch/arm/mach-adifcc/brh-time.c
 *
 * Timer code for BECC onboard timer
 *
 * Author: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef CONFG_XSCALE_PMU_TIMER

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


static unsigned long brh_gettimeoffset(void)
{
	unsigned long elapsed, usec;

	/* Elapsed timer ticks since last IRQ */
	elapsed = LATCH - (*BRH_TIMERA_VALUE);

	/* Conver to usec */
	usec = (unsigned long)(elapsed * tick) / LATCH;

	return usec;
}

static void brh_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	*BRH_TIMERA_CTRL |= BRH_TIMER_INTERRUPT;

	/*
	 * Hack...see IQ80310 timer for reasoning
	 */
	irq_exit(smp_processor_id(), irq);
//	printk("calling do_timer\n");
	do_timer(regs);
	irq_enter(smp_processor_id(), irq);
}

static struct irqaction timer_irq = {
	name: "timer",
};

extern unsigned long (*gettimeoffset)(void);
extern int setup_arm_irq(int, struct irqaction *);

void __init setup_timer(void)
{
	gettimeoffset = brh_gettimeoffset;
	timer_irq.handler = brh_timer_interrupt;
	setup_arm_irq(IRQ_BRH_TIMERA, &timer_irq);

	*BRH_TIMERA_CTRL = 0;
	*BRH_TIMERA_PRELOAD = LATCH;
	*BRH_TIMERA_CTRL = BRH_TIMER_ENABLE | BRH_TIMER_CONTINOUS;
}

#endif // !CONFIG_XSCALE_PMU_TIMER
