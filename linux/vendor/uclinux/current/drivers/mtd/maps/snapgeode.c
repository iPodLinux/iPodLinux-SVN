/****************************************************************************/

/*
 *      snapgeode.c -- mappings for SnapGear GEODE based boards
 *
 *      (C) Copyright 2000-2003, Greg Ungerer (gerg@snapgear.com)
 *      (C) Copyright 2001-2003, SnapGear (www.snapgear.com)
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
#ifdef CONFIG_MTD_CFI_INTELEXT
/****************************************************************************/

/*
 *	Intel FLASH setup. This is the only flash device, it is the entire
 *	non-volatile storage (no IDE CF or hard drive).
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
		name: "SnapGear kernel",
		offset: 0,
		size: 0x000e0000
	},
	{
		name: "SnapGear filesystem",
		offset: 0x00100000,
	},
	{
		name: "SnapGear config",
		offset: 0x000e0000,
		size: 0x00020000
	},
	{
		name: "SnapGear Intel/StrataFlash",
		offset: 0
	},
	{
		name: "SnapGear BIOS Config",
		offset: 0x007e0000,
		size: 0x00020000
	},
	{
		name: "SnapGear BIOS",
		offset: 0x007e0000,
		size: 0x00020000
	},
};

#define	PROBE	"cfi_probe"

/****************************************************************************/
#else
/****************************************************************************/

/*
 *	If only an AMD flash is fitted then it is the BIOS/boot loader.
 *	Primary non-volatile storage must be via some ither IDE mechanism
 *	(either compact flash [CF] or real hard drive).
 */

static struct map_info sg_map = {
	name: "SnapGear AMD/Flash",
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
		name: "SnapGear BIOS config",
		offset: 0x00000000,
		size: 0x00010000
	},
	{
		name: "SnapGear BIOS",
		offset: 0x00010000,
		size: 0x00010000
	},
	{
		name: "SnapGear AMD/Flash",
		offset: 0
	},
};

#define	PROBE	"jedec_probe"

/****************************************************************************/
#endif
/****************************************************************************/

#define NUM_PARTITIONS	(sizeof(sg_partitions)/sizeof(sg_partitions[0]))

/****************************************************************************/

#ifdef CONFIG_MTD_CFI_INTELEXT

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
	for (b = 0; (b < sg_partitions[3].size); b += 0x100000) {
		cfi_send_gen_cmd(0xff, 0x55, b, &sg_map, cfi,
			cfi->device_type, NULL);
	}
	return NOTIFY_OK;
}

static struct notifier_block sg_notifier_block = {
	sg_reboot_notifier, NULL, 0
};

#endif /* CONFIG_MTD_CFI_INTELEXT */

/****************************************************************************/

int __init sg_init(void)
{
	printk("SNAPGEAR: MTD BIOS setup\n");

	/*
	 *	On the GEODE the ROM CS stays mapped into high memory.
	 *	So we look for it at the top of the 32bit address space.
	 */
	sg_map.map_priv_1 = (unsigned long)
		ioremap_nocache(0xff800000, 0x800000);
	if (!sg_map.map_priv_1) {
		printk("SNAPGEAR: failed to ioremap() ROMCS\n");
		return -EIO;
	}

	if ((sg_mtd = do_map_probe(PROBE, &sg_map)) == NULL)
		return -ENXIO;

	printk(KERN_NOTICE "SNAPGEAR: %s device size = %dK\n",
		sg_mtd->name, sg_mtd->size>>10);

	sg_mtd->module = THIS_MODULE;

#ifdef CONFIG_MTD_CFI_INTELEXT
	sg_partitions[1].size = sg_mtd->size -
		(sg_partitions[1].offset + sg_mtd->erasesize);
	if (sg_mtd->size > 0x800000) {
		sg_partitions[4].offset += sg_mtd->size - 0x800000;
		sg_partitions[5].offset += sg_mtd->size - 0x800000;
	}
	register_reboot_notifier(&sg_notifier_block);
#ifndef CONFIG_BLK_DEV_INITRD
	ROOT_DEV = MKDEV(MTD_BLOCK_MAJOR, 1);
#endif
#else
	/*
	 * Setup the partitions, second from top 64KiB and top 64KiB
	 */
	if (sg_mtd->size > 0x20000) {
		sg_partitions[0].offset += sg_mtd->size - 0x20000;
		sg_partitions[1].offset += sg_mtd->size - 0x20000;
	}
#endif /* !CONFIG_MTD_CFI_INTELEXT */

	return add_mtd_partitions(sg_mtd, sg_partitions, NUM_PARTITIONS);
}

/****************************************************************************/

void __exit sg_cleanup(void)
{
#ifdef CONFIG_MTD_CFI_INTELEXT
	unregister_reboot_notifier(&sg_notifier_block);
#endif
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
MODULE_DESCRIPTION("SnapGear/GEODE flash support");

/****************************************************************************/
