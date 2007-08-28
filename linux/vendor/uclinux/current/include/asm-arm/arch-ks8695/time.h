/*
 *  linux/include/asm-arm/arch-ks8695/time.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <asm/system.h>
#include <asm/leds.h>

/*
 * Where is the timer (VA)?
 */
#define TIMER_VA_BASE  (IO_ADDRESS(KS8695_IO_BASE))

/*
 * How long is the timer interval?
 */
#define TIMER_INTERVAL	     (TICKS_PER_uSEC * mSEC_10)
#define TIMER_DATA_VALUE     (TIMER_INTERVAL >> 1)
#define TIMER_PULSE_VALUE    (TIMER_INTERVAL - TIMER_DATA_VALUE)
#define TIMER_VALUE          (TIMER_DATA_VALUE + TIMER_PULSE_VALUE)
#define TICKS2USECS(x)	     ((x) / TICKS_PER_uSEC)
#define TIMER_GET_VALUE(x)   (__raw_readl(x + KS8695_TIMER1) + __raw_readl(x + KS8695_TIMER1_PCOUNT))

extern unsigned long (*gettimeoffset)(void);

/*
 * Returns number of ms since last clock interrupt.  Note that interrupts
 * will have been disabled by do_gettimeoffset()
 */
static unsigned long ks8695_gettimeoffset(void)
{
	unsigned long ticks1, ticks2, status;

	/*
	 * Get the current number of ticks.  Note that there is a race
	 * condition between us reading the timer and checking for
	 * an interrupt.  We get around this by ensuring that the
	 * counter has not reloaded between our two reads.
	 */
	ticks2 = TIMER_GET_VALUE(TIMER_VA_BASE);
	do {
		ticks1 = ticks2;
		status = __raw_readl(TIMER_VA_BASE + KS8695_INT_STATUS);
		ticks2 = TIMER_GET_VALUE(TIMER_VA_BASE);
	} while (ticks2 > ticks1);

	/*
	 * Number of ticks since last interrupt.
	 */
	ticks1 = TIMER_VALUE - ticks2;

	/*
	 * Interrupt pending?  If so, we've reloaded once already.
	 */
	if (status & KS8695_INTMASK_TIMERINT1)
		ticks1 += TIMER_VALUE;

	/*
	 * Convert the ticks to usecs
	 */
	return TICKS2USECS(ticks1);
}

/*
 * IRQ handler for the timer
 */
static void ks8695_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	// ...clear the interrupt
        __raw_writel(KS8695_INTMASK_TIMERINT1, TIMER_VA_BASE + KS8695_INT_STATUS);

	do_timer(regs);
	do_profile(regs);
}

/*
 * Set up timer interrupt, and return the current time in seconds.
 */
static inline void setup_timer(void)
{
	timer_irq.handler = ks8695_timer_interrupt;

	/*
	 * Initialise to a known state (all timers off)
	 */
        __raw_writel(0, TIMER_VA_BASE + KS8695_TIMER_CTRL);

	/* 
	 * Make irqs happen for the system timer
	 */
	setup_arm_irq(KS8695_INT_TIMERINT1, &timer_irq);
	gettimeoffset = ks8695_gettimeoffset;

	/*
	 * enable timer 1, timer 0 for watchdog
	 */
        __raw_writel(TIMER_DATA_VALUE, TIMER_VA_BASE + KS8695_TIMER1);
        __raw_writel(TIMER_PULSE_VALUE, TIMER_VA_BASE + KS8695_TIMER1_PCOUNT);
        __raw_writel(0x02, TIMER_VA_BASE + KS8695_TIMER_CTRL);
}
