/****************************************************************************/

/*
 *      advanta.c -- mappings for Adtran Advanta 3110 board.
 *
 *      (C) Copyright 2002, SnapGear Inc (www.snapgear.com)
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

#define ADVANTA_WINDOW_MAXSIZE	0x00200000
#define ADVANTA_BUSWIDTH		1

/*
 *	PAR masks and shifts, assuming 64K pages.
 */
#define SC520_PAR_ADDR_MASK	0x00003fff
#define SC520_PAR_ADDR_SHIFT	16
#define SC520_PAR_TO_ADDR(par) \
	(((par)&SC520_PAR_ADDR_MASK) << SC520_PAR_ADDR_SHIFT)

#define SC520_PAR_SIZE_MASK	0x01ffc000
#define SC520_PAR_SIZE_SHIFT	2
#define SC520_PAR_TO_SIZE(par) \
	((((par)&SC520_PAR_SIZE_MASK) << SC520_PAR_SIZE_SHIFT) + (64*1024))

#define SC520_PAR(cs, addr, size) \
	((cs) | \
	((((size)-(64*1024)) >> SC520_PAR_SIZE_SHIFT) & SC520_PAR_SIZE_MASK) | \
	(((addr) >> SC520_PAR_ADDR_SHIFT) & SC520_PAR_ADDR_MASK))

#define SC520_PAR_BOOTCS	0x8a000000
#define	SC520_PAR_ROMCS1	0xaa000000
#define SC520_PAR_ROMCS2	0xca000000	/* Cache disabled, 64K page */

static struct mtd_info *advanta_mtd;

/****************************************************************************/

static __u8 advanta_read8(struct map_info *map, unsigned long ofs)
{
	return(readb(map->map_priv_1 + ofs));
}

static __u16 advanta_read16(struct map_info *map, unsigned long ofs)
{
	return(readw(map->map_priv_1 + ofs));
}

static __u32 advanta_read32(struct map_info *map, unsigned long ofs)
{
	return(readl(map->map_priv_1 + ofs));
}

static void advanta_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

static void advanta_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	writeb(d, map->map_priv_1 + adr);
}

static void advanta_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	writew(d, map->map_priv_1 + adr);
}

static void advanta_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	writel(d, map->map_priv_1 + adr);
}

static void advanta_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

/****************************************************************************/

static struct map_info advanta_map = {
	name: "Adtran Advanta",
	size: 256*1024,
	buswidth: ADVANTA_BUSWIDTH,
	read8: advanta_read8,
	read16: advanta_read16,
	read32: advanta_read32,
	copy_from: advanta_copy_from,
	write8: advanta_write8,
	write16: advanta_write16,
	write32: advanta_write32,
	copy_to: advanta_copy_to
};

static struct mtd_partition advanta_partitions[] = {
	{
		name: "All",
		offset: 0
	},
};

#define NUM_PARTITIONS \
	(sizeof(advanta_partitions)/sizeof(advanta_partitions[0]))

/****************************************************************************/

#if LINUX_VERSION_CODE < 0x20212 && defined(MODULE)
#define init_nettel init_module
#define cleanup_nettel cleanup_module
#endif

mod_init_t init_advanta(void)
{
	static void *mmcrp = NULL;
	volatile unsigned long *par;
	unsigned long addr, maxsize;
	int rc;

	mmcrp = (void *) ioremap_nocache(0xfffef000, 4096);
	if (mmcrp == NULL) {
		printk("Advanta: failed to disable MMCR cache??\n");
		return(-EIO);
	}

	/* Set up DoC CS */
	*(unsigned long *)(mmcrp + 0xa8) = 0x440400d0;

	/* Set up OTP EPROM CS */
	par = (volatile unsigned long *)(mmcrp + 0xc0);
	addr = 0x20000000;
	maxsize = ADVANTA_WINDOW_MAXSIZE;

	*par = SC520_PAR(SC520_PAR_BOOTCS, addr, maxsize);
	__asm__ ("wbinvd");

	advanta_map.map_priv_1 = (unsigned long)ioremap_nocache(addr, maxsize);
	if (!advanta_map.map_priv_1) {
		printk("Advanta: failed to ioremap() BOOTCS\n");
		return(-EIO);
	}

	advanta_mtd = do_map_probe("map_rom", &advanta_map);
	if (!advanta_mtd) {
		iounmap((void *)advanta_map.map_priv_1);
		return(-ENXIO);
	}

	rc = add_mtd_partitions(advanta_mtd, advanta_partitions,
		NUM_PARTITIONS);

	return rc;
}

/****************************************************************************/

mod_exit_t cleanup_advanta(void)
{
	if (advanta_mtd) {
		del_mtd_partitions(advanta_mtd);
		map_destroy(advanta_mtd);
	}
	if (advanta_map.map_priv_1) {
		iounmap((void *)advanta_map.map_priv_1);
		advanta_map.map_priv_1 = 0;
	}
}

/****************************************************************************/

module_init(init_advanta);
module_exit(cleanup_advanta);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Philip Craig <philipc@snapgear.com>");
MODULE_DESCRIPTION("Adtran Advanta 3110 OTP EPROM support");

/****************************************************************************/
