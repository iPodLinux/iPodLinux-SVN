/*
 * xmbserial.c -- serial driver for Microblaze/Xilinx UARTLITE
 *
 * Copyright (c) 2003      John Williams <jwilliams@itee.uq.edu.au>
 *
 * Ripped lock, stock and barrel from mcfserial.c, which is
 *
 * Copyright (c) 1999-2003 Greg Ungerer <gerg@snapgear.com>
 * Copyright (C) 2001-2003 SnapGear Inc. <www.snapgear.com> 
 * Copyright (c) 2000-2001 Lineo, Inc. <www.lineo.com> 
 *
 * Based on code from 68332serial.c which was:
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1998 TSHG
 * Copyright (c) 1999 Rt-Control Inc. <jeff@uclinux.org>
 */
 
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/config.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/console.h>
#include <linux/version.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <asm/semaphore.h>
#include <asm/bitops.h>
#include <asm/delay.h>
#include "xuartlite_l.h"
#include <asm/microblaze_intc.h>
#include <asm/xparameters.h>

#include <asm/uaccess.h>

#include "xmbserial.h"

/*
 *	the only event we use
 */
#undef RS_EVENT_WRITE_WAKEUP
#define RS_EVENT_WRITE_WAKEUP 0

struct timer_list xmbrs_timer_struct;

/*
 *	Default console baud rate,  we use this as the default
 *	for all ports so init can just open /dev/console and
 *	keep going.  Perhaps one day the cflag settings for the
 *	console can be used instead.
 */
#ifndef CONSOLE_BAUD_RATE
#define	CONSOLE_BAUD_RATE	9600
#define	DEFAULT_CBAUD		B9600
#endif

int xmbrs_console_inited = 0;
int xmbrs_console_port = -1;
int xmbrs_console_baud = CONSOLE_BAUD_RATE;
int xmbrs_console_cbaud = DEFAULT_CBAUD;

DECLARE_TASK_QUEUE(xmb_tq_serial);

/*
 *	Driver data structures.
 */
struct tty_driver	xmbrs_serial_driver, xmbrs_callout_driver;
static int		xmbrs_serial_refcount;

/* serial subtype definitions */
#define SERIAL_TYPE_NORMAL	1
#define SERIAL_TYPE_CALLOUT	2
  
/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 128

/* Debugging...
 */
#undef SERIAL_DEBUG_OPEN
#undef SERIAL_DEBUG_FLOW

#define _INLINE_ inline

/* IRQ 0 is timer, IRQ1 is console_uart */
#define	IRQBASE	1

#if 0
	#define db_print_char(x) db_print_char_fn(x)
#else
	#define db_print_char(x)
#endif

void SendByte(volatile unsigned int *baseaddr, unsigned char x)
{
	while(baseaddr[XUL_STATUS_REG_OFFSET/4] & XUL_SR_TX_FIFO_FULL)
		;

	baseaddr[XUL_TX_FIFO_OFFSET/4] = x; 
}

void db_print_char_fn(char x)
{
	if(x=='\n')
		SendByte((unsigned int *)XPAR_DEBUG_UART_BASEADDR, '\r');

	SendByte((unsigned int *)XPAR_DEBUG_UART_BASEADDR,x);
}

/* Routines to clear the TX and RX FIFOs respectively, whie leaving
   interrupt status unchanged */
static inline void Reset_RX_FIFO(volatile unsigned int *uartp)
{
	/* Grab status reg to determine interrupt state */
	unsigned int status = uartp[XUL_STATUS_REG_OFFSET/4];

	/* Merge RX_FIFO reset command with interrupt state */
	uartp[XUL_CONTROL_REG_OFFSET/4] = (status & XUL_CR_ENABLE_INTR) | 
					XUL_CR_FIFO_RX_RESET;
}

static inline void Reset_TX_FIFO(volatile unsigned int *uartp)
{
	unsigned int status = uartp[XUL_STATUS_REG_OFFSET/4];

	uartp[XUL_CONTROL_REG_OFFSET/4] = (status & XUL_CR_ENABLE_INTR) | 
					XUL_CR_FIFO_TX_RESET;
}

/* Disable interrupt generation by UARTlite */
static inline void DisableInterrupts(volatile unsigned int *uartp)
{
	uartp[XUL_CONTROL_REG_OFFSET/4] = 0x00;
}

/* Enable interrupt generation by UARTlite */
static inline void EnableInterrupts(volatile unsigned int *uartp)
{
	uartp[XUL_CONTROL_REG_OFFSET/4] = XUL_CR_ENABLE_INTR;
}

/* Dump ASCII value of x out to debug serial port */
void db_print_num(char x)
{
	int i,j;

	j=x;

	db_print_char((i=j/100)+'0');
	j=j-i*100;
	db_print_char((i=j/10)+'0');
	j=j-i*10;
	db_print_char((i=j)+'0');
	db_print_char('\n');
}

void db_print(char *string)
{
	while(*string)
		db_print_char(*string++);
}

/*
 *	Configuration table, UARTs to look for at startup.
 */
static struct xmb_serial xmbrs_table[] = {
  { 0, (XPAR_CONSOLE_UART_BASEADDR), IRQBASE, ASYNC_BOOT_AUTOCONF }, /* ttyS0 */
  { 0, (XPAR_DEBUG_UART_BASEADDR), IRQBASE+1, ASYNC_BOOT_AUTOCONF } /* ttyS1 */
};

#define	NR_PORTS	(sizeof(xmbrs_table) / sizeof(struct xmb_serial))

static struct tty_struct	*xmbrs_serial_table[NR_PORTS];
static struct termios		*xmbrs_serial_termios[NR_PORTS];
static struct termios		*xmbrs_serial_termios_locked[NR_PORTS];

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

#ifdef CONFIG_MAGIC_SYSRQ
/*
 *	Magic system request keys. Used for debugging...
 */
extern int	magic_sysrq_key(int ch);
#endif


