/*
 * pcf50605.c - pcf50605 RTC driver
 *
 * Copyright (c) 2004-2004 Bernard Leach <leachbj@bouncycastle.org>
 */


#include <linux/module.h>
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/major.h>
#include <linux/miscdevice.h>
#include <linux/fcntl.h>
#include <linux/rtc.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <linux/devfs_fs_kernel.h>

#include <asm/uaccess.h>
#include <asm/io.h>


static devfs_handle_t rtc_devfs_handle;
static int pcf_rtc_isopen = 0;

static int pcf_rtc_readbyte(int addr)
{
	int data;

	ipod_i2c_send_byte(0x8, addr);
	ipod_i2c_read_byte(0x8, &data);

	return data;
}

#ifndef BCD_TO_BIN
#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)
#endif

static void pcf_get_rtc(struct rtc_time *tm)
{
	tm->tm_sec = pcf_rtc_readbyte(0xa);
	tm->tm_min = pcf_rtc_readbyte(0xb);
	tm->tm_hour = pcf_rtc_readbyte(0xc);
	tm->tm_mday = pcf_rtc_readbyte(0xe);
	tm->tm_mon = pcf_rtc_readbyte(0xf);
	tm->tm_year = pcf_rtc_readbyte(0x10);

	BCD_TO_BIN(tm->tm_mday);
	BCD_TO_BIN(tm->tm_mon);

	tm->tm_mon--;
	tm->tm_year += 100;
}

static int pcf_rtc_open(struct inode *inode, struct file *file)
{
	if (pcf_rtc_isopen) {
		return -EBUSY;
	}

	pcf_rtc_isopen = 1;
	return 0;
}

static int pcf_rtc_close(struct inode *inode, struct file *file)
{
	pcf_rtc_isopen = 0;

	return 0;
}

static int pcf_rtc_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct rtc_time now;

	switch (cmd) {
	case RTC_RD_TIME:
		memset(&now, 0x0, sizeof(now));
		pcf_get_rtc(&now);
		return copy_to_user((void *)arg, &now, sizeof(now)) ? -EFAULT : 0;

	// case RTC_SET_TIME:
	}

	return -EINVAL;
}

static struct file_operations pcf_rtc_fops = {
	owner: THIS_MODULE,
	open: pcf_rtc_open,
	release: pcf_rtc_close,
	ioctl: pcf_rtc_ioctl,
};

static int __init pcf50605_init(void)
{
	if ((ipod_get_hw_version() >> 16) < 0x3) {
		return 0;
	}

	ipod_i2c_init();

	rtc_devfs_handle = devfs_register(NULL, "rtc", DEVFS_FL_DEFAULT,
		MISC_MAJOR, RTC_MINOR,
		S_IFCHR | S_IWUSR | S_IRUSR,
		&pcf_rtc_fops, NULL);
        if (rtc_devfs_handle < 0) {
                printk(KERN_WARNING "rtc: failed to register major %d, minor %d\n",
                        MISC_MAJOR, RTC_MINOR);
                return 0;
        }

	return 0;
}

static void __exit pcf50605_exit(void)
{
	devfs_unregister_chrdev(MISC_MAJOR, "rtc");
	devfs_unregister(rtc_devfs_handle);
}

module_init(pcf50605_init);
module_exit(pcf50605_exit);

MODULE_AUTHOR("Bernard Leach <leachbj@bouncycastle.org>");
MODULE_DESCRIPTION("RTC driver for PCF50605");
MODULE_LICENSE("GPL");

