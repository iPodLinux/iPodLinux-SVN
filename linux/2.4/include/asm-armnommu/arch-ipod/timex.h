/*
 * Copyright (c) 2003-2005, Bernard Leach (leachbj@bouncycastle.org)
 */

#ifndef __ASM_ARCH_TIMEX_H__
#define __ASM_ARCH_TIMEX_H__

#define USECS_PER_INT 0x2710

/* PP5002 registers */
#define PP5002_TIMER1       0xcf001100
#define PP5002_TIMER1_ACK   0xcf001104

#define PP5002_TIMER_STATUS 0xcf001110

/* PP5020 registers */
#define PP5020_TIMER1       0x60005000
#define PP5020_TIMER1_ACK   0x60005004

#define PP5020_TIMER2       0x60005008
#define PP5020_TIMER2_ACK   0x6000500c

#define PP5020_TIMER_STATUS 0x60005010

#endif