/*
 * tmp_buf is used as a temporary buffer by serial_write.  We need to
 * lock it in case the copy_from_user blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
static unsigned char xmbrs_tmp_buf[4096]; /* This is cheating */
static DECLARE_MUTEX(xmbrs_tmp_buf_sem);

/*
 *	Forward declarations...
 */
static void	xmbrs_wait_until_sent(struct tty_struct *tty, int timeout);


static inline int serial_paranoia_check(struct xmb_serial *info,
					dev_t device, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char *badmagic =
		"Warning: bad magic number for serial struct (%d, %d) in %s\n";
	static const char *badinfo =
		"Warning: null xmb_serial for (%d, %d) in %s\n";

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
 *	Function stubs for boards that do not implement DCD and DTR.
 */
static __inline__ unsigned int xmb_getppdcd(unsigned int portnr)
{
	return(0);
}

static __inline__ unsigned int xmb_getppdtr(unsigned int portnr)
{
	return(0);
}
static __inline__ void xmb_setppdtr(unsigned int portnr, unsigned int dtr)
{
}

/*
 * ------------------------------------------------------------
 * xmbrs_stop() and xmbrs_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * ------------------------------------------------------------
 */
static void xmbrs_stop(struct tty_struct *tty)
{
	volatile unsigned int	*uartp;
	struct xmb_serial	*info = (struct xmb_serial *)tty->driver_data;
	unsigned long		flags;

	if (serial_paranoia_check(info, tty->device, "xmbrs_stop"))
		return;
	
	uartp = (volatile unsigned int *) info->addr;
	save_flags_cli(flags);
	DisableInterrupts(uartp);
	restore_flags(flags);
}

static void xmbrs_start(struct tty_struct *tty)
{
	volatile unsigned int *uartp;
	struct xmb_serial	*info = (struct xmb_serial *)tty->driver_data;
	unsigned long		flags;
	
	if (serial_paranoia_check(info, tty->device, "xmbrs_start"))
		return;
	uartp = (volatile unsigned int *) info->addr;
	save_flags_cli(flags);
	EnableInterrupts(uartp);
	restore_flags(flags);
}

/*
 * ----------------------------------------------------------------------
 *
 * Here starts the interrupt handling routines.  All of the following
 * subroutines are declared as inline and are folded into
 * xmbrs_interrupt().  They were separated out for readability's sake.
 *
 * Note: xmbrs_interrupt() is a "fast" interrupt, which means that it
 * runs with interrupts turned off.  People who may want to modify
 * xmbrs_interrupt() should try to keep the interrupt handler as fast as
 * possible.  After you are done making modifications, it is not a bad
 * idea to do:
 * 
 * gcc -S -DKERNEL -Wall -Wstrict-prototypes -O6 -fomit-frame-pointer serial.c
 *
 * and look at the resulting assemble code in serial.s.
 *
 * 				- Ted Ts'o (tytso@mit.edu), 7-Mar-93
 * -----------------------------------------------------------------------
 */

/*
 * This routine is used by the interrupt handler to schedule
 * processing in the software interrupt portion of the driver.
 */
static _INLINE_ void xmbrs_sched_event(struct xmb_serial *info, int event)
{
	info->event |= 1 << event;
	queue_task(&info->tqueue, &xmb_tq_serial);
	mark_bh(SERIAL_BH);
}

static _INLINE_ void receive_chars(struct xmb_serial *info, 
		volatile unsigned int *uartp)
{
	struct tty_struct	*tty = info->tty;
	unsigned char		status, ch;
	unsigned int 		in_word;

	if (!tty)
		return;

	while ((status=uartp[XUL_STATUS_REG_OFFSET/4]) & 
			XUL_SR_RX_FIFO_VALID_DATA)
	{
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
			/*
			 * can't take any more data.  Turn off receiver
			 * so that the interrupt doesn't continually
			 * occur.
			 */
			/* XUartLite_mDisableIntr(uartp); */
			break;
		}

		/* Grab char from RX FIFO */
		in_word = uartp[XUL_RX_FIFO_OFFSET/4];
		ch = (unsigned char)in_word;

		info->stats.rx++;
		tty->flip.count++;
		*tty->flip.flag_buf_ptr++ = 0;
		
		/* Store recv'd char in buffer */
		*tty->flip.char_buf_ptr++ = ch;
	}

	queue_task(&tty->flip.tqueue, &tq_timer);
	return;
}

static _INLINE_ void transmit_chars(struct xmb_serial *info,
		volatile unsigned int *uartp)
{
	if (info->x_char) {
		/* Send special char, probably flow control */
		unsigned int ch = info->x_char;
		uartp[XUL_TX_FIFO_OFFSET/4] = ch;
		info->x_char=0;
		info->stats.tx++;
	}

	/* Don't send anything if there's nothing to send or tty is stopped */
	if ((info->xmit_cnt <= 0) || info->tty->stopped) {
		db_print_char('t');
		return;
	}
	
	/* Fill the UART TX buffer again */
	while (!(uartp[XUL_STATUS_REG_OFFSET/4] & XUL_SR_TX_FIFO_FULL)) {
		unsigned int ch = info->xmit_buf[info->xmit_tail++];
		uartp[XUL_TX_FIFO_OFFSET/4] = ch;
		info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
		info->stats.tx++;
		if (--info->xmit_cnt <= 0)
			break;
	}

	if (info->xmit_cnt < WAKEUP_CHARS)
		xmbrs_sched_event(info, RS_EVENT_WRITE_WAKEUP);

	return;
}

/*
 * This is the serial driver's generic interrupt routine
 */
void xmbrs_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct xmb_serial	*info;
	unsigned int isr;
	volatile unsigned int *uartp;

	info = &xmbrs_table[(irq - IRQBASE)];
	uartp = (volatile unsigned int *) info->addr;

	isr = uartp[XUL_STATUS_REG_OFFSET/4];

	/* Grab any valid chars */
	if (isr & XUL_SR_RX_FIFO_VALID_DATA)
		receive_chars(info, uartp);

	/* If the TX fifo has any room then refill it */
	/* Better to refill now and delay another interrupt overhead */
	if(!(isr & XUL_SR_TX_FIFO_FULL))
		transmit_chars(info, uartp); 

	return;
}

