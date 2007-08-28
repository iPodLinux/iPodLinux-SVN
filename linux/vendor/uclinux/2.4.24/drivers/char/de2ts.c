/*
 *  linux/drivers/char/de2ts.c -- Touchscreen driver for the DragonEngine
 *
 *	Copyright (C) 2003 Georges Menie
 *
 *  Thanks to Philippe Ney for his m68328digi.c driver
 *  and to Kurt Stremerch (Exys) for code sample
 *
 *  The read method is different from the m68328digi driver :
 *  we are reading several times the X position until it gets stable,
 *  then do the same for the Y position, then record the event.
 *
 *  The burr-brown controller (ADS7846) is also different and easier to interface
 *  (PENIRQ has been improved)
 *
 *  The BUSY signal is not managed (not connected on my board).
 *
 *  DragonEngine board specific feature :
 *    SPI2 connectivity is shared by the touchscreen and the eeprom so the SPI
 *    functions are externals (de2spi.c driver) and a lock is held to prevent
 *    concurrent access.
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/tqueue.h>
#include <linux/miscdevice.h>
#include <linux/timer.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/MC68VZ328.h>

#include <linux/de2ts.h>

#define MODULE_NAME "de2ts"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Georges Menie");
MODULE_DESCRIPTION("Touchscreen driver for the DragonEngine");

#undef PDEBUG
#ifdef DE2TS_DEBUG
#ifdef __KERNEL__
#define PDEBUG(fmt,args...) printk(KERN_ALERT MODULE_NAME ": " fmt, ##args);
#else
#define PDEBUG(fmt,args...) fprintf(stderr, MODULE_NAME ": " fmt, ##args);
#endif
#else
#define PDEBUG(fmt,args...)		/* void */
#endif
#undef IPDEBUG
#define IPDEBUG(fmt,args...)	/* void */

#include "de2spi.h"

#define MAX_EVENT 128

typedef struct {
	int ri, wi;
	unsigned long lock;
	struct de2ts_drv_params drv;
	struct de2ts_cal_params cal;
	struct de2ts_event tev[MAX_EVENT];
	struct timeval start;
	wait_queue_head_t rdq;
	struct fasync_struct *asq;
} Device;

static const struct de2ts_drv_params de2ts_default_drv_params = {
	DE2TS_VERSION,
	36,							/* max_samples_x */
	24,							/* max_samples_y */
	6,							/* min_equals_x */
	3,							/* min_equals_y */
	1,							/* max_error */
	10,							/* max_speed */
	0,							/* xy swap */
};

static const struct de2ts_cal_params de2ts_default_cal_params = {
	DE2TS_VERSION,
	0,
	4096,
	0,
	4096,
	4096,
	4096,
};

#define BIT(x) (1<<(x))

/* Connectivity:  Burr-Brown        DragonBall
 *                PENIRQ     ---->  PF1 (IRQ5)
 *                BUSY       ---->  NC
 *                CS         ---->  PE3
 *                DIN        ---->  PE0
 *                DOUT       ---->  PE1
 *                DCLK       ---->  PE2
 */

/*
 * ADS7846 fields (BURR-BROWN Touch Screen Controller).
 */
#define ADS7846_START_BIT BIT(7)
#define ADS7846_A2        BIT(6)
#define ADS7846_A1        BIT(5)
#define ADS7846_A0        BIT(4)
#define ADS7846_MODE      BIT(3)	/* HIGH = 8, LOW = 12 bits conversion */
#define ADS7846_SER_DFR   BIT(2)	/* LOW = differential mode            */
#define ADS7846_PD1       BIT(1)	/* PD1,PD0 = 11  PENIRQ disable       */
#define ADS7846_PD0       BIT(0)	/* PD1,PD0 = 00  PENIRQ enable        */

/*
 * X conversion.
 */
#define ADS7846_ASKX    (ADS7846_START_BIT   | \
                         ADS7846_A2          | \
						 ADS7846_A1        *0| \
						 ADS7846_A0          | \
                         ADS7846_MODE      *0| \
                         ADS7846_SER_DFR   *0| \
                         ADS7846_PD1         | \
                         ADS7846_PD0            )
/*
 * Y conversion.
 */
