/* -*- c -*- --------------------------------------------------------- *
 *
 * Copyright (C) 2003 Stefan Macher <macher@sympat.de>
 *
 * This driver is not compatible with the generic linux mouse protocol.
 * There is a microwindows driver available that supports this touchscreen
 * driver.
 * ------------------------------------------------------------------------- */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/arch/irqs.h>
#include <asm/arch/s3c44b0x.h>

#include <linux/mba44b0touch.h>

/* debugging level  */
/* ---------------- */
#ifdef MBA44B0_TS_DEBUG_LEVEL
static int mba44b0_ts_debug_mask = MBA44B0_TS_DEBUG_LEVEL;
#else
static int mba44b0_ts_debug_mask = MBA44B0_TS_DEBUG_ERR;
#endif

/* delay between interrupt and first read out of position  */
/* ------------------------------------------------------- */
#ifdef MBA44B0_TS_FIRST_POLL_DELAY
static int FirstPollDelay = MBA44B0_TS_FIRST_POLL_DELAY;
#else
static int FirstPollDelay = 2; /* default value */
#endif

/* delay between two consecutive position read outs        */
/* ------------------------------------------------------- */
#ifdef MBA44B0_TS_POLL_DELAY
static int PollDelay = MBA44B0_TS_POLL_DELAY;
#else
static int PollDelay = 10; /* default value is 100 ms */
#endif

/* prescaler of the ADC */
/* -------------------- */
#ifdef MBA44B0_TS_ADC_PRESCALER
static int adcpsr = MBA44B0_TS_ADC_PRESCALER
#else
static int adcpsr = 0xF; /* default value */
#endif

/* interrupt of the touchscreen  */
/* ----------------------------- */
#ifdef MBA44B0_TS_IRQ
static int mba44b0_irq = MBA44B0_TS_IRQ;
#else
static int mba44b0_irq = S3C44B0X_INTERRUPT_EINT3; /* default value */
#endif

/* screen informations  */
/* -------------------- */
#ifdef MBA44B0_TS_X_RES
static int xres = MBA44B0_TS_X_RES;
#else
static int xres = 320;
#endif

#ifdef MBA44B0_TS_Y_RES
static int yres = MBA44B0_TS_X_RES;
#else
static int yres = 240;
#endif

/* calibration settings  */
/* --------------------- */
struct mba44b0_cal_settings cal_settings = {
#ifdef MBA44B0_TS_CAL_X_OFF
	cal_x_off: MBA44B0_TS_CAL_X_OFF,
#else
	cal_x_off: -100,
#endif

#if ((defined MBA44B0_TS_CAL_X_SCALE_NUM) && (defined MBA44B0_TS_CAL_X_SCALE_DENOM))
	cal_x_scale_num: MBA44B0_TS_CAL_X_SCALE_NUM,
	cal_x_scale_denom: MBA44B0_TS_CAL_X_SCALE_DENOM,
#else
	cal_x_scale_num: (1<<21),
	cal_x_scale_denom: 2949120,
#endif

#ifdef MBA44B0_TS_CAL_Y_OFF
	cal_y_off: MBA44B0_TS_CAL_Y_OFF,
#else
	cal_y_off: -500,
#endif

#if ((defined MBA44B0_TS_CAL_Y_SCALE_NUM) && (defined MBA44B0_TS_CAL_Y_SCALE_DENOM))
	cal_y_scale_num:  MBA44B0_TS_CAL_Y_SCALE_NUM,
	cal_y_scale_denom: MBA44B0_TS_CAL_Y_SCALE_DENOM,
#else
	cal_y_scale_num: (1<<21),
	cal_y_scale_denom: 3407872,
#endif
};

/* function declarations           */
/* ------------------------------- */
static void           mba44b0_config_pressed(void);
static void           mba44b0_config_read_x(void);
static void           mba44b0_config_read_y(void);
static void           mba44b0_config_adc(unsigned int input_select);
static unsigned int   mba44b0_read_adc(void);
static void           mba44b0_interv_poll(unsigned long irq);
static void           mba44b0_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static int            mba44b0_open(struct inode *inode, struct file *file); 
static int            mba44b0_close(struct inode * inode, struct file * file);
static unsigned int   mba44b0_poll(struct file *file, poll_table *wait);
static int            mba44b0_ioctl(struct inode *inode, struct file * file, unsigned int cmd, unsigned long arg);
static ssize_t        mba44b0_read(struct file *file, char *buffer, size_t count, loff_t *pos);
static ssize_t        mba44b0_write(struct file *file, const char *buffer, size_t count, loff_t *ppos);
static int __init     mba44b0_init(void);
static void __exit    mba44b0_exit(void);
static void           mba44b0_map_to_coord(int x, int y, int *newx, int *newy);
/* -------------------------------- */

