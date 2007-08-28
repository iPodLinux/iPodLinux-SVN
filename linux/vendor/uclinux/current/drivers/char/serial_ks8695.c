/****************************************************************************/

/*
 *	ksserial.c -- basic UART driver for KS8695 UART
 *
 *	(C) Copyright 2003, Greg Ungerer <gerg@snapgear.com>
 *
 *	This driver is loosely based on the serial_ks8695.c driver
 *	by Kam Lee/Micrel. But this code is intended to use the old
 *	serial driver framework.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/****************************************************************************/

#include <linux/config.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/console.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/tty_driver.h>
#include <linux/serial.h>
#include <linux/generic_serial.h>
#include <linux/interrupt.h>
#include <asm/hardware.h>
#include <asm/uaccess.h>

#include "serial_ks8695.h"

//#define DEBUG	1

/****************************************************************************/

#define	KS8695_SERIAL_VERSION	"0.1"

/*
 *	Base address of the UART (this is a virtual address).
 *	(Also the interrupt controller, since we need that too).
 */
#define	KS8695_UART_ADDR	(IO_ADDRESS(KS8695_IO_BASE) + KS8695_UART_RX_BUFFER)
#define	KS8695_INTCTRL_ADDR	(IO_ADDRESS(KS8695_IO_BASE) + KS8695_INT_CONTL)
#define	KS8695_UART_CLK		25000000
#define	KS8695_UART_FIFOSIZE	16

#define	KS8695_UART_NUM		1

/****************************************************************************/

/*
 *	Local driver magic number...
 */
#define	KS8695_UART_MAGIC	0xbeefcafe

/*
 *	Major number to use. We use the AMBA defined number, to be compatible
 *	with the Micrel driver.
 */
#ifndef SERIAL_AMBA_MAJOR
#define	SERIAL_AMBA_MAJOR	204
#define	SERIAL_AMBA_MINOR	16
#define	CALLOUT_AMBA_MAJOR	205
#define	CALLOUT_AMBA_MINOR	16
#endif

#ifndef RS_EVENT_WRITE_WAKEUP
#define RS_EVENT_WRITE_WAKEUP	1
#endif

/****************************************************************************/

/*
 *	Driver data structures.
 */
static struct tty_driver ks8695_driver;
static struct tty_driver ks8695_callout_driver;

static struct tty_struct *ks8695_tty_table[KS8695_UART_NUM];
static struct termios *ks8695_termios[KS8695_UART_NUM];
static struct termios *ks8695_termios_locked[KS8695_UART_NUM];

static int ks8695_refcount;

/****************************************************************************/

/*
 *	The per-port tty structure.
 */

struct ks8695_uart_tty_port {
	struct gs_port		gs;
	struct async_icount	icount;
	unsigned int		chan;
	unsigned int		base;
	void			*uartp;
	void 			*intp;
	unsigned int		irq;
};

static struct ks8695_uart_tty_port ks8695_table[KS8695_UART_NUM];

/****************************************************************************/

static void ks8695_setsignals(void *driver_data, int dtr, int rts)
{
	struct ks8695_uart_tty_port *port = driver_data;
	struct ksuart volatile *uartp;
	unsigned int mcr;

#if DEBUG
	printk("ks8695_setsignals(dtr=%d,rts=%d)\n", dtr, rts);
#endif

	uartp = port->uartp;
	mcr = uartp->MCR;
	if (dtr)
		mcr |= KS8695_UART_MODEMC_DTR;
	else
		mcr &= ~KS8695_UART_MODEMC_DTR;
	if (rts)
		mcr |= KS8695_UART_MODEMC_RTS;
	else
		mcr &= ~KS8695_UART_MODEMC_RTS;
	uartp->MCR = mcr;
}

/****************************************************************************/

static void ks8695_enable_tx_interrupts(void *driver_data)
{
	struct ks8695_uart_tty_port *port = driver_data;
	struct ksintctrl volatile *intp;
	unsigned long flags;

#if DEBUG
	printk("ks8695_enable_tx_interrupts()\n");
#endif

	intp = port->intp;
	save_flags(flags); cli();
	intp->ENABLE |= KS8695_INT_ENABLE_TX;
	restore_flags(flags);
}

