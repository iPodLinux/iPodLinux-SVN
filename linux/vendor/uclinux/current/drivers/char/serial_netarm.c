/*
 * linux/drivers/char/serial_netarm.c
 *
 * Copyright (C) 2001  IMMS gGmbH
 *
 * This software is provided "AS-IS" and without any express or implied 
 * warranties or conditions.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This particular file is derived from serial.c by Linus Torvalds and
 * Theodore Ts'o and from Joe deBlaquiere's serial-netarm.c from
 * the 2.0.38 Net-Lx kernel.
 * see linux/drivers/char/serial.c for version history and credits
 *
 * 06/01: first working adaption to Net+ARM built-in serial interfaces
 *
 * author(s) : Rolf Peukert
 *
 */

static char *serial_version = "0.2";
static char *serial_revdate = "2002-02-27";

/*
 * Serial driver configuration section.  Here are the various options:
 *
 * SERIAL_PARANOIA_CHECK
 * 		Check the magic number for the netarm_async structure
 * 		where ever possible.
 */

#include <linux/config.h>
#include <linux/version.h>

#undef SERIAL_PARANOIA_CHECK
#define CONFIG_SERIAL_NOPAUSE_IO
#define SERIAL_DO_RESTART

/* Set of debugging defines */

#undef SERIAL_DEBUG_INTR
#undef SERIAL_DEBUG_OPEN
#undef SERIAL_DEBUG_FLOW
#undef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
#undef SERIAL_DEBUG_PCI
#undef SERIAL_DEBUG_AUTOCONF

/* Sanity checks */

#ifdef MODULE
#undef CONFIG_SERIAL_NETARM_CONSOLE
#endif

#define RS_STROBE_TIME (10*HZ)
#define RS_ISR_PASS_LIMIT 256

/*
 * End of serial driver configuration section.
 */

#include <linux/module.h>

#include <linux/types.h>
#ifdef LOCAL_HEADERS
#include "serial_local.h"
#else
#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/serial_reg.h>
#include <asm/serial.h>
#define LOCAL_VERSTRING ""
#endif

#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#ifdef CONFIG_SERIAL_NETARM_CONSOLE
#include <linux/console.h>
#endif
#ifdef CONFIG_MAGIC_SYSRQ
#include <linux/sysrq.h>
#endif
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>

#include <asm/arch/netarm_registers.h>

#ifdef SERIAL_NETARM_INLINE
#define _INLINE_ inline
#else
#define _INLINE_
#endif

static char *serial_name = "Net+ARM serial driver";

static DECLARE_TASK_QUEUE(tq_serial);

static struct tty_driver serial_driver, callout_driver;
static int serial_refcount;

static struct timer_list serial_timer;

/* serial subtype definitions */
#ifndef SERIAL_TYPE_NORMAL
#define SERIAL_TYPE_NORMAL	1
#define SERIAL_TYPE_CALLOUT	2
#endif

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256

/* Wait until transmitter is ready for another character */
#define NAS_TX_WAIT_RDY(registers) while \
 ((registers->status_a & NETARM_SER_STATA_TX_RDY) == 0){}

/* Device descriptor arrays for our serial ports */
static struct netarm_async_struct NAS_ports[NR_NAS_PORTS];

static struct serial_state rs_table[NR_NAS_PORTS] = {
	NETARM_SERIAL_PORT_DFNS	/* Defined in serial.h */
};

#ifdef CONFIG_SERIAL_NETARM_CONSOLE
static struct console sercons;
static int lsr_break_flag;
#endif
#if defined(CONFIG_SERIAL_NETARM_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
static unsigned long break_pressed; /* break, really ... */
#endif

static void autoconfig(struct serial_state * state);
static void change_speed(struct netarm_async_struct *info, struct termios *old);
static void rs_wait_until_sent(struct tty_struct *tty, int timeout);

#ifndef PREPARE_FUNC
#define PREPARE_FUNC(dev)  (dev->prepare)
#define ACTIVATE_FUNC(dev)  (dev->activate)
#define DEACTIVATE_FUNC(dev)  (dev->deactivate)
#endif

#define HIGH_BITS_OFFSET ((sizeof(long)-sizeof(int))*8)

static struct tty_struct *serial_table[NR_NAS_PORTS];
static struct termios *serial_termios[NR_NAS_PORTS];
static struct termios *serial_termios_locked[NR_NAS_PORTS];

#if defined(MODULE) && defined(SERIAL_DEBUG_MCOUNT)
#define DBG_CNT(s) printk("(%s): [%x] refc=%d, serc=%d, ttyc=%d -> %s\n", \
 kdevname(tty->device), (info->flags), serial_refcount,info->count,tty->count,s)
#else
#define DBG_CNT(s)
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
static unsigned char *tmp_buf;
#ifdef DECLARE_MUTEX
static DECLARE_MUTEX(tmp_buf_sem);
#else
static struct semaphore tmp_buf_sem = MUTEX;
#endif


static inline int
serial_paranoia_check(struct netarm_async_struct *info,
			kdev_t device, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char *badmagic =
		"Warning: bad magic number for netarm serial struct (%s) in %s\n";
	static const char *badinfo =
		"Warning: null netarm_async_struct for (%s) in %s\n";

	if (!info) {
		printk(badinfo, kdevname(device), routine);
		return 1;
	}
	if (info->magic != NAS_MAGIC) {
		printk(badmagic, kdevname(device), routine);
		return 1;
	}
#endif
	return 0;
}


/*
 * ------------------------------------------------------------
 * rs_stop() and rs_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 * ------------------------------------------------------------
 */
