/*
 * linux/include/asm-arm/arch-adifcc/timex.h
 *
 * XScale architecture timex specifications
 */

#ifdef CONFIG_XSCALE_PMU_TIMER
/* 
 * This is for a timer based on the XS80200's PMU counter 
 */
#define CLOCK_TICK_RATE 400000000 

#else

#define CLOCK_TICK_RATE 33000000

#endif
