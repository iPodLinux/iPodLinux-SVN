/*
 * arch/arm/mach-ixp2000/ixp2000-irq.c
 *
 * Low level kgdb code for the IXP2000 board
 *
 * Author: Naeem Afzal <naeem.m.afzal@intel.com>
 *
 * Copyright 2002 Intel Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/config.h>
#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/serial_reg.h>

#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>

#include <asm/kgdb.h>
#include <asm/serial.h>
#include <asm/hardware.h>
#include <asm/irq.h>


/* Standard COM flags */
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

#undef BASE_BAUD
#undef RS_TABLE_SIZE  

/*
 * IXP2400 uses 50MHz clock for uart
 */

#define BASE_BAUD ( 50000000 / 16 )

#define RS_TABLE_SIZE   1

#define STD_SERIAL_PORT_DEFNS                           \
        {                                               \
          magic: 0,                                     \
          baud_base: BASE_BAUD,                         \
          irq: IRQ_IXP2000_UART,                        \
          flags: STD_COM_FLAGS,                         \
          iomem_base: IXP2000_UART_BASE,                     \
          io_type: SERIAL_IO_MEM                        \
        } /* ttyS0 */


static struct serial_state ports[RS_TABLE_SIZE] = {
	SERIAL_PORT_DFNS
};

static short port = 0;
static volatile unsigned char *serial_base = NULL;

void kgdb_serial_init(void)
{

	// TODO: Make port number a config or cmdline option
	port = 1;

	serial_base = ports[port].iomem_base;	

	serial_base[UART_LCR] = 0x80; /* enable access to divisor reg */
	serial_base[UART_DLL] = 0x36; /*divisor latch LSB baud rate 57600 */
	serial_base[UART_DLM] = 0; /* divisor latch MSB */
	serial_base[UART_LCR] = 0x03; /* 8 data, 1 stop bit, no parity */
	serial_base[UART_FCR] = 0x07; /* turn FIFOs on */
	serial_base[UART_IER] = 40; /* enable UART and disable interrupts */

	return;
}

void kgdb_serial_putchar(unsigned char ch)
{
	unsigned char status;

	do
	{
		status = serial_base[UART_LSR];
	} while ((status & (UART_LSR_TEMT|UART_LSR_THRE)) !=
			(UART_LSR_TEMT|UART_LSR_THRE));

	*serial_base = ch;
}

unsigned char kgdb_serial_getchar(void)
{
	unsigned char ch;

	while(!(serial_base[UART_LSR] & 0x1));

	ch = *serial_base;

	return ch;
}