static void
rs_stop(struct tty_struct *tty)
{
	struct netarm_async_struct *info = (struct netarm_async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_stop"))
		return;
	
	save_flags(flags); cli();
	/* disable all interrupt sources */
	info->registers->ctrl_a = info->registers->ctrl_a & 0xFFFF0000;
	restore_flags(flags);
}

static void
rs_start(struct tty_struct *tty)
{
	struct netarm_async_struct *info = (struct netarm_async_struct *)tty->driver_data;
	unsigned long flags;
	
	if (serial_paranoia_check(info, tty->device, "rs_start"))
		return;
	
	save_flags(flags); cli();
	/* enable rx interrupts */
	info->registers->ctrl_a = info->registers->ctrl_a
	                          | NETARM_SER_CTLA_IE_RX_RDY
				  | NETARM_SER_CTLA_IE_RX_FULL;
	restore_flags(flags);
}

/*
 * ----------------------------------------------------------------------
 * Mask&Acknowledge functions, needed by arch/armnommu/mach-netarm/irq.c
 *
 * These functions save the current status bits in the netarm_async_struc
 * structure, because the status is cleared by acknowledging the IRQ.
 * Each function is used for both RX and TX interrupts.
 * ----------------------------------------------------------------------
 */
void
nas_mask_ack_serial1_irq( unsigned int irq )
{
	volatile unsigned long int *mask = 
		(volatile unsigned long *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_INTR_ENABLE_CLR);
	volatile unsigned long int *stat_a =
		(volatile unsigned long *)(NETARM_SER_MODULE_BASE +
		NETARM_SER_CH1_STATUS_A);
	
	if ((irq == IRQ_SER1_TX) || (irq == IRQ_SER1_RX))
	{
		NAS_ports[0].SCSRA = *stat_a;	/* save current status */
		*mask  	= (1 << irq);		/* set mask bit */
		*stat_a	= 0xFFFF;		/* clear all IRQ pending bits */
	}
}
void
nas_mask_ack_serial2_irq( unsigned int irq )
{
	volatile unsigned long int *mask = 
		(volatile unsigned long *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_INTR_ENABLE_CLR);
	volatile unsigned long int *stat_a =
		(volatile unsigned long *)(NETARM_SER_MODULE_BASE +
		NETARM_SER_CH2_STATUS_A);

	if ((irq == IRQ_SER2_TX) || (irq == IRQ_SER2_RX))
	{
		NAS_ports[1].SCSRA = *stat_a;	/* save current status */
		*mask  	= (1 << irq);		/* set mask bit */
		*stat_a	= 0xFFFF;		/* clear all IRQ pending bits */
	}
}

/* Hook interrupts into System */

static int 
nas_interrupts_init(void)
{
  int retval;

  retval = request_irq (IRQ_SER1_RX,
			nas_rx_interrupt_1,
			0,
			"NetARM Serial1 Receive",
			&NAS_ports[0]);
  if (retval != 0) {
    printk("nas_interrupts_init: failed to register serial1 rx interrupt (%d)\n", retval);
    return retval;
  }
  retval = request_irq (IRQ_SER2_RX,
			nas_rx_interrupt_2,
			0,
			"NetARM Serial2 Receive",
			&NAS_ports[1]);
  if (retval != 0) {
    printk("nas_interrupts_init: failed to register serial2 rx interrupt (%d)\n", retval);
    return retval;
  }
  return 0;
}


/*
 * ----------------------------------------------------------------------
 *
 * Here start the interrupt handling routines.  All of the following
 * subroutines are declared as inline and are folded into
 * rs_interrupt().  They were separated out for readability's sake.
 *
 * Note: rs_interrupt() is a "fast" interrupt, which means that it
 * runs with interrupts turned off.  People who may want to modify
 * rs_interrupt() should try to keep the interrupt handler as fast as
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
static _INLINE_ void
rs_sched_event(struct netarm_async_struct *info, int event)
{
	info->event |= 1 << event;
	queue_task(&info->tqueue, &tq_serial);
	mark_bh(SERIAL_BH);
}

static _INLINE_ void
receive_chars(struct netarm_async_struct *info, int *status, struct pt_regs * p_regs)
{
	struct tty_struct *tty = info->tty;
	unsigned int ch_uint;
	struct	async_icount *icount;
	int	buf_count = 0;
	volatile unsigned int *fifo;
	volatile unsigned char *fifo_char = NULL;
	volatile netarm_serial_channel_t *regs = info->registers ;

	icount = &info->state->icount;
	fifo = (volatile unsigned int*)&(regs->fifo) ;

	do
	{
	    fifo_char = (unsigned char *)&ch_uint ;
	    if (*status & NETARM_SER_STATA_RX_RDY)
    	    {
      		/* need to read the whole buffer (1 to 4 bytes) */
      		ch_uint = *fifo;
		/* check the status word to see how many bytes are pending */
		buf_count = NETARM_SER_STATA_RXFDB(*status);
		switch (buf_count)
      		{
		    case NETARM_SER_STATA_RXFDB_4BYTES:
			buf_count = 4;
			break;	      
		    case NETARM_SER_STATA_RXFDB_3BYTES:      
			buf_count = 3;
			break;
		    case NETARM_SER_STATA_RXFDB_2BYTES:
			buf_count = 2;
			break;
		    case NETARM_SER_STATA_RXFDB_1BYTES:
			buf_count = 1;
			break;
		    default:
			printk("receive_chars: AIEE!! GARBAGE!\n");
		}
	    }
	    else
	    {
		ch_uint = 0;
		buf_count = 0;
	    }
    
	    /* read all available characters from FIFO */
	    while ((buf_count-- > 0) && (tty->flip.count < TTY_FLIPBUF_SIZE))
	    {
		*tty->flip.flag_buf_ptr = 0 ;

		/* handle port error conditions */
		if (*status & NETARM_SER_STATA_RX_BRK)
	    	{
		    *tty->flip.flag_buf_ptr = TTY_BREAK ;
		    icount->brk++;
		/*
		 * We do the SysRQ and SAK checking
		 * here because otherwise the break
		 * may get masked by ignore_status_mask
		 * or read_status_mask.
		 */
#if defined(CONFIG_SERIAL_NETARM_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
		if (info->line == sercons.index) {
			if (!break_pressed) {
				break_pressed = jiffies;
				goto ignore_char;
			}
			break_pressed = 0;
		}
#endif
		    if (info->flags & ASYNC_SAK) do_SAK(tty);
		}
		else if (*status & NETARM_SER_STATA_RX_PARERR)
		{
		    *tty->flip.flag_buf_ptr = TTY_PARITY ;
		    icount->parity++;
		}
		else if (*status & NETARM_SER_STATA_RX_FRMERR)
		{
		    *tty->flip.flag_buf_ptr = TTY_FRAME ;
		    icount->frame++;
		}
		else if (*status & NETARM_SER_STATA_RX_OVERRUN)
		{
		    *tty->flip.flag_buf_ptr = TTY_OVERRUN ;
		    icount->overrun++;
		}

#ifdef CONFIG_SERIAL_NETARM_CONSOLE
		if (info->line == sercons.index)
		{
		    /* Recover the break flag from console xmit */
		    *status |= lsr_break_flag;
		    lsr_break_flag = 0;
		}
#endif
		if (*status & (NETARM_SER_STATA_RX_BRK))
		{
#ifdef SERIAL_DEBUG_INTR
		    printk("handling break....");
#endif
		    *tty->flip.flag_buf_ptr = TTY_BREAK;
		}

#if defined(CONFIG_SERIAL_NETARM_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
		if (break_pressed && info->line == sercons.index)
		{
		    if (ch != 0 &&
			time_before(jiffies, break_pressed + HZ*5))
		    {
			handle_sysrq(ch, p_regs, NULL, NULL);
			break_pressed = 0;
			goto ignore_char;
		    }
		    break_pressed = 0;
		}
#endif

#ifdef SERIAL_DEBUG_INTR
		printk("receive_chars: %02x (%02x)", *fifo_char, *status);
#endif
		*tty->flip.char_buf_ptr = *fifo_char;
		tty->flip.count++;
		tty->flip.flag_buf_ptr++;
		tty->flip.char_buf_ptr++;
		fifo_char++ ;
		icount->rx++;
	    }

#if defined(CONFIG_SERIAL_NETARM_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
	ignore_char:
#endif
	    /* read status anew */
	    *status = regs->status_a ;
	}
	while ((*status & NETARM_SER_STATA_RX_RDY) 
	      && (tty->flip.count < TTY_FLIPBUF_SIZE));

	tty_flip_buffer_push(tty);
}

static _INLINE_ void
check_modem_status(struct netarm_async_struct *info)
{
	int	status;
	struct	async_icount *icount;
	
	status = info->SCSRA;

	if (status & (NETARM_SER_STATA_RI|NETARM_SER_STATA_DSR
	             |NETARM_SER_STATA_DCD|NETARM_SER_STATA_CTS))
	{
		icount = &info->state->icount;
		/* update input line counters */
		if (status & NETARM_SER_STATA_RI)
			icount->rng++;
		if (status & NETARM_SER_STATA_DSR)
			icount->dsr++;
		if (status & NETARM_SER_STATA_DCD)
			icount->dcd++;
		if (status & NETARM_SER_STATA_CTS)
			icount->cts++;
		wake_up_interruptible(&info->delta_msr_wait);
	}

	if ((info->flags & ASYNC_CHECK_CD) && (status & NETARM_SER_STATA_DCD)) {
#if (defined(SERIAL_DEBUG_OPEN) || defined(SERIAL_DEBUG_INTR))
		printk("ttys%d CD now %s...", info->line,
		       (status & NETARM_SER_STATA_DCD) ? "on" : "off");
#endif		
		if (status & NETARM_SER_STATA_DCD)
			wake_up_interruptible(&info->open_wait);
		else if (!((info->flags & ASYNC_CALLOUT_ACTIVE) &&
			   (info->flags & ASYNC_CALLOUT_NOHUP))) {
#ifdef SERIAL_DEBUG_OPEN
			printk("doing serial hangup...");
#endif
			if (info->tty)
				tty_hangup(info->tty);
		}
	}
	if (info->flags & ASYNC_CTS_FLOW) {
		if (info->tty->hw_stopped) {
			if (status & NETARM_SER_STATA_CTS) {
#if (defined(SERIAL_DEBUG_INTR) || defined(SERIAL_DEBUG_FLOW))
				printk("CTS tx start...");
#endif
				info->tty->hw_stopped = 0;
			/* FIXME: do we need rs_sched_event() here?
				info->IER |= UART_IER_THRI;
				serial_out(info, UART_IER, info->IER);
				rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);*/
				return;
			}
		} else {
			if (!(status & NETARM_SER_STATA_CTS)) {
#if (defined(SERIAL_DEBUG_INTR) || defined(SERIAL_DEBUG_FLOW))
				printk("CTS tx stop...");
#endif
				info->tty->hw_stopped = 1;
			/*	info->IER &= ~UART_IER_THRI;
				serial_out(info, UART_IER, info->IER);*/
			}
		}
	}
}


/*
 * -------------------------------------------------------------------
 * This is the serial driver's interrupt routine for a single port
 * -------------------------------------------------------------------
 */

static _INLINE_ void
nas_rx_interrupt(struct netarm_async_struct *info, struct pt_regs *regs)
{
 	int status;
	int pass_counter = 0;
	
	if (!info || !info->tty) return;

	do
	{
 		status = info->SCSRA;
#ifdef SERIAL_DEBUG_INTR
		printk("status = %x...", status);
#endif
		if (status & (NETARM_SER_STATA_RX_RDY
		             |NETARM_SER_STATA_RX_HALF
			     |NETARM_SER_STATA_RX_FULL))
			receive_chars(info, &status, regs);
/* FIXME:
   do we need check_modem_status() if we don't want to transmit anything?
		check_modem_status(info);
		if (status & UART_LSR_THRE)
			transmit_chars(info, 0);
*/
		if (pass_counter++ > RS_ISR_PASS_LIMIT)
		{
#if 0
			printk("rs_single loop break.\n");
#endif
			break;
		}
#ifdef SERIAL_DEBUG_INTR
		printk("STAT_A = %x...", info->registers->status_a);
#endif
	}
	while (NETARM_SER_STATA_RX_RDY & (info->SCSRA = info->registers->status_a));
	info->last_active = jiffies;
#ifdef SERIAL_DEBUG_INTR
	printk("end.\n");
#endif
}


static void
nas_rx_interrupt_1(int irq, void *dev_id, struct pt_regs *regs)
{
#if (defined(SERIAL_DEBUG_INTR) && defined(NAS_DEBUG_VERBOSE))
  printk(" nas_rx_interrupt_1: int!\n");
#endif

  nas_rx_interrupt(&NAS_ports[0], regs) ;
}

static void
nas_rx_interrupt_2(int irq, void *dev_id, struct pt_regs *regs)
{
#if (defined(SERIAL_DEBUG_INTR) && defined(NAS_DEBUG_VERBOSE))
  printk(" nas_rx_interrupt_2: int!\n");
#endif

  nas_rx_interrupt(&NAS_ports[1], regs) ;
}

