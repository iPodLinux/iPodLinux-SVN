/*
 * drivers/char/v850e_uart.c -- Serial I/O using V850E on-chip UART or UARTB
 *
 *  Copyright (C) 2001,02,03  NEC Electronics Corporation
 *  Copyright (C) 2001,02,03  Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Written by Miles Bader <miles@gnu.org>
 */

/* This driver supports both the original V850E UART interface (called
   merely `UART' in the docs) and the newer `UARTB' interface, which is
   roughly a superset of the first one.  The selection is made at
   configure time -- if CONFIG_V850E_UARTB is defined, then UARTB is
   presumed, otherwise the old UART -- as these are on-CPU UARTS, a system
   can never have both.

   The UARTB interface also has a 16-entry FIFO mode, which is not
   yet supported by this driver.  */

#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/tty_driver.h>
#include <linux/serial.h>
#include <linux/generic_serial.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/tqueue.h>
#include <linux/interrupt.h>

#include <asm/v850e_uart.h>


/* Initial UART state.  This may be overridden by machine-dependent headers. */
#ifndef V850E_UART_INIT_BAUD
#define V850E_UART_INIT_BAUD	115200
#endif
#ifndef V850E_UART_INIT_CFLAGS
#define V850E_UART_INIT_CFLAGS	(B115200 | CS8 | CREAD)
#endif

/* A string used for prefixing printed descriptions; since the same UART
   macro is actually used on other chips than the V850E.  This must be a
   constant string.  */
#ifndef V850E_UART_CHIP_NAME
#define V850E_UART_CHIP_NAME	"V850E"
#endif


#define V850E_UART_MAGIC	0xFABCAB22 /* Used in generic_serial header */
#define V850E_UART_MINOR_BASE	64	   /* First tty minor number */


#define RS_EVENT_WRITE_WAKEUP	1 /* from generic_serial.h */

/* For use by modules eventually...  */
#define MOD_INC_USE_COUNT
#define MOD_DEC_USE_COUNT


/* Low-level UART functions.  */

/* Configure and turn on uart channel CHAN, using the termios `control
   modes' bits in CFLAGS, and a baud-rate of BAUD.  */
void v850e_uart_configure (unsigned chan, unsigned cflags, unsigned baud)
{
	int flags;
	v850e_uart_speed_t old_speed;
	v850e_uart_config_t old_config;
	v850e_uart_speed_t new_speed = v850e_uart_calc_speed (baud);
	v850e_uart_config_t new_config = v850e_uart_calc_config (cflags);

	/* Disable interrupts while we're twiddling the hardware.  */
	save_flags_cli (flags);

#ifdef V850E_UART_PRE_CONFIGURE
	V850E_UART_PRE_CONFIGURE (chan, cflags, baud);
#endif

	old_config = V850E_UART_CONFIG (chan);
	old_speed = v850e_uart_speed (chan);

	if (! v850e_uart_speed_eq (old_speed, new_speed)) {
		/* The baud rate has changed.  First, disable the UART.  */
		V850E_UART_CONFIG (chan) = V850E_UART_CONFIG_FINI;
		old_config = 0;	/* Force the uart to be re-initialized. */

		/* Reprogram the baud-rate generator.  */
		v850e_uart_set_speed (chan, new_speed);
	}

	if (! (old_config & V850E_UART_CONFIG_ENABLED)) {
		/* If we are using the uart for the first time, start by
		   enabling it, which must be done before turning on any
		   other bits.  */
		V850E_UART_CONFIG (chan) = V850E_UART_CONFIG_INIT;
		/* See the initial state.  */
		old_config = V850E_UART_CONFIG (chan);
	}

	if (new_config != old_config) {
		/* Which of the TXE/RXE bits we'll temporarily turn off
		   before changing other control bits.  */
		unsigned temp_disable = 0;
		/* Which of the TXE/RXE bits will be enabled.  */
		unsigned enable = 0;
		unsigned changed_bits = new_config ^ old_config;

		/* Which of RX/TX will be enabled in the new configuration.  */
		if (new_config & V850E_UART_CONFIG_RX_BITS)
			enable |= (new_config & V850E_UART_CONFIG_RX_ENABLE);
		if (new_config & V850E_UART_CONFIG_TX_BITS)
			enable |= (new_config & V850E_UART_CONFIG_TX_ENABLE);

		/* Figure out which of RX/TX needs to be disabled; note
		   that this will only happen if they're not already
		   disabled.  */
		if (changed_bits & V850E_UART_CONFIG_RX_BITS)
			temp_disable
				|= (old_config & V850E_UART_CONFIG_RX_ENABLE);
		if (changed_bits & V850E_UART_CONFIG_TX_BITS)
			temp_disable
				|= (old_config & V850E_UART_CONFIG_TX_ENABLE);

		/* We have to turn off RX and/or TX mode before changing
		   any associated control bits.  */
		if (temp_disable)
			V850E_UART_CONFIG (chan) = old_config & ~temp_disable;

		/* Write the new control bits, while RX/TX are disabled. */ 
		if (changed_bits & ~enable)
			V850E_UART_CONFIG (chan) = new_config & ~enable;

		v850e_uart_config_delay (new_config, new_speed);

		/* Write the final version, with enable bits turned on.  */
		V850E_UART_CONFIG (chan) = new_config;
	}

	restore_flags (flags);
}