/****************************************************************************/

static void ks8695_disable_tx_interrupts(void *driver_data)
{
	struct ks8695_uart_tty_port *port = driver_data;
	struct ksintctrl volatile *intp;
	unsigned long flags;

#if DEBUG
	printk("ks8695_disable_tx_interrupts()\n");
#endif

	intp = port->intp;
	save_flags(flags); cli();
	intp->ENABLE &= ~KS8695_INT_ENABLE_TX;
	restore_flags(flags);
}

/****************************************************************************/

static void ks8695_enable_rx_interrupts(void *driver_data)
{
	struct ks8695_uart_tty_port *port = driver_data;
	struct ksintctrl volatile *intp;
	unsigned long flags;

#if DEBUG
	printk("ks8695_enable_rx_interrupts()\n");
#endif

	intp = port->intp;
	save_flags(flags); cli();
	intp->ENABLE |= KS8695_INT_ENABLE_RX | KS8695_INT_ENABLE_ERR;
	restore_flags(flags);
}

/****************************************************************************/

static void ks8695_disable_rx_interrupts(void *driver_data)
{
	struct ks8695_uart_tty_port *port = driver_data;
	struct ksintctrl volatile *intp;
	unsigned long flags;

#if DEBUG
	printk("ks8695_disable_rx_interrupts()\n");
#endif

	intp = port->intp;
	save_flags(flags); cli();
	intp->ENABLE &= ~(KS8695_INT_ENABLE_RX | KS8695_INT_ENABLE_ERR);
	restore_flags(flags);
}

/****************************************************************************/

static void ks8695_enable_modem_interrupts(void *driver_data)
{
	struct ks8695_uart_tty_port *port = driver_data;
	struct ksintctrl volatile *intp;
	unsigned long flags;

#if DEBUG
	printk("ks8695_enable_modem_interrupts()\n");
#endif

	intp = port->intp;
	save_flags(flags); cli();
	intp->ENABLE |= KS8695_INT_ENABLE_MODEM;
	restore_flags(flags);
}

/****************************************************************************/

static void ks8695_disable_modem_interrupts(void *driver_data)
{
	struct ks8695_uart_tty_port *port = driver_data;
	struct ksintctrl volatile *intp;
	unsigned long flags;

#if DEBUG
	printk("ks8695_disable_modem_interrupts()\n");
#endif

	intp = port->intp;
	save_flags(flags); cli();
	intp->ENABLE &= ~KS8695_INT_ENABLE_MODEM;
	restore_flags(flags);
}

/****************************************************************************/

static int ks8695_get_CD(void *driver_data)
{
	struct ks8695_uart_tty_port *port = driver_data;
	struct ksuart volatile *uartp;

#if DEBUG
	printk("ks8695_get_CD()\n");
#endif

	uartp = port->uartp;
	return ((uartp->MSR & KS8695_UART_MODEM_DCD) ? 1 : 0);
}

/****************************************************************************/

static void ks8695_shutdown_port(void *driver_data)
{
	struct ks8695_uart_tty_port *port = driver_data;

#if DEBUG
	printk("ks8695_shutdown_port()\n");
#endif

	port->gs.flags &= ~GS_ACTIVE;
	ks8695_disable_modem_interrupts(port);
	if (port->gs.tty && (port->gs.tty->termios->c_cflag & HUPCL))
		ks8695_setsignals(port, 0, 0);
}

/****************************************************************************/

