/*
 * linux/include/asm-arm/arch-ixp1200/time.h
 *
 * Copyright (C) 1998 Deborah Wallach.
 * Twiddles   (C) 1999 	Hugo Fiennes <hugo@empeg.com>
 * 			Nicolas Pitre <nico@cam.org>
 *
 * Modified for IXP1200 Uday Naik
 *
 * Todo: Base LATCH on PLL register
 */

#include <asm/arch/hardware.h>
#include <asm/arch/irqs.h>


static unsigned long ixp1200_latch = 0;

/* IRQs are disabled before entering here from do_gettimeofday() */

static unsigned long ixp1200_gettimeoffset (void)
{

	unsigned long offset, usec;

	/* Get ticks since last timer service */

	offset = ixp1200_latch - *CSR_TIMER1_VALUE;
	
	usec = (unsigned long)(offset*tick)/ixp1200_latch;

	return usec;
}

static void ixp1200_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	/* clear timer 1 IRQ */
	
	*CSR_TIMER1_CLR = 1;

	do_timer(regs);

}

/*
 * We read the PLL_CFG register to determine the clock tick rate.
 * Theoretically, we could just change PLL_CFG so it always runs at
 * 232MHz, but certain models of IXP can't do that, and also, some 
 * people may want to run at slower speed due to heat/power concerns,
 * which often come up in cPCI designs.
 */
extern inline void setup_timer (void)
{
	unsigned int pll_cfg;
	unsigned int speeds[] =
		{ 29491000, 36865000, 44237000, 51610000, 58982000,
		  66355000, 73728000, 81101000, 88474000, 95846000,
		  103219000, 110592000, 132710000, 147456000, 154829000,
		  162202000, 165890000, 176947000, 191693000, 199066000 ,
		  0, 0, 232000000 };


	gettimeoffset = ixp1200_gettimeoffset;
	timer_irq.handler = ixp1200_timer_interrupt;

	pll_cfg = *((unsigned int *)CSR_PLL_CFG);

	printk("IXP1200 Running @ %dMHz\n", speeds[pll_cfg] / 1000000);

	ixp1200_latch = (speeds[pll_cfg] + HZ/2) / HZ;

	*CSR_TIMER1_CLR = 0;

	*CSR_TIMER1_LOAD	 = ixp1200_latch;  

	*CSR_TIMER1_CNTL = (1 << 7) | (1 << 6);

	setup_arm_irq(IXP1200_IRQ_TIMER1, &timer_irq);
}



