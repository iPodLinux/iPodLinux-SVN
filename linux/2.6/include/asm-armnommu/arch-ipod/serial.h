/*
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

#ifndef __ASM_ARCH_SERIAL_H
#define __ASM_ARCH_SERIAL_H

#include <asm/arch/hardware.h>
#include <asm/arch/irqs.h>
#include <linux/termios.h>

#undef SERIAL_DEBUG_INTR
#undef SERIAL_DEBUG_OPEN
#undef SERIAL_DEBUG_FLOW
#undef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
#undef SERIAL_DEBUG_PCI
#undef SERIAL_DEBUG_AUTOCONF


#define IPOD_SER0_BASE	0xc0006000
#define IPOD_SER1_BASE	0xc0006040

/* The UART is clocked at 24MHz */
#define BASE_BAUD	(24576000 / 16)

#define STD_COM_FLAGS	(ASYNC_BOOT_AUTOCONF|ASYNC_SKIP_TEST)
#define DEFAULT_CFLAGS (B9600 | CS8 | CREAD | HUPCL | CLOCAL)

#define RS_TABLE_SIZE 2

#define STD_SERIAL_PORT_DEFNS \
	{  \
	baud_base: BASE_BAUD, \
	irq: SER0_IRQ, \
	flags: STD_COM_FLAGS, \
	iomem_base: (u8*)IPOD_SER0_BASE, \
	iomem_reg_shift: 2, \
	io_type: SERIAL_IO_MEM \
	},      /* ttyS0 */ \
        {  \
        baud_base: BASE_BAUD, \
        irq: SER1_IRQ, \
        flags: STD_COM_FLAGS, \
        iomem_base: (u8*)IPOD_SER1_BASE, \
        iomem_reg_shift: 2, \
        io_type: SERIAL_IO_MEM \
	}       /* ttyS1 */

#define EXTRA_SERIAL_PORT_DEFNS

#endif