static int ks8695_set_real_termios(void *driver_data)
{
	struct ks8695_uart_tty_port *port = driver_data;
	struct ksuart volatile *uartp;
	unsigned long flags;
	unsigned int cflag;
	unsigned int lcr = 0, fcr = 0;

#if DEBUG
	printk("ks8695_set_real_termios()\n");
#endif

	cflag = port->gs.tty->termios->c_cflag;

	switch (cflag & CSIZE) {
	case CS5:	lcr |= KS8695_UART_LINEC_WLEN5; break;
	case CS6:	lcr |= KS8695_UART_LINEC_WLEN6; break;
	case CS7:	lcr |= KS8695_UART_LINEC_WLEN7; break;
	default:	lcr |= KS8695_UART_LINEC_WLEN8; break;
	}

	if (cflag & PARENB) {
		if (cflag & PARODD)
			lcr |= KS8695_UART_LINEC_PEN;
		else
			lcr |= KS8695_UART_LINEC_PEN | KS8695_UART_LINEC_EPS;
	}

	if (cflag & CSTOPB)
		lcr |= KS8695_UART_LINEC_STP2;

	fcr = KS8695_UART_FIFO_TRIG04 | KS8695_UART_FIFO_TXRST |
		KS8695_UART_FIFO_RXRST | KS8695_UART_FIFO_FEN;

	uartp = port->uartp;

	save_flags(flags); cli();
	uartp->BD = (KS8695_UART_CLK / port->gs.baud);
	uartp->FCR = fcr;
	uartp->LCR = lcr;
	restore_flags(flags);
	return 0;
}

/****************************************************************************/

static int ks8695_chars_in_buffer(void *driver_data)
{
	struct ks8695_uart_tty_port *port = driver_data;
	struct ksuart volatile *uartp;

#if DEBUG
	printk("ks8695_chars_in_buffer()\n");
#endif

	uartp = port->uartp;
	return ((uartp->LSR & KS8695_UART_LINES_TXFE) ? 0 : 1);
}

/****************************************************************************/

static void ks8695_close(void *driver_data)
{
#if DEBUG
	printk("ks8695_close()\n");
#endif
}

/****************************************************************************/

static void ks8695_hungup(void *driver_data)
{
#if DEBUG
	printk("ks8695_hungup()\n");
#endif
}

/****************************************************************************/

static void ks8695_getserial(void *driver_data, struct serial_struct *s)
{
	struct ks8695_uart_tty_port *port = driver_data;

#if DEBUG
	printk("ks8695_getserial()\n");
#endif

	s->line = port->chan;
	s->xmit_fifo_size = KS8695_UART_FIFOSIZE;
	s->port = (unsigned int) port->uartp;
	s->irq = port->irq;
}

/****************************************************************************/

