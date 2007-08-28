/* NIOSserial.c: Serial port driver for builtin NIOS UART.
 *
 * Copyright (C) 1995       David S. Miller    <davem@caip.rutgers.edu>
 * Copyright (C) 1998       Kenneth Albanowski <kjahds@kjahds.com>
 * Copyright (C) 1998-2000  D. Jeff Dionne     <jeff@uClinux.org>
 * Copyright (C) 1999       Vladimir Gurevich  <vgurevic@cisco.com>
 * Copyright (C) 2001       Vic Phillips       <vic@microtronix.com>
 * Copyright (C) 2001       Wentao Xu          <wentao@microtronix.com>
 */
#include <linux/module.h>
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
#include <linux/reboot.h>
#include <linux/keyboard.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <asm/bitops.h>
#include <asm/delay.h>
#include <asm/uaccess.h>

#include "NIOSserial.h"

extern void hard_reset_now(void);

//struct tty_struct NIOS_ttys;
//struct NIOS_serial *NIOS_consinfo = 0;

DECLARE_TASK_QUEUE(tq_serial);

#ifdef CONFIG_CONSOLE
extern wait_queue_head_t keypress_wait;
#endif

struct tty_driver serial_driver, callout_driver;
static int serial_refcount;

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

#define RS_ISR_PASS_LIMIT 256

#define _INLINE_ inline

static void change_speed(struct NIOS_serial *info);

/*
 *	Configuration table, UARTs to look for at startup.
 */
static struct NIOS_serial nios_soft[] = {
//	{ 0,0,1,0,0,0,0, (nasys_printf_uart), (nasys_printf_uart_irq) }, /* ttyS0 */
	{ 0,0,1,0,0,0,0, (int) (na_uart0), (na_uart0_irq) },		/* ttyS0 */
#ifdef na_uart1
//	{ 0,0,0,0,0,0,0, (nasys_gdb_uart), (nasys_gdb_uart_irq) },		  /* ttyS1 */
	{ 0,0,0,0,0,0,0, (int) (na_uart1), (na_uart1_irq) },		/* ttyS1 */
#endif
#ifdef na_uart2
	{ 0,0,0,0,0,0,0, (int) (na_uart2), (na_uart2_irq) },		/* ttyS2 */
#endif
#ifdef na_uart3
	{ 0,0,0,0,0,0,0, (int) (na_uart3), (na_uart3_irq) },		/* ttyS3 */
#endif
};

#define	NR_PORTS	(sizeof(nios_soft) / sizeof(struct NIOS_serial))


static struct tty_struct *serial_table[NR_PORTS];
static struct termios *serial_termios[NR_PORTS];
static struct termios *serial_termios_locked[NR_PORTS];

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

