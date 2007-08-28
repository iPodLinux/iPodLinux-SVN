/*
 * serial_s3c44b0x.c: Serial port driver for the Samsung S3C44B0X builtin UART
 *
 * Copyright (c) 2003	sympat GmbH
 *			by Thomas Eschenbacher <thomas.eschenbacher@sympat.de>
 *
 * Copyright (c) 2001	Arcturus Networks Inc. 
 * 			by Oleksandr Zhadan  <oleks@arcturusnetworks.com>
 * Copyright (c) 2002	Arcturus Networks Inc. 
 * 		       	by Michael Leslie    <mleslie@arcturusnetworks.com>
 *
 * Based on: drivers/char/trioserial.c
 * Copyright (C) 1998  Kenneth Albanowski <kjahds@kjahds.com>,
 *                     D. Jeff Dionne <jeff@arcturusnetworks.com>,
 *                     The Silver Hammer Group, Ltd.
 *
 * Based on: drivers/char/68328serial.c
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */


#include <linux/config.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/serial.h>
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
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/arch/irq.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <asm/bitops.h>
#include <asm/delay.h>

#define queue_task_irq_off queue_task
#define copy_from_user(a,b,c) memcpy_fromfs(a,b,c)
#define copy_to_user(a,b,c) memcpy_tofs(a,b,c)

#include <asm/uaccess.h>

#include "serial_s3c44b0x.h"

#if defined(CONFIG_UCBOOTSTRAP)
#include <asm/uCbootstrap.h>
extern  char *getbenv (char *a);
#endif

#define USART_CNT		1
					/* Receive FIFO trigger level	*/
#define RX_FIFO_LEVEL		4	/*  0 (off) 4 or 8 or 12 or 16	*/
					/* Transfer FIFO trigger level	*/
#define TX_FIFO_LEVEL		0	/*	0 or 4 or 8 or 12	*/

#define TX_FIFO_DEPTH		(5) 	/* maximum transmit FIFO length */
#define RX_FIFO_DEPTH		(8) 	/* maximum receive FIFO length  */

/* serial subtype definitions */
#define SERIAL_TYPE_NORMAL	1
#define SERIAL_TYPE_CALLOUT	2

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256

/* Debugging... DEBUG_INTR is bad to use when one of the zs
 * lines is your console ;(
 */
#undef SERIAL_DEBUG_INTR
#undef SERIAL_DEBUG_OPEN
#undef SERIAL_DEBUG_FLOW
#define SERIAL_DEBUG_THROTTLE

#define _INLINE_ inline

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

static struct s3c44b0x_serial s3c44b0x_info;

/*
 * tmp_buf is used as a temporary buffer by serial_write.  We need to
 * lock it in case the memcpy_fromfs blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
 
static unsigned char tmp_buf[SERIAL_XMIT_SIZE];	/* This is cheating */
static DECLARE_MUTEX (tmp_buf_sem);

/* Console hooks... */
static s3c_baud_table_t default_console_baud = { 115200, B115200 };

#if defined(CONFIG_SERIAL_S3C44B0X_CONSOLE)
static int console_number =  0;
#endif
	
DECLARE_TASK_QUEUE(tq_s3c44b0x_serial);

static struct tty_driver serial_driver, callout_driver;
static int serial_refcount;

static struct tty_struct 	*serial_table[USART_CNT];
static struct termios 		*serial_termios[USART_CNT];
static struct termios		*serial_termios_locked[USART_CNT];

static void change_speed	(struct s3c44b0x_serial *info);

static _INLINE_ void tx_stop	(void);
static _INLINE_ void tx_start	(int ints);
static _INLINE_ void rx_stop	(void);
static _INLINE_ void rx_start	(int ints);
static _INLINE_ void xmit_char		(struct s3c44b0x_serial *info, char ch);
static _INLINE_ void rs_sched_event	(struct s3c44b0x_serial *info, int event);
static _INLINE_ void uart_speed		(struct s3c44b0x_serial *info, unsigned int cflag);
static _INLINE_ void handle_status 	(struct s3c44b0x_serial *info, unsigned int status);
static _INLINE_ void fifo_reset		(void);
static _INLINE_ void fifo_init		(void);

static void set_ints_mode	(struct s3c44b0x_serial *info, int yes);
static void rs_interruptTx	(int irq, void *dev_id, struct pt_regs *regs);
static void rs_interruptRx	(int irq, void *dev_id, struct pt_regs *regs);
static void rs_interruptErr	(int irq, void *dev_id, struct pt_regs *regs);

static void batten_down_hatches (void);
extern void show_net_buffers	(void);
extern void hard_reset_now	(void);
static void handle_termios_tcsets(struct termios *ptermios, struct s3c44b0x_serial *pinfo);


/******************************************************************************/
static _INLINE_ void tx_delay(void)
{
	volatile int i;
	for (i=0; i < 50; i++) {
		i = i+1;
		i = i-1;
	};
}

static _INLINE_ void tx_start(int ints)
{
	if (ints) enable_irq(S3C44B0X_INTERRUPT_UTX0);
}

static _INLINE_ void tx_stop(void)
{
	disable_irq(S3C44B0X_INTERRUPT_UTX0);
}

static _INLINE_ void rx_start(int ints)
{
	if (ints) {
		enable_irq(S3C44B0X_INTERRUPT_URX0);
		enable_irq(S3C44B0X_INTERRUPT_UERR);
	}
}

static _INLINE_ void rx_stop(void)
{
	disable_irq(S3C44B0X_INTERRUPT_URX0);
	disable_irq(S3C44B0X_INTERRUPT_UERR);
}

static void set_ints_mode(struct s3c44b0x_serial *info, int yes)
{
	info->use_ints = yes;
	if (yes) {
		s3c44b0x_unmask_irq(S3C44B0X_INTERRUPT_UTX0);
		s3c44b0x_unmask_irq(S3C44B0X_INTERRUPT_URX0);
		s3c44b0x_unmask_irq(S3C44B0X_INTERRUPT_UERR);
	} else {
		s3c44b0x_mask_irq(S3C44B0X_INTERRUPT_UERR);
		s3c44b0x_mask_irq(S3C44B0X_INTERRUPT_URX0);
		s3c44b0x_mask_irq(S3C44B0X_INTERRUPT_UTX0);
	}
}