static int ks8695_open(struct tty_struct *tty, struct file *filp)
{
	struct ks8695_uart_tty_port *port;
	unsigned int chan;
	int rc;

#if DEBUG
	printk("ks8695_open()\n");
#endif

	chan = MINOR(tty->device) - SERIAL_AMBA_MINOR;
	if (chan >= KS8695_UART_NUM)
		return -ENODEV;

	port = &ks8695_table[chan];
	tty->driver_data = port;
	port->gs.tty = tty;
	port->gs.count++;

	rc = gs_init_port(&port->gs);
	if (rc) {
		port->gs.count--;
		return rc;
	}

	port->gs.flags |= GS_ACTIVE;
	ks8695_setsignals(port, 1, 1);
	ks8695_enable_rx_interrupts(port);
	ks8695_enable_modem_interrupts(port);

	rc = gs_block_til_ready(&port->gs, filp);
	if (rc) {
		port->gs.count--;
		return rc;
	}

	if ((port->gs.count == 1) && (port->gs.flags & ASYNC_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = port->gs.normal_termios;
		else
			*tty->termios = port->gs.callout_termios;
		ks8695_set_real_termios(port);
	}

	port->gs.session = current->session;
	port->gs.pgrp = current->pgrp;
	return 0;
}

/****************************************************************************/

static int ks8695_ioctl(struct tty_struct *tty, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct ks8695_uart_tty_port *port = tty->driver_data;
	int rc = 0;

#if DEBUG
	printk("ks8695_ioctl(cmd=%x,arg=%x)\n", (int)cmd, (int)arg);
#endif

	switch (cmd) {

	case TIOCGSOFTCAR:
		rc = put_user((tty->termios->c_cflag & CLOCAL) ? 1 : 0,
	              (unsigned int *) arg);
		break;

	case TIOCSSOFTCAR:
		if ((rc = verify_area(VERIFY_READ, (void *) arg, sizeof(int))) == 0) {
			int ival;
			get_user(ival, (unsigned int *) arg);
			tty->termios->c_cflag =
				(tty->termios->c_cflag & ~CLOCAL) |
				(ival ? CLOCAL : 0);
		}
		break;

	case TIOCGSERIAL:
		if ((rc = verify_area(VERIFY_WRITE, (void *) arg, sizeof(struct serial_struct))) == 0)
			rc = gs_getserial(&port->gs, (struct serial_struct *) arg);
		break;

	case TIOCSSERIAL:
		if ((rc = verify_area(VERIFY_READ, (void *) arg, sizeof(struct serial_struct))) == 0)
			rc = gs_setserial(&port->gs, (struct serial_struct *) arg);
		break;

	default:
		rc = -ENOIOCTLCMD;
		break;
	}

	return rc;
}

/****************************************************************************/

static int ks8695_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct ks8695_uart_tty_port *port;
	int len = 0;
	int i;

	len += sprintf(page, "KS8659 Serial driver (version %s):\n",
		KS8695_SERIAL_VERSION);

	for (i = 0; (i < KS8695_UART_NUM); i++) {
		port = &ks8695_table[i];
		len += sprintf(page+len, "%d: uart:%s port:%x irq:%x",
			port->chan, "KS8695", (int)port->uartp, port->irq);
		len += sprintf(page+len, " baud:%d tx:%d rx:%d ",
			port->gs.baud, port->icount.tx, port->icount.rx);

		if (port->icount.frame)
			len += sprintf(page+len," fe:%d", port->icount.frame);
		if (port->icount.parity)
			len += sprintf(page+len," pe:%d", port->icount.parity);
		if (port->icount.brk)
			len += sprintf(page+len," brk:%d", port->icount.brk);
		if (port->icount.overrun)
			len += sprintf(page+len," oe:%d", port->icount.overrun);

		len += sprintf(page+len, "\n");
	}

	return len;
}

/****************************************************************************/

static void ks8695_intr_tx(int irq, void *dev_id, struct pt_regs *regs)
{
	struct ks8695_uart_tty_port *port = dev_id;
	struct ksuart volatile *uartp;
	struct ksintctrl volatile *intp;
	unsigned int ier;
	int i;

#if DEBUG
	printk("ks8695_intr_tx(irq=%d)\n", irq);
#endif


	intp = port->intp;
	ier = intp->ENABLE;
	intp->ENABLE &= ~KS8695_INTMASK_UART_TX;

#if 1
	if ((port->gs.flags & GS_ACTIVE) == 0) {
		printk("%s(%d): TX interrupt, not active??\n", __FILE__, __LINE__);
		ks8695_disable_tx_interrupts(port);
		return;
	}
#endif

	uartp = port->uartp;

	for (i = 0; (i < KS8695_UART_FIFOSIZE); i++) {
		if (port->gs.xmit_cnt == 0)
			break;
		intp->STATUS = KS8695_INTMASK_UART_TX;
		uartp->TX = port->gs.xmit_buf[port->gs.xmit_tail];
		port->gs.xmit_cnt--;
		port->gs.xmit_tail = (port->gs.xmit_tail + 1) & (SERIAL_XMIT_SIZE-1);
		port->icount.tx++;
	}

	if (port->gs.xmit_cnt <= port->gs.wakeup_chars) {
		if ((port->gs.tty->flags & (1<<TTY_DO_WRITE_WAKEUP)) &&
		    port->gs.tty->ldisc.write_wakeup)
			(port->gs.tty->ldisc.write_wakeup)(port->gs.tty);
		wake_up_interruptible(&port->gs.tty->write_wait);
	}

	if (port->gs.xmit_cnt == 0) {
		port->gs.flags &= ~GS_TX_INTEN;
		ier &= ~KS8695_INTMASK_UART_TX;
#if 1
	} else {
		/*
		 * FIXME: bug in here somehwere. We shouldn't need to keep
		 * re-enabling the TX interrupt here...
		 */
		ier |= KS8695_INTMASK_UART_TX;
#endif
	}

	intp->ENABLE = ier;
}

