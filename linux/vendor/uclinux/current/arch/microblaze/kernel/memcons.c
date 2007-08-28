/*
 * arch/v850/kernel/memcons.c -- Console I/O to a memory buffer
 *
 *  Copyright (C) 2001,2002  NEC Corporation
 *  Copyright (C) 2001,2002  Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Written by Miles Bader <miles@gnu.org>
 */

#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/init.h>

#include <asm/xuartlite_l.h>
#include <asm/xparameters.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/signal.h>

/* Spinlock protecting memcons_offs.  */
static spinlock_t memcons_lock = SPIN_LOCK_UNLOCKED;

/* Hacked by JW to outoput directly to the debug console - see what happens! */

static size_t write (const char *buf, size_t len)
{
	size_t old_len;
	int flags;
	char *point;

	volatile char *uart_write = (char *)0xFFFF2004;

	old_len=len;

	/* Dump to UART console */
	/* Evil, but I'm going to cli() for this lot */
	save_flags_cli(flags);
	
	while(old_len)
	{
		volatile unsigned int *uart_status = (unsigned int *)0xFFFF2008;
		#define TX_FIFO_FULL (1<<3)

		/* Make sure FIFO isn't full */
		while((*uart_status) & TX_FIFO_FULL)
			;

		/* Write the characters */
		*uart_write=*buf;
		/* Send CR if necessary */

		if(*buf=='\n')
		{
			/* Again, make sure FIFO isn't full */
			while((*uart_status) & TX_FIFO_FULL)
				;

			*uart_write=0x0d;
		}
		buf++;
		old_len--;
	}

	restore_flags(flags);

	return len;
}


/*  Low-level console. */

static void memcons_write (struct console *co, const char *buf, unsigned len)
{
	while (len > 0)
		len -= write (buf, len);
}

static kdev_t memcons_device (struct console *co)
{
        return MKDEV (TTY_MAJOR, 64 + co->index);
}

static struct console memcons =
{
    name:	"memcons",
    write:	memcons_write,
    device:	memcons_device,
    flags:	CON_PRINTBUFFER,
    index:	-1,
};

void memcons_setup (void)
{
	register_console (&memcons);
	printk (KERN_INFO "Console: static memory buffer (memcons)\n");
}

/* Higher level TTY interface.  */

static struct tty_struct *tty_table[1] = { 0 };
static struct termios *tty_termios[1] = { 0 };
static struct termios *tty_termios_locked[1] = { 0 };
static struct tty_driver tty_driver = { 0 };
static int tty_ref_count = 0;

int memcons_tty_open (struct tty_struct *tty, struct file *filp)
{
	return 0;
}

int memcons_tty_write (struct tty_struct *tty, int from_user,
		       const unsigned char *buf, int len)
{
	return write (buf, len);
}

int memcons_tty_write_room (struct tty_struct *tty)
{
	/* I think this is the amount of room in the buffer */
	return 16;
}

int memcons_tty_chars_in_buffer (struct tty_struct *tty)
{
	/* We have no buffer.  */
	return 0;
}

/* This is the interrupt handler for the memcons console.  It's gross
   but it may actually work one day */
void memcons_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	/* Determine reason for interrupt */
	unsigned int status = XUartLitE_mGetStatusReg(XPAR_CONSOLE_UART_BASEADDR);

	
	
	printk("UART interrupt on IRQ %i!\n",irq);
}

/* Initialise IRQ handling for console_uart.  IRQ number (1) is hard coded.
   This is all gross and dodgy but i want to get it working */
void memcons_irqinit(void)
{
	unsigned int flags;
	unsigned int uart_control;

	#define CONSOLE_UART_IRQ 1

	printk("memcons: Requesting IRQ\n");

	/* Disable uart interrupts so no nasty surprises */
	XUartLite_mDisableIntr(XPAR_CONSOLE_UART_BASEADDR);

	if(request_irq(CONSOLE_UART_IRQ, memcons_interrupt, SA_INTERRUPT,
			"console UART", NULL))
	{
		printk("memcons.c: Unable to attach memcons interrupt to IRQ %i\n", CONSOLE_UART_IRQ);
	}
	else
		XUartLite_mEnableIntr(XPAR_CONSOLE_UART_BASEADDR);
}

int __init memcons_tty_init (void)
{
	unsigned int flags;

	printk("Registering memcons serial driver.\n");
	tty_driver.name = "memcons";
	tty_driver.major = TTY_MAJOR;
	tty_driver.minor_start = 64;
	tty_driver.num = 1;
	tty_driver.type = TTY_DRIVER_TYPE_SYSCONS;

	tty_driver.refcount = &tty_ref_count;

	tty_driver.table = tty_table;
	tty_driver.termios = tty_termios;
	tty_driver.termios_locked = tty_termios_locked;

	tty_driver.init_termios = tty_std_termios;

	tty_driver.open = memcons_tty_open;
	tty_driver.write = memcons_tty_write;
	tty_driver.write_room = memcons_tty_write_room;
	tty_driver.chars_in_buffer = memcons_tty_chars_in_buffer;

	tty_register_driver (&tty_driver);

	memcons_irqinit();
	
}
__initcall (memcons_tty_init);