struct file_operations mba44b0_ts_fops = {
	owner: THIS_MODULE,
	read: mba44b0_read,
	write: mba44b0_write,
	poll: mba44b0_poll,
	ioctl: mba44b0_ioctl,
	open: mba44b0_open,
	release: mba44b0_close,
	fasync: NULL,
};

static struct miscdevice mba44b0_touchscreen = {
	MBA44B0_TS_MINOR, "mba44b0_touchscreen", &mba44b0_ts_fops
};

static struct timer_list mba44b0_timer; /* a kernel timer that is used for polling */

static int mba44b0_users = 0; /* number of connected users; only one is allowed */

static int pos_x=0, pos_y=0; /* indicates the last measured position */
static int last_pos_x=0, last_pos_y=0; /* the position that was read the last time */
static unsigned char touch_pressed=0; /* 1 means the touch is currently pressed */
static int touch_event=0; /* is there an event pending */

DECLARE_WAIT_QUEUE_HEAD(touch_wait);
static spinlock_t touch_lock = SPIN_LOCK_UNLOCKED;

/*! \brief Maps the measured touch position to pixel values
 */
static void mba44b0_map_to_coord(int x, int y, int *newx, int *newy)
{
	*newx = (x + cal_settings.cal_x_off) * cal_settings.cal_x_scale_num / cal_settings.cal_x_scale_denom;
	*newy = (y + cal_settings.cal_y_off) * cal_settings.cal_y_scale_num / cal_settings.cal_y_scale_denom;
}

/*! \brief Configures the touchscreen ports for testing if
 *         the touchscreen is pressed or not
 *
 *  It configures 
 *  - TOUCH_XH and TOUCH_XL as input with active pull-up
 *  - TOUCH_YH and TOUCH_YL as output low
 *
 *  If the touch is pressed the low level of the Y plane is connected to
 *  the X plane and therefore the voltage at AIN0 should also be low
 *
 *  This setting is also used to enable the interrupt
 */
static void mba44b0_config_pressed()
{
	unsigned int val32;

	MBA44B0_TS_DEBUG(MBA44B0_TS_DEBUG_CONFIG, "switch to config pressed\n");

	/*-------------------------*/
	/* configuration of PCONE  */
	/*-------------------------*/
	val32=inl(S3C44B0X_PCONE);
	outl((val32&0xFFFF00FF)|0x500, S3C44B0X_PCONE); /* config GPE4 - GPE5 output GPE6 and GPE7 input */
	/*-------------------------*/
	/* configuration of PUPE   */
	/*-------------------------*/
	val32=inl(S3C44B0X_PUPE);
	outl((val32&0x0F)|0xC0, S3C44B0X_PUPE); /* config GPE6 and GPE7 activate pull up */
	/*-------------------------*/
	/* configuration of PDATE  */
	/*-------------------------*/
	val32=inl(S3C44B0X_PDATE);
	outl(val32&0xFFFFFF0F, S3C44B0X_PDATE); /* GPE5 low, GPE4 low */

}

/*! \brief Configures the touchscreen ports for reading out the
 *         the x position
 *
 *  It configures 
 *  - TOUCH_XH as output high
 *  - TOUCH_XL as output low
 *  - TOUCH_YH and TOUCH_YL as input with active pull-up
 *
 *  The voltage level at AIN1 then defines the X position
 */
static void mba44b0_config_read_x()
{
	unsigned int val32;

	MBA44B0_TS_DEBUG(MBA44B0_TS_DEBUG_CONFIG, "switch to config read x\n");

	/*-------------------------*/
	/* configuration of PCONE  */
	/*-------------------------*/
	val32=inl(S3C44B0X_PCONE);
	outl((val32&0xFFFF00FF)|0x5000, S3C44B0X_PCONE); /* config GPE4 - GPE5 input GPE6 and GPE7 output */
	/*-------------------------*/
	/* configuration of PUPE   */
	/*-------------------------*/
	val32=inl(S3C44B0X_PUPE);
	outl((val32&0x0F)|0x00, S3C44B0X_PUPE); /* config GPE4 and GPE5 activate pull up */
/*	outl((val32&0x0F)|0x30, S3C44B0X_PUPE); ?  config GPE4 and GPE5 activate pull up */
	/*-------------------------*/
	/* configuration of PDATE  */
	/*-------------------------*/
	val32=inl(S3C44B0X_PDATE);
	outl((val32&0xFFFFFF0F)|0x80, S3C44B0X_PDATE); /* GPE6 low, GPE7 high */

}