/*
 * -------------------------------------------------------------------
 * Here ends the serial interrupt routines.
 * -------------------------------------------------------------------
 */

/*
 * This routine is used to handle the "bottom half" processing for the
 * serial driver, known also the "software interrupt" processing.
 * This processing is done at the kernel interrupt level, after the
 * xmbrs_interrupt() has returned, BUT WITH INTERRUPTS TURNED ON.  This
 * is where time-consuming activities which can not be done in the
 * interrupt driver proper are done; the interrupt driver schedules
 * them using xmbrs_sched_event(), and they get done here.
 */
static void do_serial_bh(void)
{
	run_task_queue(&xmb_tq_serial);
}

static void do_softint(void *private_)
{
	struct xmb_serial	*info = (struct xmb_serial *) private_;
	struct tty_struct	*tty;
	
	tty = info->tty;
	if (!tty)
		return;

	if (test_and_clear_bit(RS_EVENT_WRITE_WAKEUP, &info->event)) {
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
		    tty->ldisc.write_wakeup)
			(tty->ldisc.write_wakeup)(tty);
		wake_up_interruptible(&tty->write_wait);
	}
}


/*
 *	Change of state on a DCD line.
 */
void xmbrs_modem_change(struct xmb_serial *info, int dcd)
{
	if (info->count == 0)
		return;

	if (info->flags & ASYNC_CHECK_CD) {
		if (dcd) {
			wake_up_interruptible(&info->open_wait);
		} else if (!((info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (info->flags & ASYNC_CALLOUT_NOHUP))) {
			schedule_task(&info->tqueue_hangup);
		}
	}
}

/*
 * This routine is called from the scheduler tqueue when the interrupt
 * routine has signalled that a hangup has occurred.  The path of
 * hangup processing is:
 *
 * 	serial interrupt routine -> (scheduler tqueue) ->
 * 	do_serial_hangup() -> tty->hangup() -> xmbrs_hangup()
 * 
 */
static void do_serial_hangup(void *private_)
{
	struct xmb_serial	*info = (struct xmb_serial *) private_;
	struct tty_struct	*tty;
	
	tty = info->tty;
	if (!tty)
		return;

	tty_hangup(tty);
}

static int startup(struct xmb_serial * info)
{
	volatile unsigned int *uartp;
	unsigned long		flags;
	
	if (info->flags & ASYNC_INITIALIZED)
		return 0;

	if (!info->xmit_buf) {
		info->xmit_buf = (unsigned char *) get_free_page(GFP_KERNEL);
		if (!info->xmit_buf)
			return -ENOMEM;
	}

	save_flags_cli(flags);

#ifdef SERIAL_DEBUG_OPEN
	printk("starting up ttyS%d (irq %d)...\n", info->line, info->irq);
#endif

	if (info->tty)
		clear_bit(TTY_IO_ERROR, &info->tty->flags);
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;

	/*
	 * Lastly enable the UART transmitter and receiver, and
	 * interrupt enables.
	 */
	/*
	 *	Reset UART, get it into known state...
	 */
	uartp = (volatile unsigned int *) info->addr;
	Reset_RX_FIFO(uartp);
	EnableInterrupts(uartp);

	info->flags |= ASYNC_INITIALIZED;
	restore_flags(flags);
	return 0;
}

/*
 * This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */
static void shutdown(struct xmb_serial * info)
{
	volatile unsigned int *uartp;
	unsigned long		flags;

	if (!(info->flags & ASYNC_INITIALIZED))
		return;

#ifdef SERIAL_DEBUG_OPEN
	printk("Shutting down serial port %d (irq %d)....\n", info->line,
	       info->irq);
#endif
	
	uartp = (volatile unsigned int *) info->addr;
	save_flags_cli(flags); 		/* Disable interrupts */

	/* Disable interrupts and clear RX FIFO */
	DisableInterrupts(uartp);
	Reset_RX_FIFO(uartp);

	if (info->xmit_buf) {
		free_page((unsigned long) info->xmit_buf);
		info->xmit_buf = 0;
	}

	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);
	
	info->flags &= ~ASYNC_INITIALIZED;
	restore_flags(flags);
}

/* Force a refill of the TX FIFO.  This happens in user space, outside of
   interrupts so we cli() for the duration to prevent corruption of the
   output buffer data structure */
void force_tx_fifo_fill(struct xmb_serial *info)
{
	unsigned int flags;
	volatile unsigned int *uartp=(volatile unsigned int *)info->addr;

	save_flags(flags);

	/* Block until TX FIFO can take more chars */
	while(uartp[XUL_STATUS_REG_OFFSET/4] & XUL_SR_TX_FIFO_FULL)
		;
	cli();

	/* Force out any special character */
	if (info->x_char) {
		/* Send special char, probably flow control */
		unsigned int ch = info->x_char;
		uartp[XUL_TX_FIFO_OFFSET/4]=ch;
		info->x_char=0;
		info->stats.tx++;
	}

	restore_flags(flags);

	cli();
	/* Don't send anything if there's nothing to send or tty is stopped */
	if ((info->xmit_cnt <= 0) || info->tty->stopped) {
		restore_flags(flags);
		return;
	}
	restore_flags(flags);
	
	/* Fill the UART TX buffer again */
	while(1) {
		unsigned int ch;

		if(uartp[XUL_STATUS_REG_OFFSET/4] & XUL_SR_TX_FIFO_FULL)
			break;

		cli();
		ch = info->xmit_buf[info->xmit_tail++];
		uartp[XUL_TX_FIFO_OFFSET/4]=ch;
		info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
		info->stats.tx++;
		if (--info->xmit_cnt <= 0)
			break;
		restore_flags(flags);
	}

	restore_flags(flags);

	if (info->xmit_cnt < WAKEUP_CHARS)
		xmbrs_sched_event(info, RS_EVENT_WRITE_WAKEUP);

	return;
}