static void hw_flow_control (int cmd)
{
	/* the MBA44 board does not have hw flow control */
}

static _INLINE_ void fifo_reset()
{
	u_int8_t ufcon = inb(S3C44B0X_UFCON0);
	ufcon |= S3C44B0X_UFCON_RX_FIFO_RST | S3C44B0X_UFCON_TX_FIFO_RST;
	outb(ufcon, S3C44B0X_UFCON0);
}

static _INLINE_ void fifo_init()
{
	u_int8_t ufcon = inb(S3C44B0X_UFCON0);

	if (RX_FIFO_LEVEL + TX_FIFO_LEVEL)
		ufcon |= S3C44B0X_UFCON_FIFO_EN
                      | S3C44B0X_UCON_TXINT_LEVEL
                      | S3C44B0X_UCON_RXINT_LEVEL;

	switch (RX_FIFO_LEVEL) {
		case 4 :	ufcon |= S3C44B0X_UFCON_RX_FIFO_4; break;
		case 8 :	ufcon |= S3C44B0X_UFCON_RX_FIFO_8; break;
		case 12:	ufcon |= S3C44B0X_UFCON_RX_FIFO_12; break;
		case 16:	ufcon |= S3C44B0X_UFCON_RX_FIFO_16; break;
	}
	switch (TX_FIFO_LEVEL) {
		case 0 :	ufcon |= S3C44B0X_UFCON_TX_FIFO_0; break;
		case 4 :	ufcon |= S3C44B0X_UFCON_TX_FIFO_4; break;
		case 8 :	ufcon |= S3C44B0X_UFCON_TX_FIFO_8; break;
		case 12 :	ufcon |= S3C44B0X_UFCON_TX_FIFO_12; break;
	}
	outb(ufcon, S3C44B0X_UFCON0);
}

static inline int serial_paranoia_check(struct s3c44b0x_serial *info,
					dev_t device, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char *badmagic =
		"Warning: bad magic number for serial struct (%d, %d) in %s\n";
	static const char *badinfo =
		"Warning: null s3c44b0x_serial struct for (%d, %d) in %s\n";

	if  (!info) {
		printk(badinfo, MAJOR(device), MINOR(device), routine);
		return 1;
	}
	if  (info->magic != SERIAL_MAGIC) {
		printk(badmagic, MAJOR(device), MINOR(device), routine);
		return 1;
	}
#endif
    return 0;
}

/* Sets or clears DTR on the requested line */
static inline void set_dtr(struct s3c44b0x_serial *info, int set)
{
	/* we do not need hw flow control */
}

/* Sets or clears RTS on the requested line */
/* Note: rts is only directly controllable if hw flow
 * control is not enabled */
static inline void set_rts(struct s3c44b0x_serial *info, int set)
{
	/* we do not need hw flow control */
}

/* Reads value of serial status signals */
static unsigned int get_status(struct s3c44b0x_serial *info)
{
	/* we do not need hw flow control */
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
static void rs_stop(struct tty_struct *tty)
{
    struct s3c44b0x_serial *info = (struct s3c44b0x_serial *) tty->driver_data;
    unsigned long flags = 0;

    if	(serial_paranoia_check(info, tty->device, "rs_stop"))
    	return;

    save_flags(flags);
    cli();
    tx_stop();
    rx_stop();
    restore_flags(flags);
}

static void rs_put_char(struct s3c44b0x_serial *info, char ch)
{
    unsigned long flags = 0;

    save_flags(flags);
    cli();
    xmit_char(info, ch);
    restore_flags(flags);
}

static void rs_start(struct tty_struct *tty)
{
    struct s3c44b0x_serial *info = (struct s3c44b0x_serial *) tty->driver_data;
    unsigned long flags = 0;

    if  (serial_paranoia_check(info, tty->device, "rs_start"))
	return;

    save_flags(flags);
    cli();
    rx_start(info->use_ints);
    tx_start(info->use_ints);
    restore_flags(flags);
}

/* Drop into either the boot monitor or kadb upon receiving a break
 * from keyboard/console input.
 */
static void batten_down_hatches(void)
{
#if 0
	/* If we are doing kadb, we call the debugger
	 * else we just drop into the boot monitor.
	 * Note that we must flush the user windows
	 * first before giving up control.
	 */
	if ((((unsigned long) linux_dbvec) >= DEBUG_FIRSTVADDR) &&
		(((unsigned long) linux_dbvec) <= DEBUG_LASTVADDR))
		sp_enter_debugger();
	else
		panic("s3c44b0x_serial: batten_down_hatches");
	return;
#endif
}

/*
 * ----------------------------------------------------------------------
 *
 * Here starts the interrupt handling routines.  All of the following
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
 * and look at the resulting assembly code in serial.s.
 *
 * 				- Ted Ts'o (tytso@mit.edu), 7-Mar-93
 * -----------------------------------------------------------------------
 */

/*
 * This routine is used by the interrupt handler to schedule
 * processing in the software interrupt portion of the driver.
 */
 
static _INLINE_ void rs_sched_event(struct s3c44b0x_serial *info, int event)
{
	info->event |= 1 << event;
	queue_task_irq_off(&info->tqueue, &tq_s3c44b0x_serial);
	mark_bh(SERIAL_BH);
}

/*  
 *	This is the serial driver's generic interrupt routine
 */ 
static void rs_interruptTx(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned int count, status;
	struct s3c44b0x_serial *info = &s3c44b0x_info;

	if  (info->x_char) {
		tx_delay();
		outb(info->x_char, S3C44B0X_UTXH0);
		info->x_char = 0;
		return;
	}

	if  ((info->xmit_cnt <= 0) || info->tty->stopped || info->tty->hw_stopped ) {
		tx_stop();
		s3c44b0x_mask_irq(S3C44B0X_INTERRUPT_UTX0);
		return;
	}

	for (count = TX_FIFO_DEPTH; count > 0; --count) {
		status = inb(S3C44B0X_UFSTAT0);
		if (status & S3C44B0X_UFSTAT_TX_FIFO_FULL) {
			handle_status (info, S3C44B0X_UFSTAT_TX_FIFO_FULL);
			break;
		} else {
			tx_delay();
			outb(info->xmit_buf[info->xmit_tail++], 
				S3C44B0X_UTXH0);
			info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE - 1);
			if (--info->xmit_cnt <= 0) 
				break;
		}
	}

	if (info->xmit_cnt < WAKEUP_CHARS)
		rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);

	if (info->xmit_cnt <= 0)
	{
		tx_stop();
		s3c44b0x_mask_irq(S3C44B0X_INTERRUPT_UTX0);
	}
	return;
}

