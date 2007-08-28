/*
 * linux/include/asm-arm/arch-ixp1200/uncompress.h
 *
 * Copyright (C) 1996,1997,1998 Russell King
 * Copyright (C) 2000 Intel Corporation, Inc.
 * Copyright (C) 2002 MontaVista Software, Inc.
 *
 * March 26 2000  Uday Naik     Created
 *   Feb 15 2002  Deepak Saxena Massive cleanup/rewrite
 */

#include "serial_reg.h"


#define UARTDR		(volatile unsigned long*)(0x90003c00)
#define UARTSR		(volatile unsigned long*)(0x90003400)

/*
 * nothing to do
 */

#define arch_decomp_setup()
#define arch_decomp_wdog()

/*
 * This code assumes that the serial port has already been configured
 * by a bootloader.
 */
static void puts( const char *s )
{
	int i;
    
	for (i = 0; *s; i++, s++) {

		/* wait for space in the UART's transmiter */
		while (*UARTSR & UTSR_TXF); 
		/* send the character out. */
		*UARTDR = *s;

		/* if a LF, also do CR... */
		if (*s == 10) { 
			/* wait for space in the UART's transmiter */
			while (*UARTSR & UTSR_TXF);

			/* send the CR character out. */
			*UARTDR = 13;
		}
	}
}



