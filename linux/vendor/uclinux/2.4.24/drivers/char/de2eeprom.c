/*
 *  linux/drivers/misc/de2eeprom.c -- EEPROM driver for the DragonEngine
 *
 *	Copyright (C) 2003 Georges Menie
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
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <asm/MC68VZ328.h>

#define MODULE_NAME "de2eeprom"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Georges Menie");
MODULE_DESCRIPTION("EEPROM through SPI driver for the DragonEngine");

/*
 * Define the EEPROM area managed by the driver
 * it will be a contiguous area of EEPROMSIZE bytes
 * starting at EEPROMOFFSET bytes. The driver will
 * not read or write outside this area.
 */
#ifndef EEPROMPAGESIZE
#define EEPROMPAGESIZE 32
#endif
#ifndef EEPROMOFFSET
#define EEPROMOFFSET 64
#endif
#ifndef EEPROMSIZE
#define EEPROMSIZE (2048-EEPROMOFFSET)
#endif

/*
 * Maximum read/write buffer size for the EEPROM
 *
 * the sizes can be changed as will, but a lock
 * is held during read/write, so if the
 * touchscreen is also used, it is better to
 * keep the size small
 */
#define EEPROMWRMAXSIZE 64
#define EEPROMRDMAXSIZE 128

#undef PDEBUG
#ifdef DE2EEPROM_DEBUG
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

typedef struct device_tag {
	int offs;					/* starting offset in bytes */
	int size;					/* readable/writable size in bytes */
	int rc;						/* read count stat */
	int wc;						/* write count stat */
	int ec;						/* read/write error count stat */
	char wbuf[EEPROMWRMAXSIZE];
	char rbuf[EEPROMRDMAXSIZE];
} Device;

#include "de2spi.h"

/*
 * EEPROM management functions and macros
 */

static void eepromInit(void);
static int eepromWaitReady(void);
static int eepromWriteEnable(int enable);
static int eepromRead(void *buf, unsigned short addr, int len);
static int eepromWrite(const void *buf, unsigned short addr, int len);

/* commands */
#define EE_WREN 0x0006
#define EE_WRDI 0x0004
#define EE_RDSR 0x0500
#define EE_WRSR 0x0100
#define EE_READ 0x0003
#define EE_WRIT 0x0002

#if 0
/* write protect arguments */
#define EE_WPDI 0x00
#define EE_WPQU 0x84
#define EE_WPHA 0x88
#define EE_WPFU 0x8C

static int eepromWriteProtect(int protect);
#endif

#define eepromEnable PDDATA &= ~(1<<6)
#define eepromDisable PDDATA |= (1<<6)

static void eepromInit(void)
{
	PDSEL |= (1 << 6);			// select PD6 as I/O
	PDDIR |= (1 << 6);			// select Port D bit 6 as output
	eepromDisable;				// set eeprom CS high
}

static int eepromWaitReady(void)
{
	int sts, timeout;

	for (timeout = 1000; timeout; --timeout) {
		eepromEnable;
		sts = de2spi_exchange(EE_RDSR, 16);
		eepromDisable;
		if (sts == -1)
			timeout = 0;
		if ((sts & 1) == 0)
			break;
	}

	PDEBUG("%d: %s timeout=%d\n", __LINE__, __FUNCTION__, timeout);
	return timeout == 0;
}

static int eepromWriteEnable(int e)
{
	if (eepromWaitReady())
		return -1;

	eepromEnable;
	(void) de2spi_exchange(e ? EE_WREN : EE_WRDI, 8);
	eepromDisable;

	return 0;
}

#if 0
static int eepromWriteProtect(int p)
{
	unsigned short cmd = EE_WRSR;

	if (eepromWriteEnable(1))	// inside call to eepromWaitReady
		return -1;

	switch (p) {
	case 0:
		cmd |= (unsigned char) EE_WPDI;
		break;
	case 1:
		cmd |= (unsigned char) EE_WPQU;
		break;
	case 2:
		cmd |= (unsigned char) EE_WPHA;
		break;
	case 3:
		cmd |= (unsigned char) EE_WPFU;
		break;
	}
	eepromEnable;
	(void) de2spi_exchange(cmd, 16);
	eepromDisable;

	return 0;
}
#endif

