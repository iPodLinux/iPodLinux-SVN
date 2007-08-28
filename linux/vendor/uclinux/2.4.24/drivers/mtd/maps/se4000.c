#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/cfi.h>
#include <linux/ioport.h>
#include <asm/io.h>

#define WINDOW_ADDR 	0x50000000
#define BUSWIDTH 	2
#define WINDOW_SIZE	0x01000000

#ifndef __ARMEB__
#define	B0(h)	((h) & 0xFF)
#define	B1(h)	(((h) >> 8) & 0xFF)
#else
#define	B0(h)	(((h) >> 8) & 0xFF)
#define	B1(h)	((h) & 0xFF)
#endif

static __u16 se4000_read16(struct map_info *map, unsigned long ofs)
{
    return *(__u16 *)(map->map_priv_1 + ofs);
}

/*
 * The IXP425 expansion bus only allows 16-bit wide acceses
 * when attached to a 16-bit wide device (such as the 28F128J3A),
 * so we can't use a memcpy_fromio as it does byte acceses.
 */
static void se4000_copy_from(struct map_info *map, void *to,
    unsigned long from, ssize_t len)
{
	int i;
	u8 *dest = (u8*)to;
	u16 *src = (u16 *)(map->map_priv_1 + from);
	u16 data;

	for (i = 0; i < (len / 2); i++) {
		data = src[i];
		dest[i * 2] = B0(data);
		dest[i * 2 + 1] = B1(data);
	}

	if (len & 1)
		dest[len - 1] = B0(src[i]);
}

static void se4000_write16(struct map_info *map, __u16 d, unsigned long adr)
{
    *(__u16 *)(map->map_priv_1 + adr) = d;
}

static struct map_info se4000_map = {
	name: 		"SnapGear SE4000 Flash",
	buswidth: 	BUSWIDTH,
	read16:		se4000_read16,
	copy_from:	se4000_copy_from,
	write16:	se4000_write16,
};

static struct mtd_partition *parsed_parts;

#ifdef CONFIG_MTD_CFI_INTELEXT
/*
 * Set the Intel flash back to read mode as MTD may leave it in command mode
 */

static int se4000_reboot_notifier(
	struct notifier_block *nb,
	unsigned long val,
	void *v)
{
	struct cfi_private *cfi = se4000_map.fldrv_priv;
	int i;
	
	for (i = 0; cfi && i < cfi->numchips; i++)
		cfi_send_gen_cmd(0xff, 0x55, cfi->chips[i].start, &se4000_map,
			cfi, cfi->device_type, NULL);

	return NOTIFY_OK;
}

static struct notifier_block se4000_notifier_block = {
	se4000_reboot_notifier, NULL, 0
};

#endif


static struct mtd_info *se4000_mtd;
static struct resource *mtd_resource;

static void se4000_exit(void)
{
    if (se4000_mtd) {
	del_mtd_partitions(se4000_mtd);
	map_destroy(se4000_mtd);
    }
    if (se4000_map.map_priv_1)
	iounmap((void *)se4000_map.map_priv_1);
    if (mtd_resource)
	release_mem_region(WINDOW_ADDR, WINDOW_SIZE);
  
    if (parsed_parts)
	kfree(parsed_parts);

    /* Disable flash write */
    *IXP425_EXP_CS0 &= ~IXP425_FLASH_WRITABLE;
}

static int __init se4000_init(void)
{
    int res, npart;

    /* Enable flash write */
    *IXP425_EXP_CS0 |= IXP425_FLASH_WRITABLE;

    se4000_map.map_priv_1 = 0;
    mtd_resource = request_mem_region(WINDOW_ADDR, WINDOW_SIZE, "SnapGear SE4000");
    if (!mtd_resource) {
	printk(KERN_ERR "SE4000: request_mem_region() failed\n" );
	res = -ENOMEM;
	goto Error;
    }

    se4000_map.map_priv_1 = (unsigned long) ioremap(WINDOW_ADDR, WINDOW_SIZE);
    if (!se4000_map.map_priv_1) {
	printk(KERN_ERR "SE4000: ioremap() failed\n");
	res = -EIO;
	goto Error;
    }
    se4000_map.size = WINDOW_SIZE;

    /* 
     * Probe for the CFI complaint chip
     * suposed to be 28F128J3A
     */
    se4000_mtd = do_map_probe("cfi_probe", &se4000_map);
    if (!se4000_mtd) {
	res = -ENXIO;
	goto Error;
    }
    se4000_mtd->module = THIS_MODULE;
   
    /* Try to parse RedBoot partitions */
    npart = parse_redboot_partitions(se4000_mtd, &parsed_parts);
    if (npart > 0) {
	/* found "npart" RedBoot partitions */
	res = add_mtd_partitions(se4000_mtd, parsed_parts, npart);
    } else {
	res = -EIO;
    }

    if (res)
	goto Error;

#ifdef CONFIG_MTD_CFI_INTELEXT
	register_reboot_notifier(&se4000_notifier_block);
#endif

    return res;
Error:
    se4000_exit();
    return res;
}

module_init(se4000_init);
module_exit(se4000_exit);

MODULE_DESCRIPTION("MTD map driver for SnapGear SE4000");

