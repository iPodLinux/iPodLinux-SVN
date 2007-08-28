/*
 * linux/include/asm-arm/arch-ixp425/timex.h
 *
 * XScale architecture timex specifications
 */

/*
 * We use IXP425 General purpose timer for our timer needs, it runs at 66 MHz
 */
#define CLOCK_TICK_RATE (IXP425_PERIPHERAL_BUS_CLOCK * 1000000)