#define ADS7846_ASKY    (ADS7846_START_BIT   | \
                         ADS7846_A2        *0| \
						 ADS7846_A1        *0| \
						 ADS7846_A0          | \
                         ADS7846_MODE      *0| \
                         ADS7846_SER_DFR   *0| \
                         ADS7846_PD1         | \
                         ADS7846_PD0            )

/*
 * void conversion.
 */
#define ADS7846_NOP     (ADS7846_START_BIT   | \
                         ADS7846_A2        *0| \
						 ADS7846_A1        *0| \
						 ADS7846_A0          | \
                         ADS7846_MODE      *0| \
                         ADS7846_SER_DFR   *0| \
                         ADS7846_PD1       *0| \
                         ADS7846_PD0       *0   )


static inline void de2ts_ts_enable(void)
{
	PEDATA &= ~BIT(3);
}

static inline void de2ts_ts_disable(void)
{
	PEDATA |= BIT(3);
}

static inline void de2ts_ts_pen_io(void)
{
	// PENIRQ
	PFSEL |= BIT(1);			// select PF1 as I/O
	PFDIR &= ~BIT(1);			// select Port F bit 1 as input
}

static inline void de2ts_ts_pen_irq(void)
{
	// PENIRQ
	PFPUEN |= BIT(1);			// pull up enable
	ICR &= ~ICR_POL5;			// irq on low level
	PFSEL &= ~BIT(1);			// select PF1 as IRQ5 pin
}

static inline int de2ts_ts_pen(void)
{
	// PENIRQ status
	return PFDATA & BIT(1);
}

static inline void de2ts_setup_hw(void)
{
	// CS
	PESEL |= BIT(3);			// select PE3 as I/O
	PEDIR |= BIT(3);			// select Port E bit 3 as output
	de2ts_ts_disable();

	de2ts_ts_pen_irq();
}

static inline void de2ts_reset_hw(void)
{
	de2ts_ts_disable();
	de2ts_ts_pen_io();
}

static inline int de2ts_comperr(int m1, int m2)
{
	if (m1 > m2)
		return m2 ? 100 * (m1 - m2) / m2 : 100;
	else
		return m1 ? 100 * (m2 - m1) / m1 : 100;
}

/*
 * touchscreen main reading function
 *
 * read a serie of x positions up to (max_samples_x) 
 * if (min_equals_x) consecutives read are equals
 * then continue with y positions.
 * two conscutive samples are equal if the difference
 * computed by comperr() is below (min_equal_x)
 *
 * same algo for the y position with different
 * parameters values (max_samples_y, min_equals_y)
 * because if the x position is stable, the y position
 * should read faster.
 *
 * when x and y are ok, return the average of the
 * consecutive samples.
 *
 * if x or y can not be read, just give up and do not
 * register an event, the calling function will try
 * again later.
 *
 */
static int de2ts_readts(int *px, int *py, struct de2ts_drv_params *dprm)
{
	int x, xp, xi, y, yp, yi;
	int n, xtot = 0, ytot = 0;

	xp = x = yp = y = -1;
	de2ts_ts_enable();
	(void) de2spi_exchange(ADS7846_ASKX, 8);
	for (xi = n = yi = 0; xi < dprm->max_samples_x; ++xi) {
		x = (de2spi_exchange(ADS7846_ASKX, 16) >> 3) & 0xfff;
		if (!n || de2ts_comperr(x, xp) <= dprm->max_error) {
			if (!n)
				xtot = x;
			else
				xtot += x;
			if (++n >= dprm->min_equals_x)
				break;
		} else {
			n = 1;
			xtot = x;
		}
		xp = x;
	}
	if (xi < dprm->max_samples_x) {
		(void) de2spi_exchange(ADS7846_ASKY, 16);
		for (yi = n = 0; yi < dprm->max_samples_y; ++yi) {
			y = (de2spi_exchange(ADS7846_ASKY, 16) >> 3) & 0xfff;
			if (!n || de2ts_comperr(y, yp) <= dprm->max_error) {
				if (!n)
					ytot = y;
				else
					ytot += y;
				if (++n >= dprm->min_equals_y)
					break;
			} else {
				n = 1;
				ytot = y;
			}
			yp = y;
		}
	}

	(void) de2spi_exchange(ADS7846_NOP, 8);
	(void) de2spi_exchange(0, 16);

	de2ts_ts_disable();

	if (xi < dprm->max_samples_x && yi < dprm->max_samples_y) {
		*px = xtot / dprm->min_equals_x;
		*py = ytot / dprm->min_equals_y;
	} else
		return -1;

	return 0;
}