/*! \brief Configures the touchscreen ports for reading out the
 *         the y position
 *
 *  It configures 
 *  - TOUCH_YH as output high
 *  - TOUCH_YL as output low
 *  - TOUCH_XH and TOUCH_XL as input with active pull-up
 *
 *  The voltage level at AIN0 then defines the Y position
 */
static void mba44b0_config_read_y()
{
	unsigned int val32;

	MBA44B0_TS_DEBUG(MBA44B0_TS_DEBUG_CONFIG, "switch to config read y\n");

	/*-------------------------*/
	/* configuration of PCONE  */
	/*-------------------------*/
	val32=inl(S3C44B0X_PCONE);
	outl((val32&0xFFFF00FF)|0x500, S3C44B0X_PCONE); /* config GPE4 - GPE5 output GPE6 and GPE7 input */
	/*-------------------------*/
	/* configuration of PUPE   */
	/*-------------------------*/
	val32=inl(S3C44B0X_PUPE);
	outl((val32&0x0F)|0x00, S3C44B0X_PUPE); /* config GPE6 and GPE7 activate pull up */
/*	outl((val32&0x0F)|0xC0, S3C44B0X_PUPE);    config GPE6 and GPE7 activate pull up */
	/*-------------------------*/
	/* configuration of PDATE  */
	/*-------------------------*/
	val32=inl(S3C44B0X_PDATE);
	outl((val32&0xFFFFFF0F)|0x20, S3C44B0X_PDATE); /* GPE4 low, GPE5 high */

}

/*! \brief Configures the prescaler and the input line
 *         of the ADC
 *
 *  /param input_select Valid values are 0-7 corresponding to AIN0 - AIN7
 */
static void mba44b0_config_adc(unsigned int input_select)
{
	MBA44B0_TS_DEBUG(MBA44B0_TS_DEBUG_ADC, "set ADC input to %d\n", input_select);

	/*-------------------------*/
	/* configuration of ADCPSR */
	/*-------------------------*/
	outl(adcpsr, S3C44B0X_ADCPSR); //prescaler
	/*-------------------------*/
	/* configuration of ADCCON */
	/*-------------------------*/
	outl((input_select<<2), S3C44B0X_ADCCON); 

}

/*! \brief Reads out the ADC value
 *
 *  According to the S3C44B0X manual the procedure is as follows:
 *  - Activate conversion by setting bit 0 in ADCCON
 *  - Wait until the start bit is cleared (after one ADC clock) because
 *    the ready flag if 1 for one ADC clock after conversion started
 *  - Wait until the ready bit is 1
 *  - Wait on further ADC clock because the ready bit is set on ADC clock
 *    before conversion is finished
 *
 *  \return The ADC value
 */
static unsigned int mba44b0_read_adc()
{
	unsigned int val32, i;

	MBA44B0_TS_DEBUG(MBA44B0_TS_DEBUG_INFO, "Read\n");

       /*--------------------------*/
	/* activate ADC             */
	/*--------------------------*/
	val32=inl(S3C44B0X_ADCCON);
	outl(val32 |0x1, S3C44B0X_ADCCON);
	/*----------------------------------------------*/
	/* wait because of errors in the ADC of the CPU */
	/*----------------------------------------------*/
	do
	{
		val32 = inl(S3C44B0X_ADCCON);
	}
	while(val32 & 0x1);
	while(!(val32 & 0x40)) 
	{ 
		val32 = inl(S3C44B0X_ADCCON); 
	}
	for(i=0; i < adcpsr >> 2 ; i++)
	{
		val32 = inl(S3C44B0X_ADCDAT);
	}
	return val32;
}

/*! \brief This function is called by the kernel timer and checks the actual
 *         position
 *
 * At first we check if the touch is actually pressed.
 * If not, we will reenable the interrupt and just return.
 * If it is pressed, we will check the actually position and
 * if it changed compared to the previous position we will store it
 */
