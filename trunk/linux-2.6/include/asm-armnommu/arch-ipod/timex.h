/*
 * Copyright (c) 2003,2004 Bernard Leach (leachbj@bouncycastle.org)
 */

#ifndef __ASM_ARCH_TIMEX_H__
#define __ASM_ARCH_TIMEX_H__

/*
 * We assume a constant here; this satisfies the maths in linux/timex.h
 * and linux/time.h.  CLOCK_TICK_RATE may be system dependent, but this
 * must be a constant.
 */
#define CLOCK_TICK_RATE 750000000

#endif

