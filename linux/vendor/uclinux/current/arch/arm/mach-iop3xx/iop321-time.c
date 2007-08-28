/*
 * arch/arm/mach-iop3xx/iop321-time.c
 *
 * Timer code for IOP321 based systems
 *
 * Author: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright 2002 MontaVista Software Inc.
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

static unsigned long iop321_gettimeoffset(void)
{
	unsigned long elapsed, usec;

	elapsed = *IOP321_TU_TCR0;

	usec = (unsigned long)((LATCH - elapsed) * tick) / LATCH;

	return usec;
}

static void iop321_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	u32 tisr;

	asm volatile("mrc p6, 0, %0, c6, c1, 0" : "=r" (tisr));

	tisr |= 1;

	asm volatile("mcr p6, 0, %0, c6, c1, 0" : : "r" (tisr));

	do_timer(regs);
}

extern unsigned long (*gettimeoffset)(void);

static struct irqaction timer_irq = {
	name: "timer",
};

extern int setup_arm_irq(int, struct irqaction*);

void __init setup_timer(void)
{
	u32 timer_ctl;
	u32 latch = LATCH;

	gettimeoffset = iop321_gettimeoffset;
	timer_irq.handler = iop321_timer_interrupt;
	setup_arm_irq(IRQ_IOP321_TIMER0, &timer_irq);

	timer_ctl = IOP321_TMR_EN | IOP321_TMR_PRIVILEGED | IOP321_TMR_RELOAD |
			IOP321_TMR_RATIO_1_1;

	asm volatile("mcr p6, 0, %0, c4, c1, 0" : : "r" (LATCH));

	asm volatile("mcr p6, 0, %0, c0, c1, 0"	: : "r" (timer_ctl));
}