static void xmbrs_flush_chars(struct tty_struct *tty)
{
	volatile unsigned int *uartp;
	struct xmb_serial	*info = (struct xmb_serial *)tty->driver_data;
	unsigned long		flags;

	if (serial_paranoia_check(info, tty->device, "xmbrs_flush_chars"))
		return;

	uartp = (volatile unsigned int *) info->addr;
	EnableInterrupts(uartp);

	/* If there are chars waiting in RX buffer then enable interrupt
	   to permit receiving them */
	save_flags_cli(flags);
	if ( (uartp[XUL_STATUS_REG_OFFSET/4] & XUL_SR_RX_FIFO_VALID_DATA) &&
			(info->flags & ASYNC_INITIALIZED) ) {
		EnableInterrupts(uartp);
	}

	/* Any chars pending to go out (and tty not stopped etc)? */
	if (info->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped ||
	    !info->xmit_buf)
		return;

	/* Force remaining chars out */
	save_flags_cli(flags);
	EnableInterrupts(uartp);
	force_tx_fifo_fill(info);
	restore_flags(flags);
}

static int xmbrs_write(struct tty_struct * tty, int from_user,
		    const unsigned char *buf, int count)
{
	struct xmb_serial	*info = (struct xmb_serial *)tty->driver_data;
	unsigned long		flags;
	int			c, total = 0;
	volatile unsigned int *uartp = (volatile unsigned int *)info->addr;

	if (serial_paranoia_check(info, tty->device, "xmbrs_write"))
		return 0;

	if (!tty || !info->xmit_buf)
		return 0;
	
	save_flags(flags);
	while (1) {
		cli();		
		c = MIN(count, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
				   SERIAL_XMIT_SIZE - info->xmit_head));

		if (c <= 0) {
			restore_flags(flags);
			break;
		}

		if (from_user) {
			down(&xmbrs_tmp_buf_sem);
			copy_from_user(xmbrs_tmp_buf, buf, c);
			restore_flags(flags);
			cli();		
			c = MIN(c, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
				       SERIAL_XMIT_SIZE - info->xmit_head));
			memcpy(info->xmit_buf + info->xmit_head, xmbrs_tmp_buf, c);
			up(&xmbrs_tmp_buf_sem);
		} else
			memcpy(info->xmit_buf + info->xmit_head, buf, c);

		info->xmit_head = (info->xmit_head + c) & (SERIAL_XMIT_SIZE-1);
		info->xmit_cnt += c;
		restore_flags(flags); 

		buf += c;
		count -= c;
		total += c;
	}

	if(!tty->stopped) {
		cli();
		EnableInterrupts(uartp);

		/* Kick start the TX FIFO if necessary */
		if(uartp[XUL_STATUS_REG_OFFSET/4] & XUL_SR_TX_FIFO_EMPTY)
			force_tx_fifo_fill(info); 
		restore_flags(flags);
	}

	return total;
}

static int xmbrs_write_room(struct tty_struct *tty)
{
	struct xmb_serial *info = (struct xmb_serial *)tty->driver_data;
	int	ret;
				
	if (serial_paranoia_check(info, tty->device, "xmbrs_write_room"))
		return 0;
	ret = SERIAL_XMIT_SIZE - info->xmit_cnt - 1;
	if (ret < 0)
		ret = 0;
	return ret;
}

static int xmbrs_chars_in_buffer(struct tty_struct *tty)
{
	struct xmb_serial *info = (struct xmb_serial *)tty->driver_data;
					
	if (serial_paranoia_check(info, tty->device, "xmbrs_chars_in_buffer"))
		return 0;

	return (info->xmit_cnt);
}

static void xmbrs_flush_buffer(struct tty_struct *tty)
{
	struct xmb_serial	*info = (struct xmb_serial *)tty->driver_data;
	unsigned long		flags;
				
	if (serial_paranoia_check(info, tty->device, "xmbrs_flush_buffer"))
		return;

	save_flags_cli(flags);
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	restore_flags(flags);

	wake_up_interruptible(&tty->write_wait);
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
}

/*
 * ------------------------------------------------------------
 * xmbrs_throttle()
 * 
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 * ------------------------------------------------------------
 */
static void xmbrs_throttle(struct tty_struct * tty)
{
	struct xmb_serial *info = (struct xmb_serial *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("throttle %s: %d....\n", _tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "xmbrs_throttle"))
		return;
	
	if (I_IXOFF(tty)) {
		/* Force STOP_CHAR (xoff) out */
		volatile unsigned int *uartp;
		unsigned long		flags;
		info->x_char = STOP_CHAR(tty);
		uartp = (volatile unsigned int *) info->addr;
		save_flags_cli(flags);
		EnableInterrupts(uartp);
		force_tx_fifo_fill(info);
		restore_flags(flags);
	}

}

static void xmbrs_unthrottle(struct tty_struct * tty)
{
	struct xmb_serial *info = (struct xmb_serial *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("unthrottle %s: %d....\n", _tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "xmbrs_unthrottle"))
		return;
	
	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else {
			/* Force START_CHAR (xon) out */
			volatile unsigned int *uartp;
			unsigned long		flags;
			info->x_char = START_CHAR(tty);
			uartp = (volatile unsigned int *) info->addr;
			save_flags_cli(flags);
			EnableInterrupts(uartp);
			force_tx_fifo_fill(info); 
			restore_flags(flags);
		}
	}

}

/*
 * ------------------------------------------------------------
 * xmbrs_ioctl() and friends
 * ------------------------------------------------------------
 */

static int get_serial_info(struct xmb_serial * info,
			   struct serial_struct * retinfo)
{
	struct serial_struct tmp;
  
	if (!retinfo)
		return -EFAULT;
	memset(&tmp, 0, sizeof(tmp));
	tmp.type = info->type;
	tmp.line = info->line;
	tmp.port = info->addr;
	tmp.irq = info->irq;
	tmp.flags = info->flags;
	tmp.baud_base = info->baud_base;
	tmp.close_delay = info->close_delay;
	tmp.closing_wait = info->closing_wait;
	tmp.custom_divisor = info->custom_divisor;
	copy_to_user(retinfo,&tmp,sizeof(*retinfo));
	return 0;
}

