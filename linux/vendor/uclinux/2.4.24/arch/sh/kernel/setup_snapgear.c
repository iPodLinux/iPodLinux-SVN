/****************************************************************************/
/* 
 * linux/arch/sh/kernel/setup_snapgear.c
 *
 * Copyright (C) 2002  David McCullough <davidm@snapgear.com>
 *
 * Based on files with the following comments:
 *
 *           Copyright (C) 2000  Kazumoto Kojima
 *
 *           Modified for 7751 Solution Engine by
 *           Ian da Silva and Jeremy Siegel, 2001.
 */
/****************************************************************************/

#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/timer.h>
#include <linux/ledman.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/sched.h>

#include <asm/irq.h>
#include <asm/io.h>

/****************************************************************************/
/*
 * EraseConfig handling functions
 */

static void
eraseconfig_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
#if defined(CONFIG_SH_SECUREEDGE5410)
{
	volatile char dummy;
	dummy = * (volatile char *) 0xb8000000;
}
#endif
#ifdef CONFIG_LEDMAN
	ledman_signalreset();
#else
	printk("SnapGear: erase switch interrupt!\n");
#endif
}

static int __init
eraseconfig_init(void)
{
	printk("SnapGear: EraseConfig init\n");
	/* Setup "EraseConfig" switch on external IRQ 0 */
	if (request_irq(IRL0_IRQ, eraseconfig_interrupt, SA_INTERRUPT,
				"Erase Config", NULL))
		printk("SnapGear: failed to register IRQ%d for Reset witch\n",
				IRL0_IRQ);
	else
		printk("SnapGear: registered EraseConfig switch on IRQ%d\n",
				IRL0_IRQ);
	return(0);
}

module_init(eraseconfig_init);

/****************************************************************************/
/*
 * Initialize IRQ setting
 *
 * IRL0 = erase switch
 * IRL1 = eth0
 * IRL2 = eth1
 * IRL3 = crypto
 */

void __init
init_snapgear_IRQ(void)
{
	/* enable individual interrupt mode for externals */
	ctrl_outw(ctrl_inw(INTC_ICR) | INTC_ICR_IRLM, INTC_ICR);

	printk("Setup SnapGear IRQ/IPR ...\n");

	make_ipr_irq(IRL0_IRQ, IRL0_IPR_ADDR, IRL0_IPR_POS, IRL0_PRIORITY);
	make_ipr_irq(IRL1_IRQ, IRL1_IPR_ADDR, IRL1_IPR_POS, IRL1_PRIORITY);
	make_ipr_irq(IRL2_IRQ, IRL2_IPR_ADDR, IRL2_IPR_POS, IRL2_PRIORITY);
	make_ipr_irq(IRL3_IRQ, IRL3_IPR_ADDR, IRL3_IPR_POS, IRL3_PRIORITY);
}

/****************************************************************************/
/*
 * Initialize the board
 */

void __init
setup_snapgear(void)
{
	extern void secureedge5410_rtc_init(void);

#if defined(CONFIG_SH_SECUREEDGE5410)
{
	volatile char dummy;
	dummy = * (volatile char *) 0xb8000000;
}
#endif

	/* XXX: RTC setting comes here */
#ifdef CONFIG_RTC
	secureedge5410_rtc_init();
#endif
}

/****************************************************************************/