static inline int serial_paranoia_check(struct NIOS_serial *info,
					dev_t device, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char *badmagic =
		"Warning: bad magic number for serial struct (%d, %d) in %s\n";
	static const char *badinfo =
		"Warning: null NIOS_serial for (%d, %d) in %s\n";

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
 * This is used to figure out the divisor speeds and the timeouts
 */
static int baud_table[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
	9600, 19200, 38400, 57600, 115200, 0 };

/* Sets or clears DTR/RTS on the requested line */
static inline void NIOS_rtsdtr(struct NIOS_serial *ss, int set)
{
	if (set) {
		/* set the RTS/CTS line */
	} else {
		/* clear it */
	}
	return;
}

/* Utility routines */
static inline int get_baud(struct NIOS_serial *ss)
{
	return 0;	/* not implemented yet */
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
	struct NIOS_serial *info = (struct NIOS_serial *)tty->driver_data;
	np_uart *	uart= (np_uart *)(info->port);
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_stop"))
		return;

	save_flags(flags); cli();
	uart->np_uartcontrol &= ~np_uartcontrol_itrdy_mask;
	restore_flags(flags);
}

void rs_put_char(char ch, struct NIOS_serial *info)
{
	int flags, loops = 0;
	np_uart *	uart= (np_uart *)(info->port);

	save_flags(flags); cli();

	while (!(uart->np_uartstatus & np_uartstatus_trdy_mask) && (loops < 1000)) {
		loops++;
	}

	uart->np_uarttxdata = ch;
	restore_flags(flags);
}

static void rs_start(struct tty_struct *tty)
{
	struct NIOS_serial *info = (struct NIOS_serial *)tty->driver_data;
	unsigned long flags;
	np_uart *	uart= (np_uart *)(info->port);

	if (serial_paranoia_check(info, tty->device, "rs_start"))
		return;

	save_flags(flags); cli();
	if (info->xmit_cnt && info->xmit_buf && !(uart->np_uartcontrol & np_uartcontrol_itrdy_mask)) {
#ifdef USE_INTS
		uart->np_uartcontrol &= np_uartcontrol_itrdy_mask;
#endif
	}
	restore_flags(flags);
}

/* Drop into either the boot monitor or gdb upon receiving a break
 * from keyboard/console input.
 */
static void batten_down_hatches(void)
{
	/* Drop into the debugger */
	nios_gdb_breakpoint();
}

static _INLINE_ void status_handle(struct NIOS_serial *info, unsigned short status)
{
	return;
}

/*
 * This routine is used by the interrupt handler to schedule
 * processing in the software interrupt portion of the driver.
 */
static _INLINE_ void rs_sched_event(struct NIOS_serial *info,
				    int event)
{
	info->event |= 1 << event;
	queue_task(&info->tqueue, &tq_serial);
	mark_bh(SERIAL_BH);
}

static _INLINE_ void receive_chars(struct NIOS_serial *info, struct pt_regs *regs, unsigned short rx)
{
	struct tty_struct *tty = info->tty;
	unsigned char ch;
	np_uart *	uart= (np_uart *)(info->port);

	/*
	 * This do { } while() loop will get ALL chars out of Rx FIFO
         */
	do {
		ch = uart->np_uartrxdata;

		if(info->is_cons) {
#ifdef CONFIG_MAGIC_SYSRQ
			if(rx & np_uartstatus_brk_mask) {
				batten_down_hatches();
				return;
			} else if (ch == 0x10) { /* ^P */
				show_state();
				show_mem();
				return;
			} else if (ch == 0x12) { /* ^R */
				hard_reset_now();
				return;
#ifdef DEBUG
			} else if (ch == 0x02) { /* ^B */
				batten_down_hatches();
				return;
			} else if (ch == 0x01) { /* ^A */
				asm("trap 0");		/* Back to monitor */
				return;			/* (won't be coming back) */
#endif
			}
#endif /* CONFIG_MAGIC_SYSRQ */
			/* It is a 'keyboard interrupt' ;-) */
#ifdef CONFIG_CONSOLE
			wake_up(&keypress_wait);
#endif
		}

		if(!tty)
			goto clear_and_exit;

		/*
		 * Make sure that we do not overflow the buffer
		 */
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
			queue_task(&tty->flip.tqueue, &tq_timer);
			return;
		}

		if(rx & np_uartstatus_pe_mask) {
			*tty->flip.flag_buf_ptr++ = TTY_PARITY;
			status_handle(info, rx);
		} else if(rx & np_uartstatus_roe_mask) {
			*tty->flip.flag_buf_ptr++ = TTY_OVERRUN;
			status_handle(info, rx);
		} else if(rx & np_uartstatus_fe_mask) {
			*tty->flip.flag_buf_ptr++ = TTY_FRAME;
			status_handle(info, rx);
		} else if(rx & np_uartstatus_brk_mask) {
			*tty->flip.flag_buf_ptr++ = TTY_BREAK;
			status_handle(info, rx);
		} else {
			*tty->flip.flag_buf_ptr++ = 0; /* XXX */
		}
		*tty->flip.char_buf_ptr++ = ch;
		tty->flip.count++;

	} while((rx = uart->np_uartstatus) & np_uartstatus_rrdy_mask);

	queue_task(&tty->flip.tqueue, &tq_timer);

clear_and_exit:
	return;
}

