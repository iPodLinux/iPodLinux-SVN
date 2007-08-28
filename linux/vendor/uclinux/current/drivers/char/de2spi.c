/*
 *  linux/drivers/misc/de2spi.c -- SPI driver for the DragonEngine
 *
 *	Copyright (C) 2003 Georges Menie
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

/*
 * This driver exports 3 functions:
 *
 * int de2spi_attach(int div)
 *   call this function to check if the spi is free, and
 *   to lock it until you call detach.
 *
 * void de2spi_detach(void)
 *   to release the spi
 *
 * unsigned short de2spi_exchange(unsigned short data, int bitcount)
 *   call this function to transmit/receive data though the spi
 *   once it is attached
 *
 * This locking mechanism is mandatory on the DragonEngine board,
 * the touchscreen controller and the EEPROM are connected to SPI2
 * so a lock should be obtained before reading them
 *
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
#include <asm/uaccess.h>
#include <asm/MC68VZ328.h>

/* MC68VZ328.h does not define the 'canonical' names
 * for the SPI2 registers...
 */

#define SPICONT2 SPIMCONT
#define SPIDATA2 SPIMDATA

#define MODULE_NAME "de2spi"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Georges Menie");
MODULE_DESCRIPTION("SPI2 driver for the DragonEngine");

#undef PDEBUG
#ifdef DE2SPI_DEBUG
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
#define BIT(x) (1<<(x))

static unsigned long spilock;
static unsigned short cdiv;

/* div is the desired speed (see de2spi.h) */
int de2spi_attach(int div)
{
	if (test_and_set_bit(0, &spilock))
		return -EBUSY;

	/* store sysclk divisor */
	cdiv = ((div & 0x7) << 13);

	PDEBUG("de2spi attached\n");
	return 0;
}

void de2spi_detach(void)
{
	clear_bit(0, &spilock);
	PDEBUG("de2spi detached\n");
}

int de2spi_exchange(unsigned short data, int bitcount)
{
	int timeout;

	/* setup xfer parameters + enable */
	SPICONT2 = cdiv | (((bitcount) - 1) & 15) | BIT(9);
	SPIDATA2 = data;

	/* start xfer */
	SPICONT2 |= BIT(8);

	/* wait ready */
	for (timeout = 30; timeout && (SPICONT2 & BIT(7)) == 0; --timeout)
		/* nothing */ ;

	/* read data received */
	data = SPIDATA2;

	/* disable spi */
	SPICONT2 = 0;

	if (timeout == 0)
		return -1;

	return data;
}

static int __init de2spi_init(void)
{
	/* spilock is used to prevent concurrent access
	 * for the eeprom or the touchscreen controller
	 */
	spilock = 0;

	/* disable spi */
	SPICONT2 = 0;

	/* dedicated func on PE0-2 */
	PESEL &= ~(BIT(2) | BIT(1) | BIT(0));

	printk(KERN_INFO "%s: spi2 driver installed.\n", MODULE_NAME);
	return 0;
}

module_init(de2spi_init);