/****************************************************************************/

static void ks8695_intr_rx(int irq, void *dev_id, struct pt_regs *regs)
{
	struct ks8695_uart_tty_port *port = dev_id;
	struct ksuart volatile *uartp;
	unsigned int lsr;
	unsigned char status, c;

#if DEBUG
	printk("ks8695_intr_rx(irq=%d)\n", irq);
#endif

	uartp = port->uartp;

	while ((lsr = uartp->LSR) & KS8695_UART_LINES_RXFE) {
		c = uartp->RX;
		if (lsr & KS8695_UART_LINES_OE) {
			status = TTY_OVERRUN;
			port->icount.overrun++;
		} else if (lsr & KS8695_UART_LINES_FE) {
			status = TTY_FRAME;
			port->icount.frame++;
		} else if (lsr & KS8695_UART_LINES_PE) {
			status = TTY_PARITY;
			port->icount.parity++;
		} else if (lsr & KS8695_UART_LINES_BE) {
			status = TTY_BREAK;
			port->icount.brk++;
		} else {
			status = TTY_NORMAL;
			port->icount.rx++;
		}

		tty_insert_flip_char(port->gs.tty, c, status);
	}

	tty_schedule_flip(port->gs.tty);
}

/****************************************************************************/

static void ks8695_intr_modem(int irq, void *dev_id, struct pt_regs *regs)
{
	struct ks8695_uart_tty_port *port = dev_id;
	struct ksuart volatile *uartp;
	unsigned int msr;

#if DEBUG
	printk("ks8695_intr_modem(irq=%d)\n", irq);
#endif

	uartp = port->uartp;
	msr = uartp->MSR;

	if (msr & KS8695_UART_MODEM_DDCD) {
		if (port->gs.flags & ASYNC_CHECK_CD) {
			if (ks8695_get_CD(port)) {
				if (~(port->gs.flags & ASYNC_NORMAL_ACTIVE) ||
				    ~(port->gs.flags & ASYNC_CALLOUT_ACTIVE)) 
					wake_up_interruptible(&port->gs.open_wait);
			} else {
				if (!((port->gs.flags & ASYNC_CALLOUT_ACTIVE) &&
				    (port->gs.flags & ASYNC_CALLOUT_NOHUP)))
					if (port->gs.tty)
						tty_hangup(port->gs.tty);
			}
		}
	}

	if (msr & KS8695_UART_MODEM_DDSR)
		/* FIXME */;
	if (msr & KS8695_UART_MODEM_DCTS)
		/* FIXME */;
}

/****************************************************************************/

static struct real_driver ks8695_uart_driver = {
	.enable_tx_interrupts = ks8695_enable_tx_interrupts,
	.disable_tx_interrupts = ks8695_disable_tx_interrupts,
	.enable_rx_interrupts = ks8695_enable_rx_interrupts,
	.disable_rx_interrupts = ks8695_disable_rx_interrupts,
	.get_CD = ks8695_get_CD,
	.shutdown_port = ks8695_shutdown_port,
	.set_real_termios = ks8695_set_real_termios,
	.chars_in_buffer = ks8695_chars_in_buffer,
	.close = ks8695_close,
	.hungup = ks8695_hungup,
	.getserial = ks8695_getserial,
};

/****************************************************************************/

