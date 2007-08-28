/*
 * uncompress.h 
 *
 *  Copyright (C) 2002 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _ARCH_UNCOMPRESS_H_
#define _ARCH_UNCOMPRESS_H_

#include <asm/hardware.h>
#include <linux/serial_reg.h>

#define UART_BASE   ((volatile u32*)IXP425_CONSOLE_UART_BASE_PHYS)

static __inline__ void putc(char c)
{
	/* Check THRE and TEMT bits before we transmit the character.
	 */
#define TX_DONE (UART_LSR_TEMT|UART_LSR_THRE)
	while ((UART_BASE[UART_LSR] & TX_DONE) != TX_DONE); 
	*UART_BASE = c;
}

/*
 * This does not append a newline
 */
static void puts(const char *s)
{
	while (*s)
	{
		putc(*s);
		if (*s == '\n')
			putc('\r');
		s++;
	}
}

#define arch_decomp_setup()
#define arch_decomp_wdog()

#endif
