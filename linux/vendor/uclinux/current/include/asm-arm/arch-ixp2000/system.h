/*
 * linux/include/asm-arm/arch-ixp2400/system.h
 *
 * Copyright (C) 2002 Intel Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

static inline void arch_idle(void)
{
#if 0
	while (!current->need_resched && !hlt_counter) {
		cpu_do_idle(IDLE_CLOCK_SLOW);
		cpu_do_idle(IDLE_WAIT_FAST);
		cpu_do_idle(IDLE_CLOCK_FAST);
	}
#endif
}


static inline void arch_reset(char mode)
{
	cli();
	/* Use on-chip reset capability */
	if (*IXP2000_STRAP_OPTIONS & CFG_PCI_BOOT_HOST) {
		*(IXP2000_RESET0) |= (RSTALL);
		udelay(1000);
	}
}
