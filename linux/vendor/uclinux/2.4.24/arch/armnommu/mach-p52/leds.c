/*
 *  linux/arch/arm/mach-p52/leds.c
 *  TBD
 */
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/leds.h>
#include <asm/system.h>
#include <asm/mach-types.h>

static spinlock_t leds_lock;


static int __init leds_init(void)
{
	return 0;
}

__initcall(leds_init);