/*
 * -------------------------------------------------------------------
 * Here end the serial interrupt routines
 * -------------------------------------------------------------------
 */


/*
 * This routine is used to handle the "bottom half" processing for the
 * serial driver, known also the "software interrupt" processing.
 * This processing is done at the kernel interrupt level, after the
 * rs_interrupt() has returned, BUT WITH INTERRUPTS TURNED ON.  This
 * is where time-consuming activities which can not be done in the
 * interrupt driver proper are done; the interrupt driver schedules
 * them using rs_sched_event(), and they get done here.
 */
static void
do_serial_bh(void)
{
	run_task_queue(&tq_serial);
}

static void
do_softint(void *private_)
{
	struct netarm_async_struct *info = (struct netarm_async_struct *) private_;
	struct tty_struct          *tty;
	
	tty = info->tty;
	if (!tty)
		return;

	if (test_and_clear_bit(RS_EVENT_WRITE_WAKEUP, &info->event)) {
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
		    tty->ldisc.write_wakeup)
			(tty->ldisc.write_wakeup)(tty);
		wake_up_interruptible(&tty->write_wait);
#ifdef SERIAL_HAVE_POLL_WAIT
		wake_up_interruptible(&tty->poll_wait);
#endif
	}
}


/*
 * ---------------------------------------------------------------
 * Low level utility subroutines for the serial driver:  routines
 * to initialize and startup a serial port, and routines to shutdown a
 * serial port.  Useful stuff like that.
 * ---------------------------------------------------------------
 */

static int 
nas_validate_baud(int baud)
{
  if (baud >= MIN_BAUD_RATE && baud <= MAX_BAUD_RATE) return 0;
  return 1;
}

/*
 * This is used to figure out the divisor speeds and the timeouts
 */

static int nas_baud_table[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
	9600, 19200, 38400, 57600, 115200, 0 };

static int nas_n_baud_table = sizeof(nas_baud_table)/sizeof(int);

/* nas_tty_get_baud_rate is borrowed from 2.2.12 tty_io.c		*/
/* since the netarm serial driver is based upon the 2.2.x source 	*/
/* rather than the 2.0.X. The reasons for this are purely historical 	*/

int
nas_tty_get_baud_rate(struct tty_struct *tty)
{
	unsigned int cflag, i;

	cflag = tty->termios->c_cflag;

	i = cflag & CBAUD;
	if (i & CBAUDEX)
	{
		i &= ~CBAUDEX;
		if (i < 1 || i+15 >= nas_n_baud_table) 
			tty->termios->c_cflag &= ~CBAUDEX;
		else
			i += 15;
	}
	return nas_baud_table[i];
}

static int
startup(struct netarm_async_struct * info)
{
	unsigned long		flags;
	int			baud, retval=0;
	struct serial_state 	*state = info->state;
	unsigned long 		page;
	volatile netarm_serial_channel_t *regs;

	/* check the baud rate */
	baud = nas_tty_get_baud_rate(info->tty);
	if (nas_validate_baud(baud) != 0)
	{
	    printk("nas_startup_port: invalid baud rate from tty struct: %d\n", baud);
	    return -EINVAL;
	}
	info->baud = baud;

	page = get_zeroed_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	save_flags(flags); cli();

	if (info->flags & ASYNC_INITIALIZED) {
		free_page(page);
		goto errout;
	}

	if (!CONFIGURED_SERIAL_PORT(state) || !state->type) {
		if (info->tty)
			set_bit(TTY_IO_ERROR, &info->tty->flags);
		free_page(page);
		goto errout;
	}
	if (info->xmit.buf)
		free_page(page);
	else
		info->xmit.buf = (unsigned char *) page;

#ifdef SERIAL_DEBUG_OPEN
	printk("Starting up ttyS%d (irq %d) at %d baud...\n",
	        info->line, state->irq, info->baud);
#endif

	/* Wake up and initialize SER module */
	regs = info->registers;
	regs->rx_match = 0;
	regs->rx_buf_timer = 0;
	regs->bitrate = NETARM_SER_BR_X16(baud);
	regs->rx_char_timer = NETARM_SER_RXGAP(baud);
	regs->ctrl_b = NETARM_SER_CTLB_RCGT_EN | NETARM_SER_CTLB_UART_MODE;
#ifdef POLLED_SERIAL
	regs->ctrl_a |= NETARM_SER_CTLA_ENABLE ;
#else
	/* enable interrupts */
	regs->ctrl_a |= NETARM_SER_CTLA_ENABLE | NETARM_SER_CTLA_IE_RX_RDY | 
	                NETARM_SER_CTLA_IE_RX_FULL ;
#endif
	/*
	 * Set up the tty->alt_speed kludge
	 */
	if (info->tty) {
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
			info->tty->alt_speed = 57600;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
			info->tty->alt_speed = 115200;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
			info->tty->alt_speed = 230400;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
			info->tty->alt_speed = 460800;
	}
	/*
	 * and set the speed of the serial port
	 */
	change_speed(info, 0);

	info->flags |= ASYNC_INITIALIZED;
	restore_flags(flags);
	return 0;
errout:
	restore_flags(flags);
	return retval;
}

/*
 * This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */
static void
shutdown(struct netarm_async_struct * info)
{
	unsigned long flags;
	struct serial_state *state;
	volatile netarm_serial_channel_t *regs;

	if (!(info->flags & ASYNC_INITIALIZED))
		return;

	state = info->state;
	regs  = info->registers;

#ifdef SERIAL_DEBUG_OPEN
	printk("Shutting down serial port %d (irq %d)...\n", info->line,
	       state->irq);
#endif	
	save_flags(flags); cli(); /* Disable interrupts */

	/*
	 * clear delta_msr_wait queue to avoid mem leaks: we may free the irq
	 * here so the queue might never be waken up
	 */
	wake_up_interruptible(&info->delta_msr_wait);
	
	if (info->xmit.buf) {
		unsigned long pg = (unsigned long) info->xmit.buf;
		info->xmit.buf = 0;
		free_page(pg);
	}

	/* turn off the port */
	regs->ctrl_a &= ~NETARM_SER_CTLA_ENABLE ;	
	
	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);

	info->flags &= ~ASYNC_INITIALIZED;
	restore_flags(flags);
}

/*
 * This routine is called to set the specified baud rate for a serial port.
 */
static void
change_speed(struct netarm_async_struct *info, struct termios *old_termios)
{
	volatile netarm_serial_channel_t *regs;
	int	quot = 0, baud_base, baud;
	unsigned cflag, cval;
	int	bits;
	unsigned long flags;

	regs  = info->registers;
	if (!info->tty || !info->tty->termios)
		return;
	cflag = info->tty->termios->c_cflag;
	if (!CONFIGURED_NAS_PORT(info))
		return;

	/* byte size and parity */
	switch (cflag & CSIZE) {
	      case CS5: cval = NETARM_SER_CTLA_5BITS; bits = 7; break;
	      case CS6: cval = NETARM_SER_CTLA_6BITS; bits = 8; break;
	      case CS7: cval = NETARM_SER_CTLA_7BITS; bits = 9; break;
	      case CS8: cval = NETARM_SER_CTLA_8BITS; bits = 10; break;
	      /* Never happens, but GCC is too dumb to figure it out */
	      default:  cval = NETARM_SER_CTLA_5BITS; bits = 7; break;
	}
	if (cflag & CSTOPB) {
		cval |= NETARM_SER_CTLA_2STOP;
		bits++;
	}
	if (cflag & PARENB) {
		cval |= NETARM_SER_CTLA_P_ODD;
		bits++;
		if (!(cflag & PARODD))
			cval |= NETARM_SER_CTLA_P_EVEN;
	}
	/* Determine divisor based on baud rate */
	baud = nas_tty_get_baud_rate(info->tty);
	if (!baud)
		baud = 9600;

#ifdef SERIAL_DEBUG_OPEN
	printk("change_speed: cflag %d, baud rate %d\n", cflag, baud);
#endif

	baud_base = info->state->baud_base;
	quot = baud_base / baud;
	/* As a last resort, if the quotient is zero, default to 9600 bps */
	if (!quot) quot = baud_base / 9600;
	
	info->quot = quot;
	info->timeout = ((info->xmit_fifo_size*HZ*bits*quot) / baud_base);
	info->timeout += HZ/50;		/* Add .02 seconds of slop */

	/* CTS flow control flag and modem status interrupts */
	if (cflag & CRTSCTS)
		cval |= NETARM_SER_CTLA_CTSTX | NETARM_SER_CTLA_RTSRX;

	/*
	 * Set up parity check flag
	 */
#define RELEVANT_IFLAG(iflag) (iflag & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

	save_flags(flags); cli();
	regs = info->registers;
	regs->rx_match = 0;
	regs->rx_buf_timer = 0;
	regs->bitrate = NETARM_SER_BR_X16(baud);
	regs->rx_char_timer = NETARM_SER_RXGAP(baud);
	regs->ctrl_b = NETARM_SER_CTLB_RCGT_EN | NETARM_SER_CTLB_UART_MODE;
#ifdef POLLED_SERIAL
	regs->ctrl_a = NETARM_SER_CTLA_ENABLE | cval;
#else
	/* enable interrupts */
	regs->ctrl_a = NETARM_SER_CTLA_ENABLE | NETARM_SER_CTLA_IE_RX_RDY | 
	               NETARM_SER_CTLA_IE_RX_FULL | cval;
#endif

#ifdef SERIAL_DEBUG_OPEN
	printk("change_speed: bitrate %x, rxgap %x\n",
		regs->bitrate, regs->rx_char_timer );
#endif
	restore_flags(flags);
}

