/*
 * linux/include/asm-arm/arch-ixdp2400/uncompress.h
 * Author: Naeem Afzal <naeem.m.afzal@intel.com>
 *
 * Copyright 2002 Intel Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

/* At this point, the MMU is not on, so use physical addresses */


#define UART_BASE	0xc0030000

#define PHYS(x)          ((volatile unsigned long *)(UART_BASE + x))

#define UARTDR          PHYS(0x00)      /* Transmit reg dlab=0 */
#define UARTDLL         PHYS(0x00)      /* Divisor Latch reg dlab=1*/
#define UARTDLM         PHYS(0x04)      /* Divisor Latch reg dlab=1*/
#define UARTIER         PHYS(0x04)      /* Interrupt enable reg */
#define UARTFCR         PHYS(0x08)      /* FIFO control reg dlab =0*/
#define UARTLCR         PHYS(0x0c)      /* Control reg */
#define UARTSR          PHYS(0x14)      /* Status reg */

/*
 * The following code assumes the serial port has already been
 * initialized by the bootloader or such...
 */

#define TEMT 0x20 /* bit5=1, means transmit holding reg empty */

static void puts( const char *s )
{
    int i;
    
    for (i = 0; *s; i++, s++) {

	/* wait for space in the UART's transmiter */
	while (*UARTSR & !TEMT);

	/* send the character out. */
	*UARTDR = *s;
	/* if a LF, also do CR... */
	if (*s == 10) {

	  /* wait for space in the UART's transmiter */
	  while (*UARTSR & !TEMT);

	  /* send the CR character out. */
	  *UARTDR = 13;
	  
	}

    }

}

static void arch_decomp_setup()
{
        *UARTLCR = 0x80808080; 	/* dlab=0 enable access to divisor reg */
       	*UARTDLL = 0x36363636;  /*50MHz divisor latch LSB baud rate 57600 */
        *UARTDLM = 0; 		/* divisor latch MSB */
        *UARTLCR = 0x03030303; 	/* 8 data, 1 stop bit, no parity */
        *UARTIER = 0x0; 	/* disable interrupts */
        *UARTFCR = 0x01010101; 	/* turn FIFOs on for 1byte interrupt*/
        *UARTIER = 0x40404040; 	/* enable UART and disable interrupts */
}

#define arch_decomp_wdog()