/* */

#define LIGHT_TIMEOUT (3*60*HZ)
static struct timer_list light_timer;
static struct tq_struct event_task;
static struct de2ts_event ev_prev;

static void light_off(unsigned long data)
{
	PBDATA |= 0x20;		/* disable CCFL backlight */
}

static void light_on(void)
{
	PBDATA &= ~0x20;	/* enable CCFL backligh */

	if (light_timer.expires != jiffies + LIGHT_TIMEOUT)
		mod_timer(&light_timer, jiffies + LIGHT_TIMEOUT);
}

/* if there is no room in the event queue, then
 * the event is discarded. The calling process may
 * check ev_no to see if events have been lost.
 */
static void de2ts_addevent(Device * dev, struct de2ts_event *ev)
{
	if (((dev->wi + 1) % MAX_EVENT) != dev->ri) {
		dev->tev[dev->wi] = *ev;
		dev->wi = (++dev->wi) % MAX_EVENT;
		wake_up_interruptible(&dev->rdq);
		if (dev->asq)
			kill_fasync(&dev->asq, SIGIO, POLL_IN);
	}
}

static unsigned long de2ts_settime(struct timeval *start)
{
	struct timeval now;

	do_gettimeofday(&now);
	return (now.tv_sec - start->tv_sec) * 1000 + now.tv_usec / 1000;
}

/* try to debounce the penirq signal while it is
 * read as an IO and not in IRQ mode
 */
static int de2ts_penstate(void)
{
	int i = 0, st, stc;

	goto start_loop;

	for (; i < 200; ++i) {
		if (de2ts_ts_pen() == st) {
			if (++stc > 20)
				return st;
		} else {
		  start_loop:
			stc = 0;
			st = de2ts_ts_pen();
		}
	}

	return 1;
}

/*
 * As long as the penirq signal stay low,
 * a timer task is used to read the touchscreen
 * this is to make sure we don't generate too
 * many events. With a 100 Hz timer, we will
 * have one event each 10 ms maximum.
 */
static void timer_handler(void *dev_id)
{
	Device *dev;
	struct de2ts_event ev;
	static int debounce = 0;
	int speed, tmpf;

	dev = (Device *) dev_id;

	light_on();

	if (!dev->lock) {
		/* device not opened */
		/* re-enable IRQ */
		de2ts_ts_pen_irq();
		return;
	}

	if (de2ts_penstate() == 0) {
		/* if SPI2 is busy, postpone the read to the next timer tick */
		if (de2spi_attach(SPICLK_SYSCLK_16) == 0) {
			tmpf = de2ts_readts(&ev.xraw, &ev.yraw, &dev->drv);
			de2spi_detach();
			if (tmpf == 0) {
	
				/* timestamp the event */
				ev.ev_time = de2ts_settime(&dev->start);
	
				/* convert the event to the user screen coordinates */
				ev.x = dev->cal.xrng * (ev.xraw - dev->cal.xoff) / dev->cal.xden;
				if (ev.x < 0)
					ev.x = 0;
				if (ev.x >= dev->cal.xrng)
					ev.x = dev->cal.xrng - 1;
	
				ev.y = dev->cal.yrng * (ev.yraw - dev->cal.yoff) / dev->cal.yden;
				if (ev.y < 0)
					ev.y = 0;
				if (ev.y >= dev->cal.yrng)
					ev.y = dev->cal.yrng - 1;
	
				if (dev->drv.xysw) {
					int t = ev.x;
					ev.x = ev.y;
					ev.y = t;
				}
	
				/* qualify the event */
				switch (ev_prev.event) {
				case EV_PEN_DOWN:
					ev.event = EV_PEN_MOVE;
					break;
				case EV_PEN_MOVE:
					ev.event = EV_PEN_MOVE;
					break;
				case EV_PEN_UP:
					ev.event = EV_PEN_DOWN;
					break;
				default:			/* no previous event */
					ev.event = EV_PEN_DOWN;
					break;
				}
	
				/* compute the speed from the last position to the current one  */
				/* if we are in MOVE mode only                                  */
				if (ev.event == EV_PEN_MOVE) {
					speed = ((ev.xraw - ev_prev.xraw)>>4) * ((ev.xraw - ev_prev.xraw)>>4);
					speed += ((ev.yraw - ev_prev.yraw)>>4) * ((ev.yraw - ev_prev.yraw)>>4);
					if (ev.ev_time != ev_prev.ev_time)
						speed /= (ev.ev_time - ev_prev.ev_time);
				} else {
					speed = 0;
				}
	
				/* ignore the event if:                                    */
				/*   the speed is higher than the max_speed parameters or  */
				/*   the event is the same as the previous one             */
				if (speed <= dev->drv.max_speed && (ev.x != ev_prev.x || ev.y != ev_prev.y
									|| ev.event != ev_prev.event)) {
					ev.ev_no = ev_prev.ev_no + 1;
					de2ts_addevent(dev, &ev);
					ev_prev = ev;
				} else {
					/* update the timestamp of the last good event, to help not to reject */
					/* too many events once a first one is rejected                       */
					ev_prev.ev_time = ev.ev_time;
				}
			}
			debounce = 0;
		}
		/* penirq was down or SPI2 is busy, so re-enable the timer task */
		queue_task(&event_task, &tq_timer);

	}

	else {

		/* penirq is up, try to debounce it by re-queuing the task 3 times more */
		if (++debounce < 4) {
			queue_task(&event_task, &tq_timer);

		} else {
			
			/* penirq is really down, register the last good event as a PEN_UP event */
			/* if this is not the very first event or if the last good event is not  */
			/* already a PEN_UP event                                                */
			if (ev_prev.event && ev_prev.event != EV_PEN_UP) {
				ev_prev.ev_time = de2ts_settime(&dev->start);
				++ev_prev.ev_no;
				ev_prev.event = EV_PEN_UP;
				de2ts_addevent(dev, &ev_prev);
			}

			/* re-enable IRQ */
			de2ts_ts_pen_irq();
		}
	}
}