static void mba44b0_interv_poll(unsigned long irq)
{
	unsigned int val32=0;
	unsigned int x, y, map_x, map_y, meas, i;

	MBA44B0_TS_DEBUG(MBA44B0_TS_DEBUG_INFO, "polling\n");

	mba44b0_config_pressed(); /* configure the touchscreen ports for measuring if any key is pressed */
	mba44b0_config_adc(0); /* measure on AIN0 */

	val32 = mba44b0_read_adc();
	/*-------------------------------------*/
	/* check if touch is no longer pressed */
	/*-------------------------------------*/
	if(val32 > MBA44B0_TS_PRESS_THRESHOLD)
	{ /* it is not pressed */
		if(touch_pressed)
			touch_event = 1; /* release event */
		touch_pressed = 0;
		enable_irq(irq); /* reenable the touchscreen interrupt */
		return;
	}
       /* it is pressed so calculate the new position */
	/* we start with reading the x position */
	mba44b0_config_read_x();
	mba44b0_config_adc(1);
	meas = 0;
	for(i=0; i < 40; i++)
	{
		x = mba44b0_read_adc();
		if(i>=8)
			meas += x;
	}
	x = meas >> 5;
	printk("x=%d ", x);
	mba44b0_config_read_y();
	mba44b0_config_adc(0);
	meas = 0;
	for(i=0; i < 40; i++)
	{
		y = 1023 - mba44b0_read_adc();
		if(i>=8)
			meas += y;
	}
	y = meas >> 5;
	printk("y=%d\n",y);

	mba44b0_map_to_coord(x, y, &map_x, &map_y);
	printk("cal x=%d, y=%d\n", map_x, map_y);
	spin_lock(&touch_lock);
	pos_x = map_x;
	pos_y = map_y;
	if( (pos_x != last_pos_x) || 
	    (pos_y != last_pos_y) ||
	    (!touch_pressed) )
 /* an event occured if the position changed or if the touch is newly pressed */
		touch_event=1;

	touch_pressed = 1;
	spin_unlock(&touch_lock);


	init_timer(&mba44b0_timer);
	mba44b0_timer.function = mba44b0_interv_poll;
	mba44b0_timer.data = irq;
	mba44b0_timer.expires = PollDelay;
	add_timer(&mba44b0_timer);

	MBA44B0_TS_DEBUG(MBA44B0_TS_DEBUG_POS, "x:%d y:%d\n", x, y);

	wake_up_interruptible(&touch_wait);
}


static void mba44b0_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	/*-------------------------*/
	/* disable irq             */
	/*-------------------------*/
	disable_irq(irq);

	MBA44B0_TS_DEBUG(MBA44B0_TS_DEBUG_IRQ, "irq\n");
	
	/*-------------------------*/
	/* setup kernel timer      */
	/*-------------------------*/
	init_timer(&mba44b0_timer);
	mba44b0_timer.function = mba44b0_interv_poll;
	mba44b0_timer.data = irq;
	mba44b0_timer.expires = FirstPollDelay;
	add_timer(&mba44b0_timer);
	/*--------------------------*/
	/* return from interrupt    */
	/*--------------------------*/
	return;

}

static int mba44b0_open(struct inode *inode, struct file *file) 
{
	MBA44B0_TS_DEBUG(MBA44B0_TS_DEBUG_INFO, "open\n");
	if(!mba44b0_users) /* only one user is allowed!! */
		mba44b0_users++;
	else
		return -EBUSY;

	enable_irq(mba44b0_irq);
	return 0;
}

static int mba44b0_close(struct inode * inode, struct file * file)
{
	MBA44B0_TS_DEBUG(MBA44B0_TS_DEBUG_INFO, "close\n");
	mba44b0_users--;
	if(!mba44b0_users) /* must be also true since we allow only one user */
		disable_irq(mba44b0_irq);
	return 0;
}

static unsigned int mba44b0_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &touch_wait, wait);
	if(touch_event)
		return (POLLIN | POLLRDNORM);

	return 0;
}

static int mba44b0_ioctl(struct inode *inode, struct file * file,
	unsigned int cmd, unsigned long arg)
{
	switch(cmd)
	{
	case MBA44B0_TS_SET_CAL_PARAMS:
		if(!arg) {
			MBA44B0_TS_DEBUG(MBA44B0_TS_DEBUG_ERR, "Set calibration params with zero pointer\n");
			return -EINVAL;
		}
		copy_from_user(&cal_settings, (void*)arg, sizeof(cal_settings));
		break;
		
	case MBA44B0_TS_GET_CAL_PARAMS:
		if(!arg) {
			MBA44B0_TS_DEBUG(MBA44B0_TS_DEBUG_ERR, "Get calibration params with zero pointer\n");
			return -EINVAL;
		}
		copy_to_user((void*)arg, &cal_settings, sizeof(cal_settings));
		break;
	
	case MBA44B0_TS_SET_X_RES:
		copy_from_user(&xres, (void*)arg, sizeof(xres));
		break;

	case MBA44B0_TS_SET_Y_RES:
		copy_from_user(&yres, (void*)arg, sizeof(yres));
		break;

	default:
		MBA44B0_TS_DEBUG(MBA44B0_TS_DEBUG_ERR, "Received unknown ioctl command\n");
		return -EINVAL;
	}

	return 0;
}


