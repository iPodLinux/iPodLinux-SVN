/*
 * Copyright (c) 2003,2004 Bernard Leach (leachbj@bouncycastle.org)
 */

#ifndef __ARCH_ASM_SYSTEM_H__
#define __ARCH_ASM_SYSTEM_H__

static __inline__ void arch_idle(void)
{
	// while (!current->need_resched && !hlt_counter);
	cpu_do_idle();
}

extern __inline__ void arch_reset(char mode)
{
	extern void ipod_hard_reset(void);
	ipod_hard_reset();
}

#endif

