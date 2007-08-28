/*
 * include/asm-arm/arch-adifcc/serial.h
 *
 * Author: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright (c) 2001 MontaVista Software, Inc.
 */

#ifndef _ARCH_SERIAL_H_
#define _ARCH_SERIAL_H_

/* Standard COM flags */
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

/*
 * We use SERIAL_IO_MEM type since these are memory mapped UARTs,
 * not ISA style I/O ports.
 */
 
#ifdef CONFIG_ARCH_ADI_EVB

#define BASE_BAUD ( 1852000 / 16 )

#define RS_TABLE_SIZE 1

/*
 * One serial port, int goes to FIQ, so we run in polled mode
 */
#define STD_SERIAL_PORT_DEFNS				\
	{						\
	  magic: 0,					\
	  baud_base: BASE_BAUD,				\
	  irq: 0,					\
	  flags: STD_COM_FLAGS,				\
	  iomem_base: 0xff400000,			\
	  io_type: SERIAL_IO_MEM			\
	} /* ttyS0 */

#elif defined(CONFIG_ARCH_BRH)

/*
 * BRH uses 33MHz clock for uart
 */
#define BASE_BAUD ( 33000000 / 16 )

#define	RS_TABLE_SIZE	2

#define STD_SERIAL_PORT_DEFNS				\
	{						\
	  magic: 0,					\
	  baud_base: BASE_BAUD,				\
	  irq: IRQ_BRH_UART_A,				\
	  flags: STD_COM_FLAGS,				\
	  iomem_base: BRH_UART0_BASE,			\
	  io_type: SERIAL_IO_MEM			\
	}, /* ttyS0 */					\
	{						\
	  magic: 0,					\
	  baud_base: BASE_BAUD,				\
	  irq: IRQ_BRH_UART_B,				\
	  flags: STD_COM_FLAGS,				\
	  iomem_base: BRH_UART1_BASE,			\
	  io_type: SERIAL_IO_MEM			\
	} /* ttyS1 */
#endif

#define EXTRA_SERIAL_PORT_DEFNS

#endif // _ARCH_SERIAL_H_