static _INLINE_ void transmit_chars(struct NIOS_serial *info)
{
	np_uart *	uart= (np_uart *)(info->port);
	if (info->x_char) {
		/* Send next char */
		uart->np_uarttxdata = info->x_char;
		info->x_char = 0;
		goto clear_and_return;
	}

	if((info->xmit_cnt <= 0) || info->tty->stopped) {
		/* That's peculiar... TX ints off */
		uart->np_uartcontrol &= ~np_uartcontrol_itrdy_mask;
		goto clear_and_return;
	}

	/* Send char */
	uart->np_uarttxdata = info->xmit_buf[info->xmit_tail++];
	info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
	info->xmit_cnt--;

	if (info->xmit_cnt < WAKEUP_CHARS)
		rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);

	if(info->xmit_cnt <= 0) {
		/* All done for now... TX ints off */
		uart->np_uartcontrol &= ~np_uartcontrol_itrdy_mask;
		goto clear_and_return;
	}

clear_and_return:
	/* Clear interrupt (should be auto)*/
	return;
}

/*
 * This is the serial driver's generic interrupt routine
 */
void rs_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	struct NIOS_serial * info = (struct NIOS_serial *) dev_id;
	np_uart *	uart= (np_uart *)(info->port);
	unsigned short stat = uart->np_uartstatus;
	uart->np_uartstatus = 0;		/* clear any error status */

	if (stat & np_uartstatus_rrdy_mask) receive_chars(info, regs, stat);
	if (stat & np_uartstatus_trdy_mask) transmit_chars(info);
	return;
}

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
	run_task_queue(&tq_serial);
}

static void do_softint(void *private_)
{
	struct NIOS_serial	*info = (struct NIOS_serial *) private_;
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
	struct NIOS_serial	*info = (struct NIOS_serial *) private_;
	struct tty_struct	*tty;

	tty = info->tty;
	if (!tty)
		return;

	tty_hangup(tty);
}



static int startup(struct NIOS_serial * info)
{
	unsigned long flags;
	np_uart *	uart= (np_uart *)(info->port);

	if (info->flags & S_INITIALIZED)
		return 0;

	if (!info->xmit_buf) {
		info->xmit_buf = (unsigned char *) get_free_page(GFP_KERNEL);
		if (!info->xmit_buf)
			return -ENOMEM;
	}

	save_flags(flags); cli();

	/*
	 * Clear the FIFO buffers and disable them
	 * (they will be reenabled in change_speed())
	 */

	change_speed(info);

	info->xmit_fifo_size = 1;
	uart->np_uartcontrol = np_uartcontrol_itrdy_mask
												 | np_uartcontrol_irrdy_mask
												 | np_uartcontrol_ibrk_mask;
	uart->np_uartrxdata;

	if (info->tty)
		clear_bit(TTY_IO_ERROR, &info->tty->flags);
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;

	info->flags |= S_INITIALIZED;
	restore_flags(flags);
	return 0;
}

/*
 * This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */
