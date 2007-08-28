/*
 * pcf50605.c - pcf50605 RTC driver
 *
 * Copyright (c) 2004-2005 Bernard Leach <leachbj@bouncycastle.org>
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

#include <linux/proc_fs.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/pm.h>

#include <asm/uaccess.h>
#include <asm/io.h>

#include <asm/hardware.h>


static devfs_handle_t rtc_devfs_handle;
static int pcf_rtc_isopen;

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

extern int i2c_readbyte(unsigned int, int);

/* ADCMUX 0010 -> VCC (in diag) = 455 */
/* ADCMUX ???? -> Battery (in diag) = 463 */
/* ADCMUX ???? -> Temp (in diag) = 349 */
/* ADCMUX ???? -> ACC (in diag) = 255 */
static int pcf_a2d_read(int adc_input)
{
	int hi, lo;

	/* Enable ACD module */
	ipod_i2c_send(0x8, 0x33, 0x80);	/* ACDC1, ACDAPE = 1 */

	/* select ADCIN1 - subtractor, and start sampling process */
	ipod_i2c_send(0x8, 0x2f, (adc_input<<1) | 0x1);	/* ADCC2, ADCMUX = adc_input, ADCSTART = 1 */

	/* ADCC2, wait for ADCSTART = 0 (wait for sampling to start) */
	while ((i2c_readbyte(0x8, 0x2f) & 1)) /* do nothing */;

	/* ADCS2, wait ADCRDY = 0 (wait for sampling to end) */
	while (!(i2c_readbyte(0x8, 0x31) & 0x80)) /* do nothing */;

	hi = i2c_readbyte(0x8, 0x30);		/* ADCS1 */
	lo = (i2c_readbyte(0x8, 0x31) & 0x3);	/* ADCS2 */

	return (hi << 2) | lo;
}

static int pcf_get_charger_status(void)
{
	int charge;
       
	/* OOCS, CHGOK */
	charge = (i2c_readbyte(0x8, 0x1) >> 5) & 0x1;

	return charge;
}

static int pcf_battery_read(void)
{
	return pcf_a2d_read(0x3);	/* ADCIN1, subtractor */
}

static int pcf_battery_temp_read(void)
{
	ipod_i2c_send(0x8, 0x38, 0x7);	/* GPOC1 - constant high */
	return pcf_a2d_read(0x4);	/* BATTEMP, ratiometric */
}

void pcf_standby_mode(void)
{
	ipod_i2c_send(0x8, 0x8, 0x1 | (1 << 5) | (1 << 6));
}

static int ipod_apm_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	char *p;

	static char driver_version[] = "1.16";      /* no spaces */
	int pcf_batt_value;

	unsigned short  ac_line_status = 0xff;
	unsigned short  battery_status = 0;
	unsigned short  battery_flag   = 0xff;
	int             percentage     = -1;
	int             time_units     = -1;
	char            *units         = "?";

       	pcf_batt_value = pcf_battery_read();

	if (pcf_get_charger_status()) {
		battery_status = 0x3;	/* charging */
	}
	time_units = pcf_batt_value;
	units = "ipods";

        /* Arguments, with symbols from linux/apm_bios.h.  Information is
           from the Get Power Status (0x0a) call unless otherwise noted.

           0) Linux driver version (this will change if format changes)
           1) APM BIOS Version.  Usually 1.0, 1.1 or 1.2.
           2) APM flags from APM Installation Check (0x00):
              bit 0: APM_16_BIT_SUPPORT
              bit 1: APM_32_BIT_SUPPORT
              bit 2: APM_IDLE_SLOWS_CLOCK
              bit 3: APM_BIOS_DISABLED
              bit 4: APM_BIOS_DISENGAGED
           3) AC line status
              0x00: Off-line
              0x01: On-line
              0x02: On backup power (BIOS >= 1.1 only)
              0xff: Unknown
           4) Battery status
              0x00: High
              0x01: Low
              0x02: Critical
              0x03: Charging
              0x04: Selected battery not present (BIOS >= 1.2 only)
              0xff: Unknown
           5) Battery flag
              bit 0: High
              bit 1: Low
              bit 2: Critical
              bit 3: Charging
              bit 7: No system battery
              0xff: Unknown
           6) Remaining battery life (percentage of charge):
              0-100: valid
              -1: Unknown
           7) Remaining battery life (time units):
              Number of remaining minutes or seconds
              -1: Unknown
           8) min = minutes; sec = seconds */

	p = page;
	p += sprintf(p, "%s %d.%d 0x%02x 0x%02x 0x%02x 0x%02x %d%% %d %s\n",
		driver_version,
		1, // (apm_info.bios.version >> 8) & 0xff,
		0, // apm_info.bios.version & 0xff,
		0, // apm_info.bios.flags,
		ac_line_status,
		battery_status,
		battery_flag,
		percentage,
		time_units,
		units);

	return p - page;
}

static int __init pcf50605_init(void)
{
	struct proc_dir_entry *standby_proc;

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

	create_proc_read_entry("apm", 0, NULL, ipod_apm_read, NULL);

        /* Install our power off handler.. */
	pm_power_off = pcf_standby_mode;

	return 0;
}

static void __exit pcf50605_exit(void)
{
	pm_power_off = NULL;
	remove_proc_entry("apm", NULL);
	devfs_unregister_chrdev(MISC_MAJOR, "rtc");
	devfs_unregister(rtc_devfs_handle);
}

module_init(pcf50605_init);
module_exit(pcf50605_exit);

MODULE_AUTHOR("Bernard Leach <leachbj@bouncycastle.org>");
MODULE_DESCRIPTION("RTC driver for PCF50605");
MODULE_LICENSE("GPL");