static int set_serial_info(struct xmb_serial * info,
			   struct serial_struct * new_info)
{
	struct serial_struct new_serial;
	struct xmb_serial old_info;
	int 			retval = 0;

	if (!new_info)
		return -EFAULT;
	copy_from_user(&new_serial,new_info,sizeof(new_serial));
	old_info = *info;

	if (!suser()) {
		if ((new_serial.baud_base != info->baud_base) ||
		    (new_serial.type != info->type) ||
		    (new_serial.close_delay != info->close_delay) ||
		    ((new_serial.flags & ~ASYNC_USR_MASK) !=
		     (info->flags & ~ASYNC_USR_MASK)))
			return -EPERM;
		info->flags = ((info->flags & ~ASYNC_USR_MASK) |
			       (new_serial.flags & ASYNC_USR_MASK));
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
	info->flags = ((info->flags & ~ASYNC_FLAGS) |
			(new_serial.flags & ASYNC_FLAGS));
	info->type = new_serial.type;
	info->close_delay = new_serial.close_delay;
	info->closing_wait = new_serial.closing_wait;

check_and_exit:
	retval = startup(info);
	return retval;
}

/*
 * get_lsr_info - get line status register info
 *
 * Purpose: Let user call ioctl() to get info when the UART physically
 * 	    is emptied.  On bus types like RS485, the transmitter must
 * 	    release the bus after transmitting. This must be done when
 * 	    the transmit shift register is empty, not be done when the
 * 	    transmit holding register is empty.  This functionality
 * 	    allows an RS485 driver to be written in user space. 
 */
static int get_lsr_info(struct xmb_serial * info, unsigned int *value)
{
	volatile unsigned int *uartp;
	unsigned long		flags;
	unsigned char		status;

	uartp = (volatile unsigned int *) info->addr;
	save_flags_cli(flags);
	/* FIXME */
	status = (uartp[XUL_STATUS_REG_OFFSET/4] & XUL_SR_TX_FIFO_EMPTY) ? TIOCSER_TEMT : 0;
	restore_flags(flags);

	put_user(status,value);
	return 0;
}

/*
 * This routine sends a break character out the serial port.
 */
static void send_break(	struct xmb_serial * info, int duration)
{
	volatile unsigned int *uartp;
	unsigned long		flags;

	if (!info->addr)
		return;
	current->state = TASK_INTERRUPTIBLE;
	uartp = (volatile unsigned int *) info->addr;

	save_flags_cli(flags);
	/* uartp[XMBUART_UCR] = XMBUART_UCR_CMDBREAKSTART; */
	schedule_timeout(duration);
	/* uartp[XMBUART_UCR] = XMBUART_UCR_CMDBREAKSTOP; */
	restore_flags(flags);
}

static int xmbrs_ioctl(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg)
{
	struct xmb_serial * info = (struct xmb_serial *)tty->driver_data;
	unsigned int val;
	int retval, error;
	int dtr, rts;

	if (serial_paranoia_check(info, tty->device, "xmbrs_ioctl"))
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
						sizeof(struct xmb_serial));
			if (error)
				return error;
			copy_to_user((struct xmb_serial *) arg,
				    info, sizeof(struct xmb_serial));
			return 0;
			
		case TIOCMGET:
			if ((error = verify_area(VERIFY_WRITE, (void *) arg,
                            sizeof(unsigned int))))
                                return(error);
			val = 0;
			put_user(val, (unsigned int *) arg);
			break;

                case TIOCMBIS:
			if ((error = verify_area(VERIFY_WRITE, (void *) arg,
                            sizeof(unsigned int))))
				return(error);

			get_user(val, (unsigned int *) arg);
			rts = (val & TIOCM_RTS) ? 1 : -1;
			dtr = (val & TIOCM_DTR) ? 1 : -1;
			break;

                case TIOCMBIC:
			if ((error = verify_area(VERIFY_WRITE, (void *) arg,
                            sizeof(unsigned int))))
				return(error);
			get_user(val, (unsigned int *) arg);
			rts = (val & TIOCM_RTS) ? 0 : -1;
			dtr = (val & TIOCM_DTR) ? 0 : -1;
			break;

                case TIOCMSET:
			if ((error = verify_area(VERIFY_WRITE, (void *) arg,
                            sizeof(unsigned int))))
				return(error);
			get_user(val, (unsigned int *) arg);
			rts = (val & TIOCM_RTS) ? 1 : 0;
			dtr = (val & TIOCM_DTR) ? 1 : 0;
			break;

#ifdef TIOCSET422
		case TIOCSET422:
			get_user(val, (unsigned int *) arg);
			xmb_setpa(XMBPP_PA11, (val ? 0 : XMBPP_PA11));
			break;
		case TIOCGET422:
			val = (xmb_getpa() & XMBPP_PA11) ? 0 : 1;
			put_user(val, (unsigned int *) arg);
			break;
#endif

		default:
			return -ENOIOCTLCMD;
		}
	return 0;
}

static void xmbrs_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	struct xmb_serial *info = (struct xmb_serial *)tty->driver_data;

	if (tty->termios->c_cflag == old_termios->c_cflag)
		return;

	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
#if 0
		xmbrs_start(tty);
#endif
	}
}

/*
 * ------------------------------------------------------------
 * xmbrs_close()
 * 
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent.  Then, we unlink its
 * S structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 * ------------------------------------------------------------
 */