static int eepromRead(void *buf, unsigned short addr, int len)
{
	register int data, i;

	if (eepromWaitReady())
		return -1;

	PDEBUG("%d: %s(buf, %d, %d)\n", __LINE__, __FUNCTION__,
		   (int) addr, len);

	eepromEnable;
	(void) de2spi_exchange(EE_READ, 8);
	(void) de2spi_exchange(addr, 16);
	for (i = 0; i < (len >> 1); ++i) {
		data = de2spi_exchange(0, 16);
		*(unsigned char *) buf++ = (unsigned char) (data >> 8);
		*(unsigned char *) buf++ = (unsigned char) data;
	}
	if (len & 1) {
		*(unsigned char *) buf = (unsigned char) de2spi_exchange(0, 8);
	}
	eepromDisable;

	return 0;
}

static int eepromWrite(const void *buf, unsigned short addr, int len)
{
	unsigned char rbuf[EEPROMWRMAXSIZE];
	register const unsigned char *ptr;
	register int i, rcnt = 0;

	if (len > EEPROMWRMAXSIZE)
		return -1;

  retry:
	if (eepromWriteEnable(1))	// inside call to eepromWaitReady
		return -2;

	PDEBUG("%d: %s(buf, %d, %d)\n", __LINE__, __FUNCTION__,
		   (int) addr, len);

	eepromEnable;
	(void) de2spi_exchange(EE_WRIT, 8);
	(void) de2spi_exchange(addr, 16);
	for (i = 0, ptr = buf; i < (len >> 1); ++i, ptr += 2) {
		(void) de2spi_exchange(((*ptr) << 8) | (*(ptr + 1)), 16);
	}
	if (len & 1) {
		(void) de2spi_exchange(*ptr, 8);
	}
	eepromDisable;

	if (eepromRead(rbuf, addr, len))
		return -3;
	if (memcmp(buf, rbuf, len)) {
		if (++rcnt > 4)
			return -4;
		goto retry;
	}

	return 0;
}

/*
 * driver functions and macros
 */

static ssize_t
de2eeprom_read(struct file *filp, char *buf, size_t count, loff_t * offp)
{
	Device *dev;
	ssize_t ret;
	register int offi, st;

	PDEBUG("%d: %s(filp, buf, %d, %d)\n", __LINE__,
		   __FUNCTION__, (int) count, (int) *offp);

	dev = (Device *) filp->private_data;
	if (!dev)
		return -ENODEV;

	if (!count)
		return 0;

	if (*offp >= dev->size) {
		ret = 0;
	} else {
		offi = *offp;
		if (offi + count > dev->size) {
			count = dev->size - offi;
		}
		if (count > sizeof dev->rbuf)
			count = sizeof dev->rbuf;
		if (de2spi_attach(SPICLK_SYSCLK_16)) {
			return -ERESTARTSYS;
		}
		if ((st = eepromRead(dev->rbuf, offi + dev->offs, count)) != 0) {
			de2spi_detach();
			++dev->ec;
			PDEBUG("%d: %s read error %d\n", __LINE__, __FUNCTION__, st);
			return -EFAULT;
		}
		de2spi_detach();
		if (copy_to_user(buf, dev->rbuf, count)) {
			PDEBUG("%d: %s copy_to_user error\n", __LINE__, __FUNCTION__);
			return -EFAULT;
		}
		PDEBUG("%d: %s %d bytes red at %d\n", __LINE__,
			   __FUNCTION__, count, offi + dev->offs);
		++dev->rc;
		*offp += count;
		ret = count;
	}
	return ret;
}