/*  Low-level console. */

static void v850e_uart_cons_write (struct console *co,
				   const char *s, unsigned count)
{
	if (count > 0) {
		unsigned chan = co->index;
		unsigned irq = V850E_UART_TX_IRQ (chan);
		int irq_was_enabled, irq_was_pending, flags;

		/* We don't want to get `transmission completed'
		   interrupts, since we're busy-waiting, so we disable them
		   while sending (we don't disable interrupts entirely
		   because sending over a serial line is really slow).  We
		   save the status of the tx interrupt and restore it when
		   we're done so that using printk doesn't interfere with
		   normal serial transmission (other than interleaving the
		   output, of course!).  This should work correctly even if
		   this function is interrupted and the interrupt printks
		   something.  */

		/* Disable interrupts while fiddling with tx interrupt.  */
		save_flags_cli (flags);
		/* Get current tx interrupt status.  */
		irq_was_enabled = v850e_intc_irq_enabled (irq);
		irq_was_pending = v850e_intc_irq_pending (irq);
		/* Disable tx interrupt if necessary.  */
		if (irq_was_enabled)
			v850e_intc_disable_irq (irq);
		/* Turn interrupts back on.  */
		restore_flags (flags);

		/* Send characters.  */
		while (count > 0) {
			int ch = *s++;

			if (ch == '\n') {
				/* We don't have the benefit of a tty
				   driver, so translate NL into CR LF.  */
				v850e_uart_wait_for_xmit_ok (chan);
				v850e_uart_putc (chan, '\r');
			}

			v850e_uart_wait_for_xmit_ok (chan);
			v850e_uart_putc (chan, ch);

			count--;
		}

		/* Restore saved tx interrupt status.  */
		if (irq_was_enabled) {
			/* Wait for the last character we sent to be
			   completely transmitted (as we'll get an
			   interrupt interrupt at that point).  */
			v850e_uart_wait_for_xmit_done (chan);
			/* Clear pending interrupts received due
			   to our transmission, unless there was already
			   one pending, in which case we want the
			   handler to be called.  */
			if (! irq_was_pending)
				v850e_intc_clear_pending_irq (irq);
			/* ... and then turn back on handling.  */
			v850e_intc_enable_irq (irq);
		}
	}
}

static kdev_t v850e_uart_cons_device (struct console *c)
{
        return MKDEV (TTY_MAJOR, V850E_UART_MINOR_BASE + c->index);
}

static struct console v850e_uart_cons =
{
    name:	"ttyS",
    write:	v850e_uart_cons_write,
    device:	v850e_uart_cons_device,
    flags:	CON_PRINTBUFFER,
    index:	-1,
};

