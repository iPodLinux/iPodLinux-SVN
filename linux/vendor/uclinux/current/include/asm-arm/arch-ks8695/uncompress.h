/*
 *  linux/include/asm-arm/arch-ks8695/uncompress.h
 *
 *  Copyright (C) 1999 ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <asm/arch/platform.h>

/*
 * This does not append a newline
 */
static void puts(const char *s)
{
	char val;

	while (*s) {
		while (!((*(volatile u_int *) (KS8695_IO_BASE + KS8695_UART_LINE_STATUS))
			 & KS8695_UART_LINES_TXFE));
		val = *s;
		(*(volatile u_int *) (KS8695_IO_BASE + KS8695_UART_TX_HOLDING)) = (u_int) val;

		if (*s == '\n') {
			while (!((*(volatile u_int *) (KS8695_IO_BASE + KS8695_UART_LINE_STATUS))
				 & KS8695_UART_LINES_TXFE));

			val = '\r';
			(*(volatile u_int *) (KS8695_IO_BASE + KS8695_UART_TX_HOLDING)) 
			         = (u_int) val;
		}
		s++;
	}
	while (!((*(volatile u_int *) (KS8695_IO_BASE + KS8695_UART_LINE_STATUS))
		 & KS8695_UART_LINES_TXFE));
}

/*
 * nothing to do
 */
#define arch_decomp_setup()

#define arch_decomp_wdog()