static void xmbrs_close(struct tty_struct *tty, struct file * filp)
{
	volatile unsigned int *uartp;
	struct xmb_serial	*info = (struct xmb_serial *)tty->driver_data;
	unsigned long		flags;

	if (!info || serial_paranoia_check(info, tty->device, "xmbrs_close"))
		return;
	
	save_flags_cli(flags);
	
	if (tty_hung_up_p(filp)) {
		restore_flags(flags);
		return;
	}
	
#ifdef SERIAL_DEBUG_OPEN
	printk("xmbrs_close ttyS%d, count = %d\n", info->line, info->count);
#endif
	if ((tty->count == 1) && (info->count != 1)) {
		/*
		 * Uh, oh.  tty->count is 1, which means that the tty
		 * structure will be freed.  Info->count should always
		 * be one in these conditions.  If it's greater than
		 * one, we've got real problems, since it means the
		 * serial port won't be shutdown.
		 */
		printk("xmbrs_close: bad serial port count; tty->count is 1, "
		       "info->count is %d\n", info->count);
		info->count = 1;
	}
	if (--info->count < 0) {
		printk("xmbrs_close: bad serial port count for ttyS%d: %d\n",
		       info->line, info->count);
		info->count = 0;
	}
	if (info->count) {
		restore_flags(flags);
		return;
	}
	info->flags |= ASYNC_CLOSING;

	/*
	 * Save the termios structure, since this port may have
	 * separate termios for callout and dialin.
	 */
	if (info->flags & ASYNC_NORMAL_ACTIVE)
		info->normal_termios = *tty->termios;
	if (info->flags & ASYNC_CALLOUT_ACTIVE)
		info->callout_termios = *tty->termios;

	/*
	 * Now we wait for the transmit buffer to clear; and we notify 
	 * the line discipline to only process XON/XOFF characters.
	 */
	tty->closing = 1;
	if (info->closing_wait != ASYNC_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, info->closing_wait);

	/*
	 * At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and tell the
	 * interrupt driver to stop checking the data ready bit in the
	 * line status register.
	 */
	uartp = (volatile unsigned int *) info->addr;
	DisableInterrupts(uartp);

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
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE|
			 ASYNC_CLOSING);
	wake_up_interruptible(&info->close_wait);
	restore_flags(flags);
}

/*
 * xmbrs_wait_until_sent() --- wait until the transmitter is empty
 */
static void
xmbrs_wait_until_sent(struct tty_struct *tty, int timeout)
{

	struct xmb_serial * info = (struct xmb_serial *)tty->driver_data;
	volatile unsigned int *uartp;
	unsigned long orig_jiffies, fifo_time, char_time, fifo_cnt;
	
	if (serial_paranoia_check(info, tty->device, "xmbrs_wait_until_sent"))
		return;
	
	orig_jiffies = jiffies;

	/*
	 * Set the check interval to be 1/5 of the approximate time
	 * to send the entire fifo, and make it at least 1.  The check
	 * interval should also be less than the timeout.
	 *
	 * Note: we have to use pretty tight timings here to satisfy
	 * the NIST-PCTS.
	 */
	fifo_time = (XUL_FIFO_SIZE * HZ * 10) / info->baud;
	char_time = fifo_time / 5;
	if (char_time == 0)
		char_time = 1;
	if (timeout && timeout < char_time)
		char_time = timeout;

	/*
	 * Clamp the timeout period at 2 * the time to empty the
	 * fifo.  Just to be safe, set the minimum at .5 seconds.
	 */
	fifo_time *= 2;
	if (fifo_time < (HZ/2))
		fifo_time = HZ/2;
	if (!timeout || timeout > fifo_time)
		timeout = fifo_time;

	/*
	 * Account for the number of bytes in the UART
	 * transmitter FIFO plus any byte being shifted out.
	 */
	uartp = (volatile unsigned int *) info->addr;
	for (;;) {
		unsigned int status = uartp[XUL_STATUS_REG_OFFSET/4];

		if(status & XUL_SR_TX_FIFO_EMPTY)
			break;

		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(char_time);
		if (signal_pending(current))
			break;
		if (timeout && time_after(jiffies, orig_jiffies + timeout))
			break;
	}
}
		
/*
 * xmbrs_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
void xmbrs_hangup(struct tty_struct *tty)
{
	struct xmb_serial * info = (struct xmb_serial *)tty->driver_data;
	
	if (serial_paranoia_check(info, tty->device, "xmbrs_hangup"))
		return;
	
	xmbrs_flush_buffer(tty);
	shutdown(info);
	info->event = 0;
	info->count = 0;
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE);
	info->tty = 0;
	wake_up_interruptible(&info->open_wait);
}

/*
 * ------------------------------------------------------------
 * xmbrs_open() and friends
 * ------------------------------------------------------------
 */
static int block_til_ready(struct tty_struct *tty, struct file * filp,
			   struct xmb_serial *info)
{
	DECLARE_WAITQUEUE(wait, current);
	int		retval;
	int		do_clocal = 0;

	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (info->flags & ASYNC_CLOSING) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
		if (info->flags & ASYNC_HUP_NOTIFY)
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
		if (info->flags & ASYNC_NORMAL_ACTIVE)
			return -EBUSY;
		if ((info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (info->flags & ASYNC_SESSION_LOCKOUT) &&
		    (info->session != current->session))
		    return -EBUSY;
		if ((info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (info->flags & ASYNC_PGRP_LOCKOUT) &&
		    (info->pgrp != current->pgrp))
		    return -EBUSY;
		info->flags |= ASYNC_CALLOUT_ACTIVE;
		return 0;
	}
	
	/*
	 * If non-blocking mode is set, or the port is not enabled,
	 * then make the check up front and then exit.
	 */
	if ((filp->f_flags & O_NONBLOCK) ||
	    (tty->flags & (1 << TTY_IO_ERROR))) {
		if (info->flags & ASYNC_CALLOUT_ACTIVE)
			return -EBUSY;
		info->flags |= ASYNC_NORMAL_ACTIVE;
		return 0;
	}

	if (info->flags & ASYNC_CALLOUT_ACTIVE) {
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
	 * xmbrs_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */
	retval = 0;
	add_wait_queue(&info->open_wait, &wait);
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready before block: ttyS%d, count = %d\n",
	       info->line, info->count);
#endif
	info->count--;
	info->blocked_open++;
	while (1) {
		cli();
		sti();
		current->state = TASK_INTERRUPTIBLE;
		if (tty_hung_up_p(filp) ||
		    !(info->flags & ASYNC_INITIALIZED)) {
#ifdef SERIAL_DO_RESTART
			if (info->flags & ASYNC_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;	
#else
			retval = -EAGAIN;
#endif
			break;
		}
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    !(info->flags & ASYNC_CLOSING) &&
		    (do_clocal))
			break;
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
#ifdef SERIAL_DEBUG_OPEN
		printk("block_til_ready blocking: ttyS%d, count = %d\n",
		       info->line, info->count);
#endif
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&info->open_wait, &wait);
	if (!tty_hung_up_p(filp))
		info->count++;
	info->blocked_open--;
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready after blocking: ttyS%d, count = %d\n",
	       info->line, info->count);
#endif
	if (retval)
		return retval;
	info->flags |= ASYNC_NORMAL_ACTIVE;
	return 0;
}	