/* Output a single character to the device */
static void
rs_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct netarm_async_struct *info = (struct netarm_async_struct *)tty->driver_data;
 	volatile netarm_serial_channel_t *regs;
	unsigned char *fifo;
#ifdef	NAS_DEBUG_VERBOSE
  printk(" rs_put_char called\n");
#endif
	regs = info->registers;
	fifo = (unsigned char *)&(regs->fifo);

	/* write the character (polled) */
	NAS_TX_WAIT_RDY(regs);
        *fifo = ch;
}

/* not needed in polled output: */
static void
rs_flush_chars(struct tty_struct *tty)
{
	struct netarm_async_struct *info = (struct netarm_async_struct *)tty->driver_data;
				
	serial_paranoia_check(info, tty->device, "rs_flush_chars");
	return;
}

/* Write a number of characters (polled) */
static int
rs_write(struct tty_struct * tty, int from_user,
         const unsigned char *buf, int count)
{
	struct netarm_async_struct *info = (struct netarm_async_struct *)tty->driver_data;
	volatile netarm_serial_channel_t *regs;
	unsigned int *buf32;
	unsigned int *fifo;
	unsigned char *bytefifo;
	int acount;
	int scount = 0;

#ifdef	NAS_DEBUG_VERBOSE
	printk(" rs_write: called with %d chars\n", count);
#endif
				
	if (serial_paranoia_check(info, tty->device, "rs_write"))
		return 0;
	if (!tty || !info->xmit.buf || !tmp_buf)
		return 0;

	regs = info->registers;
	fifo = (unsigned int *)&(regs->fifo);
	bytefifo = (unsigned char *)&(regs->fifo);

	/* count to align */
	acount = (4 - (((unsigned int)buf) & 0x3)) & 0x03;
	if ( acount > count ) acount = count;
	count -= acount;

	/* word align */
	if ( acount > 0 )
	{
		while ( acount > 0 )
		{
			NAS_TX_WAIT_RDY(regs);
			*bytefifo = *buf;	
	      		buf++;
	      		acount--;
	      		scount++;
		}
  	}
	buf32 = (unsigned int *)buf;
	while ( count > 3 )
  	{
		NAS_TX_WAIT_RDY(regs);
    		*fifo = *buf32;
		buf32++;
		count  -= 4;
		scount += 4;
	}
	if ( count > 0 )
  	{
  		buf = (unsigned char *)buf32;
  		while ( count > 0 )
		{
			NAS_TX_WAIT_RDY(regs);
			*bytefifo = *buf;
			buf++;
			count--;
			scount++;
		}
  	}
	return scount;
}

