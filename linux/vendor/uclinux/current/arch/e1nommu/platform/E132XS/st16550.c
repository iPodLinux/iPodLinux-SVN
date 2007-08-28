/*
 * linux/arch/e1nommu/platform/E132XS/st16550.c
 *
 * Driver for the second serial Debug_UART of the E1-32XS Evaluation board 
 * This driver is used for debugging purposes, in the future it should be 
 * replaced with the Serial driver provided by the standard linux kernel
 * /drivers/char/serial.c !!!!
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Original Copyright (C) 1995, 1996, 1997 by Ralf Baechle
 * Original Copyright (C) 2001 by Liam Davies (ldavies@agile.tv)
 * Copyright (C) 2002 by Yannis Mitsos <yannis.mitsos@gdt.gr>
 */
#include <linux/init.h>
#include <linux/console.h>
#include <linux/kdev_t.h>
#include <linux/major.h>
#include <linux/serial_reg.h>

#include <asm/delay.h>
#include <asm/st16550_serial.h>
#include <asm/io.h>

//static unsigned long Debug_UART = ((CONFIG_SERIAL_CONSOLE_CS + 4) << 22);

static __inline__ void st16550_cons_put_char(char ch, unsigned long ioaddr)
{
	char lsr;

	do {
#ifndef CONFIG_IO_ACCESS 
		lsr = inb(ioaddr | (UART_LSR << IORegAddress));
#else
		lsr = inb(ioaddr + UART_LSR);
#endif
	} while ((lsr & (UART_LSR_TEMT | UART_LSR_THRE)) != (UART_LSR_TEMT | UART_LSR_THRE));
	outb(ch, (ioaddr | (UART_TX << IORegAddress)));
}

static __inline__ char st16550_cons_get_char(unsigned long ioaddr)
{
		int i;
	while ((inb(ioaddr + UART_LSR) & UART_LSR_DR) == 0)
//		udelay(1);
		for(i=0; i<=10000;i++);
	return inb(ioaddr + UART_RX);
}

void st16550_console_write(struct console *co, const char *s, unsigned count)
{
	char lsr, ier;
	unsigned i;
	
#ifndef CONFIG_IO_ACCESS 
	ier = inb(Debug_UART | (UART_IER << IORegAddress));
	outb(0x00, (Debug_UART | (UART_IER << IORegAddress)));
#else
	ier = inb(Debug_UART + UART_IER);
	outb(0x00, Debug_UART + UART_IER);
#endif
	for (i=0; i < count; i++, s++) {

		if(*s == '\n')
			st16550_cons_put_char('\r', Debug_UART);
		st16550_cons_put_char(*s, Debug_UART);
	}

	do {
#ifndef CONFIG_IO_ACCESS
		lsr = inb(Debug_UART | (UART_LSR << IORegAddress));
#else
		lsr = inb(Debug_UART + UART_LSR);
#endif	
   	} while ((lsr & (UART_LSR_TEMT | UART_LSR_THRE)) != (UART_LSR_TEMT | UART_LSR_THRE));

#ifndef CONFIG_IO_ACCESS
	outb(ier, (Debug_UART | (UART_IER << IORegAddress)));
#else
	outb(ier, Debug_UART + UART_IER);
#endif
}

char getDebugChar(void)
{
	return st16550_cons_get_char(Debug_UART);
}

void putDebugChar(char kgdb_char)
{
	st16550_cons_put_char(kgdb_char, Debug_UART);
}


static kdev_t 
st16550_console_dev(struct console *c)
{
	return MKDEV(TTY_MAJOR, 64 + c->index);
}



static struct console st16550_console = {
    name:	 	"ttyS",
    write:	 	st16550_console_write,
	read: 	 	NULL,
    device:	 	st16550_console_dev,
	unblank: 	NULL,
    setup:	 	NULL,
    flags:		CON_PRINTBUFFER,
    index:		-1,
	cflag:		0
//	console:	NULL
};


void __init st16550_setup_console(void)
{
	register_console(&st16550_console);
}


void st16550_serial_init(void)
{
	/*
	 * Configure active Debug_UART, (CHANNELOFFSET already set.)
	 *
	 * Set 8 bits, 1 stop bit, no parity.
	 *
	 * LCR<7>       0       divisor latch access bit
	 * LCR<6>       0       break control (1=send break)
	 * LCR<5>       0       stick parity (0=space, 1=mark)
	 * LCR<4>       0       parity even (0=odd, 1=even)
	 * LCR<3>       0       parity enable (1=enabled)
	 * LCR<2>       0       # stop bits (0=1, 1=1.5)
	 * LCR<1:0>     11      bits per character(00=5, 01=6, 10=7, 11=8)
	 */

#ifndef CONFIG_IO_ACCESS
	outb(0x3, (Debug_UART | (UART_LCR << IORegAddress)));

	outb(FIFO_ENABLE, (Debug_UART | (UART_FCR << IORegAddress)));	/* Enable the FIFO */

	outb(INT_ENABLE, (Debug_UART | (UART_IER << IORegAddress)));	/* Enable appropriate interrupts */
#else
	outb(0x3, Debug_UART + UART_LCR);

	outb(FIFO_ENABLE, Debug_UART + UART_FCR);	/* Enable the FIFO */

	outb(INT_ENABLE, Debug_UART + UART_IER);	/* Enable appropriate interrupts */
#endif
}


void st16550_serial_set(unsigned long baud)
{
	unsigned char sav_lcr;
	int value;
	unsigned char DLM_value, DLL_value;

	/*
	 * Enable access to the divisor latches by setting DLAB in LCR.
	 *
	 */
#ifndef CONFIG_IO_ACCESS
	sav_lcr = inb(Debug_UART | (UART_LCR << IORegAddress));
#else
	sav_lcr = inb(Debug_UART + UART_LCR);
#endif

	/*
	 * Set baud rate 
	 */
    value = (int)((int)(XTAL >> 4) / baud); /* Yannis: FIXME check if the devision works */ 	
	DLL_value = (value & 0xff);
	DLM_value = ((value & 0xff00)>>8);
	
#ifndef CONFIG_IO_ACCESS
	outb(0x83, (Debug_UART | (UART_LCR << IORegAddress)));
	outb(DLM_value, (Debug_UART | (UART_DLM << IORegAddress)));	
	outb(DLL_value, (Debug_UART | (UART_DLL << IORegAddress)));
	outb(0x03, (Debug_UART | (UART_LCR << IORegAddress)));

	/*
	 * Restore line control register
	 */
	outb(sav_lcr, (Debug_UART | (UART_LCR << IORegAddress)));
#else
	outb(0x83, Debug_UART + UART_LCR);
	outb(DLM_value, Debug_UART + UART_DLM);	
	outb(DLL_value, Debug_UART + UART_DLL);
	outb(0x03, Debug_UART + UART_LCR);

	/*
	 * Restore line control register
	 */
	outb(sav_lcr, Debug_UART + UART_LCR);
#endif
}
