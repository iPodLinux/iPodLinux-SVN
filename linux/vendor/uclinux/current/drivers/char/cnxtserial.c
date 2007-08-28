/****************************************************************************
*
*	Name:			cnxtserial.c
*
*	Description:	Provides serial port character I/O (16550)
*
*	Copyright:		(c) 2002 Conexant Systems Inc.
*
*****************************************************************************

  This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option)
any later version.

  This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
more details.

  You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc., 59
Temple Place, Suite 330, Boston, MA 02111-1307 USA

****************************************************************************
*  $Author: davidm $
*  $Revision: 1.1 $
*  $Modtime: 8/19/02 10:51a $
****************************************************************************/

#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/config.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/arch/irq.h>
#include <asm/system.h> 
#include <asm/segment.h>
#include <asm/bitops.h>
#include <asm/delay.h>
#include <asm/arch/bspcfg.h>
#include "cnxtserial.h"
#include <asm/hardware.h>
#include <linux/init.h>
#include <asm/uaccess.h>

#include <asm/arch/bsptypes.h>
#include <asm/arch/cnxtbsp.h>
#include <asm/arch/gpio.h>

static struct cnxt_serial uart_info;

static int console_initialized = 0;

DECLARE_TASK_QUEUE(tq_serial);

struct tty_driver serial_driver, callout_driver;
static int serial_refcount;

static struct tty_struct *serial_table[2];
static struct termios *serial_termios[2];
static struct termios *serial_termios_locked[2];

static int baud_table[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
	9600, 19200, 38400, 57600, 115200, 0 };

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

/*
 * tmp_buf is used as a temporary buffer by serial_write.  We need to
 * lock it in case the memcpy_fromfs blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
static unsigned char tmp_buf[SERIAL_XMIT_SIZE]; /* This is cheating */
DECLARE_MUTEX(tmp_buf_sem);

static inline int serial_paranoia_check(struct cnxt_serial *info,
					dev_t device, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char *badmagic =
		"Warning: bad magic number for serial struct (%d, %d) in %s\n";
	static const char *badinfo =
		"Warning: null m68k_serial for (%d, %d) in %s\n";

	if (!info) {
		printk(badinfo, MAJOR(device), MINOR(device), routine);
		return 1;
	}
	if (info->magic != SERIAL_MAGIC) {
		printk(badmagic, MAJOR(device), MINOR(device), routine);
		return 1;
	}
#endif
	return 0;
}




/*
 * 1. Generic uart stuff
 */


 
static void  tx_enable(struct uart_regs *uart){
	uart->irqe	|= IER_Tx_Holding_Reg;
}


static void tx_start(struct uart_regs *uart, int ints)
{
  if(ints)
    tx_enable(uart);
}


static void start_rx(void)
{
	struct uart_regs *uart = uart_info.uart;

#if 0
	if(uart_info.use_ints)
#endif
	{
		/* enable IRQs */
		uart->irqe	|= IER_Rx_Holding_Reg;
	}
}


static void wait_EOT(struct uart_regs *uart)
{
  volatile struct uart_regs* puart;
  unsigned long timeout = 10000;
  puart = (volatile struct uart_regs*)uart;
  while(1){
    if(timeout-- == 0) {
      printk("eot timeout\n");
      break;
    }
    if(puart->line_status_reg & LSR_Tx_Fifo_Empty)
      break;
  }
}

static void xmit_string(struct cnxt_serial * info, char *p, int len) 
{
	struct uart_regs *uart = info->uart;
	uart->fifo = (char)*p;	
	tx_start(uart, info->use_ints);
}


static void cnxt_put_char(char ch)
{
	int flags = 0;

	if(uart_info.use_ints)
	{
		uart_info.uart->fifo=ch;
	}
	else
	{
		save_flags(flags);
		cli();
		wait_EOT(uart_info.uart);
		uart_info.uart->fifo=ch; 
		restore_flags(flags);
	}
}


static void rs_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct cnxt_serial *info = (struct cnxt_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_put_char"))
	{
		return;
	}

	cnxt_put_char(ch);
}	


static void uart_speed(void)
{
  struct uart_regs *uart = uart_info.uart;
  
  uart->line_ctrl = LCR_Divisor_Latch;
  uart->fifo = (char)UART_115200;  /* lower baud rate div */
  uart->irqe = (char)(UART_115200>>8);  /* upper baud rate div */
  uart->line_ctrl = 0;
  uart->line_ctrl =  LCR_8_Bit_Word_1;
  uart->iir =  (FCR_Fifo_Enable | FCR_Rx_Fifo_Reset | FCR_Tx_Fifo_Reset ); /* fifo ctrl */
  uart->irqe = IER_Rx_Holding_Reg;  /* enable rx intr */
  tx_start(uart, uart_info.use_ints);
  start_rx();
}