static int
rs_write_room(struct tty_struct *tty)
{
	struct netarm_async_struct *info = (struct netarm_async_struct *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_write_room"))
		return 0;
	return CIRC_SPACE(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

static int 
rs_chars_in_buffer(struct tty_struct *tty)
{
	struct netarm_async_struct *info = (struct netarm_async_struct *)tty->driver_data;
				
	if (serial_paranoia_check(info, tty->device, "rs_chars_in_buffer"))
		return 0;
	return CIRC_CNT(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

/* not needed in polled output: */
static void 
rs_flush_buffer(struct tty_struct *tty)
{
	struct netarm_async_struct *info = (struct netarm_async_struct *)tty->driver_data;
	
	serial_paranoia_check(info, tty->device, "rs_flush_buffer");
	return;
}

/*
 * This function is used to send a high-priority XON/XOFF character to
 * the device
 */
static void
rs_send_xchar(struct tty_struct *tty, char ch)
{
	rs_put_char( tty, ch );
}

/*
 * ------------------------------------------------------------
 * rs_throttle()
 * 
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 * ------------------------------------------------------------
 */
static void
rs_throttle(struct tty_struct * tty)
{
	struct netarm_async_struct *info = (struct netarm_async_struct *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("throttle %s: %d....\n", tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "rs_throttle"))
		return;
	
	if (I_IXOFF(tty))
		rs_send_xchar(tty, STOP_CHAR(tty));

	if (tty->termios->c_cflag & CRTSCTS)
		info->registers->ctrl_a &= ~NETARM_SER_CTLA_RTS_EN;
}

static void
rs_unthrottle(struct tty_struct * tty)
{
	struct netarm_async_struct *info = (struct netarm_async_struct *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("unthrottle %s: %d....\n", tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "rs_unthrottle"))
		return;
	
	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			rs_send_xchar(tty, START_CHAR(tty));
	}
	if (tty->termios->c_cflag & CRTSCTS)
		info->registers->ctrl_a |= NETARM_SER_CTLA_RTS_EN;
}

/*
 * ------------------------------------------------------------
 * rs_ioctl() and friends
 * ------------------------------------------------------------
 */

static int
get_serial_info(struct netarm_async_struct * info, struct serial_struct * retinfo)
{
	struct serial_struct tmp;
	struct serial_state *state = info->state;
   
	if (!retinfo)
		return -EFAULT;
	memset(&tmp, 0, sizeof(tmp));
	tmp.type = state->type;
	tmp.line = state->line;
	tmp.port = state->port;
	if (HIGH_BITS_OFFSET)
		tmp.port_high = state->port >> HIGH_BITS_OFFSET;
	else
		tmp.port_high = 0;
	tmp.irq = state->irq;
	tmp.flags = state->flags;
	tmp.xmit_fifo_size = state->xmit_fifo_size;
	tmp.baud_base = state->baud_base;
	tmp.close_delay = state->close_delay;
	tmp.closing_wait = state->closing_wait;
	tmp.custom_divisor = state->custom_divisor;
	tmp.hub6 = state->hub6;
	tmp.io_type = state->io_type;
	if (copy_to_user(retinfo,&tmp,sizeof(*retinfo)))
		return -EFAULT;
	return 0;
}

static int
set_serial_info(struct netarm_async_struct * info, struct serial_struct * new_info)
{
	struct serial_struct new_serial;
 	struct serial_state old_state, *state;
	unsigned int		i,change_irq,change_port;
	int 			retval = 0;
	unsigned long		new_port;

	if (copy_from_user(&new_serial,new_info,sizeof(new_serial)))
		return -EFAULT;
	state = info->state;
	old_state = *state;

	new_port = new_serial.port;
	if (HIGH_BITS_OFFSET)
		new_port += (unsigned long) new_serial.port_high << HIGH_BITS_OFFSET;

	change_irq = new_serial.irq != state->irq;
	change_port = (new_port != ((int) state->port)) ||
		(new_serial.hub6 != state->hub6);
  
	if (!capable(CAP_SYS_ADMIN)) {
		if (change_irq || change_port ||
		    (new_serial.baud_base != state->baud_base) ||
		    (new_serial.type != state->type) ||
		    (new_serial.close_delay != state->close_delay) ||
		    (new_serial.xmit_fifo_size != state->xmit_fifo_size) ||
		    ((new_serial.flags & ~ASYNC_USR_MASK) !=
		     (state->flags & ~ASYNC_USR_MASK)))
			return -EPERM;
		state->flags = ((state->flags & ~ASYNC_USR_MASK) |
			       (new_serial.flags & ASYNC_USR_MASK));
		info->flags = ((info->flags & ~ASYNC_USR_MASK) |
			       (new_serial.flags & ASYNC_USR_MASK));
		state->custom_divisor = new_serial.custom_divisor;
		goto check_and_exit;
	}

	new_serial.irq = irq_cannonicalize(new_serial.irq);

	if ((new_serial.irq >= NR_IRQS) || (new_serial.irq < 0) || 
	    (new_serial.baud_base < 9600)|| (new_serial.type != PORT_NETARM))
	{
		return -EINVAL;
	}

	if (new_serial.type == PORT_NETARM)
		new_serial.xmit_fifo_size = NETARM_SER_FIFO_SIZE;

	/* Make sure address is not already in use */
	if (new_serial.type) {
		for (i = 0 ; i < NR_NAS_PORTS; i++)
			if ((state != &rs_table[i]) &&
			    (rs_table[i].port == new_port) &&
			    rs_table[i].type)
				return -EADDRINUSE;
	}

	if ((change_port || change_irq) && (state->count > 1))
		return -EBUSY;
	/*
	 * OK, past this point, all the error checking has been done.
	 * At this point, we start making changes.....
	 */
	state->baud_base = new_serial.baud_base;
	state->flags = ((state->flags & ~ASYNC_FLAGS) |
			(new_serial.flags & ASYNC_FLAGS));
	info->flags = ((state->flags & ~ASYNC_INTERNAL_FLAGS) |
		       (info->flags & ASYNC_INTERNAL_FLAGS));
	state->custom_divisor = new_serial.custom_divisor;
	state->close_delay = new_serial.close_delay * HZ/100;
	state->closing_wait = new_serial.closing_wait * HZ/100;
#if (LINUX_VERSION_CODE > 0x20100)
	info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;
#endif
	info->xmit_fifo_size = state->xmit_fifo_size =
		new_serial.xmit_fifo_size;

	if ((state->type != PORT_UNKNOWN) && state->port) {
		release_region(state->port,8);
	}
	state->type = new_serial.type;
	if (change_port || change_irq) {
		/*
		 * We need to shutdown the serial port at the old
		 * port/irq combination.
		 */
		shutdown(info);
		state->irq = new_serial.irq;
		info->port = state->port = new_port;
	}
	if ((state->type != PORT_UNKNOWN) && state->port) {
		request_region(state->port,8,"serial(set)");
	}
	
check_and_exit:
	if (!state->port || !state->type)
		return 0;
	if (info->flags & ASYNC_INITIALIZED) {
		if (((old_state.flags & ASYNC_SPD_MASK) !=
		     (state->flags & ASYNC_SPD_MASK)) ||
		    (old_state.custom_divisor != state->custom_divisor)) {
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
				info->tty->alt_speed = 57600;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
				info->tty->alt_speed = 115200;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
				info->tty->alt_speed = 230400;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
				info->tty->alt_speed = 460800;
			change_speed(info, 0);
		}
	} else
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
static int
get_lsr_info(struct netarm_async_struct * info, unsigned int *value)
{
	unsigned char status;
	unsigned int result;
	unsigned long flags;

	save_flags(flags); cli();
	status = info->registers->status_a;
	restore_flags(flags);
	result = ((status & NETARM_SER_STATA_TX_FULL) ? TIOCSER_TEMT : 0);

	/*
	 * If we're about to load something into the transmit
	 * register, we'll pretend the transmitter isn't empty to
	 * avoid a race condition (depending on when the transmit
	 * interrupt happens).
	 */
	if (info->x_char || 
	    ((CIRC_CNT(info->xmit.head, info->xmit.tail,
		       SERIAL_XMIT_SIZE) > 0) &&
	     !info->tty->stopped && !info->tty->hw_stopped))
		result &= TIOCSER_TEMT;

	if (copy_to_user(value, &result, sizeof(int)))
		return -EFAULT;
	return 0;
}


static int
do_autoconfig(struct netarm_async_struct * info)
{
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	
	if (info->state->count > 1)
		return -EBUSY;
	
	shutdown(info);
	autoconfig(info->state);

	return startup(info);
}

/*
 * rs_break() --- routine which turns the break handling on or off
 */
static void
rs_break(struct tty_struct *tty, int break_state)
{
	struct netarm_async_struct * info = (struct netarm_async_struct *)tty->driver_data;
	unsigned long flags, control;
	
	if (serial_paranoia_check(info, tty->device, "rs_break"))
		return;
	if (!CONFIGURED_NAS_PORT(info))
		return;

	save_flags(flags); cli();
	control = info->registers->ctrl_a;
	if (break_state == -1)
		control |= NETARM_SER_CTLA_BRK;
	else
		control &= ~NETARM_SER_CTLA_BRK;
	info->registers->ctrl_a = control;
	restore_flags(flags);
}

static int
rs_ioctl(struct tty_struct *tty, struct file * file,
         unsigned int cmd, unsigned long arg)
{
	struct netarm_async_struct * info = (struct netarm_async_struct *)tty->driver_data;
	struct async_icount cprev, cnow;	/* kernel counter temps */
	struct serial_icounter_struct icount;
	unsigned long flags, control;
	
	if (serial_paranoia_check(info, tty->device, "rs_ioctl"))
		return -ENODEV;

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
	    (cmd != TIOCSERCONFIG) && (cmd != TIOCSERGSTRUCT) &&
	    (cmd != TIOCMIWAIT) && (cmd != TIOCGICOUNT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
		    return -EIO;
	}
	
	switch (cmd) {
/*		case TIOCMGET:
			return get_modem_info(info, (unsigned int *) arg);
		case TIOCMBIS:
		case TIOCMBIC:
		case TIOCMSET:
			return set_modem_info(info, cmd, (unsigned int *) arg);
*/
		case TIOCGSERIAL:
			return get_serial_info(info,
					       (struct serial_struct *) arg);
		case TIOCSSERIAL:
			return set_serial_info(info,
					       (struct serial_struct *) arg);
		case TIOCSERCONFIG:
			return do_autoconfig(info);

		case TIOCSERGETLSR: /* Get line status register */
			return get_lsr_info(info, (unsigned int *) arg);

		case TIOCSERGSTRUCT:
			if (copy_to_user((struct netarm_async_struct *) arg,
					 info, sizeof(struct netarm_async_struct)))
				return -EFAULT;
			return 0;
							
		/*
		 * Wait for any of the 4 modem inputs (DCD,RI,DSR,CTS) to change
		 * - mask passed in arg for lines of interest
 		 *   (use |'ed TIOCM_RNG/DSR/CD/CTS for masking)
		 * Caller should use TIOCGICOUNT to see which one it was
		 */
		case TIOCMIWAIT:
			save_flags(flags); cli();
			/* note the counters on entry */
			cprev = info->state->icount;
			restore_flags(flags);
			/* Force modem status interrupts on */
			control = info->registers->ctrl_a;
			control |= NETARM_SER_CTLA_IE_RX_DCD |
			           NETARM_SER_CTLA_IE_RX_RI |
				   NETARM_SER_CTLA_IE_RX_DSR |
				   NETARM_SER_CTLA_IE_TX_CTS;
			info->registers->ctrl_a = control;
			while (1)
			{
				interruptible_sleep_on(&info->delta_msr_wait);
				/* see if a signal did it */
				if (signal_pending(current))
					return -ERESTARTSYS;
				save_flags(flags); cli();
				cnow = info->state->icount; /* atomic copy */
				restore_flags(flags);
				if (cnow.rng == cprev.rng && cnow.dsr == cprev.dsr && 
				    cnow.dcd == cprev.dcd && cnow.cts == cprev.cts)
					return -EIO; /* no change => error */
				if ( ((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
				     ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
				     ((arg & TIOCM_CD)  && (cnow.dcd != cprev.dcd)) ||
				     ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts)) ) {
					return 0;
				}
				cprev = cnow;
			}
			/* NOTREACHED */

		/* 
		 * Get counter of input serial line interrupts (DCD,RI,DSR,CTS)
		 * Return: write counters to the user passed counter struct
		 * NB: both 1->0 and 0->1 transitions are counted except for
		 *     RI where only 0->1 is counted.
		 */
		case TIOCGICOUNT:
			save_flags(flags); cli();
			cnow = info->state->icount;
			restore_flags(flags);
			icount.cts = cnow.cts;
			icount.dsr = cnow.dsr;
			icount.rng = cnow.rng;
			icount.dcd = cnow.dcd;
			icount.rx = cnow.rx;
			icount.tx = cnow.tx;
			icount.frame = cnow.frame;
			icount.overrun = cnow.overrun;
			icount.parity = cnow.parity;
			icount.brk = cnow.brk;
			icount.buf_overrun = cnow.buf_overrun;
			
			if (copy_to_user((void *)arg, &icount, sizeof(icount)))
				return -EFAULT;
			return 0;
		case TIOCSERGWILD:
		case TIOCSERSWILD:
			/* "setserial -W" is called in Debian boot */
			printk ("TIOCSER?WILD ioctl obsolete, ignored.\n");
			return 0;

		default:
			return -ENOIOCTLCMD;
		}
	return 0;
}

static void
rs_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	struct netarm_async_struct *info = (struct netarm_async_struct *)tty->driver_data;
	unsigned int cflag = tty->termios->c_cflag;
	
	if (   (cflag == old_termios->c_cflag)
	    && (   RELEVANT_IFLAG(tty->termios->c_iflag) 
		== RELEVANT_IFLAG(old_termios->c_iflag)))
	  return;

	change_speed(info, old_termios);

	/* Handle turning off CRTSCTS */
	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		rs_start(tty);
	}
}

/*
 * ------------------------------------------------------------
 * rs_close()
 * 
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent.  Then, we unlink its
 * async structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 * ------------------------------------------------------------
 */
static void
rs_close(struct tty_struct *tty, struct file * filp)
{
	struct netarm_async_struct * info = (struct netarm_async_struct *)tty->driver_data;
	struct serial_state *state;
	unsigned long flags;

	if (!info || serial_paranoia_check(info, tty->device, "rs_close"))
		return;

	state = info->state;
	
	save_flags(flags); cli();
	
	if (tty_hung_up_p(filp)) {
		DBG_CNT("before DEC-hung");
		MOD_DEC_USE_COUNT;
		restore_flags(flags);
		return;
	}
	
#ifdef SERIAL_DEBUG_OPEN
	printk("rs_close ttyS%d, count = %d\n", info->line, state->count);
#endif
	if ((tty->count == 1) && (state->count != 1)) {
		/*
		 * Uh, oh.  tty->count is 1, which means that the tty
		 * structure will be freed.  state->count should always
		 * be one in these conditions.  If it's greater than
		 * one, we've got real problems, since it means the
		 * serial port won't be shutdown.
		 */
		printk("rs_close: bad serial port count; tty->count is 1, "
		       "state->count is %d\n", state->count);
		state->count = 1;
	}
	if (--state->count < 0) {
		printk("rs_close: bad serial port count for ttys%d: %d\n",
		       info->line, state->count);
		state->count = 0;
	}
	if (state->count) {
		DBG_CNT("before DEC-2");
		MOD_DEC_USE_COUNT;
		restore_flags(flags);
		return;
	}
	info->flags |= ASYNC_CLOSING;
	restore_flags(flags);
	/*
	 * Save the termios structure, since this port may have
	 * separate termios for callout and dialin.
	 */
	if (info->flags & ASYNC_NORMAL_ACTIVE)
		info->state->normal_termios = *tty->termios;
	if (info->flags & ASYNC_CALLOUT_ACTIVE)
		info->state->callout_termios = *tty->termios;
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
	if (info->flags & ASYNC_INITIALIZED) {
		info->registers->ctrl_a &= ~NETARM_SER_CTLA_IE_RX_ALL;
		/*
		 * Before we drop DTR, make sure the UART transmitter
		 * has completely drained; this is especially
		 * important if there is a transmit FIFO!
		 */
		rs_wait_until_sent(tty, info->timeout);
	}
	shutdown(info);
	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
	tty->closing = 0;
	info->event = 0;
	info->tty = 0;
	if (info->blocked_open) {
		if (info->close_delay) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(info->close_delay);
		}
		wake_up_interruptible(&info->open_wait);
	}
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE|
			 ASYNC_CLOSING);
	wake_up_interruptible(&info->close_wait);
	MOD_DEC_USE_COUNT;
}

/*
 * rs_wait_until_sent() --- wait until the transmitter is empty
 * (only used in rs_close? --rp)
 */
static void
rs_wait_until_sent(struct tty_struct *tty, int timeout)
{
	struct netarm_async_struct * info = (struct netarm_async_struct *)tty->driver_data;
	unsigned long orig_jiffies, char_time;
	int lsr;
	
	if (serial_paranoia_check(info, tty->device, "rs_wait_until_sent"))
		return;

	if (info->state->type == PORT_UNKNOWN)
		return;

	if (info->xmit_fifo_size == 0)
		return; /* Just in case.... */

	orig_jiffies = jiffies;
	/*
	 * Set the check interval to be 1/5 of the estimated time to
	 * send a single character, and make it at least 1.  The check
	 * interval should also be less than the timeout.
	 * 
	 * Note: we have to use pretty tight timings here to satisfy
	 * the NIST-PCTS.
	 */
	char_time = (info->timeout - HZ/50) / info->xmit_fifo_size;
	char_time = char_time / 5;
	if (char_time == 0)
		char_time = 1;
	if (timeout && timeout < char_time)
		char_time = timeout;
	/*
	 * If the transmitter hasn't cleared in twice the approximate
	 * amount of time to send the entire FIFO, it probably won't
	 * ever clear.  This assumes the UART isn't doing flow
	 * control, which is currently the case.  Hence, if it ever
	 * takes longer than info->timeout, this is probably due to a
	 * UART bug of some kind.  So, we clamp the timeout parameter at
	 * 2*info->timeout.
	 */
	if (!timeout || timeout > 2*info->timeout)
		timeout = 2*info->timeout;
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
	printk("In rs_wait_until_sent(%d) check=%lu...", timeout, char_time);
	printk("jiff=%lu...", jiffies);
#endif
	while (!((lsr = info->registers->status_a) & NETARM_SER_STATA_TX_FULL))
	{
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
		printk("lsr = %d (jiff=%lu)...", lsr, jiffies);
#endif
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(char_time);
		if (signal_pending(current))
			break;
		if (timeout && time_after(jiffies, orig_jiffies + timeout))
			break;
	}
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
	printk("lsr = %d (jiff=%lu)...done\n", lsr, jiffies);
#endif
}

/*
 * rs_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
static void
rs_hangup(struct tty_struct *tty)
{
	struct netarm_async_struct * info = (struct netarm_async_struct *)tty->driver_data;
	struct serial_state *state = info->state;
	
	if (serial_paranoia_check(info, tty->device, "rs_hangup"))
		return;

	state = info->state;
	
	rs_flush_buffer(tty);
	if (info->flags & ASYNC_CLOSING)
		return;
	shutdown(info);
	info->event = 0;
	state->count = 0;
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE);
	info->tty = 0;
	wake_up_interruptible(&info->open_wait);
}

/*
 * ------------------------------------------------------------
 * rs_open() and friends
 * ------------------------------------------------------------
 */
static int
block_til_ready(struct tty_struct *tty, struct file * filp,
		struct netarm_async_struct *info)
{
	DECLARE_WAITQUEUE(wait, current);
	struct serial_state *state = info->state;
	int		retval;
	int		do_clocal = 0, extra_count = 0;
	unsigned long	flags;

	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (tty_hung_up_p(filp) ||
	    (info->flags & ASYNC_CLOSING)) {
		if (info->flags & ASYNC_CLOSING)
			interruptible_sleep_on(&info->close_wait);
		return -EAGAIN;
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
		if (state->normal_termios.c_cflag & CLOCAL)
			do_clocal = 1;
	} else {
		if (tty->termios->c_cflag & CLOCAL)
			do_clocal = 1;
	}
	
	/*
	 * Block waiting for the carrier detect and the line to become
	 * free (i.e., not in use by the callout).  While we are in
	 * this loop, state->count is dropped by one, so that
	 * rs_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */
	retval = 0;
	add_wait_queue(&info->open_wait, &wait);
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready before block: ttys%d, count = %d\n",
	       state->line, state->count);
#endif
	save_flags(flags); cli();
	if (!tty_hung_up_p(filp)) {
		extra_count = 1;
		state->count--;
	}
	restore_flags(flags);
	info->blocked_open++;
	while (1)
	{
		save_flags(flags); cli();
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (tty->termios->c_cflag & CBAUD))
			info->registers->ctrl_a |= NETARM_SER_CTLA_DTR_EN |
			                           NETARM_SER_CTLA_RTS_EN;
		restore_flags(flags);
		set_current_state(TASK_INTERRUPTIBLE);
		if (tty_hung_up_p(filp) ||
		    !(info->flags & ASYNC_INITIALIZED)) {
			retval = -EAGAIN;
			break;
		}
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    !(info->flags & ASYNC_CLOSING) &&
		    (do_clocal || (info->registers->status_a &
				   NETARM_SER_STATA_DCD)))
			break;
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
#ifdef SERIAL_DEBUG_OPEN
		printk("block_til_ready blocking: ttys%d, count = %d\n",
		       info->line, state->count);