void v850e_uart_cons_init (unsigned chan)
{
	v850e_uart_configure (chan, V850E_UART_INIT_CFLAGS,
			      V850E_UART_INIT_BAUD);
	v850e_uart_cons.index = chan;
	register_console (&v850e_uart_cons);
	printk ("Console: %s on-chip UART channel %d\n",
		V850E_UART_CHIP_NAME, chan);
}

/* This is what the init code actually calls.  */
void v850e_uart_console_init (void)
{
	v850e_uart_cons_init (V850E_UART_CONSOLE_CHANNEL);
}


/* Interface for generic serial driver layer.  */

struct v850e_uart_tty_port {
	struct gs_port gs;
	unsigned chan;
	struct tq_struct tqueue;
};

/* Transmit a character, if any are pending.  */
void v850e_uart_tty_tx (struct v850e_uart_tty_port *port)
{
	unsigned chan = port->chan;
	int flags;

	/* If there are characters to transmit, try to transmit one of them. */
	if (port->gs.xmit_cnt > 0 && v850e_uart_xmit_ok (port->chan)) {
		v850e_uart_putc (chan, port->gs.xmit_buf[port->gs.xmit_tail]);
		port->gs.xmit_tail
			= (port->gs.xmit_tail + 1) & (SERIAL_XMIT_SIZE - 1);
		port->gs.xmit_cnt--;

		if (port->gs.xmit_cnt <= port->gs.wakeup_chars) {
			port->gs.event |= 1 << RS_EVENT_WRITE_WAKEUP;
			queue_task (&port->tqueue, &tq_immediate);
			mark_bh (IMMEDIATE_BH);
		}
	}

	save_flags_cli (flags);
	if (port->gs.xmit_cnt == 0)
		port->gs.flags &= ~GS_TX_INTEN;
	restore_flags (flags);
}

static void v850e_uart_tty_disable_tx_interrupts (void *driver_data)
{
	struct v850e_uart_tty_port *port = driver_data;
	v850e_intc_disable_irq (V850E_UART_TX_IRQ (port->chan));
}

static void v850e_uart_tty_enable_tx_interrupts (void *driver_data)
{
	struct v850e_uart_tty_port *port = driver_data;
	v850e_intc_disable_irq (V850E_UART_TX_IRQ (port->chan));
	v850e_uart_tty_tx (port);
	v850e_intc_enable_irq (V850E_UART_TX_IRQ (port->chan));
}

static void v850e_uart_tty_disable_rx_interrupts (void *driver_data)
{
	struct v850e_uart_tty_port *port = driver_data;
	v850e_intc_disable_irq (V850E_UART_RX_IRQ (port->chan));
}

static void v850e_uart_tty_enable_rx_interrupts (void *driver_data)
{
	struct v850e_uart_tty_port *port = driver_data;
	v850e_intc_enable_irq (V850E_UART_RX_IRQ (port->chan));
}

static int v850e_uart_tty_get_CD (void *driver_data)
{
	return 1;		/* Can't really detect it, sorry... */
}

static void v850e_uart_tty_shutdown_port (void *driver_data)
{
	struct v850e_uart_tty_port *port = driver_data;

	port->gs.flags &= ~ GS_ACTIVE;

	/* Disable port interrupts.  */
	free_irq (V850E_UART_TX_IRQ (port->chan), port);
	free_irq (V850E_UART_RX_IRQ (port->chan), port);

	/* Turn off xmit/recv enable bits.  */
	V850E_UART_CONFIG (port->chan)
		&= ~(V850E_UART_CONFIG_RX_ENABLE
		     | V850E_UART_CONFIG_TX_ENABLE);
	/* Then reset the channel.  */
	V850E_UART_CONFIG (port->chan) = V850E_UART_CONFIG_FINI;
}

static int v850e_uart_tty_set_real_termios (void *driver_data)
{
	struct v850e_uart_tty_port *port = driver_data;
	unsigned cflag = port->gs.tty->termios->c_cflag;
	v850e_uart_configure (port->chan, cflag, port->gs.baud);
}

static int v850e_uart_tty_chars_in_buffer (void *driver_data)
{
	/* There's only a one-character `buffer', and the only time we
	   actually know there's a character in it is when we receive a
	   character-received interrupt -- in which case we immediately
	   remove it anyway.  */
	return 0;
}

