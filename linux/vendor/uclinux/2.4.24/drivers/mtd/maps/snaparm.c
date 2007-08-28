/****************************************************************************/

/*
 *      snaparm.c -- mappings for SnapGear ARM based boards
 *
 *      (C) Copyright 2000-2003, Greg Ungerer (gerg@snapgear.com)
 *      (C) Copyright 2001-2003, SnapGear (www.snapgear.com)
 *
 *	I expect most SnapGear ARM based boards will have similar
 *	flash arrangements. So this map driver can handle them all.
 */

/****************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/cfi.h>
#include <linux/reboot.h>
#include <asm/io.h>

/****************************************************************************/

static struct mtd_info *sg_mtd;

/****************************************************************************/

/*
 *	Define physical addresss flash is mapped at. Will be different on
 *	different boards.
 */
#if CONFIG_ARCH_SE4000
#define	FLASH_ADDR	0x50000000		/* Physical flash address */
#define	FLASH_SIZE	0x00800000		/* Maximum flash size */
#endif

#if CONFIG_ARCH_KS8695
#define	FLASH_ADDR	0x02000000		/* Physical flash address */
#define	FLASH_SIZE	0x00800000		/* Maximum flash size */
#endif

/****************************************************************************/

static __u8 sg_read8(struct map_info *map, unsigned long ofs)
{
	return(readb(map->map_priv_1 + ofs));
}

static __u16 sg_read16(struct map_info *map, unsigned long ofs)
{
	return(readw(map->map_priv_1 + ofs));
}

static __u32 sg_read32(struct map_info *map, unsigned long ofs)
{
	return(readl(map->map_priv_1 + ofs));
}

static void sg_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

static void sg_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	writeb(d, map->map_priv_1 + adr);
}

static void sg_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	writew(d, map->map_priv_1 + adr);
}

static void sg_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	writel(d, map->map_priv_1 + adr);
}

static void sg_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

/****************************************************************************/

/*
 *	Intel FLASH setup. This is the only flash device, it is the entire
 *	non-volatile storage (no IDE CF or hard drive or anything else).
 */

static struct map_info sg_map = {
	name: "SnapGear Intel/StrataFlash",
	size: 0x800000,
	buswidth: 1,
	read8: sg_read8,
	read16: sg_read16,
	read32: sg_read32,
	copy_from: sg_copy_from,
	write8: sg_write8,
	write16: sg_write16,
	write32: sg_write32,
	copy_to: sg_copy_to
};

static struct mtd_partition sg_partitions[] = {
	{
		name: "SnapGear Boot Loader",
		offset: 0,
		size: 0x00020000
	},
	{
		name: "SnapGear non-volatile configuration",
		offset: 0x00020000,
		size: 0x00020000
	},
	{
		name: "SnapGear image",
		offset: 0x40000,
		size: 0x003c0000
	},
};

/****************************************************************************/

#define NUM_PARTITIONS	(sizeof(sg_partitions)/sizeof(sg_partitions[0]))

/****************************************************************************/

/*
 *	Set the Intel flash back to read mode. Sometimes MTD leaves the
 *	flash in status mode, and if you reboot there is no code to
 *	execute (the flash devices do not get a RESET) :-(
 */
static int sg_reboot_notifier(struct notifier_block *nb, unsigned long val, void *v)
{
	struct cfi_private *cfi = sg_map.fldrv_priv;
	unsigned long b;

	/* Make sure all FLASH chips are put back into read mode */
	for (b = 0; (b < sg_partitions[2].size); b += 0x400000) {
		cfi_send_gen_cmd(0xff, 0x55, b, &sg_map, cfi,
			cfi->device_type, NULL);
	}
	return NOTIFY_OK;
}

static struct notifier_block sg_notifier_block = {
	sg_reboot_notifier, NULL, 0
};

/****************************************************************************/

/*
 *	Find the MTD device with the given name.
 */

static int sg_getmtdindex(char *name)
{
	struct mtd_info *mtd;
	int i, index;

	index = -1;
	for (i = 0; (i < MAX_MTD_DEVICES); i++) {
		mtd = get_mtd_device(NULL, i);
		if (mtd) {
			if (strcmp(mtd->name, name) == 0)
				index = mtd->index;
			put_mtd_device(mtd);
			if (index >= 0)
				break;
		}
	}
	return index;
}

/****************************************************************************/

int __init sg_init(void)
{
	int index, rc;

	printk("SNAPGEAR: MTD flash setup\n");

	/*
	 *	Map flash into our virtual address space.
	 */
	sg_map.map_priv_1 = (unsigned long)
		ioremap_nocache(FLASH_ADDR, FLASH_SIZE);
	if (!sg_map.map_priv_1) {
		printk("SNAPGEAR: failed to ioremap() flash\n");
		return -EIO;
	}

	if ((sg_mtd = do_map_probe("cfi_probe", &sg_map)) == NULL)
		return -ENXIO;

	printk(KERN_NOTICE "SNAPGEAR: %s device size = %dK\n",
		sg_mtd->name, sg_mtd->size>>10);

	sg_mtd->module = THIS_MODULE;
	register_reboot_notifier(&sg_notifier_block);
	rc =  add_mtd_partitions(sg_mtd, sg_partitions, NUM_PARTITIONS);

#ifndef CONFIG_BLK_DEV_INITRD
	index = sg_getmtdindex("SnapGear image");
	if (index >= 0)
		ROOT_DEV = MKDEV(MTD_BLOCK_MAJOR, index);
#endif

	return rc;
}

/****************************************************************************/

void __exit sg_cleanup(void)
{
	unregister_reboot_notifier(&sg_notifier_block);
	if (sg_mtd) {
		del_mtd_partitions(sg_mtd);
		map_destroy(sg_mtd);
	}
	if (sg_map.map_priv_1) {
		iounmap((void *)sg_map.map_priv_1);
		sg_map.map_priv_1 = 0;
	}
}

/****************************************************************************/

module_init(sg_init);
module_exit(sg_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Greg Ungerer <gerg@snapgear.com>");
MODULE_DESCRIPTION("SnapGear/ARM flash support");

/****************************************************************************/
