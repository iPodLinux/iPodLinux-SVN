/*
 * linux/include/asm-arm/arch-ixp1200/serial.h
 *
 * This is empty since the IXP1200 has no 16550 serial ports.  If
 * someone builds an IXP1200 board with an off-chip port, this
 * needs to be filled in appropriately.
 */

#ifndef _ARCH_SERIAL_H_
#define _ARCH_SERIAL_H_

#define BASE_BAUD (3686400 / 16)

/* Base port for UART */
#define UART_PORT CSR_UARTSR

/* serial port flags */
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

#define SERIAL_PORT_DFNS \
	{ 0, BASE_BAUD, 0, 0, STD_COM_FLAGS } /* ttyS0 */	

#endif