static void shutdown(struct NIOS_serial * info)
{
	unsigned long	flags;
	np_uart *	uart= (np_uart *)(info->port);

	uart->np_uartcontrol = 0; /* All off! */
	if (!(info->flags & S_INITIALIZED))
		return;

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

/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */
static void change_speed(struct NIOS_serial *info)
{
#if 1		/* NIOS usually has a fixed speed */

//	unsigned long ustcnt;
	unsigned cflag;
	unsigned divisor;
	int	i;
	np_uart *	uart= (np_uart *)(info->port);

	if (!info->tty || !info->tty->termios)
		return;
	cflag = info->tty->termios->c_cflag;

//	ustcnt = uart->np_uartcontrol;
//	uart->np_uartcontrol = ustcnt & ~UCTRL_TE;

	i = cflag & CBAUD;
	if (i & CBAUDEX) {
		i = (i & ~CBAUDEX) + B38400;
	}

	divisor = (unsigned)(((unsigned)nasys_clock_freq *1.0 / baud_table[i]) + 1) -1;
	uart->np_uartdivisor = divisor;
	if (uart->np_uartdivisor == divisor)
		info->baud = baud_table[i];

//	ustcnt &= ~(UCTRL_PE | UCTRL_PS);
//	if ((cflag & CSIZE) == CS8)
//		ustcnt |= USTCNT_8_7;
//	if (cflag & CSTOPB)
//		ustcnt |= USTCNT_STOP;
//	if (cflag & PARENB)
//		ustcnt |= UCTRL_PE;
//	if (!(cflag & PARODD))
//		ustcnt |= UCTRL_PS;
//	if (cflag & CRTSCTS) {
//		ustcnt |= UCTRL_FL;
//	} else {
//		ustcnt &= ~UCTRL_FL;
//	}
//	ustcnt |= UCTRL_TE;
//	uart->np_uartcontrol = ustcnt;

#endif
}

/*
 * Fair output driver allows a process to speak.
 */
#if 0
static void rs_fair_output(void)
{
	int left;		/* Output no more than that */
	unsigned long flags;
	struct NIOS_serial *info = &nios_soft;
	char c;

	if (info == 0) return;
	if (info->xmit_buf == 0) return;

	save_flags(flags);  cli();
	left = info->xmit_cnt;
	while (left != 0) {
		c = info->xmit_buf[info->xmit_tail];
		info->xmit_tail = (info->xmit_tail+1) & (SERIAL_XMIT_SIZE-1);
		info->xmit_cnt--;
		restore_flags(flags);

		rs_put_char(c, info);

		save_flags(flags);  cli();
		left = MIN(info->xmit_cnt, left-1);
	}

	/* Last character is being transmitted now (hopefully). */
	udelay(5);

	restore_flags(flags);
	return;
}
#endif

/*
 * console_print_NIOS is registered for printk.
 */
void console_print_NIOS(const char *p)
{
	char c;

	while((c=*(p++)) != 0) {
		if(c == '\n')
			rs_put_char('\r', nios_soft);
		rs_put_char(c, nios_soft);
	}

	/* Comment this if you want to have a strict interrupt-driven output */
	/* rs_fair_output(); */

	return;
}

static void rs_set_ldisc(struct tty_struct *tty)
{
	struct NIOS_serial *info = (struct NIOS_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_set_ldisc"))
		return;

	info->is_cons = (tty->termios->c_line == N_TTY);

	printk("ttyS%d console mode %s\n", info->line, info->is_cons ? "on" : "off");
}

static void rs_flush_chars(struct tty_struct *tty)
{
	struct NIOS_serial *info = (struct NIOS_serial *)tty->driver_data;
	unsigned long flags;
	np_uart *	uart= (np_uart *)(info->port);

	if (serial_paranoia_check(info, tty->device, "rs_flush_chars"))
		return;

	save_flags(flags); cli();

	if (info->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped ||
	    !info->xmit_buf)
		goto flush_exit;

	/* Enable transmitter */

	uart->np_uartcontrol |= np_uartcontrol_itrdy_mask;

	if (uart->np_uartstatus & np_uartstatus_trdy_mask) {
		/* Send char */
		uart->np_uarttxdata = info->xmit_buf[info->xmit_tail++];
		info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
		info->xmit_cnt--;
	}

flush_exit:
	restore_flags(flags);
}

extern void console_printn(const char * b, int count);

static int rs_write(struct tty_struct * tty, int from_user,
		    const unsigned char *buf, int count)
{
	int	c, total = 0;
	struct NIOS_serial *info = (struct NIOS_serial *)tty->driver_data;
	unsigned long flags;
	np_uart *	uart= (np_uart *)(info->port);

	if (serial_paranoia_check(info, tty->device, "rs_write"))
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

	if (info->xmit_cnt && !tty->stopped && !tty->hw_stopped) {
		/* Enable transmitter */
		cli();

		uart->np_uartcontrol |= np_uartcontrol_itrdy_mask;

		if (uart->np_uartstatus & np_uartstatus_trdy_mask) {
			uart->np_uarttxdata = info->xmit_buf[info->xmit_tail++];
			info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
			info->xmit_cnt--;
		}

		restore_flags(flags);
	}
	restore_flags(flags);
	return total;
}

static int rs_write_room(struct tty_struct *tty)
{
	struct NIOS_serial *info = (struct NIOS_serial *)tty->driver_data;
	int	ret;

	if (serial_paranoia_check(info, tty->device, "rs_write_room"))
		return 0;
	ret = SERIAL_XMIT_SIZE - info->xmit_cnt - 1;
	if (ret < 0)
		ret = 0;
	return ret;
}

static int rs_chars_in_buffer(struct tty_struct *tty)
{
	struct NIOS_serial *info = (struct NIOS_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_chars_in_buffer"))
		return 0;
	return info->xmit_cnt;
}

static void rs_flush_buffer(struct tty_struct *tty)
{
	struct NIOS_serial *info = (struct NIOS_serial *)tty->driver_data;

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
 * ------------------------------------------------------------
 * rs_throttle()
 *
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 * ------------------------------------------------------------
 */
static void rs_throttle(struct tty_struct * tty)
{
	struct NIOS_serial *info = (struct NIOS_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_throttle"))
		return;

	if (I_IXOFF(tty))
		info->x_char = STOP_CHAR(tty);

	/* Turn off RTS line (do this atomic) */
}

static void rs_unthrottle(struct tty_struct * tty)
{
	struct NIOS_serial *info = (struct NIOS_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_unthrottle"))
		return;

	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			info->x_char = START_CHAR(tty);
	}

	/* Assert RTS line (do this atomic) */
}

/*
 * ------------------------------------------------------------
 * rs_ioctl() and friends
 * ------------------------------------------------------------
 */

static int get_serial_info(struct NIOS_serial * info,
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

static int set_serial_info(struct NIOS_serial * info,
			   struct serial_struct * new_info)
{
	struct serial_struct new_serial;
	struct NIOS_serial old_info;
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
static int get_lsr_info(struct NIOS_serial * info, unsigned int *value)
{
	unsigned char status;

	status = 0;
	put_user(status,value);
	return 0;
}

/*
 * This routine sends a break character out the serial port.
 */
static void send_break(	struct NIOS_serial * info, int duration)
{
}

static int rs_ioctl(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg)
{
	int error;
	struct NIOS_serial * info = (struct NIOS_serial *)tty->driver_data;
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
						sizeof(struct NIOS_serial));
			if (error)
				return error;
			copy_to_user((struct NIOS_serial *) arg,
				    info, sizeof(struct NIOS_serial));
			return 0;

		default:
			return -ENOIOCTLCMD;
		}
	return 0;
}

static void rs_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	struct NIOS_serial *info = (struct NIOS_serial *)tty->driver_data;
	int	oldbaud = info->baud;

	if (tty->termios->c_cflag == old_termios->c_cflag)
		return;

	change_speed(info);

	if (info->baud == oldbaud) /* can not change */
		tty->termios->c_cflag = old_termios->c_cflag;
	else	/* only speed can be changed */
		tty->termios->c_cflag = (tty->termios->c_cflag & CBAUD) | CS8 | CREAD | HUPCL | CLOCAL;
#if 0
	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		rs_start(tty);
	}
#endif
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
static void rs_close(struct tty_struct *tty, struct file * filp)
{
	struct NIOS_serial * info = (struct NIOS_serial *)tty->driver_data;
	unsigned long flags;
	np_uart *	uart= (np_uart *)(info->port);

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

	uart->np_uartcontrol &= ~(np_uartcontrol_irrdy_mask);

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

/*
 * rs_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
void rs_hangup(struct tty_struct *tty)
{
	struct NIOS_serial * info = (struct NIOS_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_hangup"))
		return;

	rs_flush_buffer(tty);
	shutdown(info);
	info->event = 0;
	info->count = 0;
	info->flags &= ~(S_NORMAL_ACTIVE|S_CALLOUT_ACTIVE);
	info->tty = 0;
	wake_up_interruptible(&info->open_wait);
}

/*
 * ------------------------------------------------------------
 * rs_open() and friends
 * ------------------------------------------------------------
 */
static int block_til_ready(struct tty_struct *tty, struct file * filp,
			   struct NIOS_serial *info)
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
			NIOS_rtsdtr(info, 1);
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
	struct NIOS_serial	*info;
	int 			retval, line;

	line = MINOR(tty->device) - tty->driver.minor_start;

	if (line >=serial_driver.num) /* too many */
		return -ENODEV;

	info = &nios_soft[line];

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

	return 0;
}

/* Finally, routines used to initialize the serial driver. */

static void show_serial_version(void)
{
	printk("NIOS serial driver version 0.0\n");
}


volatile int test_done;

/* rs_init inits the driver */
static int __init rs_nios_init(void)
{
	int flags;
	struct NIOS_serial *info;
	int			i;

	/* Setup base handler, and timer table. */
	init_bh(SERIAL_BH, do_serial_bh);

	show_serial_version();

	/* Initialize the tty_driver structure */
	/* SPARC: Not all of this is exactly right for us. */

	memset(&serial_driver, 0, sizeof(struct tty_driver));
	serial_driver.magic = TTY_DRIVER_MAGIC;
	serial_driver.name = "ttyS";
	serial_driver.major = TTY_MAJOR;
	serial_driver.minor_start = 64;
	serial_driver.num = NR_PORTS;
	serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver.subtype = SERIAL_TYPE_NORMAL;
	serial_driver.init_termios = tty_std_termios;


        serial_driver.init_termios.c_cflag =
		B115200 | CS8 | CREAD | HUPCL | CLOCAL;

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

	save_flags(flags); cli();

	/*
	 *	Configure all the attached serial ports.
	 */
	for (i = 0, info = nios_soft; (i < NR_PORTS); i++, info++) {
		info->magic = SERIAL_MAGIC;
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
		info->callout_termios =callout_driver.init_termios;
		info->normal_termios = serial_driver.init_termios;
		init_waitqueue_head(&info->open_wait);
		init_waitqueue_head(&info->close_wait);
		info->line = i;

		printk("%s%d (irq = %d) is a builtin NIOS UART\n",
			serial_driver.name, info->line, info->irq);

//		rs_setsignals(info, 0, 0);
		if (request_irq(info->irq, rs_interrupt, 0, "NIOS serial", info))
			panic("Unable to attach NIOS serial interrupt\n");
	}

	restore_flags(flags);
	return 0;
}

/*
 * register_serial and unregister_serial allows for serial ports to be
 * configured at run-time, to support PCMCIA modems.
 */
/* NIOS: Unused at this time, just here to make things link. */
int register_serial(struct serial_struct *req)
{
	return -1;
}

void unregister_serial(int line)
{
	return;
}

module_init(rs_nios_init);

int nios_console_setup(struct console *cp, char *arg)
{
	return 0;
}


static kdev_t nios_console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, 64 + c->index);
}


void nios_console_write (struct console *co, const char *str,
			   unsigned int count)
{
    while (count--) {
        if (*str == '\n')
           rs_put_char('\r', nios_soft);
        rs_put_char( *str++, nios_soft);
    }
}


static struct console nios_driver = {
	name:		"ttyS",
	write:		nios_console_write,
	read:		NULL,
	device:		nios_console_device,
	/*wait_key:	NULL,*/
	unblank:	NULL,
	setup:		nios_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
	cflag:		0,
	next:		NULL
};


void nios_console_init(void)
{
	register_console(&nios_driver);
}