/* the penirq interrupt handler temporary disable the interrupts
 * by configuring the signal as a regular IO signal, then it queue
 * a timer task to do the touchscreen reading on the next timer tick
 * the timer task will re-queue itself while the penirq signal is down
 * or re-enable the interrupt
 */
static void penirq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	Device *dev;

	dev = (Device *) dev_id;

	/* pause IRQ */
	de2ts_ts_pen_io();

	event_task.routine = timer_handler;
	event_task.data = dev;
	queue_task(&event_task, &tq_timer);
}

/* */

static ssize_t
de2ts_read(struct file *filp, char *buf, size_t count, loff_t * offp)
{
	Device *dev;
	ssize_t ret, maxrd;

	dev = (Device *) filp->private_data;
	if (!dev)
		return -ENODEV;

	if (offp != &filp->f_pos)
		return -ESPIPE;

	while (dev->wi == dev->ri) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		PDEBUG("de2ts_read going to sleep\n");
		if (wait_event_interruptible(dev->rdq, (dev->wi != dev->ri)))
			return -ERESTARTSYS;
	}

	ret =
		(count / sizeof(struct de2ts_event)) * sizeof(struct de2ts_event);

	if (ret == 0)
		return -EFAULT;

	maxrd = dev->wi - dev->ri;
	while (maxrd < 0)
		maxrd += MAX_EVENT;
	maxrd *= sizeof(struct de2ts_event);

	if (ret > maxrd)
		ret = maxrd;

	if (copy_to_user(buf, &dev->tev[dev->ri], ret))
		return -EFAULT;

	PDEBUG("de2ts_read returning %ld events\n",
		   ret / sizeof(struct de2ts_event));

	dev->ri += ret / sizeof(struct de2ts_event);
	dev->ri %= MAX_EVENT;

	return ret;
}

static unsigned int de2ts_poll(struct file *filp, poll_table * table)
{
	Device *dev;

	dev = (Device *) filp->private_data;
	if (!dev)
		return -ENODEV;

	poll_wait(filp, &dev->rdq, table);
	if (dev->wi != dev->ri)
		return POLLIN | POLLRDNORM;

	return 0;
}