#define S3C44B0X_UERSTAT_ERROR  (	S3C44B0X_UERSTAT_OVERRUN_ERROR |\
					S3C44B0X_UERSTAT_PARITY_ERROR |\
					S3C44B0X_UERSTAT_FRAME_ERROR |\
					S3C44B0X_UERSTAT_BREAK_DETECT	)

static void rs_interruptRx(int irq, void *dev_id, struct pt_regs *regs)
{
	struct s3c44b0x_serial *info = &s3c44b0x_info;
	struct tty_struct *tty = info->tty;
	unsigned int count;
	volatile u_int8_t status, fifo_status;

	// printk("--- Rx INT ---\n"); // ###
	
	if (!info || !tty || (!(info->flags & S_INITIALIZED)))
		return;

	if ((tty->flip.count + RX_FIFO_DEPTH) >= TTY_FLIPBUF_SIZE)
		queue_task_irq_off(&tty->flip.tqueue, &tq_timer);

	count = RX_FIFO_DEPTH;
	do {
		/* check all error flags and accept data if valid */
		fifo_status = inb(S3C44B0X_UFSTAT0);
		if (!(fifo_status & S3C44B0X_UFSTAT_RX_FIFO_COUNT))
			break;  //if 0 bytes exit

		status = inb(S3C44B0X_UERSTAT0);
		if (!(status & S3C44B0X_UERSTAT_ERROR) )
			*tty->flip.flag_buf_ptr = TTY_NORMAL;
		else {
			if (fifo_status & S3C44B0X_UFSTAT_RX_FIFO_FULL) {
				*tty->flip.flag_buf_ptr = TTY_NORMAL;
				handle_status (info, S3C44B0X_UFSTAT_RX_FIFO_FULL);
			}
			if (status & S3C44B0X_UERSTAT_OVERRUN_ERROR) {
				*tty->flip.flag_buf_ptr = TTY_OVERRUN;
				handle_status (info, S3C44B0X_UERSTAT_OVERRUN_ERROR);
			}
			if 	(status & S3C44B0X_UERSTAT_BREAK_DETECT) {
				*tty->flip.flag_buf_ptr = TTY_BREAK;
				handle_status (info, S3C44B0X_UERSTAT_BREAK_DETECT);
			}
			if	(status & S3C44B0X_UERSTAT_PARITY_ERROR) {
				*tty->flip.flag_buf_ptr = TTY_PARITY;
				handle_status (info, S3C44B0X_UERSTAT_PARITY_ERROR);
			}
			if 	(status & S3C44B0X_UERSTAT_FRAME_ERROR) {
				*tty->flip.flag_buf_ptr = TTY_FRAME;
				handle_status (info, S3C44B0X_UERSTAT_FRAME_ERROR);
			}
		}
		*tty->flip.char_buf_ptr++ = 
			inb(S3C44B0X_URXH0);
		tty->flip.flag_buf_ptr++;
		tty->flip.count++;
	} while ((--count > 0) && !(status & S3C44B0X_UERSTAT_ERROR) );

//     if (fifo_status & (U_RFOV | U_E_RxTO))
// 		handle_status (info, (U_RFOV | U_E_RxTO));
	
	queue_task_irq_off(&tty->flip.tqueue, &tq_timer);	
}

static void rs_interruptErr(int irq, void *dev_id, struct pt_regs *regs)
{
	rs_interruptRx(irq, dev_id, regs);
}

static _INLINE_ void handle_status ( struct s3c44b0x_serial *info, unsigned int status )
{
//     if (status & U_TFFUL) {
// 		return;				/* FIXME - do something */
// 	}
//     if (status & U_RFFUL) {
// 		info->usart->cr |= U_SBR;	
// 	}
//     if (status & U_BSD) {
// 		batten_down_hatches();
// 	}
//     info->usart->csr |= status;
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
 * rs_interrupt() has returned, BUT WITH INTERRUPTS TURNED ON.  This
 * is where time-consuming activities which can not be done in the
 * interrupt driver proper are done; the interrupt driver schedules
 * them using rs_sched_event(), and they get done here.
 */
static void do_serial_bh(void)
{
	run_task_queue(&tq_s3c44b0x_serial);
}

static void do_softint(void *private_)
{
	struct s3c44b0x_serial *info = (struct s3c44b0x_serial *) private_;
	struct tty_struct *tty;

	tty = info->tty;
	if (!tty ) return;

	if  (test_and_clear_bit(RS_EVENT_WRITE_WAKEUP, &info->event)) {
		if  ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc.write_wakeup) 
		(tty->ldisc.write_wakeup) (tty);
		wake_up_interruptible(&tty->write_wait);
	}
}

/*
 * This routine is called from the scheduler tqueue when the interrupt
 * routine has signalled that a hangup has occurred.  The path of
 * hangup processing is:
 *
 * 	serial interrupt routine -> (scheduler tqueue) ->
 * 	do_serial_hangup() -> tty->hangup() -> rs_hangup()
 *
 */
static void do_serial_hangup(void *private_)
{
	struct s3c44b0x_serial *info = (struct s3c44b0x_serial *) private_;
	struct tty_struct *tty;
	
	tty = info->tty;
	if (!tty)	return;
	tty_hangup(tty);
}

static u_int16_t calcCD(u_int32_t br)
{
	/* effectively add 1/2 LSB before dividing */
	u_int32_t divisor = (CONFIG_ARM_CLK >> 3) / br - 1;
	return (divisor >> 1);
}