/*
 * 2. Kernel log console
 */
 
static void cnxt_init_console(void)
{
	memset(&uart_info, 0, sizeof(struct cnxt_serial));
	
	uart_info.uart          = (struct uart_regs*)UART0_Base_Addr;
	uart_info.tty	        = 0;
	uart_info.irqmask	= BIT24;
	uart_info.irq	        = CNXT_INT_LVL_GPIO;
	uart_info.port	        = 1;
	uart_info.use_ints	= 0;
	uart_info.is_cons	= 1;

	console_initialized = 1;
}


int cnxt_console_setup(struct console *cp, char *arg)
{
	if (!cp)
		return(-1);
	cnxt_init_console();
	uart_speed();
	return(0);
}


static kdev_t cnxt_console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, 64 + c->index);
}


/*
 *	Output a single character, using UART polled mode.
 *	This is ised for console output.
 */
void cnxt_console_write(struct console *cp, const char *p, unsigned len)
{
	if (!console_initialized)
	{
		cnxt_init_console();
		uart_speed();
	}
	while (len-- > 0)
	{
		if (*p == '\n')
		{
			cnxt_put_char('\r');
		}
		cnxt_put_char(*p++);
	}
}


struct console  cnxt_console = {
	name:		"ttyS0",
	write:		cnxt_console_write,
	read:	        NULL,
	device:		cnxt_console_device,
	unblank:	NULL,
	setup:		cnxt_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
	cflag:		0,
	next:		NULL
};

void serial_cnxt_console_init(void)
{
  register_console(&cnxt_console);
}

/*
 * 3. serial tty stuff
 */

/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */
static void change_speed(struct cnxt_serial *info)
{
	unsigned cflag;
	int	i;

	if (!info->tty || !info->tty->termios)
		return;
	cflag = info->tty->termios->c_cflag;

	/* set the baudrate */
	i = cflag & CBAUD;

	info->baud = baud_table[i];
	uart_speed();
	start_rx();
	tx_start(info->uart, info->use_ints);
	return;
}


static void do_serial_hangup(void *private_)
{
	struct cnxt_serial	*info = (struct cnxt_serial *) private_;
	struct tty_struct	*tty;
	
	tty = info->tty;
	if (!tty)
		return;

	tty_hangup(tty);
}


static int startup(struct cnxt_serial * info)
{
	unsigned long flags;
	
	if (info->flags & S_INITIALIZED)
		return 0;

	if (!info->xmit_buf) {
		info->xmit_buf = (unsigned char *) get_free_page(GFP_KERNEL);
		if (!info->xmit_buf)
			return -ENOMEM;
	}

	save_flags(flags); cli();

#if 0
	/*
	 * Clear the FIFO buffers and disable them
	 * (they will be reenabled in change_speed())
	 */

	USTCNT = USTCNT_UEN;
	info->xmit_fifo_size = 1;
	USTCNT = USTCNT_UEN | USTCNT_RXEN | USTCNT_TXEN;
	(void)URX;

	/*
	 * Finally, enable sequencing and interrupts
	 */

#ifdef USE_INTS
	USTCNT = USTCNT_UEN | USTCNT_RXEN | 
                 USTCNT_RX_INTR_MASK | USTCNT_TX_INTR_MASK;
#else
	USTCNT = USTCNT_UEN | USTCNT_RXEN | USTCNT_RX_INTR_MASK;
#endif
#endif

	if (info->tty)
		clear_bit(TTY_IO_ERROR, &info->tty->flags);
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;

	/*
	 * and set the speed of the serial port
	 */
#if 0
	change_speed(info);
#endif
	info->flags |= S_INITIALIZED;
	restore_flags(flags);
	return 0;
}


/*
 * ------------------------------------------------------------
 * rs_open() and friends
 * ------------------------------------------------------------
 */
static int block_til_ready(struct tty_struct *tty, struct file * filp,
			   struct cnxt_serial *info)
{
	DECLARE_WAITQUEUE(wait, current);
	int		retval;
	int		do_clocal = 0;

	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (info->flags & S_CLOSING) {
		interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
		if (info->flags & S_HUP_NOTIFY)
			return -EAGAIN;
		else
			return -ERESTARTSYS;
#else
		return -EAGAIN;
#endif

	}