static void v850e_uart_tty_close (void *driver_data)
{
	MOD_DEC_USE_COUNT;
}

static void v850e_uart_tty_hungup (void *driver_data)
{
	MOD_DEC_USE_COUNT;
}

static void v850e_uart_tty_getserial (void *driver_data, struct serial_struct *s)
{
	struct v850e_uart_tty_port *port = driver_data;
	s->line = port->chan;
	s->xmit_fifo_size = 1;
	s->irq = V850E_UART_RX_IRQ (port->chan); /* actually have TX irq too, but... */
}

static struct real_driver v850e_uart_tty_gs_driver = {
	disable_tx_interrupts: 	v850e_uart_tty_disable_tx_interrupts,
	enable_tx_interrupts:	v850e_uart_tty_enable_tx_interrupts,
	disable_rx_interrupts:	v850e_uart_tty_disable_rx_interrupts,
	enable_rx_interrupts:	v850e_uart_tty_enable_rx_interrupts,
	get_CD:			v850e_uart_tty_get_CD,
	shutdown_port:		v850e_uart_tty_shutdown_port,
	set_real_termios:	v850e_uart_tty_set_real_termios,
	chars_in_buffer:	v850e_uart_tty_chars_in_buffer,
	close:			v850e_uart_tty_close,
	hungup:			v850e_uart_tty_hungup,
	getserial:		v850e_uart_tty_getserial,
};

static struct v850e_uart_tty_port v850e_uart_tty_ports[V850E_UART_NUM_CHANNELS];

static void init_v850e_uart_tty_ports (void)
{
	int chan;
	for (chan = 0; chan < V850E_UART_NUM_CHANNELS; chan++) {
		struct v850e_uart_tty_port *port = &v850e_uart_tty_ports[chan];

		port->chan = chan;

		port->gs.magic = V850E_UART_MAGIC;
		port->gs.rd = &v850e_uart_tty_gs_driver;

		init_waitqueue_head (&port->gs.open_wait);
		init_waitqueue_head (&port->gs.close_wait);

		port->gs.normal_termios	= tty_std_termios;
		port->gs.normal_termios.c_cflag = V850E_UART_INIT_CFLAGS;

		port->gs.close_delay = HZ / 2;   /* .5s */
		port->gs.closing_wait = 30 * HZ; /* 30s */
	}
}


/* TTY interrupt handlers.  */

void v850e_uart_tty_tx_irq (int irq, void *data, struct pt_regs *regs)
{
	struct v850e_uart_tty_port *port = data;
	if (port->gs.flags & GS_ACTIVE)
		v850e_uart_tty_tx (port);
	else
		v850e_uart_tty_disable_tx_interrupts (data);
}

void v850e_uart_tty_rx_irq (int irq, void *data, struct pt_regs *regs)
{
	struct v850e_uart_tty_port *port = data;

	if (port->gs.flags & GS_ACTIVE) {
		unsigned ch_stat;
		unsigned err = v850e_uart_err (port->chan);
		unsigned ch = v850e_uart_getc (port->chan);

		if (err & V850E_UART_ERR_OVERRUN)
			ch_stat = TTY_OVERRUN;
		else if (err & V850E_UART_ERR_FRAME)
			ch_stat = TTY_FRAME;
		else if (err & V850E_UART_ERR_PARITY)
			ch_stat = TTY_PARITY;
		else
			ch_stat = TTY_NORMAL;

		tty_insert_flip_char (port->gs.tty, ch, ch_stat);
		tty_schedule_flip (port->gs.tty);
	} else
		v850e_uart_tty_disable_rx_interrupts (port);
}


/* Higher level TTY interface.  */

static struct tty_struct *v850e_uart_ttys[V850E_UART_NUM_CHANNELS] = { 0 };
static struct termios *v850e_uart_tty_termios[V850E_UART_NUM_CHANNELS] = { 0 };
static struct termios *v850e_uart_tty_termios_locked[V850E_UART_NUM_CHANNELS] = { 0 };
static struct tty_driver v850e_uart_tty_driver = { 0 };
static int v850e_uart_tty_refcount = 0;

