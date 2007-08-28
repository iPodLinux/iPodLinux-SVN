/*
 * arch/microblaze/kernel/microblazE_timer.c
 *
 *  Copyright (C) 2001,2002  NEC Corporation
 *  Copyright (C) 2001,2002  Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Written by Miles Bader <miles@gnu.org>
 */

#include <linux/kernel.h>

#include <asm/mbvanilla.h>		/* Get timer base addr etc */
#include <asm/microblaze_timer.h>

/* Start interval timer TIMER (0-3).  The timer will issue the
   corresponding INTCMD interrupt RATE times per second.
   This function does not enable the interrupt.  */
void microblaze_timer_configure (unsigned timer, unsigned rate)
{
	unsigned count;

	/* 
	 * Work out timer compare value for a given clock freq 
	 * and interrupt rate
	 */
	count = CONFIG_CPU_CLOCK_FREQ / rate;

	/* Do the actual hardware timer initialization:  */

	/* Enable timer, enable interrupt generation, and put into count down mode */
	/* Set the compare counter */
	timer_set_compare(MICROBLAZE_TIMER_BASE_ADDR, timer, count);

	/* Reset timer and clear interrupt */
	timer_set_csr(MICROBLAZE_TIMER_BASE_ADDR, timer, 
			TIMER_INTERRUPT | TIMER_RESET);

	/* start the timer */
	timer_set_csr(MICROBLAZE_TIMER_BASE_ADDR, timer, 
			TIMER_ENABLE | TIMER_ENABLE_INTR | TIMER_RELOAD | TIMER_DOWN_COUNT);

}