	/*
	 * If this is a callout device, then just make sure the normal
	 * device isn't being used.
	 */
	if (tty->driver.subtype == SERIAL_TYPE_CALLOUT) {
		if (info->flags & S_NORMAL_ACTIVE)
			return -EBUSY;
		if ((info->flags & S_CALLOUT_ACTIVE) &&
		    (info->flags & S_SESSION_LOCKOUT) &&
		    (info->session != current->session))
		    return -EBUSY;
		if ((info->flags & S_CALLOUT_ACTIVE) &&
		    (info->flags & S_PGRP_LOCKOUT) &&
		    (info->pgrp != current->pgrp))
		    return -EBUSY;
		info->flags |= S_CALLOUT_ACTIVE;
		return 0;
	}
	
	/*
	 * If non-blocking mode is set, or the port is not enabled,
	 * then make the check up front and then exit.
	 */
	if ((filp->f_flags & O_NONBLOCK) ||
	    (tty->flags & (1 << TTY_IO_ERROR))) {
		if (info->flags & S_CALLOUT_ACTIVE)
			return -EBUSY;
		info->flags |= S_NORMAL_ACTIVE;
		return 0;
	}

	if (info->flags & S_CALLOUT_ACTIVE) {
		if (info->normal_termios.c_cflag & CLOCAL)
			do_clocal = 1;
	} else {
		if (tty->termios->c_cflag & CLOCAL)
			do_clocal = 1;
	}
	
	/*
	 * Block waiting for the carrier detect and the line to become
	 * free (i.e., not in use by the callout).  While we are in
	 * this loop, info->count is dropped by one, so that
	 * rs_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */
	retval = 0;
	add_wait_queue(&info->open_wait, &wait);