/*
 * This routine is called whenever a serial port is opened. It
 * enables interrupts for a serial port, linking in its structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */
int xmbrs_open(struct tty_struct *tty, struct file * filp)
{
	struct xmb_serial	*info;
	int 			retval, line;

	line = MINOR(tty->device) - tty->driver.minor_start;
	if ((line < 0) || (line >= NR_PORTS))
		return -ENODEV;
	info = xmbrs_table + line;
	if (serial_paranoia_check(info, tty->device, "xmbrs_open"))
		return -ENODEV;
#ifdef SERIAL_DEBUG_OPEN
	printk("xmbrs_open ttyS%d, count = %d\n", info->line, info->count);
#endif
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
#ifdef SERIAL_DEBUG_OPEN
		printk("xmbrs_open returning after block_til_ready with %d\n",
		       retval);
#endif
		return retval;
	}

	if ((info->count == 1) && (info->flags & ASYNC_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = info->normal_termios;
		else 
			*tty->termios = info->callout_termios;
	}

	info->session = current->session;
	info->pgrp = current->pgrp;

#ifdef SERIAL_DEBUG_OPEN
	printk("xmbrs_open ttyS%d successful...\n", info->line);
#endif
	return 0;
}

/*
 *	Based on the line number set up the internal interrupt stuff.
 */
static void xmbrs_irqinit(struct xmb_serial *info)
{
	volatile unsigned int *uartp;

	switch (info->line) {
	case 0:
	case 1:
		break;
	default:
		printk("SERIAL: don't know how to handle UART %d interrupt?\n",
			info->line);
		return;
	}

	uartp = (volatile unsigned int *) info->addr;

	/* Clear mask, so no surprise interrupts. */
	DisableInterrupts(uartp);

	if (request_irq(info->irq, xmbrs_interrupt, SA_INTERRUPT,
	    "Microblaze UARTlite", NULL)) {
		printk("SERIAL: Unable to attach Xilinx UARTlite %d interrupt "
			"vector=%d\n", info->line, info->irq);
	}

	/* UART still can't generate interrupts - leave it this way
	   until it is actually opened */

	return;
}

char *xmbrs_drivername = "Microblaze UARTlite serial driver version 1.00\n";

/*
 * Serial stats reporting...
 */
int xmbrs_readproc(char *page, char **start, off_t off, int count,
		         int *eof, void *data)
{
	struct xmb_serial	*info;
	char			str[20];
	int			len, i;

	len = sprintf(page, xmbrs_drivername);
	for (i = 0; (i < NR_PORTS); i++) {
		info = &xmbrs_table[i];
		len += sprintf((page + len), "%d: port:%x irq=%d baud:%d ",
			i, info->addr, info->irq, info->baud);
		if (info->stats.rx || info->stats.tx)
			len += sprintf((page + len), "tx:%d rx:%d ",
			info->stats.tx, info->stats.rx);
		if (info->stats.rxframing)
			len += sprintf((page + len), "fe:%d ",
			info->stats.rxframing);
		if (info->stats.rxparity)
			len += sprintf((page + len), "pe:%d ",
			info->stats.rxparity);
		if (info->stats.rxbreak)
			len += sprintf((page + len), "brk:%d ",
			info->stats.rxbreak);
		if (info->stats.rxoverrun)
			len += sprintf((page + len), "oe:%d ",
			info->stats.rxoverrun);

		str[0] = str[1] = 0;

		len += sprintf((page + len), "%s\n", &str[1]);
	}

	return(len);
}


/* Finally, routines used to initialize the serial driver. */

static void show_serial_version(void)
{
	printk(xmbrs_drivername);
}