static void __init ks8695_init_ports(void)
{
	struct ks8695_uart_tty_port *port;
	int i;

	for (i = 0; (i < KS8695_UART_NUM); i++) {
		port = &ks8695_table[i];
		port->chan = i;
		port->base = KS8695_UART_ADDR;
		port->uartp = (void *) KS8695_UART_ADDR;
		port->intp = (void *) KS8695_INTCTRL_ADDR;
		port->irq = KS8695_INT_UART_RX;
		port->gs.magic = KS8695_UART_MAGIC;
		port->gs.rd = &ks8695_uart_driver;
		port->gs.close_delay = HZ / 2;
		port->gs.closing_wait = 30 * HZ;
		port->gs.normal_termios = ks8695_driver.init_termios;
		port->gs.callout_termios = ks8695_callout_driver.init_termios;
		memset(&port->icount, 0, sizeof(port->icount));
		printk("ttyS%d at 0x%p is builtin UART\n", i, port->uartp);

		if (request_irq(KS8695_INT_UART_TX, ks8695_intr_tx,
		    SA_SHIRQ, "ks8695 uart TX", port)) {
			printk("KS8695_SERIAL: fail to get %d interrupt\n",
				KS8695_INT_UART_TX);
		}
		if (request_irq(KS8695_INT_UART_RX, ks8695_intr_rx,
		    SA_SHIRQ, "ks8695 uart RX", port)) {
			printk("KS8695_SERIAL: fail to get %d interrupt\n",
				KS8695_INT_UART_RX);
		}
		if (request_irq(KS8695_INT_UART_LINE_ERR, ks8695_intr_rx,
		    SA_SHIRQ, "ks8695 uart error", port)) {
			printk("KS8695_SERIAL: fail to get %d interrupt\n",
				KS8695_INT_UART_LINE_ERR);
		}
		if (request_irq(KS8695_INT_UART_MODEMS, ks8695_intr_modem,
		    SA_SHIRQ, "ks8695 uart modem", port)) {
			printk("KS8695_SERIAL: fail to get %d interrupt\n",
				KS8695_INT_UART_MODEMS);
		}
	}
}

/****************************************************************************/

static int __init ks8695_init(void)
{
	printk("KS8695_SERIAL: Kendin/Micrel KS8695 serial driver\n");

	memset(&ks8695_driver, 0, sizeof(ks8695_driver));
	ks8695_driver.magic = TTY_DRIVER_MAGIC;
	ks8695_driver.driver_name = "serial_ks8695";
	ks8695_driver.name = "ttyAM";
	ks8695_driver.major = SERIAL_AMBA_MAJOR;
	ks8695_driver.minor_start = SERIAL_AMBA_MINOR;
	ks8695_driver.num = KS8695_UART_NUM;
	ks8695_driver.type = TTY_DRIVER_TYPE_SERIAL;
	ks8695_driver.subtype = SERIAL_TYPE_NORMAL;
	ks8695_driver.init_termios = tty_std_termios;
	ks8695_driver.init_termios.c_cflag = 
		B115200 | CS8 | CREAD | HUPCL | CLOCAL;
	ks8695_driver.flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS;
	ks8695_driver.refcount = &ks8695_refcount;
	ks8695_driver.table = ks8695_tty_table;
	ks8695_driver.termios = ks8695_termios;
	ks8695_driver.termios_locked = ks8695_termios_locked;

	ks8695_driver.open = ks8695_open;
	ks8695_driver.close = gs_close;
	ks8695_driver.write = gs_write;
	ks8695_driver.put_char = gs_put_char;
	ks8695_driver.flush_chars = gs_flush_chars;
	ks8695_driver.write_room = gs_write_room;
	ks8695_driver.chars_in_buffer = gs_chars_in_buffer;
	ks8695_driver.flush_buffer = gs_flush_buffer;
	ks8695_driver.ioctl = ks8695_ioctl;
	//ks8695_driver.throttle = ks8695_throttle;
	//ks8695_driver.unthrottle = ks8695_unthrottle;
	ks8695_driver.set_termios = gs_set_termios;
	ks8695_driver.stop = gs_stop;
	ks8695_driver.start = gs_start;
	ks8695_driver.hangup = gs_hangup;
	//ks8695_driver.break_ctl = ks8695_break;
	//ks8695_driver.send_xchar = ks8695_send_xchar;
	//ks8695_driver.wait_until_sent = ks8695_wait_until_sent;
	ks8695_driver.read_proc = ks8695_read_proc;

	ks8695_callout_driver = ks8695_driver;
	ks8695_callout_driver.name = "cuaam";
	ks8695_callout_driver.major = CALLOUT_AMBA_MAJOR;
	ks8695_callout_driver.subtype = SERIAL_TYPE_CALLOUT;
	ks8695_callout_driver.read_proc = 0;

	ks8695_init_ports();

	if (tty_register_driver(&ks8695_driver))
		panic("KJS8695_SERIAL: couldn't register serial driver\n");
	if (tty_register_driver(&ks8695_callout_driver))
		panic("KJS8695_SERIAL: couldn't register callout driver\n");

	return 0;
}