static ssize_t
de2eeprom_write(struct file *filp, const char *buf, size_t count,
				loff_t * offp)
{
	Device *dev;
	ssize_t ret;
	register int offi, wcnt, t;
	register char *wptr;
	int st;

	PDEBUG("%d: %s(filp, buf, %d, %d)\n", __LINE__,
		   __FUNCTION__, (int) count, (int) *offp);

	dev = (Device *) filp->private_data;
	if (!dev) {
		return -ENODEV;
	}

	if (!count)
		return 0;

	if (*offp >= dev->size)
		return -ENOSPC;

	offi = *offp;

	if (offi + count > dev->size) {
		count = dev->size - offi;
	}

	if (count > EEPROMWRMAXSIZE)
		count = EEPROMWRMAXSIZE;

	if (copy_from_user(dev->wbuf, buf, count)) {
		PDEBUG("%d: %s copy_from_user error\n", __LINE__, __FUNCTION__);
		return -EFAULT;
	}

	wptr = dev->wbuf;
	wcnt = count;

	if (de2spi_attach(SPICLK_SYSCLK_16)) {
		return -ERESTARTSYS;
	}

	if (eepromWriteEnable(1)) {
		PDEBUG("%d: %s writeenable error\n", __LINE__, __FUNCTION__);
		++dev->ec;
		ret = -EFAULT;
		goto out2;
	}

	/* write first partial page */
	if ((t = (offi + dev->offs) % EEPROMPAGESIZE)) {
		t = EEPROMPAGESIZE - t;
		if (t > count)
			t = count;
		if ((st = eepromWrite(wptr, offi + dev->offs, t)) != 0) {
			++dev->ec;
			PDEBUG("%d: %s write error %d\n", __LINE__, __FUNCTION__, st);
			ret = -EFAULT;
			goto out1;
		}
		wptr += t;
		offi += t;
		wcnt -= t;
	}
	/* write full pages */
	while (wcnt > EEPROMPAGESIZE) {
		if ((st =
			 eepromWrite(wptr, offi + dev->offs, EEPROMPAGESIZE)) != 0) {
			++dev->ec;
			PDEBUG("%d: %s write error %d\n", __LINE__, __FUNCTION__, st);
			ret = -EFAULT;
			goto out1;
		}
		wptr += EEPROMPAGESIZE;
		offi += EEPROMPAGESIZE;
		wcnt -= EEPROMPAGESIZE;
	}
	/* write last partial page */
	if (wcnt > 0) {
		if ((st = eepromWrite(wptr, offi + dev->offs, wcnt)) != 0) {
			++dev->ec;
			PDEBUG("%d: %s write error %d\n", __LINE__, __FUNCTION__, st);
			ret = -EFAULT;
			goto out1;
		}
	}

	PDEBUG("%d: %s %d bytes written at %d\n", __LINE__,
		   __FUNCTION__, count, offi + dev->offs);
	++dev->wc;
	*offp += count;
	ret = count;

  out1:
	if (eepromWriteEnable(0)) {
		PDEBUG("%d: %s writeenable error\n", __LINE__, __FUNCTION__);
	}
  out2:
	de2spi_detach();
	return ret;
}

static loff_t de2eeprom_llseek(struct file *filp, loff_t off, int whence)
{
	Device *dev = filp->private_data;
	loff_t ret;

	PDEBUG("%d: %s(filp, %d, %d)\n", __LINE__, __FUNCTION__,
		   (int) off, whence);

	if (!dev) {
		return -ENODEV;
	}

	switch (whence) {
	case 0:					/* SEEK_SET */
		ret = off;
		break;
	case 1:					/* SEEK_CUR */
		ret = filp->f_pos + off;
		break;
	case 2:					/* SEEK_END */
		ret = dev->size + off;
		break;
	default:
		return -EINVAL;
	}
	if (ret < 0)
		return -EINVAL;
	filp->f_pos = ret;
	return ret;
}

static Device dev;

static int de2eeprom_open(struct inode *inode, struct file *filp)
{
	if (!filp->private_data) {
		filp->private_data = &dev;
	}

	return 0;
}

static int de2eeprom_release(struct inode *inode, struct file *filp)
{
	return 0;
}

struct file_operations de2eeprom_fops = {
	owner:THIS_MODULE,
	read:de2eeprom_read,
	write:de2eeprom_write,
	llseek:de2eeprom_llseek,
	open:de2eeprom_open,
	release:de2eeprom_release,
};

/*
 * The proc entry displays the managed area and some counters
 * rd : number of successful read operations
 * wr : number of successful write operations
 * err: number of errors during read or write
 */
static int
de2eeprom_proc_read(char *buf, char **start, off_t offset, int count,
					int *eof, void *data)
{
	int len = 0;

	len +=
		sprintf(buf + len,
				" offs  size         rd         wr        err\n");
	len +=
		sprintf(buf + len, "%5d %5d %10d %10d %10d\n", dev.offs, dev.size,
				dev.rc, dev.wc, dev.ec);

	*eof = 1;
	return len;
}

static struct miscdevice de2eeprom = {
	NVRAM_MINOR, MODULE_NAME, &de2eeprom_fops
};

static int __init de2eeprom_init(void)
{
	if (misc_register(&de2eeprom) < 0)
		printk(KERN_ALERT
			   "%s: error registering eeprom driver.\n", MODULE_NAME);

	create_proc_read_entry(MODULE_NAME, 0, NULL, de2eeprom_proc_read,
						   NULL);

	memset(&dev, 0, sizeof(Device));
	dev.offs = EEPROMOFFSET;
	dev.size = EEPROMSIZE;

	eepromInit();

	printk(KERN_INFO
		   "%s: eeprom driver installed (c,10,%d).\n", MODULE_NAME,
		   de2eeprom.minor);

	return 0;
}

static void __exit de2eeprom_cleanup(void)
{
	remove_proc_entry(MODULE_NAME, NULL);
	misc_deregister(&de2eeprom);
}

module_init(de2eeprom_init);
module_exit(de2eeprom_cleanup);