/* xmbrs_init inits the driver */
static int __init
xmbrs_init(void)
{
	struct xmb_serial	*info;
	unsigned long		flags;
	int			i;

	init_bh(SERIAL_BH, do_serial_bh);

	show_serial_version();

	/* Initialize the tty_driver structure */
	memset(&xmbrs_serial_driver, 0, sizeof(struct tty_driver));
	xmbrs_serial_driver.magic = TTY_DRIVER_MAGIC;
	xmbrs_serial_driver.name = "ttyS";
	xmbrs_serial_driver.major = TTY_MAJOR;
	xmbrs_serial_driver.minor_start = 64;
	xmbrs_serial_driver.num = NR_PORTS;
	xmbrs_serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	xmbrs_serial_driver.subtype = SERIAL_TYPE_NORMAL;
	xmbrs_serial_driver.init_termios = tty_std_termios;

	xmbrs_serial_driver.init_termios.c_cflag =
		xmbrs_console_cbaud | CS8 | CREAD | HUPCL | CLOCAL;
	xmbrs_serial_driver.flags = TTY_DRIVER_REAL_RAW;
	xmbrs_serial_driver.refcount = &xmbrs_serial_refcount;
	xmbrs_serial_driver.table = xmbrs_serial_table;
	xmbrs_serial_driver.termios = xmbrs_serial_termios;
	xmbrs_serial_driver.termios_locked = xmbrs_serial_termios_locked;

	xmbrs_serial_driver.open = xmbrs_open;
	xmbrs_serial_driver.close = xmbrs_close;
	xmbrs_serial_driver.write = xmbrs_write;
 	xmbrs_serial_driver.flush_chars = xmbrs_flush_chars;	
	xmbrs_serial_driver.write_room = xmbrs_write_room;	
	xmbrs_serial_driver.chars_in_buffer = xmbrs_chars_in_buffer;	
	xmbrs_serial_driver.flush_buffer = xmbrs_flush_buffer;		
	xmbrs_serial_driver.ioctl = xmbrs_ioctl;			
	xmbrs_serial_driver.throttle = xmbrs_throttle;			
	xmbrs_serial_driver.unthrottle = xmbrs_unthrottle;		
	xmbrs_serial_driver.set_termios = xmbrs_set_termios;		
	xmbrs_serial_driver.stop = xmbrs_stop;			
	xmbrs_serial_driver.start = xmbrs_start;
	xmbrs_serial_driver.hangup = xmbrs_hangup;			
	xmbrs_serial_driver.read_proc = xmbrs_readproc;			
	xmbrs_serial_driver.wait_until_sent = xmbrs_wait_until_sent;	
	xmbrs_serial_driver.driver_name = "serial";

	/*
	 * The callout device is just like normal device except for
	 * major number and the subtype code.
	 */
	xmbrs_callout_driver = xmbrs_serial_driver;
	xmbrs_callout_driver.name = "cua";
	xmbrs_callout_driver.major = TTYAUX_MAJOR;
	xmbrs_callout_driver.subtype = SERIAL_TYPE_CALLOUT;
	xmbrs_callout_driver.read_proc = 0;
	xmbrs_callout_driver.proc_entry = 0;

	if (tty_register_driver(&xmbrs_serial_driver)) {
		printk(__FUNCTION__ ": Couldn't register serial driver\n");
		return(-EBUSY);
	}

	if (tty_register_driver(&xmbrs_callout_driver)) {
		printk(__FUNCTION__ ": Couldn't register callout driver\n");
		return(-EBUSY);
	}
	
	save_flags_cli(flags); 

	/*
	 *	Configure all the attached serial ports.
	 */
	for (i = 0, info = xmbrs_table; (i < NR_PORTS); i++, info++) {
		info->magic = SERIAL_MAGIC;
		info->line = i;
		info->tty = 0;
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
		info->callout_termios = xmbrs_callout_driver.init_termios;
		info->normal_termios = xmbrs_serial_driver.init_termios;
		init_waitqueue_head(&info->open_wait);
		init_waitqueue_head(&info->close_wait);

		xmbrs_irqinit(info);

		printk("%s%d at 0x%04x (irq = %d)", xmbrs_serial_driver.name,
			info->line, info->addr, info->irq);
		printk(" is a Microblaze UARTlite\n");
	}

	restore_flags(flags);
	return 0;
}

module_init(xmbrs_init);
/* module_exit(xmbrs_fini); */

/****************************************************************************/
/*                          Serial Console                                  */
/****************************************************************************/

/*
 *	Quick and dirty UART initialization, for console output.
 */

void xmbrs_init_console(void)
{
	/*
	 *	Reset UART, get it into known state...
	 */

	volatile unsigned int *uartp= (volatile unsigned int *) XPAR_CONSOLE_UART_BASEADDR;

	/* Reset TX and RX FIFOs */
	Reset_RX_FIFO(uartp);
	Reset_TX_FIFO(uartp);

	/* Disable UART interrupt generation */
	DisableInterrupts(uartp);

	xmbrs_console_inited++;
	return;
}


/*
 *	Setup for console. Argument comes from the boot command line.
 */

int xmbrs_console_setup(struct console *cp, char *arg)
{
	if (!cp)
		return(-1);

	xmbrs_console_port = cp->index;
	xmbrs_init_console(); 
	return(0);
}


static kdev_t xmbrs_console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, 64 + c->index);
}


/*
 *	Output a single character, using UART polled mode.
 *	This is used for console output.
 */

void xmbrs_put_char(char ch)
{
	volatile unsigned int *uartp, out_word=ch;
	unsigned long		flags;
	int			i;

	uartp = (volatile unsigned int *) XPAR_CONSOLE_UART_BASEADDR;

	save_flags_cli(flags);

	/* wait for tx buffer not full */
	for (i = 0; (i < 0x10000); i++) {
		if (!(uartp[XUL_STATUS_REG_OFFSET/4] & XUL_SR_TX_FIFO_FULL))
			break;
	}

	/* Did it drain? */
	if (i < 0x10000) {
		/* Send the char */
		uartp[XUL_TX_FIFO_OFFSET/4]=out_word;

		/* Wait for it to go */
		for (i = 0; (i < 0x10000); i++)
			if (!(uartp[XUL_STATUS_REG_OFFSET/4] & XUL_SR_TX_FIFO_FULL))
				break;
	}

	/* Was it sent? */
	if (i >= 0x10000)
		xmbrs_init_console(); /* try and get it back */

	restore_flags(flags);

	return;
}


/*
 * rs_console_write is registered for printk output.
 */

void xmbrs_console_write(struct console *cp, const char *p, unsigned len)
{
	if (!xmbrs_console_inited)
		xmbrs_init_console();
	while (len-- > 0) {
		if (*p == '\n')
			xmbrs_put_char('\r');
		xmbrs_put_char(*p++);
	}
}

/*
 * declare our consoles
 */

struct console xmbrs_console = {
	name:		"ttyS",
	write:		xmbrs_console_write,
	read:		NULL,
	device:		xmbrs_console_device,
	unblank:	NULL,
	setup:		xmbrs_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
	cflag:		0,
	next:		NULL
};

/* void __init xmbrs_console_init(void) */
void xmbrs_console_init(void)
{
	register_console(&xmbrs_console);
	printk(KERN_INFO "Console: xmbserial on UARTLite\n");
}

/****************************************************************************/