static ssize_t mba44b0_read(struct file *file, char *buffer,
			  size_t count, loff_t *pos)
{
	struct ts_event event;
        unsigned long flags;
	
        if(count != sizeof(struct ts_event))
                return -EINVAL;

	while(!touch_event)
        {
                if(file->f_flags&O_NDELAY)
		{
			return -EWOULDBLOCK;
		}
                if(signal_pending(current))
		{
			return -ERESTARTSYS;
		}
		if (wait_event_interruptible(touch_wait, touch_event))
			return -ERESTARTSYS;
        }

         /* Grab the event */

        spin_lock_irqsave(&touch_lock, flags);

	event.x = pos_x;
	event.y = pos_y;
	event.pressure = touch_pressed;
	touch_event = 0;
	last_pos_x = pos_x;
	last_pos_y = pos_y;

        spin_unlock_irqrestore(&touch_lock, flags);
  
	event.pad = 0;

/* Restrict to screen resolution */
	if(event.x < 0)
		event.x = 0;
	else if(event.x >= xres)
		event.x = xres-1;
	if(event.y < 0)
		event.y = 0;
	else if(event.y >= yres)
		event.y = yres-1;


	if(copy_to_user(buffer, &event, sizeof(struct ts_event)))
		return -EFAULT;
        return count;	
}

static ssize_t mba44b0_write(struct file *file, const char *buffer, size_t count,
			   loff_t *ppos)
{
	return -EINVAL;
}

static int __init mba44b0_init(void)
{
	unsigned int val32;

	printk("mba44b0_touch: initializing...\n");

	/* configure touchscreen ports for interrupt */
	mba44b0_config_pressed();
	/*-------------------------------*/
	/* configure interrupt           */
	/*-------------------------------*/
	val32=inl(S3C44B0X_INTMOD);
	outl((val32&0xF7FFFFF), S3C44B0X_INTMOD); /* normal IRQ */
	val32=inl(S3C44B0X_EXTINT);
	outl((val32&0xFFFF8FFF)|0x2000, S3C44B0X_EXTINT); /*falling edge */
	val32=inl(S3C44B0X_PCONG);
	outl(val32 | 0xC0, S3C44B0X_PCONG); /* use Port G pin 3 as ext int 3 */
	
	if(request_irq(mba44b0_irq, mba44b0_interrupt, 0, "mba44b0_touchscreen",NULL))
	{
		printk("mba44b0: unable to get IRQ\n");
		return -EBUSY;
	}

	disable_irq(mba44b0_irq);

        /* register misc device */
	if(misc_register(&mba44b0_touchscreen)<0)
	{
		free_irq(mba44b0_irq, NULL);
		return -ENODEV;
	}

	printk("mba44b0_touch: init ready\n");

	return 0;
}

static void __exit mba44b0_exit(void)
{
	misc_deregister(&mba44b0_touchscreen);
	free_irq(mba44b0_irq, NULL);
	printk(KERN_INFO "mba44b0 touchscreen uninstalled\n");
}

MODULE_AUTHOR("Stefan Macher & Alexander Assel; sympat GmbH Nuremberg");
MODULE_DESCRIPTION("MBA44B0 touch screen driver");

MODULE_PARM(FirstPollDelay, "i");
MODULE_PARM_DESC(FirstPollDelay, "Is the delay between the interrupt and the first read out of the touchscreen position in jiffies (normallay 10 ms) (default is 2 -> 20 ms)");

MODULE_PARM(PollDelay, "i");
MODULE_PARM_DESC(PollDelay, "Is the delay between two consecutive read out of the touchscreen position in jiffies (normallay 10 ms) (default is 10 -> 100 ms)");

MODULE_PARM(irq, "i");
MODULE_PARM_DESC(irq, "IRQ number of MBA44B0 touch screen controller (default is 22 -> EINT3)");

MODULE_PARM(adcpsr, "i");
MODULE_PARM_DESC(adcpsr, "Prescaler configuration for the ADC; valid values are 1...255 (default is 15)");

MODULE_PARM(xres, "i");
MODULE_PARM_DESC(xres, "X resolution of the display (default is 320 pixels)");

MODULE_PARM(yres, "i");
MODULE_PARM_DESC(yres, "Y resolution of the display (default is 240 pixels)");

MODULE_LICENSE("GPL");

module_init(mba44b0_init);
module_exit(mba44b0_exit);

/*
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