/****************************************************************************/
/*                           SERIAL CONSOLE                                 */
/****************************************************************************/

static kdev_t ks8695_console_device(struct console *c)
{
        return MKDEV(SERIAL_AMBA_MAJOR, SERIAL_AMBA_MINOR + c->index);
}

/****************************************************************************/

static int __init ks8695_console_setup(struct console *co, char *options)
{
	struct ksuart volatile *uartp = (void *) KS8695_UART_ADDR;
	unsigned int cflag = CREAD | HUPCL | CLOCAL;
	unsigned int lcr = 0;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int doflow = 0;
	char *s;

	if (options) {
		baud = simple_strtoul(options, NULL, 10);
		for (s = options; ((*s >= '0') && (*s <= '9')); s++)
			;
		if (*s)
			parity = *s++;
		if (*s)
			bits = *s++ - '0';
		if (*s)
			doflow = (*s++ == 'r');
	}

	switch (baud) {
	case 300:
		cflag |= B300;
		break;
	case 1200:
		cflag |= B1200;
		break;
	case 2400:
		cflag |= B2400;
		break;
	case 4800:
		cflag |= B4800;
		break;
	case 9600:
		cflag |= B9600;
		break;
	case 19200:
		cflag |= B19200;
		break;
	case 38400:
		cflag |= B38400;
		break;
	case 57600:
		cflag |= B57600;
		break;
	case 230400:
		cflag |= B230400;
		break;
	default:
		cflag |= B115200;
		baud = 115200;
		break;
	}

	switch (bits) {
	case 7:
		cflag |= CS7;
		lcr |= KS8695_UART_LINEC_WLEN7;
		break;
	default:
		cflag |= CS8;
		lcr |= KS8695_UART_LINEC_WLEN8;
		bits = 8;
		break;
	}

	switch (parity) {
	case 'o':
		cflag |= PARENB | PARODD;
		lcr |= KS8695_UART_LINEC_PEN;
		break;
	case 'e':
		cflag |= PARENB;
		lcr |= KS8695_UART_LINEC_PEN | KS8695_UART_LINEC_EPS;
		break;
	}

	co->cflag = cflag;
	uartp->BD = (KS8695_UART_CLK / baud);
	uartp->LCR = lcr;
	return 0;
}

/****************************************************************************/

static void ks8695_console_writechar(struct console *co, const char c)
{
	struct ksuart volatile *uartp = (void *) KS8695_UART_ADDR;
	int i;

	for (i = 0; (i < 0x10000); i++) {
		if (uartp->LSR & KS8695_UART_LINES_TXFE)
			break;
	}
	
	uartp->TX = c;
	
	for (i = 0; (i < 0x10000); i++) {
		if (uartp->LSR & KS8695_UART_LINES_TXFE)
			break;
	}
}

/****************************************************************************/

static void ks8695_console_write(struct console *co, const char *s, unsigned int count)
{
	for (; (count > 0); count--)
                ks8695_console_writechar(co, *s++);
}

/****************************************************************************/

static struct console ks8695_console = {
	.name = "ttyAM",
	.write = ks8695_console_write,
	.device = ks8695_console_device,
	.setup = ks8695_console_setup,
	.flags = CON_PRINTBUFFER,
	.index = -1,
};

void __init ks8695_console_init(void)
{
	register_console(&ks8695_console);
}

/****************************************************************************/

module_init(ks8695_init);
MODULE_DESCRIPTION("Kendin/Micrel serial port driver");
MODULE_AUTHOR("Greg Ungerer <gerg@snapgear.com>");
MODULE_LICENSE("GPL");

/****************************************************************************/