	info->count--;
	info->blocked_open++;
	while (1) {
		cli();
		if (!(info->flags & S_CALLOUT_ACTIVE))
		  //m68k_rtsdtr(info, 1);
		
		sti();
		current->state = TASK_INTERRUPTIBLE;
		if (tty_hung_up_p(filp) ||
		    !(info->flags & S_INITIALIZED)) {
#ifdef SERIAL_DO_RESTART
			if (info->flags & S_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;	
#else
			retval = -EAGAIN;
#endif
			break;
		}
		if (!(info->flags & S_CALLOUT_ACTIVE) &&
		    !(info->flags & S_CLOSING) && do_clocal)
			break;
                if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&info->open_wait, &wait);
	if (!tty_hung_up_p(filp))
		info->count++;
	info->blocked_open--;

	if (retval)
		return retval;
	info->flags |= S_NORMAL_ACTIVE;
	return 0;
}	


/*
 * This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its S structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */
int rs_open(struct tty_struct *tty, struct file * filp)
{
	int 		  retval, line;
	struct cnxt_serial *info;

	line = MINOR(tty->device) - tty->driver.minor_start;

	
	if (line != 0) /* we have exactly one */
		return -ENODEV;

	info = &uart_info;

	if (serial_paranoia_check(info, tty->device, "rs_open"))
		return -ENODEV;

	info->count++;
	tty->driver_data = info;
	info->tty = tty;

	/*
	 * Start up serial port
	 */
	retval = startup(info);
	if (retval)
		return retval;
	
	retval = block_til_ready(tty, filp, info);
	if (retval) {

		printk("rs_open returning after block_til_ready with %d\n",
		       retval);
		return retval;
	}

	if ((info->count == 1) && (info->flags & S_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = info->normal_termios;
		else 
			*tty->termios = info->callout_termios;
		change_speed(info);
	}

	info->session = current->session;
	info->pgrp = current->pgrp;

	// Enable GPIO interrupt line for console uart 
	SetGPIOIntEnable(GPIOINT_UART1, IRQ_ON);
	return 0;
}



/*
 * This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */
static void shutdown(struct cnxt_serial * info)
{
	unsigned long	flags;

	
	if (!(info->flags & S_INITIALIZED))
	  return;
	
#ifdef SERIAL_DEBUG_OPEN
	printk("Shutting down serial port %d (irq %d)....\n", info->line,
	       info->irq);
#endif
	
	save_flags(flags); cli(); /* Disable interrupts */
	
	if (info->xmit_buf) {
		free_page((unsigned long) info->xmit_buf);
		info->xmit_buf = 0;
	}

	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);
	
	info->flags &= ~S_INITIALIZED;
	restore_flags(flags);
}


static void rs_close(struct tty_struct *tty, struct file * filp)
{
	struct cnxt_serial * info = (struct cnxt_serial *)tty->driver_data;
	unsigned long flags;

	if (!info || serial_paranoia_check(info, tty->device, "rs_close"))
		return;
	
	save_flags(flags); cli();
	
	if (tty_hung_up_p(filp)) {
		restore_flags(flags);
		return;
	}
	
	if ((tty->count == 1) && (info->count != 1)) {
		/*
		 * Uh, oh.  tty->count is 1, which means that the tty
		 * structure will be freed.  info->count should always
		 * be one in these conditions.  If it's greater than
		 * one, we've got real problems, since it means the
		 * serial port won't be shutdown.
		 */
		printk("rs_close: bad serial port count; tty->count is 1, "
		       "info->count is %d\n", info->count);
		info->count = 1;
	}
	if (--info->count < 0) {
		printk("rs_close: bad serial port count for ttyS%d: %d\n",
		       info->line, info->count);
		info->count = 0;
	}
	if (info->count) {
		restore_flags(flags);
		return;
	}
	// closing port so disable interrupts
	//set_ints_mode(0);
	info->use_ints = 0;

	info->flags |= S_CLOSING;
	/*
	 * Save the termios structure, since this port may have
	 * separate termios for callout and dialin.
	 */
	if (info->flags & S_NORMAL_ACTIVE)
		info->normal_termios = *tty->termios;
	if (info->flags & S_CALLOUT_ACTIVE)
		info->callout_termios = *tty->termios;
	/*
	 * Now we wait for the transmit buffer to clear; and we notify 
	 * the line discipline to only process XON/XOFF characters.
	 */
	tty->closing = 1;
	if (info->closing_wait != S_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, info->closing_wait);
	/*
	 * At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and tell the
	 * interrupt driver to stop checking the data ready bit in the
	 * line status register.
	 */

	shutdown(info);
	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
	tty->closing = 0;
	info->event = 0;
	info->tty = 0;
	if (tty->ldisc.num != ldiscs[N_TTY].num) {
		if (tty->ldisc.close)
			(tty->ldisc.close)(tty);
		tty->ldisc = ldiscs[N_TTY];
		tty->termios->c_line = N_TTY;
		if (tty->ldisc.open)
			(tty->ldisc.open)(tty);
	}
	if (info->blocked_open) {
		if (info->close_delay) {
			current->state = TASK_INTERRUPTIBLE;
			schedule_timeout(info->close_delay);
	}
		wake_up_interruptible(&info->open_wait);
	}
	info->flags &= ~(S_NORMAL_ACTIVE|S_CALLOUT_ACTIVE|
			 S_CLOSING);
	wake_up_interruptible(&info->close_wait);
	restore_flags(flags);
}

static void rs_flush_buffer(struct tty_struct *tty)
{
	struct cnxt_serial *info = (struct cnxt_serial *)tty->driver_data;		
	if (serial_paranoia_check(info, tty->device, "rs_flush_buffer"))
	  return;
	cli();
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	sti();
	wake_up_interruptible(&tty->write_wait);
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
}


/*
 * rs_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
void rs_hangup(struct tty_struct *tty)
{
  struct cnxt_serial * info = (struct cnxt_serial *)tty->driver_data;
	
  if (serial_paranoia_check(info,tty->device, "rs_hangup"))
		return;
	
	rs_flush_buffer(tty);
	shutdown(info);
	info->event = 0;
	info->count = 0;
	info->flags &= ~(S_NORMAL_ACTIVE|S_CALLOUT_ACTIVE);
	info->tty = 0;
	wake_up_interruptible(&info->open_wait);
}


static int rs_write(struct tty_struct * tty, int from_user,
		    const unsigned char *buf, int count)
{
	int	c, total = 0;
	unsigned long flags;
	struct cnxt_serial *info = (struct cnxt_serial *)tty->driver_data;
  
	if (serial_paranoia_check(info,tty->device, "rs_write"))
	  return 0;
	
	if (!tty || !info->xmit_buf)
	  return 0;
	
	save_flags(flags);
	while (1) {
	  cli();		
	  c = MIN(count, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
			     SERIAL_XMIT_SIZE - info->xmit_head));
	  if (c <= 0)
	    break;
	  
	  if (from_user) {
	    down(&tmp_buf_sem);
	    copy_from_user(tmp_buf, buf, c);
	    c = MIN(c, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
			   SERIAL_XMIT_SIZE - info->xmit_head));
	    memcpy(info->xmit_buf + info->xmit_head, tmp_buf, c);
	    up(&tmp_buf_sem);
	  } else
	    memcpy(info->xmit_buf + info->xmit_head, buf, c);
	  
	  info->xmit_head = (info->xmit_head + c) & (SERIAL_XMIT_SIZE-1);
	  
	  info->xmit_cnt += c;
	  restore_flags(flags);
	  buf += c;
	  count -= c;
	  total += c;
	}
	
	if (info->xmit_cnt && !tty->stopped && !tty->hw_stopped ){
	  /* Enable transmitter */
	  cli();		