#endif
		schedule();
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&info->open_wait, &wait);
	if (extra_count)
		state->count++;
	info->blocked_open--;
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready after blocking: ttys%d, count = %d\n",
	       info->line, state->count);
#endif
	if (retval)
		return retval;
	info->flags |= ASYNC_NORMAL_ACTIVE;
	return 0;
}

static int
get_async_struct(int line, struct netarm_async_struct **ret_info)
{
	struct netarm_async_struct *info;
	struct serial_state *sstate;

	/* only two lines are available */
	if ((line == 0) || (line == 1))
	{
	    sstate = rs_table + line;
	    sstate->count++;
	    if (sstate->info) {
	    		/* this shouldn't happen here */
		    *ret_info = (struct netarm_async_struct *) sstate->info;
		    return 0;
	    }
	    info = &NAS_ports[line];
	    memset(info, 0, sizeof(struct netarm_async_struct));
	    init_waitqueue_head(&info->open_wait);
	    init_waitqueue_head(&info->close_wait);
	    init_waitqueue_head(&info->delta_msr_wait);
	    info->magic = NAS_MAGIC;
	    info->port = sstate->port;
	    info->flags = sstate->flags;
	    info->io_type = sstate->io_type;
	    info->registers = get_serial_channel( line );
	    info->xmit_fifo_size = sstate->xmit_fifo_size;
	    info->line = line;
	    info->tqueue.routine = do_softint;
	    info->tqueue.data = info;
	    info->state = sstate;
	    sstate->info = (struct async_struct *) info;
	    *ret_info = info;
	    return 0;
	}
	else
	    return -ENODEV;
}

/*
 * This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its async structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */
static int
rs_open(struct tty_struct *tty, struct file * filp)
{
	struct netarm_async_struct	*info;
	int 			retval, line;
	unsigned long		page;

	MOD_INC_USE_COUNT;
	line = MINOR(tty->device) - tty->driver.minor_start;
	if ((line < 0) || (line >= NR_NAS_PORTS)) {
		MOD_DEC_USE_COUNT;
		return -ENODEV;
	}
	retval = get_async_struct(line, &info);
	if (retval) {
		MOD_DEC_USE_COUNT;
		return retval;
	}
	tty->driver_data = info;
	info->tty = tty;
	if (serial_paranoia_check(info, tty->device, "rs_open")) {
		MOD_DEC_USE_COUNT;		
		return -ENODEV;
	}

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open %s%d, count = %d\n", tty->driver.name, info->line,
	       info->state->count);
#endif
#if (LINUX_VERSION_CODE > 0x20100)
	info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;
