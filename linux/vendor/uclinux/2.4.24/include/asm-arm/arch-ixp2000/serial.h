/*
 * include/asm-arm/arch-ixp2000/serial.h
 *
 * Serial port defn for ixp2000 based systems
 *
 * Author: Deepak Saxena <dsaxena@mvsita.com>
 *
 * Copyright (c) 2002 MontaVista Software, Inc.
 */


#define BASE_BAUD 50000000 / 16 // IS THIS ALWAYS TRUE?

#define STD_COM_FLAGS	(ASYNC_SKIP_TEST)

#define RS_TABLE_SIZE	1

#define	STD_SERIAL_PORT_DEFNS			\
	{					\
		type: PORT_XSCALE,		\
		xmit_fifo_size: 64,		\
		baud_base: BASE_BAUD,		\
		irq: IRQ_IXP2000_UART,		\
		flags: STD_COM_FLAGS,		\
		iomem_base: IXP2000_UART_BASE+3,\
		io_type: SERIAL_IO_MEM,		\
		iomem_reg_shift: 2		\
	}

#define EXTRA_SERIAL_PORT_DEFNS