		if(!info->use_ints){	
			while(info->xmit_cnt) {
				wait_EOT(info->uart);
				/* Send char */
				info->uart->fifo = info->xmit_buf[info->xmit_tail++];
				info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
				info->xmit_cnt--;
			}
		}else{
			if (info->xmit_cnt){
				/* Send char */
				wait_EOT(info->uart);
				xmit_string(info,&info->xmit_buf[info->xmit_tail], info->xmit_cnt);
				info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
				info->xmit_cnt=0;
			}
		}
		restore_flags(flags);
	} 


	restore_flags(flags);
	return total;
}

static int rs_write_room(struct tty_struct *tty)
{
	int	ret;
	struct cnxt_serial *info = (struct cnxt_serial *)tty->driver_data;
	if (serial_paranoia_check(info,tty->device, "rs_write_room"))
		return 0;
	ret = SERIAL_XMIT_SIZE - info->xmit_cnt - 1;
	if (ret < 0)
		ret = 0;
	return ret;
}

static int rs_chars_in_buffer(struct tty_struct *tty)
{
	struct cnxt_serial *info = (struct cnxt_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_chars_in_buffer"))
		return 0;
	return info->xmit_cnt;
}

static void rs_flush_chars(struct tty_struct *tty)
{
	struct cnxt_serial *info = (struct cnxt_serial *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_flush_chars"))
		return;
	if(!info->use_ints){	
		for(;;) {
			if (info->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped ||
				!info->xmit_buf)
				return;

			/* Enable transmitter */
			save_flags(flags); cli();
			tx_start(info->uart,info->use_ints);
		}
	}else{
		if (info->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped ||
			!info->xmit_buf)
			return;

			/* Enable transmitter */
		save_flags(flags); cli();
		tx_start(info->uart, info->use_ints);
	}

	if(!info->use_ints)	
		wait_EOT(info->uart);
		/* Send char */
	//xmit_char(info->xmit_buf[info->xmit_tail++]);
	xmit_string(info,(char *)&info->xmit_buf[info->xmit_tail++],1);
	info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
	info->xmit_cnt--;
	
	restore_flags(flags);
}



static void do_serial_bh(void)
{
	run_task_queue(&tq_serial);
}

static void do_softint(void *private_)
{
	struct cnxt_serial	*info = (struct cnxt_serial *) private_;
	struct tty_struct	*tty;
	
	tty = info->tty;
	if (!tty)
		return;
#if 0
	if (clear_bit(RS_EVENT_WRITE_WAKEUP, &info->event)) {
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
		    tty->ldisc.write_wakeup)
			(tty->ldisc.write_wakeup)(tty);
		wake_up_interruptible(&tty->write_wait);
	}
#endif   
}

/*
 * This routine sends a break character out the serial port.
 */
static void send_break(	struct cnxt_serial * info, int duration)
{
#if 0
	current->state = TASK_INTERRUPTIBLE;
	current->timeout = jiffies + duration;
	cli();
	sti();
#else
	printk("send_break\n");
#endif
}

static int get_serial_info(struct cnxt_serial * info,
			   struct serial_struct * retinfo)
{
	struct serial_struct tmp;
  
	if (!retinfo)
		return -EFAULT;
	memset(&tmp, 0, sizeof(tmp));
	tmp.type = info->type;
	tmp.line = info->line;
	tmp.port = info->port;
	tmp.irq = info->irq;
	tmp.flags = info->flags;
	tmp.baud_base = info->baud_base;
	tmp.close_delay = info->close_delay;
	tmp.closing_wait = info->closing_wait;
	tmp.custom_divisor = info->custom_divisor;
	copy_to_user(retinfo,&tmp,sizeof(*retinfo));
	return 0;
}


static int set_serial_info(struct cnxt_serial * info,
			   struct serial_struct * new_info)
{
	struct serial_struct new_serial;
	struct cnxt_serial old_info;
	int 			retval = 0;

	if (!new_info)
		return -EFAULT;
	copy_from_user(&new_serial,new_info,sizeof(new_serial));
	old_info = *info;

	if (!suser()) {
		if ((new_serial.baud_base != info->baud_base) ||
		    (new_serial.type != info->type) ||
		    (new_serial.close_delay != info->close_delay) ||
		    ((new_serial.flags & ~S_USR_MASK) !=
		     (info->flags & ~S_USR_MASK)))
			return -EPERM;
		info->flags = ((info->flags & ~S_USR_MASK) |
			       (new_serial.flags & S_USR_MASK));
		info->custom_divisor = new_serial.custom_divisor;
		goto check_and_exit;
	}

	if (info->count > 1)
		return -EBUSY;

	/*
	 * OK, past this point, all the error checking has been done.
	 * At this point, we start making changes.....
	 */

	info->baud_base = new_serial.baud_base;
	info->flags = ((info->flags & ~S_FLAGS) |
			(new_serial.flags & S_FLAGS));
	info->type = new_serial.type;
	info->close_delay = new_serial.close_delay;
	info->closing_wait = new_serial.closing_wait;

check_and_exit:
	retval = startup(info);
	return retval;
}


static int get_lsr_info(struct cnxt_serial * info, unsigned int *value)
{
	unsigned char status;

	status = 0;
	put_user(status,value);
	return 0;
}


static void rs_throttle(struct tty_struct * tty)
{
	struct cnxt_serial *info = (struct cnxt_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_throttle"))
		return;
	
	if (I_IXOFF(tty))
		info->x_char = STOP_CHAR(tty);

	/* Turn off RTS line (do this atomic) */
}

static void rs_unthrottle(struct tty_struct * tty)
{
	struct cnxt_serial *info = (struct cnxt_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_unthrottle"))
		return;
#if 0	
	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			info->x_char = START_CHAR(tty);
	}
#endif
	/* Assert RTS line (do this atomic) */
}

static int rs_ioctl(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg)
{
	int error;
	struct cnxt_serial * info = (struct cnxt_serial *)tty->driver_data;
	int retval;

	if (serial_paranoia_check(info, tty->device, "rs_ioctl"))
		return -ENODEV;

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
	    (cmd != TIOCSERCONFIG) && (cmd != TIOCSERGWILD)  &&
	    (cmd != TIOCSERSWILD) && (cmd != TIOCSERGSTRUCT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
		    return -EIO;
	}
	
	switch (cmd) {
		case TCSBRK:	/* SVID version: non-zero arg --> no break */
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			if (!arg)
				send_break(info, HZ/4);	/* 1/4 second */
			return 0;
		case TCSBRKP:	/* support for POSIX tcsendbreak() */
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			send_break(info, arg ? arg*(HZ/10) : HZ/4);
			return 0;
		case TIOCGSOFTCAR:
			error = verify_area(VERIFY_WRITE, (void *) arg,sizeof(long));
			if (error)
				return error;
			put_user(C_CLOCAL(tty) ? 1 : 0,
				    (unsigned long *) arg);
			return 0;
		case TIOCSSOFTCAR:
			get_user(arg, (unsigned long *) arg);
			tty->termios->c_cflag =
				((tty->termios->c_cflag & ~CLOCAL) |
				 (arg ? CLOCAL : 0));
			return 0;
		case TIOCGSERIAL:
			error = verify_area(VERIFY_WRITE, (void *) arg,
						sizeof(struct serial_struct));
			if (error)
				return error;
			return get_serial_info(info,
					       (struct serial_struct *) arg);
		case TIOCSSERIAL:
			return set_serial_info(info,
					       (struct serial_struct *) arg);
		case TIOCSERGETLSR: /* Get line status register */
			error = verify_area(VERIFY_WRITE, (void *) arg,
				sizeof(unsigned int));
			if (error)
				return error;
			else
			    return get_lsr_info(info, (unsigned int *) arg);

		case TIOCSERGSTRUCT:
			error = verify_area(VERIFY_WRITE, (void *) arg,
						sizeof(struct cnxt_serial));
			if (error)
				return error;
			copy_to_user((struct cnxt_serial *) arg,
				    info, sizeof(struct cnxt_serial));
			return 0;
			
		default:
			return -ENOIOCTLCMD;
		}
	return 0;
}


static void rs_stop(struct tty_struct *tty)
{
	struct cnxt_serial *info = (struct cnxt_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_stop"))
		return;
#if 0
	save_flags(flags); cli();
	tx_stop(info->uart);
	restore_flags(flags);
#endif

}

static void rs_start(struct tty_struct *tty)
{
	unsigned long flags;
	struct cnxt_serial *info = (struct cnxt_serial *)tty->driver_data;

	if (serial_paranoia_check(info,tty->device, "rs_start"))
		return;
	
	save_flags(flags);
	cli();
	tx_start(info->uart, info->use_ints);
	start_rx();
	restore_flags(flags);
}

static void rs_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	struct cnxt_serial *info = (struct cnxt_serial *)tty->driver_data;

	if (tty->termios->c_cflag == old_termios->c_cflag)
		return;

	change_speed(info);

	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		rs_start(tty);
	}
	
}


static void rs_set_ldisc(struct tty_struct *tty)
{
	struct cnxt_serial *info = (struct cnxt_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_set_ldisc"))
		return;

	info->is_cons = (tty->termios->c_line == N_TTY);
	
	printk("ttyS%d console mode %s\n", info->line, info->is_cons ? "on" : "off");
}


/*
 * This routine is used by the interrupt handler to schedule
 * processing in the software interrupt portion of the driver.
 */
static void rs_sched_event(struct cnxt_serial *info, int event)
{
	info->event |= 1 << event;
	queue_task(&info->tqueue, &tq_serial);
	mark_bh(SERIAL_BH);
}

static inline void receive_chars(struct cnxt_serial *info, unsigned long status)
{
	unsigned char ch;

	struct uart_regs *uart = info->uart;
	
#if 0
	// hack to receive chars by polling from anywhere
	struct tty_struct *tty = info->tty;
	if (!(info->flags & S_INITIALIZED))
		return;
#else
	struct tty_struct *tty = info->tty;
	if (!(info->flags & S_INITIALIZED))
		return;
#endif	
	
	ch = (unsigned char)uart->fifo;

	if(info->is_cons)
	{
		
		if (ch == 0x10)
		{ 
			show_state();
			show_free_areas();
			show_buffers();
			//show_net_buffers();
			return;
		}
		else if (ch == 0x12)
		{ 
			HARD_RESET_NOW();
			return;
		}
#ifdef CONFIG_CONSOLE
		// It is a 'keyboard interrupt' ;-) 
		wake_up(&keypress_wait);
#endif
	}

	/* Look for kgdb 'stop' character, consult the gdb documentation
	 * for remote target debugging and arch/sparc/kernel/sparc-stub.c
	 * to see how all this works.
	 */

	
	if(!tty)
	{
	  printk("no tty\n");
	  goto clear_and_exit;
	}
#if 0
	if (tty->flip.count >= TTY_FLIPBUF_SIZE)
		queue_task_irq_off(&tty->flip.tqueue, &tq_timer);
	tty->flip.count++;
	if(status & US_PARE)
		*tty->flip.flag_buf_ptr++ = TTY_PARITY;
	else if(status & US_OVRE)
		*tty->flip.flag_buf_ptr++ = TTY_OVERRUN;
	else if(status & US_FRAME)
		*tty->flip.flag_buf_ptr++ = TTY_FRAME;
	else
		*tty->flip.flag_buf_ptr++ = 0; 
#endif
        tty->flip.count++;
        *tty->flip.flag_buf_ptr++ = 0;
	*tty->flip.char_buf_ptr++ = ch;

	
	queue_task(&tty->flip.tqueue, &tq_timer);
	

clear_and_exit:
	
	return;
}


static inline void transmit_chars(struct cnxt_serial *info)
{
  struct uart_regs *uart = info->uart;

	if (info->x_char) {
		/* Send next char */
	  //xmit_char(info->x_char);
		uart->fifo=info->x_char;		
		info->x_char = 0;
		goto clear_and_return;
	}

	if((info->xmit_cnt <= 0) || info->tty->stopped) {
		/* That's peculiar... */
		goto clear_and_return;
	}

	/* Send char */
	//xmit_char(info->xmit_buf[info->xmit_tail++]);
	info->uart->fifo=info->xmit_buf[info->xmit_tail++];
	
	info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
	info->xmit_cnt--;

	if (info->xmit_cnt < 256)
		rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);

	if(info->xmit_cnt <= 0) {
		goto clear_and_return;
	}

clear_and_return:
	/* Clear interrupt (should be auto)*/
	return;
}



void rs_interrupt(int irq, void * dev_id, struct pt_regs * regs)
{
	char status;

	struct cnxt_serial * info = &uart_info;
  	struct uart_regs *uart = uart_info.uart;
  	
	status = (uart->iir & 0x0f); /* only concerned w/ lower nibble */
	
	if (status == ISR_Tx_Rdy_Source) {
		transmit_chars(info);
	}
	if ((status == ISR_Rx_Rdy_Source) ||
            (status == ISR_Rx_Rdy_TO_Src )){
		receive_chars(info,status);
	}
	#if 0
		if(!info->use_ints){
			serialpoll.data = (void *)&sp_uart_info;		
			queue_task_irq_off(&serialpoll, &tq_timer);
		}
	#endif


	return;
}


static int __init
rs_cnxt_init(void)
{
	int flags;
	struct cnxt_serial *info;
		
	/* Setup base handler, and timer table. */
	init_bh(SERIAL_BH, do_serial_bh);
	
	/* Initialize the tty_driver structure */
	
	memset(&serial_driver, 0, sizeof(struct tty_driver));
	serial_driver.magic = TTY_DRIVER_MAGIC;
	serial_driver.name = "ttyS";
	serial_driver.major = TTY_MAJOR;
	serial_driver.minor_start = 64;
	serial_driver.num = 1;
	serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver.subtype = SERIAL_TYPE_NORMAL;
	serial_driver.init_termios = tty_std_termios;
	serial_driver.init_termios.c_cflag = 
			B57600 | CS8 | CREAD | HUPCL | CLOCAL;
	serial_driver.flags = TTY_DRIVER_REAL_RAW;
	serial_driver.refcount = &serial_refcount;
	serial_driver.table = serial_table;
	serial_driver.termios = serial_termios;
	serial_driver.termios_locked = serial_termios_locked;

	serial_driver.open = rs_open;
	serial_driver.close = rs_close;
	serial_driver.write = rs_write;
	serial_driver.put_char = rs_put_char;
	serial_driver.flush_chars = rs_flush_chars;
	serial_driver.write_room = rs_write_room;
	serial_driver.chars_in_buffer = rs_chars_in_buffer;
	serial_driver.flush_buffer = rs_flush_buffer;
	serial_driver.ioctl = rs_ioctl;
	serial_driver.throttle = rs_throttle;
	serial_driver.unthrottle = rs_unthrottle;
	serial_driver.set_termios = rs_set_termios;
	serial_driver.stop = rs_stop;
	serial_driver.start = rs_start;
	serial_driver.hangup = rs_hangup;
	serial_driver.set_ldisc = rs_set_ldisc;

	/*
	 * The callout device is just like normal device except for
	 * major number and the subtype code.
	 */
	callout_driver = serial_driver;
	callout_driver.name = "cua";
	callout_driver.major = TTYAUX_MAJOR;
	callout_driver.subtype = SERIAL_TYPE_CALLOUT;

	if (tty_register_driver(&serial_driver))
		panic("Couldn't register serial driver\n");
	if (tty_register_driver(&callout_driver))
		panic("Couldn't register callout driver\n");
	
	save_flags(flags); cli();


	info = &uart_info;
	info->magic = SERIAL_MAGIC;
	info->uart = (struct uart_regs*)UART0_Base_Addr;
	info->port = UART0_Base_Addr;
	info->tty = 0;
	info->irq = CNXT_INT_LVL_GPIO;
	info->custom_divisor = 16;
	info->close_delay = 50;
	info->closing_wait = 3000;
	info->x_char = 0;
	info->event = 0;
	info->count = 0;
	info->blocked_open = 0;
	info->tqueue.routine = do_softint;
	info->tqueue.data = info;
	info->tqueue_hangup.routine = do_serial_hangup;
	info->tqueue_hangup.data = info;
	info->callout_termios =callout_driver.init_termios;
	info->normal_termios = serial_driver.init_termios;
	init_waitqueue_head(&info->open_wait);
	init_waitqueue_head(&info->close_wait);
	info->line = 0;
	info->is_cons = 1; /* Means shortcuts work */

	printk("%s%d at 0x%08x (irq = %d)", serial_driver.name, info->line, 
	       info->port, info->irq);
       printk(" is a 16c550 UART\n");
       printk("info = %08x\n",info);
       
#if DEBUG_CONSOLE_ON_UART_2
	SetGPIOIntEnable( GPIO25, IRQ_OFF );
	SetGPIOInputEnable( GPIO25, GP_INPUTOFF );
#endif

	SetGPIOIntEnable(GPIOINT_UART1, IRQ_OFF);
	SetGPIODir( GPIOINT_UART1, GP_INPUT );
	SetGPIOIntPolarity (GPIOINT_UART1, GP_IRQ_POL_POSITIVE);
	SetGPIOInputEnable( GPIOINT_UART1, GP_INPUTON );

	GPIO_SetGPIOIRQRoutine(GPIOINT_UART1, GPIOB25_Handler);
	SetGPIOIntEnable(GPIOINT_UART1, IRQ_ON);
	
	restore_flags(flags);

	return 0;
}



module_init(rs_cnxt_init);