#endif

	if (!tmp_buf) {
		page = get_zeroed_page(GFP_KERNEL);
		if (tmp_buf)
			free_page(page);
		else {
			if (!page) {
				MOD_DEC_USE_COUNT;
				return -ENOMEM;
			}
			tmp_buf = (unsigned char *) page;
		}
	}

	/*
	 * If the port is the middle of closing, bail out now
	 */
	if (tty_hung_up_p(filp) ||
	    (info->flags & ASYNC_CLOSING)) {
		if (info->flags & ASYNC_CLOSING)
			interruptible_sleep_on(&info->close_wait);
		MOD_DEC_USE_COUNT;
		return -EAGAIN;
	}

	/*
	 * Start up serial port
	 */
	retval = startup(info);
	if (retval) {
		MOD_DEC_USE_COUNT;
		return retval;
	}

	retval = block_til_ready(tty, filp, info);
	if (retval) {
#ifdef SERIAL_DEBUG_OPEN
		printk("rs_open returning after block_til_ready with %d\n",
		       retval);
#endif
		MOD_DEC_USE_COUNT;
		return retval;
	}

	if ((info->state->count == 1) &&
	    (info->flags & ASYNC_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = info->state->normal_termios;
		else 
			*tty->termios = info->state->callout_termios;
		change_speed(info, 0);
	}
#ifdef CONFIG_SERIAL_NETARM_CONSOLE
	if (sercons.cflag && sercons.index == line) {
		tty->termios->c_cflag = sercons.cflag;
		sercons.cflag = 0;
		change_speed(info, 0);
	}
#endif
	info->session = current->session;
	info->pgrp = current->pgrp;

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open ttyS%d successful\n", info->line);
#endif
	return 0;
}

/*====================================================================
  Support for /proc/netarm-serial
 */
#ifdef CONFIG_PROC_FS

static struct proc_dir_entry *proc_nas;

static inline int
line_info(char *buf, struct serial_state *state)
{
	char	stat_buf[30];
	int	ret;
	unsigned long flags, status, control;
	netarm_serial_channel_t *regs;

	ret = sprintf(buf, "%d: NetARM SER port:%lX irq:%d",
		      state->line, state->port, state->irq);

	if (!state->port || (state->type == PORT_UNKNOWN)) {
		ret += sprintf(buf+ret, "\n");
		return ret;
	}

	/*
	 * Figure out the current RS-232 lines
	 */

	regs = (netarm_serial_channel_t *) state->iomem_base;
	save_flags(flags); cli();
	status = regs->status_a;
	control = regs->ctrl_a;
	restore_flags(flags); 

	stat_buf[0] = 0;
	stat_buf[1] = 0;
	if (control & NETARM_SER_CTLA_RTS_EN)
		strcat(stat_buf, "|RTS");
	if (status & NETARM_SER_STATA_CTS)
		strcat(stat_buf, "|CTS");
	if (control & NETARM_SER_CTLA_DTR_EN)
		strcat(stat_buf, "|DTR");
	if (status & NETARM_SER_STATA_DSR)
		strcat(stat_buf, "|DSR");
	if (status & NETARM_SER_STATA_DCD)
		strcat(stat_buf, "|CD");
	if (status & NETARM_SER_STATA_RI)
		strcat(stat_buf, "|RI");

	ret += sprintf(buf+ret, " tx:%d rx:%d",
		      state->icount.tx, state->icount.rx);

	if (state->icount.frame)
		ret += sprintf(buf+ret, " fe:%d", state->icount.frame);
	
	if (state->icount.parity)
		ret += sprintf(buf+ret, " pe:%d", state->icount.parity);
	
	if (state->icount.brk)
		ret += sprintf(buf+ret, " brk:%d", state->icount.brk);	

	if (state->icount.overrun)
		ret += sprintf(buf+ret, " oe:%d", state->icount.overrun);

	/*
	 * Last thing is the RS-232 status lines
	 */
	ret += sprintf(buf+ret, " %s\n", stat_buf+1);
	return ret;
}

int
nas_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int i, len = 0, l;
	off_t	begin = 0;

	len += sprintf(page, "serinfo:1.0 driver:%s%s revision:%s\n",
		       serial_version, LOCAL_VERSTRING, serial_revdate);
	for (i = 0; i < NR_NAS_PORTS && len < 4000; i++) {
		l = line_info(page + len, &rs_table[i]);
		len += l;
		if (len+begin > off+count)
			goto done;
		if (len+begin < off) {
			begin += len;
			len = 0;
		}
	}
	*eof = 1;
done:
	if (off >= len+begin)
		return 0;
	*start = page + (off-begin);
	return ((count < begin+len-off) ? count : begin+len-off);
}
#endif
/* end of proc support routines */

/*
 * ---------------------------------------------------------------------
 * rs_init() and friends
 *
 * rs_init() is called at boot-time to initialize the serial driver.
 * ---------------------------------------------------------------------
 */

/*
 * This routine prints out the appropriate serial driver version
 * number, and identifies which options were configured into this
 * driver.
 */
static char serial_options[] __initdata =
#ifdef CONFIG_SERIAL_NETARM_CONSOLE
       " CONSOLE"
#define SERIAL_OPT
#endif
#ifdef SERIAL_OPT
       " enabled\n";
#else
       " no serial options enabled\n";
#endif
#undef SERIAL_OPT

static _INLINE_ void
show_serial_version(void)
{
 	printk(KERN_INFO "%s version %s%s (%s) with%s", serial_name,
	       serial_version, LOCAL_VERSTRING, serial_revdate,
	       serial_options);
}

/*
 * This routine is called by rs_init() to initialize a specific serial
 * port.  It determines what type of UART chip this serial port is
 * using: 8250, 16450, 16550, 16550A.
 * In the NetARM case, we already know what's there.
 */
static void
autoconfig(struct serial_state * state)
{
	struct netarm_async_struct *info, scr_info;
	unsigned long flags;

	if (!CONFIGURED_SERIAL_PORT(state))
		return;
		
	state->type = PORT_NETARM;

	info = &scr_info;	/* This is just for serial_{in,out} */

	info->magic = SERIAL_MAGIC;
	info->state = state;
	info->port = state->port;
	info->flags = state->flags;
	info->io_type = state->io_type;
	info->registers = (netarm_serial_channel_t *) state->iomem_base;

	save_flags(flags); cli();
	/* Reset FIFO */
	info->registers->status_a = NETARM_SER_STATA_CLR_ALL;
	info->SCSRA = 0;
	restore_flags(flags);
}

int register_serial(struct serial_struct *req);
void unregister_serial(int line);

#ifdef MODULE
#if (LINUX_VERSION_CODE > 0x20100)
EXPORT_SYMBOL(register_serial);
EXPORT_SYMBOL(unregister_serial);
#else
static struct symbol_table serial_syms = {
#include <linux/symtab_begin.h>
	X(register_serial),
	X(unregister_serial),
#include <linux/symtab_end.h>
};
#endif
#endif

/*
 * The serial driver boot-time initialization code
 */
static int __init
rs_init(void)
{
	int i;
	struct serial_state * state;

	init_bh(SERIAL_BH, do_serial_bh);
	show_serial_version();

	/* Initialize the tty_driver structure */
	
	memset(&serial_driver, 0, sizeof(struct tty_driver));
	serial_driver.magic = TTY_DRIVER_MAGIC;
#if (LINUX_VERSION_CODE > 0x20100)
	serial_driver.driver_name = "NetARM serial";
#endif
#if (LINUX_VERSION_CODE > 0x2032D && defined(CONFIG_DEVFS_FS))
	serial_driver.name = "tts/%d";
#else
	serial_driver.name = "ttyS";
#endif
	serial_driver.major = TTY_MAJOR;
	serial_driver.minor_start = 64 + SERIAL_DEV_OFFSET;
	serial_driver.num = NR_NAS_PORTS;
	serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver.subtype = SERIAL_TYPE_NORMAL;
	serial_driver.init_termios = tty_std_termios;
	serial_driver.init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	serial_driver.flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS;
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
	serial_driver.break_ctl = rs_break;
	serial_driver.send_xchar = rs_send_xchar;
	serial_driver.wait_until_sent = rs_wait_until_sent;
	serial_driver.read_proc = nas_read_proc;
	
	/*
	 * The callout device is just like normal device except for
	 * major number and the subtype code.
	 */
	callout_driver = serial_driver;
#if (LINUX_VERSION_CODE > 0x2032D && defined(CONFIG_DEVFS_FS))
	callout_driver.name = "cua/%d";
#else
	callout_driver.name = "cua";
#endif
	callout_driver.major = TTYAUX_MAJOR;
	callout_driver.subtype = SERIAL_TYPE_CALLOUT;
	callout_driver.read_proc = 0;
	callout_driver.proc_entry = 0;

	if (tty_register_driver(&serial_driver))
		panic("Couldn't register serial driver\n");
	if (tty_register_driver(&callout_driver))
		panic("Couldn't register callout driver\n");
	
	for (i = 0, state = rs_table; i < NR_NAS_PORTS; i++,state++)
	{
		state->magic = SSTATE_MAGIC;
		state->line = i;
		state->type = PORT_NETARM;
		state->xmit_fifo_size = NETARM_SER_FIFO_SIZE;
		state->custom_divisor = 0;
		state->close_delay = 5*HZ/10;
		state->closing_wait = 30*HZ;
		state->callout_termios = callout_driver.init_termios;
		state->normal_termios = serial_driver.init_termios;
		state->icount.cts = state->icount.dsr = 
			state->icount.rng = state->icount.dcd = 0;
		state->icount.rx = state->icount.tx = 0;
		state->icount.frame = state->icount.parity = 0;
		state->icount.overrun = state->icount.brk = 0;
		if (i == 0)
			state->irq = IRQ_SER1_RX;
		else
			state->irq = IRQ_SER2_RX;
		state->io_type = SERIAL_IO_MEM;
/* no need to call request_region
		if (state->port && check_region(state->port,8))
			continue;
*/
		if (state->flags & ASYNC_BOOT_AUTOCONF)
			autoconfig(state);
		printk(KERN_INFO "ttyS%02d at 0x%04lx (irq = %d) is a NetARM\n",
		       state->line + SERIAL_DEV_OFFSET,
		       state->port, state->irq );
		tty_register_devfs(&serial_driver, 0,
				   serial_driver.minor_start + state->line);
		tty_register_devfs(&callout_driver, 0,
				   callout_driver.minor_start + state->line);
	}
	nas_interrupts_init();

#ifdef CONFIG_PROC_FS
	if ((proc_nas = create_proc_entry( "netarm-serial", 0, 0 )))
	  proc_nas->read_proc = nas_read_proc;
#endif

	return 0;
}

