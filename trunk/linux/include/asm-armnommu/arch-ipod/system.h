/*
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

#ifndef __ARCH_ASM_SYSTEM_H__
#define __ARCH_ASM_SYSTEM_H__

static __inline__ void arch_idle(void)
{
	while (!current->need_resched && !hlt_counter);
}

extern __inline__ void arch_reset(char mode)
{
}

#endif

