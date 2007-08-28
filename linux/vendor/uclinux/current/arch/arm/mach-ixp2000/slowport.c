/*
 * arch/arm/mach-ixp2000/slowport.c
 *
 * Slowport access functions.  We could make these inlines, but then
 * we would have a global spinlock floating around and that's ugly.
 *
 * Copyright (c) 2002 MontaVista Software, Inc.
 */


#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <asm/system.h>
#include <asm/hardware.h>

static spinlock_t slowport_lock = SPIN_LOCK_UNLOCKED;
static struct slowport_cfg old_cfg;
static unsigned long flags;

/*
 * The only reason we need to save/restore the state is b/c
 * the CCR register affects the state of both device 1 and
 * device 2.  This means that we could really screw up the
 * flash access if the CCR is not restored.
 */
void acquire_slowport(struct slowport_cfg *new_cfg)
{
	spin_lock_irqsave(&slowport_lock, flags);

	old_cfg.CCR = *IXP2000_SLOWPORT_CCR;
	old_cfg.WTC = *IXP2000_SLOWPORT_WTC2;
	old_cfg.RTC = *IXP2000_SLOWPORT_RTC2;
	old_cfg.PCR = *IXP2000_SLOWPORT_PCR;
	old_cfg.ADC = *IXP2000_SLOWPORT_ADC;

	*IXP2000_SLOWPORT_CCR = new_cfg->CCR;
	*IXP2000_SLOWPORT_WTC2 = new_cfg->WTC;
	*IXP2000_SLOWPORT_RTC2 = new_cfg->RTC;
	*IXP2000_SLOWPORT_PCR = new_cfg->PCR;
	*IXP2000_SLOWPORT_ADC = new_cfg->ADC;
}

void release_slowport(void)
{
	*IXP2000_SLOWPORT_CCR = old_cfg.CCR;
	*IXP2000_SLOWPORT_WTC2 = old_cfg.WTC;
	*IXP2000_SLOWPORT_RTC2 = old_cfg.RTC;
	*IXP2000_SLOWPORT_PCR = old_cfg.PCR;
	*IXP2000_SLOWPORT_ADC = old_cfg.ADC;

	spin_unlock_irqrestore(&slowport_lock, flags);
}