static void __exit
rs_fini(void) 
{
	unsigned long flags;
	int e1, e2;
	int i;

	/* printk("Unloading %s: version %s\n", serial_name, serial_version); */
	del_timer_sync(&serial_timer);
	save_flags(flags); cli();
        remove_bh(SERIAL_BH);
	if ((e1 = tty_unregister_driver(&serial_driver)))
		printk("serial: failed to unregister serial driver (%d)\n",
		       e1);
	if ((e2 = tty_unregister_driver(&callout_driver)))
		printk("serial: failed to unregister callout driver (%d)\n", 
		       e2);
	restore_flags(flags);
/*
	for (i = 0; i < NR_NAS_PORTS; i++) {
		if ((rs_table[i].type != PORT_UNKNOWN) && rs_table[i].port) {
			release_region(rs_table[i].port, 8);
		}
	}
*/
	if (tmp_buf) {
		unsigned long pg = (unsigned long) tmp_buf;
		tmp_buf = NULL;
		free_page(pg);
	}
}

module_init(rs_init);
module_exit(rs_fini);
MODULE_DESCRIPTION("NetARM serial driver");
MODULE_AUTHOR("Rolf Peukert <peukert@imms.de>");


/*
 * ------------------------------------------------------------
 * Serial console driver
 * ------------------------------------------------------------
 */
#ifdef CONFIG_SERIAL_NETARM_CONSOLE

#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)

static struct netarm_async_struct async_sercons;

/*
 *	Wait for transmitter & holding register to empty
 */
static inline void
wait_for_xmitr(struct netarm_async_struct *info)
{
	unsigned int status, tmout = 1000000;

	do
	{
		status = info->registers->status_a;

		if (status & NETARM_SER_STATA_RX_BRK)
			lsr_break_flag = NETARM_SER_STATA_RX_BRK;
		if (--tmout == 0)
			break;
	} while ((status & NETARM_SER_STATA_TX_DMAEN) == 0);
	/* This waits until transmitter has finished all characters in fifo.
	   FIXME: Waiting for the transmit holding register to empty
	   (checking NETARM_SER_STATA_TX_FULL) doesn't seem to work. */

	/* Wait for flow control if necessary */
	if (info->flags & ASYNC_CONS_FLOW)
	{
		tmout = 1000000;
		while (--tmout && \
		       (info->registers->status_a & NETARM_SER_STATA_CTS) == 0)
		{}
	}
}

/*
 *	Print a string to the serial port trying not to disturb
 *	any possible real use of the port...
 *
 *	The console_lock must be held when we get here.
 */
static void
serial_console_write(struct console *co, const char *s, unsigned count)
{
	static struct netarm_async_struct *info = &async_sercons;
	volatile netarm_serial_channel_t *regs;
	unsigned long ier;
	volatile unsigned long *ser_port_fifo;
	unsigned long *bPtr = (unsigned long *)s;
	unsigned char *zPtr = (unsigned char *)s;
	unsigned long ctmp;
	int scount;

	/*
	 *	First save the IER then disable the interrupts
	 */
	regs = info->registers;
	ier  = regs->ctrl_a;
	regs->ctrl_a = ier & ~(NETARM_SER_CTLA_IE_RX_ALL | NETARM_SER_CTLA_IE_TX_ALL);

	/*
	 *	Now, do all characters, add CR to every LF
	 */
	ser_port_fifo = (volatile unsigned long *) &(regs->fifo);
	zPtr = (unsigned char *)bPtr;
	while ( count > 0 )
	{
		ctmp = 0 ;
		scount = 0 ;
  		while ( ( count > 0 ) && ( scount < 4 ) )
		{
			ctmp += *zPtr << ( scount << 3 ) ;
			if (*zPtr == '\n')
			{
				if ( scount < 3 )
				{
					scount++;
					ctmp += '\r' << (scount << 3) ; 
				}
				else
				{
					NAS_TX_WAIT_RDY(regs);
					*ser_port_fifo = ctmp;
					scount = 0 ;
					ctmp = '\r' ;	
				}
			}
			zPtr++;
			count -- ;
			scount ++ ;
		}
		NAS_TX_WAIT_RDY(regs);
		*ser_port_fifo = ctmp;	
	}
	/*
	 *	Finally, Wait for transmitter & holding register to empty
	 * 	and restore the IER
	 */
	wait_for_xmitr(info);
	regs->ctrl_a = ier;
}

static kdev_t
serial_console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, 64 + c->index);
}

/*
 *	Setup initial baud/bits/parity/flow control. We do two things here:
 *	- construct a cflag setting for the first rs_open()
 *	- initialize the serial port
 *	Return non-zero if we didn't find a serial port.
 */
static int __init
serial_console_setup(struct console *co, char *options)
{
	static struct netarm_async_struct *info;
	volatile netarm_serial_channel_t *regs;
	struct serial_state *state;
	unsigned cval;
	int	baud = 9600;
	int	bits = 8;
	int	parity = 'n';
	int	doflow = 0;
	int	cflag = CREAD | HUPCL | CLOCAL;
	int	quot = 0;
	char	*s;

	if (options) {
		baud = simple_strtoul(options, NULL, 10);
		s = options;
		while(*s >= '0' && *s <= '9')
			s++;
		if (*s) parity = *s++;
		if (*s) bits   = *s++ - '0';
		if (*s) doflow = (*s++ == 'r');
	}

	/*
	 *	Now construct a cflag setting.
	 *      'cval' will be written to the NetARM "Control A" register
	 */
	cval = NETARM_SER_CTLA_ENABLE;
	switch(baud) {
		case 1200:
			cflag |= B1200;
			break;
		case 2400:
			cflag |= B2400;
			break;
		case 4800:
			cflag |= B4800;
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
		case 115200:
			cflag |= B115200;
			break;
		case 9600:
		default:
			cflag |= B9600;
			break;
	}
	switch(bits) {
		case 7:
			cflag |= CS7;
			cval  |= NETARM_SER_CTLA_7BITS;
			break;
		default:
		case 8:
			cflag |= CS8;
			cval  |= NETARM_SER_CTLA_8BITS;
			break;
	}
	switch(parity) {
		case 'o': case 'O':
			cflag |= PARODD;
			cval  |= NETARM_SER_CTLA_P_ODD;
			break;
		case 'e': case 'E':
			cflag |= PARENB;
			cval  |= NETARM_SER_CTLA_P_EVEN;
			break;
	}
	co->cflag = cflag;

	/*
	 *	Divisor, bytesize and parity
	 */
	state = rs_table + co->index;
	if (doflow)
		state->flags |= ASYNC_CONS_FLOW;
	info = &async_sercons;
	info->magic = SERIAL_MAGIC;
	info->state = state;
	info->port = state->port;
	info->flags = state->flags;
	info->io_type = state->io_type;
	info->registers = (netarm_serial_channel_t *) state->iomem_base;
	quot = state->baud_base / baud;
	cval |= NETARM_SER_CTLA_2STOP;
	/*
	 *	Disable UART interrupts, set DTR and RTS high
	 *	and set speed.
	 */
	regs = info->registers;
	regs->rx_match = 0;
	regs->rx_buf_timer = 0;
	regs->bitrate = NETARM_SER_BR_X16(baud);
	regs->rx_char_timer = NETARM_SER_RXGAP(baud);
	regs->ctrl_b = NETARM_SER_CTLB_RCGT_EN | NETARM_SER_CTLB_UART_MODE;
	regs->ctrl_a = cval | NETARM_SER_CTLA_DTR_EN | NETARM_SER_CTLA_RTS_EN;
	regs->status_a = NETARM_SER_STATA_CLR_ALL;
	/*
	 *	If we read 0xff from the status, there is no UART here.
	 */
	if (regs->status_a == 0xffffffff)
		return -1;

	return 0;
}

static struct console sercons = {
	name:		"ttyS",
	write:		serial_console_write,
	device:		serial_console_device,
	setup:		serial_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

/*
 *	Register console.
 */
void __init
serial_netarm_console_init(void)
{
	register_console(&sercons);
}
#endif /* CONFIG_SERIAL_NETARM_CONSOLE */