int v850e_uart_tty_open (struct tty_struct *tty, struct file *filp)
{
	int err;
	struct v850e_uart_tty_port *port;
	unsigned chan = MINOR (tty->device) - V850E_UART_MINOR_BASE;

	if (chan >= V850E_UART_NUM_CHANNELS)
		return -ENODEV;

	port = &v850e_uart_tty_ports[chan];

	tty->driver_data = port;
	port->gs.tty = tty;
	port->gs.count++;

	port->tqueue.routine = gs_do_softint;
	port->tqueue.data = &port->gs;

	/*
	 * Start up serial port
	 */
	err = gs_init_port (&port->gs);
	if (err)
		goto failed_1;

	port->gs.flags |= GS_ACTIVE;

	if (port->gs.count == 1) {
		MOD_INC_USE_COUNT;

		/* Alloc RX irq.  */
		err = request_irq (V850E_UART_RX_IRQ (chan),
				   v850e_uart_tty_rx_irq,
				   SA_INTERRUPT, "v850e_uart", port);
		if (err)
			goto failed_2;

		/* Alloc TX irq.  */
		err = request_irq (V850E_UART_TX_IRQ (chan),
				   v850e_uart_tty_tx_irq,
				   SA_INTERRUPT, "v850e_uart", port);
		if (err) {
			free_irq (V850E_UART_RX_IRQ (chan), port);
			goto failed_2;
		}
	}

	err = gs_block_til_ready (port, filp);
	if (err)
		goto failed_3;

	*tty->termios = port->gs.normal_termios;

	v850e_uart_tty_enable_rx_interrupts (port);

	port->gs.session = current->session;
	port->gs.pgrp = current->pgrp;

	return 0;

failed_3:
	free_irq (V850E_UART_TX_IRQ (chan), port);
	free_irq (V850E_UART_RX_IRQ (chan), port);
failed_2:
	MOD_DEC_USE_COUNT;
failed_1:
	port->gs.count--;

	return err;
}

int __init v850e_uart_tty_init (void)
{
	struct tty_driver *d = &v850e_uart_tty_driver;

	d->driver_name     = "v850e_uart";
#ifdef CONFIG_DEVFS_FS
	d->name            = "tts/%d";
#else
	d->name            = "ttyS";
#endif

	d->major       	   = TTY_MAJOR;
	d->minor_start 	   = V850E_UART_MINOR_BASE;
	d->num 	       	   = V850E_UART_NUM_CHANNELS;
	d->type        	   = TTY_DRIVER_TYPE_SERIAL;
	d->subtype     	   = SERIAL_TYPE_NORMAL;

	d->refcount 	   = &v850e_uart_tty_refcount;

	d->table 	   = v850e_uart_ttys;
	d->termios 	   = v850e_uart_tty_termios;
	d->termios_locked  = v850e_uart_tty_termios_locked;

	d->init_termios    = tty_std_termios;
	d->init_termios.c_cflag = V850E_UART_INIT_CFLAGS;

	d->open 	   = v850e_uart_tty_open;
	d->put_char 	   = gs_put_char;
	d->write 	   = gs_write;
	d->write_room 	   = gs_write_room;
	d->start 	   = gs_start;
	d->stop 	   = gs_stop;
	d->close 	   = gs_close;
	d->write 	   = gs_write;
	d->put_char 	   = gs_put_char;
	d->flush_chars 	   = gs_flush_chars;
	d->write_room 	   = gs_write_room;
	d->chars_in_buffer = gs_chars_in_buffer;
	d->set_termios 	   = gs_set_termios;
	d->throttle   	   = 0; /* V850E uarts have no hardware flow control */
	d->unthrottle 	   = 0;	/* " */
	d->stop 	   = gs_stop;
	d->start 	   = gs_start;
	d->hangup 	   = gs_hangup;
	d->flush_buffer    = gs_flush_buffer;

	init_v850e_uart_tty_ports ();

	tty_register_driver (&v850e_uart_tty_driver);
}
__initcall (v850e_uart_tty_init);
