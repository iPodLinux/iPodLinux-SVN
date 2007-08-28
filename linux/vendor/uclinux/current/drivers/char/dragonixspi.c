/*****************************************************************************/

/*
 *	ds1302.c -- driver for Dallas Semiconductor DS1302 Real Time Clock.
 *
 * 	(C) Copyright 2001, Greg Ungerer (gerg@snapgear.com)
 */

/*****************************************************************************/

#include <linux/config.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/wait.h>
#include <linux/rtc.h>

/* Defines */
#define NONE 0
#define PEN  1
#define CLK  2
#define RTC_SIZE 0x80

/* File seek operations */
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

/* Global data */
static spinlock_t rtc_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t hw_lock = SPIN_LOCK_UNLOCKED;

/* Utility functions */
int __inline__ bcd2int(int val)
{
	return((((val & 0xf0) >> 4) * 10) + (val & 0xf));
}

/* SPI IO functions */
static void uwait(unsigned long x)
{
 int loop;
 while(x>0)
 {
  for(loop=0;loop<10;loop++);
  x--;
 }
}

static unsigned long SPIIO(unsigned char chip, unsigned long shifts, unsigned long out)
{
 int loop;
 unsigned long in=0;

 *(volatile unsigned  char *)0x4000103=(chip&0x03);
 uwait(5);

 for(loop=shifts;loop>0;loop--)
 {
  *(volatile unsigned  char *)0x4000103= (((out&(1<<(loop-1)))!=0 ? 0x10 : 0x00) | (chip&0x03));        /* data out */
  uwait(1);
  *(volatile unsigned  char *)0x4000103= (((out&(1<<(loop-1)))!=0 ? 0x10 : 0x00) | (chip&0x03) | 0x04); /* clk */
  uwait(5);
  in=(in<<1) | (((*(volatile unsigned  char *)0x4000103)&0x10)>>4);
  uwait(5);
  *(volatile unsigned  char *)0x4000103= (((out&(1<<(loop-1)))!=0 ? 0x10 : 0x00) | (chip&0x03));        /* data out */
  uwait(5);
 }
 *(volatile unsigned  char *)0x4000103=0; /* deselect all */
 return(in);
}


/* RTC kernel functions */
/* Called from kernel to get the rtc date */
void dragonixrtc_gettod(int *year, int *mon, int *day, int *hour, int *min, int *sec)
{
	/* Ensure exclusive access to hardware */
	spin_lock_irq(&hw_lock);
	*sec   =       bcd2int(SPIIO(CLK,16,0x00<<8) & 0xff);
	*min   =       bcd2int(SPIIO(CLK,16,0x01<<8) & 0xff);
	*hour  =       bcd2int(SPIIO(CLK,16,0x02<<8) & 0xff);
	*day   =       bcd2int(SPIIO(CLK,16,0x04<<8) & 0xff);
	*mon   =       bcd2int(SPIIO(CLK,16,0x05<<8) & 0xff);
	*year  =       bcd2int(SPIIO(CLK,16,0x06<<8) & 0xff);
	spin_unlock_irq(&hw_lock);

}
/* Called from kernel to synchronize the kernel clock to the RTC */
int dragonixrtc_set_clock_mmss(unsigned long nowtime)
{
	unsigned long	m, s;

#if 1
	printk("ds1302_set_clock_mmss(nowtime=%ld)\n", nowtime);
#endif

	/* FIXME: not implemented yet... */
	s = nowtime % 60;
	m = nowtime / 60;
	return(0);
}
/* End of kernel functions */

/* RTC file operation functions */
static int rtc_open(struct inode *inode, struct file *file) {
	//printk("RTC device opened\n");
	/* Nothing to do here, the lock are made in i/o functions */

	return 0;
}

static int rtc_release(struct inode *inode, struct file *file) {
	//printk("Device closed");

	return 0;
}

static loff_t rtc_lseek(struct file *file, loff_t offset, int orig) {
	loff_t pos = 0;

	switch(orig) {
	case SEEK_SET:
		pos = offset;
		break;
	case SEEK_CUR:
		pos = file->f_pos + offset;
		break;
	case SEEK_END:
		pos = (RTC_SIZE - 1) + offset;
		break;
	}
	if(pos < 0)
		pos = 0;
	if(pos >= RTC_SIZE)
		pos = RTC_SIZE - 1;
	file->f_pos = pos;
	return(pos);
}

static ssize_t rtc_read(struct file *file, char *buffer, size_t length, loff_t *offset) {
	int	total;

	if (file->f_pos >= RTC_SIZE)
		return(0);
	if (length > (RTC_SIZE - file->f_pos))
		length = RTC_SIZE - file->f_pos;

	spin_lock_irq(&hw_lock);
	for (total = 0; (total < length); total++) {
		*buffer = SPIIO(CLK, 16, (file->f_pos + total) << 8) & 0xff;
		buffer++;
	}
	spin_unlock_irq(&hw_lock);

	file->f_pos += total;
	return(total);

}

static ssize_t rtc_write(struct file *file, const char *buffer, size_t length, loff_t *offset) {
	int	total;

	if (file->f_pos >= RTC_SIZE)
		return(0);
	if (length > (RTC_SIZE - file->f_pos))
		length = RTC_SIZE - file->f_pos;

	spin_lock_irq(&hw_lock);
	for (total = 0; (total < length); total++, buffer++)
		SPIIO(CLK, 16, ((file->f_pos + total) << 8) | 0x8000 | *buffer);
	spin_lock_irq(&hw_lock);

	file->f_pos += total;
	return(total);
}
/* End of RTC file operations */

/* RTC device definition */
static struct file_operations dragonixrtc_fops =
{
	llseek: rtc_lseek,
	owner:	THIS_MODULE,
	read: rtc_read,
	write: rtc_write,
	open: rtc_open,
	release: rtc_release
};

static struct miscdevice dragonixrtc_dev =
{
	RTC_MINOR,
	"rtc",
	&dragonixrtc_fops
};

/*****************************************************************************/

static int __init dragonixspi_init(void)
{
	// Register the RTC device
	misc_register(&dragonixrtc_dev);

	printk ("Dragonix SPI: Copyright (C) 2002, Marcos Lois Bermúdez "
		"(marcos.lois@securez.org)\n");

	return 0;
}

static void __exit dragonixspi_exit(void)
{
	// not used
}

/*****************************************************************************/
module_init(dragonixspi_init);
module_exit(dragonixspi_exit);
