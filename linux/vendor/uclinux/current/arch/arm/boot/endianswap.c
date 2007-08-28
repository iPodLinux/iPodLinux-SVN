/*
 * endianswap.c : Swaps endianness on 4-byte boundaries.
 * Author: 	Nicolas Pitre
 * Copyright:	(C) 2001 MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * The purpose of this tool is to convert a kernel image which is compiled
 * to run in a different endianness than what the CPU is initially set with.
 * The kernel will switch the CPU endian mode as required, but since the 
 * bootloader lays the kernel in memory according to the current endianness
 * we have to swap it over.  As long as the desired endianness is set before
 * any memory access smaller than a word is performed there shouldn't be any
 * problem executing such code.
 */

#include <stdio.h>

int main(int argc, char *argv[])
{
	int a, b, c, d;

	for (;;) {
		a = getchar();
		b = getchar();
		c = getchar();
		d = getchar();
		if ((a|b|c|d) & ~0xff)
			break;
		putchar(d);
		putchar(c);
		putchar(b);
		putchar(a);
	}
	if (a < 0)
		return 0;
	if (b < 0)
		b = 0;
	if (c < 0)
		c = 0;
	if (d < 0)
		d = 0;
	putchar(d);
	putchar(c);
	putchar(b);
	putchar(a);
	return 0;
}