static _INLINE_ void uart_speed(struct s3c44b0x_serial *info, unsigned int cflag)
{
//	unsigned baud = info->baud;
//	u_int16_t divisor = calcCD(baud);
// ###	outw(divisor, S3C44B0X_UBRDIV);
}

static int startup(struct s3c44b0x_serial *info)
{
	unsigned long flags;

	if (info->flags & S_INITIALIZED)
		return 0;

	if (!info->xmit_buf) {
		info->xmit_buf = (unsigned char *) get_free_page(GFP_KERNEL);
		if (!info->xmit_buf) return -ENOMEM;
	}

	save_flags(flags);    cli();

#ifdef SERIAL_DEBUG_OPEN
	printk("starting up ttyS%d (irq %d)...\n", info->line, info->irq);
#endif

	/*
	 * Clear the FIFO buffers and disable them
	 * (they will be reenabled in change_speed())
	 */
	fifo_reset();

	/*
	 * Now, initialize the UART 
	 */ 
	if (info->tty) clear_bit(TTY_IO_ERROR, &info->tty->flags);
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	
	/*
	 * and set the speed of the serial port
	 */
	change_speed(info);
	
	info->flags |= S_INITIALIZED;
	restore_flags(flags);
	return 0;
}

/*
 * This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */
static void shutdown(struct s3c44b0x_serial *info)
{
	unsigned long flags;
	
	tx_stop();
	rx_stop();	/* All off! */
	if (!(info->flags & S_INITIALIZED))
		return;
	
#ifdef SERIAL_DEBUG_OPEN
	printk("Shutting down serial port %d (irq %d)....\n", info->line,
			info->irq);
#endif
	
	save_flags(flags);    cli();		/* Disable interrupts */
	
	if (info->xmit_buf) {
		free_page((unsigned long) info->xmit_buf);
		info->xmit_buf = 0;
	}

	if  (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);

	info->flags &= ~S_INITIALIZED;
	restore_flags(flags);
}

static s3c_baud_table_t baud_table[] = {
	{      0, B0 },
	{     50, B50 },
	{     75, B75 },
	{    110, B110 },
	{    134, B134 },
	{    150, B150 },
	{    200, B200 },
	{    300, B300 },
	{    600, B600 },
	{   1200, B1200 },
	{   1800, B1800 },
	{   2400, B2400 },
	{   4800, B4800 },
	{   9600, B9600 },
	{  19200, B19200 },
	{  38400, B38400 },
	{  57600, B57600 },
	{ 115200, B115200 },
	{ 230400, B230400 },
	{      -1, -1 }
};

/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */
static void change_speed(struct s3c44b0x_serial *info)
{
	unsigned cflag;
	int      table_index;
	u_int8_t ulcon, ucon;
	
	if (!info->tty || !info->tty->termios)
		return;
	cflag = info->tty->termios->c_cflag;
	
	tx_stop();
	rx_stop();
	
	/* UART baudrate setup */
	table_index = cflag & CBAUD;
	if (table_index & CBAUDEX)	{
		table_index &= ~CBAUDEX; 	 /* remove CBAUDEX bit */
		table_index += CBAUD & ~CBAUDEX; /* add high rate offset */
	}
	info->baud = baud_table[table_index].rate;
	uart_speed(info, cflag);

	/* UART line control register setup */
	ulcon = 0x00;
	switch  (cflag & CSIZE) {
		case CS5: ulcon |= S3C44B0X_ULCON_WORDLN_5; break;
		case CS6: ulcon |= S3C44B0X_ULCON_WORDLN_6; break;
		case CS7: ulcon |= S3C44B0X_ULCON_WORDLN_7; break;
		case CS8: ulcon |= S3C44B0X_ULCON_WORDLN_8; break;
	}
	if (cflag & PARENB)
		ulcon |= (cflag & PARODD) ? 
			S3C44B0X_ULCON_PAR_ODD : S3C44B0X_ULCON_PAR_EVEN;
	if (cflag & CSTOPB)
		ulcon |= S3C44B0X_ULCON_STOPB_2;
	outb(ulcon, S3C44B0X_ULCON0);

	/* UART control register setup */
	ucon = 0;
	ucon |= S3C44B0X_UCON_RX_MODE_INT_POLL;   /* receive mode: IRQ or polling */
	ucon |= S3C44B0X_UCON_TX_MODE_INT_POLL;   /* transmit mode: IRQ or polling */
	ucon |= S3C44B0X_UCON_RX_ERR_INT_EN;      /* Rx status interrupt enable */
	ucon |= S3C44B0X_UCON_RX_TIMEOUT_EN;      /* Rx timeout enable (5 bits) */
	outb(ucon, S3C44B0X_UCON0);
	
	tx_start(info->use_ints);
	rx_start(info->use_ints);

	return;
}

/*
 *	Wait for transmitter & holding register to empty
 */
static inline void wait_for_xmitr(void)
{
	volatile unsigned int status, tmout = 100000;
	do {
		status = inb(S3C44B0X_UTRSTAT0);
		if (--tmout == 0)
			break;
	} while((status & (	S3C44B0X_UTRSTAT_TBE |		\
				S3C44B0X_UTRSTAT_TSE)) !=	\
				(S3C44B0X_UTRSTAT_TBE |		\
				 S3C44B0X_UTRSTAT_TSE)	);
}

static _INLINE_ void xmit_char(struct s3c44b0x_serial *info, char ch)
{
	wait_for_xmitr();	
	tx_delay();
	outb(ch, S3C44B0X_UTXH0);
}

static void rs_set_ldisc(struct tty_struct *tty)
{
	struct s3c44b0x_serial *info = (struct s3c44b0x_serial *) tty->driver_data;

	if  (serial_paranoia_check(info, tty->device, "rs_set_ldisc"))
		return;

	info->is_cons = (tty->termios->c_line == N_TTY);

	printk("ttyS%d console mode %s\n", info->line, info->is_cons ? "on" : "off");
}

