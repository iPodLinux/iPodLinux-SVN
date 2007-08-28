/*
 * linux/include/asm-arm/arch-ixp1200/timex.h
 *
 * IXP1200-based architecture timex specifications
 *
 * Copyright (c) 2002 MontaVista Sofware, Inc.
 * Author: Deepak Saxena <dsaxena@mvista.com>
 *
 */

/*
 * We don't really use this to determine the bogomips since
 * we read the CPU speed from the PLL_CFG register.
 */
#define CLOCK_TICK_RATE		232*1000*1000