static int de2ts_ioctl(struct inode *inode, struct file *filp,
					   unsigned int cmd, unsigned long arg)
{
	int i;
	Device *dev;
	char *p_in;
	char *p_out;
	struct de2ts_drv_params new_drv_params;
	struct de2ts_cal_params new_cal_params;

	dev = (Device *) filp->private_data;
	if (!dev)
		return -ENODEV;

	switch (cmd) {

	case DE2TS_DRV_PARAMS_GET:
		if (verify_area(VERIFY_WRITE, (char *) arg,
						sizeof(struct de2ts_drv_params)))
			return -EBADRQC;
		p_in = (char *) &dev->drv;
		p_out = (char *) arg;
		for (i = 0; i < sizeof(struct de2ts_drv_params); i++)
			put_user(p_in[i], p_out + i);
		return 0;

	case DE2TS_CAL_PARAMS_GET:
		if (verify_area(VERIFY_WRITE, (char *) arg,
						sizeof(struct de2ts_cal_params)))
			return -EBADRQC;
		p_in = (char *) &dev->cal;
		p_out = (char *) arg;
		for (i = 0; i < sizeof(struct de2ts_cal_params); i++)
			put_user(p_in[i], p_out + i);
		return 0;

	case DE2TS_DRV_PARAMS_SET:
		if (verify_area(VERIFY_READ, (char *) arg,
						sizeof(struct de2ts_drv_params)))
			return -EBADRQC;
		p_in = (char *) &new_drv_params;
		p_out = (char *) arg;
		for (i = 0; i < sizeof(struct de2ts_drv_params); i++) {
			get_user(p_in[i], p_out + i);
		}

		if (new_drv_params.version == 0) {
			/* reset parameters */
			memcpy(&dev->drv, &de2ts_default_drv_params,
				   sizeof(struct de2ts_drv_params));
		} else {
			/* check version */
			if (new_drv_params.version != -1) {
				if (new_drv_params.version != DE2TS_VERSION) {
					printk(KERN_ALERT
						   "%s: error: driver version is %d, requested version is %d\n",
						   MODULE_NAME, DE2TS_VERSION,
						   new_drv_params.version);
					return -EBADRQC;
				}
			}

			/* All the params are set with minimal test. */
			memcpy(&dev->drv, &new_drv_params,
				   sizeof(struct de2ts_drv_params));
		}
		return 0;

	case DE2TS_CAL_PARAMS_SET:
		if (verify_area(VERIFY_READ, (char *) arg,
						sizeof(struct de2ts_cal_params)))
			return -EBADRQC;
		p_in = (char *) &new_cal_params;
		p_out = (char *) arg;
		for (i = 0; i < sizeof(struct de2ts_cal_params); i++) {
			get_user(p_in[i], p_out + i);
		}

		if (new_cal_params.version == 0) {
			/* reset parameters */
			memcpy(&dev->cal, &de2ts_default_cal_params,
				   sizeof(struct de2ts_cal_params));
		} else {
			/* check version */
			if (new_cal_params.version != -1) {
				if (new_cal_params.version != DE2TS_VERSION) {
					printk(KERN_ALERT
						   "%s: error: driver version is %d, requested version is %d\n",
						   MODULE_NAME, DE2TS_VERSION,
						   new_cal_params.version);
					return -EBADRQC;
				}
			}

			if (new_cal_params.xden == 0 || new_cal_params.yden == 0) {
				printk(KERN_ALERT
					   "%s: error: xden or yden should not be set to 0\n",
					   MODULE_NAME);
				return -EBADRQC;
			}

			/* All the params are set with minimal test. */
			memcpy(&dev->cal, &new_cal_params,
				   sizeof(struct de2ts_cal_params));
		}
		return 0;

	default:
		break;
	}
	return -ENOIOCTLCMD;
}

static int de2ts_fasync(int inode, struct file *filp, int mode)
{
	int retval;
	Device *dev;

	dev = (Device *) filp->private_data;
	if (!dev)
		return -ENODEV;

	retval = fasync_helper(inode, filp, mode, &dev->asq);
	if (retval < 0)
		return retval;

	return 0;
}

/* only one open device allowed */
static unsigned long de2ts_lock;

static int de2ts_release(struct inode *inode, struct file *filp)
{
	Device *dev;

	dev = (Device *) filp->private_data;
	if (!dev)
		return -ENODEV;

	de2ts_fasync(-1, filp, 0);

	clear_bit(0, &dev->lock);

	clear_bit(0, &de2ts_lock);

	return 0;
}

/* only one device managed */
static Device de2ts_dev;