static void rs_flush_chars(struct tty_struct *tty)
{
	struct s3c44b0x_serial *info = (struct s3c44b0x_serial *) tty->driver_data;
	unsigned long flags;

	if  (serial_paranoia_check(info, tty->device, "rs_flush_chars"))
		return;

	if  (!info->use_ints) {
		for (;;) {
		if  (info->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped || !info->xmit_buf) 
			return;
		save_flags(flags);	    cli();
		tx_start(info->use_ints);
		xmit_char(info, info->xmit_buf[info->xmit_tail++]);
		info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE - 1);
		info->xmit_cnt--;
		}
	} else {
		if  (info->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped || !info->xmit_buf) 
			return;
		save_flags(flags);	cli();
		tx_start(info->use_ints);
		xmit_char(info, info->xmit_buf[info->xmit_tail++]);
		info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE - 1);
		info->xmit_cnt--;
	}
	restore_flags(flags);
}

static int rs_write(struct tty_struct *tty, int from_user,
					const unsigned char *buf, int count)
{
	int c, total = 0;
	struct s3c44b0x_serial *info = (struct s3c44b0x_serial *) tty->driver_data;
	unsigned long flags;

	if  (serial_paranoia_check(info, tty->device, "rs_write"))
		return 0;

	if  (!tty || !info->xmit_buf || !tmp_buf)
		return 0;

	save_flags(flags);
	if (from_user) {
		down(&tmp_buf_sem);
		cli();
		while (1) {
			c = MIN(count, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
				SERIAL_XMIT_SIZE - info->xmit_head));
			if  (c <= 0)
				break;
			
			memcpy(info->xmit_buf + info->xmit_head, buf, c);
			info->xmit_head = (info->xmit_head + c) & (SERIAL_XMIT_SIZE - 1);
			info->xmit_cnt += c;
			restore_flags(flags);
			buf += c;
			count -= c;
			total += c;
		}
		restore_flags(flags);
		up(&tmp_buf_sem);

	} else {
		cli();
		while (1) {
			c = MIN(count, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
					SERIAL_XMIT_SIZE - info->xmit_head));
			if  (c <= 0)
				break;
			memcpy(info->xmit_buf + info->xmit_head, buf, c);
			info->xmit_head = (info->xmit_head + c) & (SERIAL_XMIT_SIZE - 1);
			info->xmit_cnt += c;
			buf += c;
			count -= c;
			total += c;
		}
		restore_flags(flags);
	}

	if  (info->xmit_cnt && !tty->stopped && !tty->hw_stopped) {
		if   (!info->use_ints) {
			while (info->xmit_cnt) {
				/* Send char */
				xmit_char(info, info->xmit_buf[info->xmit_tail++]);
				info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE - 1);
				info->xmit_cnt--;
			}
		} 
		else {
			tx_start(info->use_ints);
		}
	} 
	restore_flags(flags);
	
	return total;
}

static int rs_write_room(struct tty_struct *tty)
{
	struct s3c44b0x_serial *info = (struct s3c44b0x_serial *) tty->driver_data;
	int ret;

	if  (serial_paranoia_check(info, tty->device, "rs_write_room"))
		return 0;
	ret = SERIAL_XMIT_SIZE - info->xmit_cnt - 1;
	if  (ret < 0)
		ret = 0;
	return ret;
}

static int rs_chars_in_buffer(struct tty_struct *tty)
{
	struct s3c44b0x_serial *info = (struct s3c44b0x_serial *) tty->driver_data;

	if  (serial_paranoia_check(info, tty->device, "rs_chars_in_buffer"))
		return 0;
	return info->xmit_cnt;
}

static void rs_flush_buffer(struct tty_struct *tty)
{
	unsigned long flags;
	struct s3c44b0x_serial *info = (struct s3c44b0x_serial *) tty->driver_data;

	if  (serial_paranoia_check(info, tty->device, "rs_flush_buffer"))
		return;
	save_flags(flags);    cli();
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	restore_flags(flags);
	wake_up_interruptible(&tty->write_wait);
	if  ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc.write_wakeup) 
		(tty->ldisc.write_wakeup) (tty);
}

/*
 * ------------------------------------------------------------
 * rs_throttle()
 *
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 * ------------------------------------------------------------
 */
static void rs_throttle(struct tty_struct *tty)
{
	struct s3c44b0x_serial *info = (struct s3c44b0x_serial *) tty->driver_data;
	unsigned int flags;

#ifdef SERIAL_DEBUG_THROTTLE
	char buf[64];

	printk("throttle %s: %d\n", tty_name(tty, buf),
			tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "rs_throttle"))
		return;

	if (I_IXOFF(tty))
		info->x_char = STOP_CHAR(tty);

	/* Turn off RTS line (do this atomically) */
	if (tty->termios->c_cflag & CRTSCTS) {
			save_flags(flags);
			cli();
			set_rts (info, 0);
			restore_flags(flags);
	}
}

static void rs_unthrottle(struct tty_struct *tty)
{
	struct s3c44b0x_serial *info = (struct s3c44b0x_serial *) tty->driver_data;
	unsigned int flags;
#ifdef SERIAL_DEBUG_THROTTLE
	char buf[64];

	printk("unthrottle %s: %d\n", tty_name(tty, buf),
		tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "rs_unthrottle"))
		return;

	if (I_IXOFF(tty)) {
		if (info->x_char)
		info->x_char = 0;
		else
		info->x_char = START_CHAR(tty);
	}

	/* Assert RTS line (do this atomically) */
	if (tty->termios->c_cflag & CRTSCTS) {
		save_flags(flags);
		cli();
		set_rts (info, 1);
		restore_flags(flags);
	}
}

/*
 * ------------------------------------------------------------
 * rs_ioctl() and friends
 * ------------------------------------------------------------
 */

static int get_serial_info(struct s3c44b0x_serial *info,
			   struct serial_struct *retinfo)
{
	struct serial_struct tmp;

	if  (!retinfo)
		return -EFAULT;
	memset(&tmp, 0, sizeof(tmp));
	tmp.type = info->type;
	tmp.line = info->line;
	tmp.irq = info->irq;
	tmp.port = info->port;
	tmp.flags = info->flags;
	tmp.baud_base = info->baud_base;
	tmp.close_delay = info->close_delay;
	tmp.closing_wait = info->closing_wait;
	tmp.custom_divisor = info->custom_divisor;
	memcpy_tofs(retinfo, &tmp, sizeof(*retinfo));
	return 0;
}