static int de2ts_open(struct inode *inode, struct file *filp)
{
	Device *dev;

	if (test_and_set_bit(0, &de2ts_lock))
		return -EDEADLK;

	if (!filp->private_data) {
		filp->private_data = &de2ts_dev;
	}
	dev = filp->private_data;

	if (test_and_set_bit(0, &dev->lock))
		return -EDEADLK;

	init_waitqueue_head(&dev->rdq);
	do_gettimeofday(&dev->start);

	return 0;
}

static int
de2ts_proc_read(char *buf, char **start, off_t offset, int count,
				int *eof, void *data)
{
	int len = 0, evnb;

	evnb = de2ts_dev.wi - de2ts_dev.ri;
	if (evnb < 0) evnb += MAX_EVENT;

	len += sprintf(buf + len, "\nEvent queue: %d/%d\n", evnb, MAX_EVENT);

	len += sprintf(buf + len, "\nCurrent parameters (v%d):\n", de2ts_dev.drv.version);
	len += sprintf(buf + len, "max_samples_x: %d\n", de2ts_dev.drv.max_samples_x);
	len += sprintf(buf + len, "max_samples_y: %d\n", de2ts_dev.drv.max_samples_y);
	len += sprintf(buf + len, " min_equals_x: %d\n", de2ts_dev.drv.min_equals_x);
	len += sprintf(buf + len, " min_equals_y: %d\n", de2ts_dev.drv.min_equals_y);
	len += sprintf(buf + len, "    max_error: %d\n", de2ts_dev.drv.max_error);
	len += sprintf(buf + len, "    max_speed: %d\n", de2ts_dev.drv.max_speed);
	len += sprintf(buf + len, "         xysw: %d\n", de2ts_dev.drv.xysw);

	len += sprintf(buf + len, "\nCurrent calibration values (v%d):\n", de2ts_dev.cal.version);
	len += sprintf(buf + len, "%d %d %d %d %d %d\n",
		de2ts_dev.cal.xoff,
		de2ts_dev.cal.xden,
		de2ts_dev.cal.yoff,
		de2ts_dev.cal.yden,
		de2ts_dev.cal.xrng,
		de2ts_dev.cal.yrng);

	*eof = 1;
	return len;
}

static struct file_operations de2ts_fops = {
	owner:THIS_MODULE,
	poll:de2ts_poll,
	read:de2ts_read,
	ioctl:de2ts_ioctl,
	fasync:de2ts_fasync,
	release:de2ts_release,
	open:de2ts_open,
};

static struct miscdevice de2ts = {
	MK712_MINOR, MODULE_NAME, &de2ts_fops
};

static int __init de2ts_init(void)
{
	int err;
	
	err = misc_register(&de2ts);
	if (err < 0) {
		printk(KERN_ALERT
			   "%s: error registering touchscreen driver.\n", MODULE_NAME);
		return err;
	}

	err = request_irq(IRQ5_IRQ_NUM, penirq_handler,
					  IRQ_FLG_STD, "touchscreen", &de2ts_dev);
	if (err) {
		printk(KERN_ALERT
			   "%s: Cannot attach IRQ %d\n", MODULE_NAME, IRQ5_IRQ_NUM);
		return err;
	}

	memset(&de2ts_dev, 0, sizeof(Device));
	memcpy(&de2ts_dev.drv, &de2ts_default_drv_params,
		   sizeof(struct de2ts_drv_params));
	memcpy(&de2ts_dev.cal, &de2ts_default_cal_params,
		   sizeof(struct de2ts_cal_params));

	de2ts_setup_hw();

	init_timer(&light_timer);
	light_timer.function = light_off;
	light_timer.expires = jiffies + LIGHT_TIMEOUT;
	add_timer(&light_timer);

	create_proc_read_entry(MODULE_NAME, 0, NULL, de2ts_proc_read, NULL);

	printk(KERN_INFO
		   "%s: touchscreen driver installed (c,10,%d).\n", MODULE_NAME,
		   de2ts.minor);

	return 0;
}

static void __exit de2ts_cleanup(void)
{
	de2ts_reset_hw();
	free_irq(IRQ5_IRQ_NUM, &de2ts_dev);
	remove_proc_entry(MODULE_NAME, NULL);
	misc_deregister(&de2ts);
}

module_init(de2ts_init);
module_exit(de2ts_cleanup);