static int set_serial_info(struct s3c44b0x_serial *info,
			   struct serial_struct *new_info)
{
	struct serial_struct new_serial;
	struct s3c44b0x_serial old_info;
	int retval = 0;

	if  (!new_info)
		return -EFAULT;
	memcpy_fromfs(&new_serial, new_info, sizeof(new_serial));
	old_info = *info;

	if  (!suser()) {
		if	((new_serial.baud_base != info->baud_base) 	||
			(new_serial.type != info->type) 		||
			(new_serial.close_delay != info->close_delay) 	||
			((new_serial.flags & ~S_USR_MASK) 		!=
			(info->flags & ~S_USR_MASK))) return -EPERM;
		info->flags = ((info->flags & ~S_USR_MASK) |
			(new_serial.flags & S_USR_MASK));
		info->custom_divisor = new_serial.custom_divisor;
		goto check_and_exit;
	}

	if  (info->count > 1)
		return -EBUSY;

		/*
		* OK, past this point, all the error checking has been done.
		* At this point, we start making changes.....
		*/

	info->baud_base = new_serial.baud_base;
	info->flags = ((info->flags & ~S_FLAGS) | (new_serial.flags & S_FLAGS));
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
static int get_lsr_info(struct s3c44b0x_serial *info, unsigned int *value)
{
	unsigned char status;

	cli();
//    status = (info->usart->csr & U_CTS) ? 1 : 0;
	status = 0x00;
	sti();
	put_user(status, value);
	return 0;
}

/*
 * This routine sends a break character out the serial port.
 */
static void send_break(struct s3c44b0x_serial *info, int duration)
{
	u_int8_t ucon;
	
	current->state = TASK_INTERRUPTIBLE;
	cli();
	
	ucon = inb(S3C44B0X_UCON0);
	ucon |= S3C44B0X_UCON_SEND_BREAK;
	outb(ucon, S3C44B0X_UCON0);
	
	schedule_timeout(info->close_delay);
	
	ucon &= ~S3C44B0X_UCON_SEND_BREAK;
	outb(ucon, S3C44B0X_UCON0);
	
	sti();
}

static int rs_ioctl(struct tty_struct *tty, struct file *file,
					unsigned int cmd, unsigned long arg)
{
	int error;
	struct s3c44b0x_serial *info = (struct s3c44b0x_serial *) tty->driver_data;
	unsigned int val;
	int retval;

	if (serial_paranoia_check(info, tty->device, "rs_ioctl"))
		return -ENODEV;

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
		(cmd != TIOCSERCONFIG) && (cmd != TIOCSERGWILD) &&
		(cmd != TIOCSERSWILD) && (cmd != TIOCSERGSTRUCT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
			return -EIO;
	}

	switch (cmd) {
	case TCSBRK:				/* SVID version: non-zero arg --> no break */
		retval = tty_check_change(tty);
		if (retval)
			return retval;
		tty_wait_until_sent(tty, 0);
		if (!arg)
			send_break(info, HZ / 4);	/* 1/4 second */
		return 0;
	case TCSBRKP:				/* support for POSIX tcsendbreak() */
		retval = tty_check_change(tty);
		if (retval)
			return retval;
		tty_wait_until_sent(tty, 0);
		send_break(info, arg ? arg * (HZ / 10) : HZ / 4);
		return 0;
	case TIOCGSOFTCAR:
		error = verify_area(VERIFY_WRITE, (void *) arg, sizeof(long));

		if (error)
			return error;
		put_user(C_CLOCAL(tty) ? 1 : 0, (unsigned long *) arg);
		return 0;
	case TIOCSSOFTCAR:
		arg = get_user(arg, (unsigned long *) arg);
		tty->termios->c_cflag = ((tty->termios->c_cflag & ~CLOCAL) | (arg ? CLOCAL : 0));
		return 0;
	case TIOCGSERIAL:
		error = verify_area(VERIFY_WRITE, (void *) arg,
			sizeof(struct serial_struct));
		if (error)
			return error;
		return get_serial_info(info, (struct serial_struct *) arg);
	case TIOCSSERIAL:
		return set_serial_info(info, (struct serial_struct *) arg);
	case TIOCSERGETLSR:		/* Get line status register */
		error = verify_area(VERIFY_WRITE, (void *) arg,
			sizeof(unsigned int));
		if (error)
			return error;
		else
			return get_lsr_info(info, (unsigned int *) arg);

	case TIOCSERGSTRUCT:
		error = verify_area(VERIFY_WRITE, (void *) arg,
			sizeof(struct s3c44b0x_serial));
		if (error)
			return error;
		memcpy_tofs((struct s3c44b0x_serial *) arg, info,
			sizeof(struct s3c44b0x_serial));
		return 0;


	case TIOCMGET:	/* get all modem bits */
		if ((error = verify_area(VERIFY_WRITE, (void *) arg,
			sizeof(unsigned int))))
			return(error);
		val = get_status(info);
		put_user(val, (unsigned int *) arg);
		break;

	case TIOCMBIS:	/* modem bits set */
		if ((error = verify_area(VERIFY_WRITE, (void *) arg,
					 sizeof(unsigned int))))
			return(error);
		
		get_user(val, (unsigned int *) arg);
		
		if (val & TIOCM_RTS) set_rts(info, 1);
		if (val & TIOCM_DTR) set_dtr(info, 1);
		break;

	case TIOCMBIC:	/* modem bits clear */
		if ((error = verify_area(VERIFY_WRITE, (void *) arg,
					 sizeof(unsigned int))))
			return(error);
		get_user(val, (unsigned int *) arg);
			
		if (val & TIOCM_RTS) set_rts(info, 0);
		if (val & TIOCM_DTR) set_dtr(info, 0);
		break;

	case TIOCMSET:	/* set all modem bits */
		if ((error = verify_area(VERIFY_WRITE, (void *) arg,
					 sizeof(unsigned int))))
			return(error);
		get_user(val, (unsigned int *) arg);

		set_rts (info, ((val & TIOCM_RTS) ? 1 : 0) );
		set_dtr (info, ((val & TIOCM_DTR) ? 1 : 0) );
		break;

	case TCSETS:	
	  /*
	   * take care if the speed change, leave rest to termios by falling through
	   */
	  handle_termios_tcsets((struct termios *)arg, info);

	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}

static void handle_termios_tcsets(  struct termios *ptermios, 
				    struct s3c44b0x_serial *pinfo)
{
	if  (pinfo->tty->termios->c_cflag != ptermios->c_cflag) {
		pinfo->tty->termios->c_cflag = ptermios->c_cflag;
	}
	change_speed(pinfo);
}
	  
static void rs_set_termios( struct tty_struct *tty,
			    struct termios *old_termios)
{
	struct s3c44b0x_serial *info = (struct s3c44b0x_serial *) tty->driver_data;

	if (tty->termios->c_cflag == old_termios->c_cflag)
		return;

	change_speed(info);

	/* Handle turning off CRTSCTS */
	if ((old_termios->c_cflag & CRTSCTS) &&
		!(tty->termios->c_cflag & CRTSCTS)) 
	{
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
 * S structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 * ------------------------------------------------------------
 */
static void rs_close(struct tty_struct *tty, struct file *filp)
{
	struct s3c44b0x_serial *info = (struct s3c44b0x_serial *) tty->driver_data;
	unsigned long flags;
	
	if  (!info || serial_paranoia_check(info, tty->device, "rs_close"))
		return;
	
	save_flags(flags);  cli();
	
	if  (tty_hung_up_p(filp)) {
		restore_flags(flags);
		return;
	}
#ifdef SERIAL_DEBUG_OPEN
	printk("rs_close ttyS%d, count = %d\n", info->line, info->count);
#endif
	if ((tty->count == 1) && (info->count != 1)) {
		/*
		 * Uh, oh.  tty->count is 1, which means that the tty
		 * structure will be freed.  Info->count should always
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
	set_ints_mode(info, 0);

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
			(tty->ldisc.close) (tty);
		tty->ldisc = ldiscs[N_TTY];
		tty->termios->c_line = N_TTY;
		if (tty->ldisc.open)
			(tty->ldisc.open) (tty);
	}
	if (info->blocked_open) {
		if (info->close_delay) {
			current->state = TASK_INTERRUPTIBLE;
/*			current->timeout = jiffies + info->close_delay;  */
			schedule_timeout(info->close_delay);
		}
		wake_up_interruptible(&info->open_wait);
	}
	info->flags &= ~(S_NORMAL_ACTIVE | S_CALLOUT_ACTIVE | S_CLOSING);
	wake_up_interruptible(&info->close_wait);
	restore_flags(flags);
}

/*
 * rs_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
static void rs_hangup(struct tty_struct *tty)
{
	struct s3c44b0x_serial *info = (struct s3c44b0x_serial *) tty->driver_data;
	
	if	(serial_paranoia_check(info, tty->device, "rs_hangup"))
		return;
	
	rs_flush_buffer(tty);
	shutdown(info);
	info->event = 0;
	info->count = 0;
	info->flags &= ~(S_NORMAL_ACTIVE | S_CALLOUT_ACTIVE);
	info->tty = 0;
	wake_up_interruptible(&info->open_wait);
}

/*
 * ------------------------------------------------------------
 * rs_open() and friends
 * ------------------------------------------------------------
 */
static int block_til_ready(struct tty_struct *tty, struct file *filp,
			   struct s3c44b0x_serial *info)
{
	unsigned long flags;
	DECLARE_WAITQUEUE(wait, current);
	int retval;
	int do_clocal = 0;

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
			(info->session != current->session)) return -EBUSY;
		if ((info->flags & S_CALLOUT_ACTIVE) &&
			(info->flags & S_PGRP_LOCKOUT) &&
			(info->pgrp != current->pgrp)) return -EBUSY;
		info->flags |= S_CALLOUT_ACTIVE;
		return 0;
	}

	/*
	 * If non-blocking mode is set, or the port is not enabled,
	 * then make the check up front and then exit.
	 */
	if ((filp->f_flags & O_NONBLOCK) || (tty->flags & (1 << TTY_IO_ERROR))) {
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
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready before block: ttyS%d, count = %d\n",
		   info->line, info->count);
#endif
	info->count--;
	info->blocked_open++;
	while (1) {
		save_flags(flags);
		cli();
		if (!(info->flags & S_CALLOUT_ACTIVE))
			set_dtr(info, 1);
		restore_flags(flags);
		current->state = TASK_INTERRUPTIBLE;
		if (tty_hung_up_p(filp) || !(info->flags & S_INITIALIZED)) {
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
/*		if (current->signal & ~current->blocked) {  */
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
	info->flags |= S_NORMAL_ACTIVE;
	return 0;
}

/*
 * This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its S structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */
int rs_open(struct tty_struct *tty, struct file *filp)
{
	struct s3c44b0x_serial *info = &s3c44b0x_info;
	int retval, line;

	line = MINOR(tty->device) - tty->driver.minor_start;

	/* check if line is sane */
	if (line < 0 || line >= USART_CNT)
		return -ENODEV;

	if (serial_paranoia_check(info, tty->device, "rs_open"))
		return -ENODEV;

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open %s%d, count = %d\n", tty->driver.name, info->line,
		info->count);
#endif
	info->count++;
	tty->driver_data = info;
	info->tty = tty;

	/*
	 * Start up serial port
	 */
	set_ints_mode(info, 1);
	
	retval = startup(info);
	if (retval)
		return retval;

	retval = block_til_ready(tty, filp, info);
	if (retval) {
#ifdef SERIAL_DEBUG_OPEN
		printk("rs_open returning after block_til_ready with %d\n",
			   retval);
#endif
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

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open ttyS%d successful...\n", info->line);
#endif
	return 0;
}

static struct irqaction irq_uartTx =
	{ rs_interruptTx,  0, 0, "S3C44B0X_UART_TX",  NULL, NULL };
static struct irqaction irq_uartRx =
	{ rs_interruptRx,  0, 0, "S3C44B0X_UART_RX",  NULL, NULL };
static struct irqaction irq_uartErr =
	{ rs_interruptErr, 0, 0, "S3C44B0X_UART_ERR", NULL, NULL };
	
extern int setup_arm_irq(int, struct irqaction *);

static void interrupts_init(void)
{
	setup_arm_irq(S3C44B0X_INTERRUPT_UTX0,  &irq_uartTx);
	setup_arm_irq(S3C44B0X_INTERRUPT_URX0,  &irq_uartRx);
	setup_arm_irq(S3C44B0X_INTERRUPT_UERR, &irq_uartErr);
}

static void show_serial_version(void)
{
	printk("Samsung S3C44B0X UART driver version 0.1 <thomas.eschenbacher@sympat.de>\n");
}

/* rs_init inits the driver */
static int __init rs_s3c44b0x_init(void)
{
	int flags, i;
	struct s3c44b0x_serial *info;
	
	/* Setup base handler, and timer table. */
	init_bh(SERIAL_BH, do_serial_bh);
	
	show_serial_version();
	
	/* Initialize the tty_driver structure */
	
	/* set the tty_struct pointers to NULL to let the layer */
	/* above allocate the structs. */
	for (i=0; i < USART_CNT; i++)
		serial_table[i] = NULL;
	
	memset(&serial_driver, 0, sizeof(struct tty_driver));
	
	serial_driver.magic = TTY_DRIVER_MAGIC;
	serial_driver.name = "ttyS";
	serial_driver.major = TTY_MAJOR;
	serial_driver.minor_start = 64;
	serial_driver.num = 1;
	serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver.subtype = SERIAL_TYPE_NORMAL;
	serial_driver.init_termios = tty_std_termios;
	serial_driver.init_termios.c_cflag = CS8 | CREAD | HUPCL | CLOCAL;
	serial_driver.init_termios.c_cflag |= B115200;
	serial_driver.flags = TTY_DRIVER_REAL_RAW;
	serial_driver.refcount = &serial_refcount;
	serial_driver.table = serial_table;
	serial_driver.termios = serial_termios;
	serial_driver.termios_locked = serial_termios_locked;

	serial_driver.open = rs_open;
	serial_driver.close = rs_close;
	serial_driver.write = rs_write;
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
	
	save_flags(flags);
	cli();
	for (i = 0; i < USART_CNT; i++) {
		info = &s3c44b0x_info;
		info->magic = SERIAL_MAGIC;
		info->tty = 0;
		info->irqmask = S3C44B0X_INTERRUPT_UTX0 || 
		                S3C44B0X_INTERRUPT_URX0 || 
		                S3C44B0X_INTERRUPT_UERR;
		info->irq = S3C44B0X_INTERRUPT_UTX0;
		info->port = (i) ? 1 : 2;
		info->line = i;

#if defined(CONFIG_CONSOLE_NULL)
		info->is_cons = 0;
#else
		if (i == console_number)
			info->is_cons = 1;
#endif
		set_ints_mode(info, 0);
		/* info->custom_divisor = 16; */ /* unused? */
		info->custom_divisor = 0;
		info->close_delay = 50;
		info->closing_wait = 3000;
		info->cts_state = 1;
		info->x_char = 0;
		info->event = 0;
		info->count = 0;
		info->blocked_open = 0;
		info->tqueue.routine = do_softint;
		info->tqueue.data = info;
		info->tqueue_hangup.routine = do_serial_hangup;
		info->tqueue_hangup.data = info;
		info->callout_termios = callout_driver.init_termios;
		info->normal_termios = serial_driver.init_termios;
		
		init_waitqueue_head(&info->open_wait);
		init_waitqueue_head(&info->close_wait);
		
		printk("%s%d (irq = %d)", serial_driver.name, info->line,
			   info->irq);
		printk(" is a builtin Samsung S3C44B0X UART\n");
	}
	
	interrupts_init();
	
	restore_flags(flags);
	return 0;
}

module_init(rs_s3c44b0x_init);

/*
 * ------------------------------------------------------------
 * Serial console driver
 * ------------------------------------------------------------
 */

#ifdef CONFIG_SERIAL_S3C44B0X_CONSOLE

static kdev_t s3c44b0x_console_device(struct console *);
static int    s3c44b0x_console_setup(struct console *, char *);
static void   s3c44b0x_console_write (struct console *, const char *, unsigned int );
static int    s3c44b0x_console_initialized = 0;

static struct console s3c44b0x_driver = {
	name:		"ttyS",
	write:		s3c44b0x_console_write,
	device:		s3c44b0x_console_device,
	setup:		s3c44b0x_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

/* static void init_console(struct s3c44b0x_serial *info) */
static int s3c44b0x_console_setup(struct console *c, char *p)
{
	struct s3c44b0x_serial *info = &s3c44b0x_info;
	
	memset(info, 0, sizeof(struct s3c44b0x_serial));
	info->use_ints = 0;
	info->cts_state = 1;
	info->is_cons = 1;
//	info->usart->cr = 0; 
//	info->usart->cr = (U_IRQ_TMODE | U_IRQ_RMODE | U_WL8);
	fifo_init();
	info->baud = B115200;
	
//     info->usart->brgr = calcCD(info->baud);
//     info->usart->ier = 0;
	
	s3c44b0x_console_initialized = 1;
	return 0;
}

static kdev_t s3c44b0x_console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, 64 + c->index);
}

static void s3c44b0x_console_write (struct console *co, const char *str,
			   unsigned int count)
{
#if defined(CONFIG_CONSOLE_NULL)
	return;
#else
	static struct s3c44b0x_serial *info = &s3c44b0x_info;
	
	if (!s3c44b0x_console_initialized)
		printk ("WARNING: Console used unintialized\n");
	
	while (count--) {
		if  (*str == '\n')
			rs_put_char(info,'\r');
        	rs_put_char(info, *str++ );
	}
#endif
}

void __init s3c44b0x_console_init(void)
{
	register_console(&s3c44b0x_driver);
}

#endif /* CONFIG_SERIAL_S3C44B0X_CONSOLE */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
