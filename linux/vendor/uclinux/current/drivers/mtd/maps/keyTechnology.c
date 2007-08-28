/****************************************************************************/
/*
 * Flash memory access on Key Technology devices
 * Copyright (C) Key Technology, Inc. <ktreis@keyww.com>
 *
 * Mostly hijacked from nettel-uc.c
 * Copyright (C) Lineo, <davidm@moreton.com.au>
 */
/****************************************************************************/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <linux/fs.h>

#include <asm/io.h>
#include <asm/delay.h>

/****************************************************************************/

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

#define SIZE_2MB	(2 * 1024 * 1024)
#define SIZE_8MB	(8 * 1024 * 1024)

/*
 *	If you set AUTO_PROBE,  then we will just use the biggest device
 *	we can find
 */
#undef AUTO_PROBE

/****************************************************************************/

static __u8 keyTechnology_read8(struct map_info *map, unsigned long ofs)
{
	return *(__u8 *)(map->map_priv_1 + ofs);
}

static __u16 keyTechnology_read16(struct map_info *map, unsigned long ofs)
{
	return *(__u16 *)(map->map_priv_1 + ofs);
}

static __u32 keyTechnology_read32(struct map_info *map, unsigned long ofs)
{
	return *(__u32 *)(map->map_priv_1 + ofs);
}

static void keyTechnology_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void keyTechnology_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*(__u8 *)(map->map_priv_1 + adr) = d;
}

static void keyTechnology_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*(__u16 *)(map->map_priv_1 + adr) = d;
}

static void keyTechnology_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*(__u32 *)(map->map_priv_1 + adr) = d;
}

static void keyTechnology_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

/****************************************************************************/

static struct map_info keyTechnology_flash_map = {
	name:		"Flash",
	read8:		keyTechnology_read8,
	read16:		keyTechnology_read16,
	read32:		keyTechnology_read32,
	copy_from:	keyTechnology_copy_from,
	write8:		keyTechnology_write8,
	write16:	keyTechnology_write16,
	write32:	keyTechnology_write32,
	copy_to:	keyTechnology_copy_to,
};

static struct map_info keyTechnology_ram_map = {
	name:		"RAM",
	read8:		keyTechnology_read8,
	read16:		keyTechnology_read16,
	read32:		keyTechnology_read32,
	copy_from:	keyTechnology_copy_from,
	write8:		keyTechnology_write8,
	write16:	keyTechnology_write16,
	write32:	keyTechnology_write32,
	copy_to:	keyTechnology_copy_to,
};

static struct mtd_info *ram_mtdinfo;
static struct mtd_info *flash_mtdinfo;

/****************************************************************************/

static struct mtd_partition keyTechnology_romfs[] = {
	{ name: "romfs", offset: 0 }
};

/****************************************************************************/
/*
 *	The layout of our flash,  note the order of the names,  this
 *	means we use the same major/minor for the same purpose on all
 *	layouts (when possible)
 */

static struct mtd_partition keyTechnology_2mb[] = {
	{ name: "bootloader",	offset: 0x00000000, size: 0x00004000 },
	{ name: "ethernet",		offset: 0x00004000, size: 0x00002000 },
	{ name: "cmdline",		offset: 0x00006000, size: 0x00002000 },
	{ name: "kernel+romfs",	offset: 0x00040000, size: 0x000c0000 },
	{ name: "config",		offset: 0x00100000, size: 0x00100000 }
};

/****************************************************************************/
/*
 * Find the MTD device with the given name
 */

static struct mtd_info *get_mtd_named(char *name)
{
	int i;
	struct mtd_info *mtd;

	for (i = 0; i < MAX_MTD_DEVICES; i++) {
		mtd = get_mtd_device(NULL, i);
		if (mtd) {
			if (strcmp(mtd->name, name) == 0)
				return(mtd);
			put_mtd_device(mtd);
		}
	}
	return(NULL);
}

/****************************************************************************/

static int
keyTechnology_point(struct mtd_info *mtd, loff_t from, size_t len,
		size_t *retlen, u_char **mtdbuf)
{
	struct map_info *map = (struct map_ptr *) mtd->priv;
	*mtdbuf = map->map_priv_1 + from;
	*retlen = len;
	return(0);
}

/****************************************************************************/

static int __init
keyTechnology_probe(int ram, unsigned long addr, int size, int buswidth)
{
	struct mtd_info *mymtd;
	struct map_info *map_ptr;

	if (ram)
		map_ptr = &keyTechnology_ram_map;
	else
		map_ptr = &keyTechnology_flash_map;

	map_ptr->buswidth = buswidth;
	map_ptr->map_priv_2 = addr;
	map_ptr->size = size;

	printk(KERN_NOTICE "Key MTD %s probe(0x%lx,%d,%d): %lx at %lx\n",
			ram ? "ram" : "flash",
			addr, size, buswidth, map_ptr->size, map_ptr->map_priv_2);

	map_ptr->map_priv_1 = (unsigned long)
			ioremap_nocache(map_ptr->map_priv_2, map_ptr->size);

	if (!map_ptr->map_priv_1) {
		printk("Failed to ioremap_nocache\n");
		return -EIO;
	}

	if (!ram) {
		mymtd = do_map_probe("cfi_probe", map_ptr);
	} else {
		mymtd = do_map_probe("map_ram", map_ptr);
	}

	if (!mymtd) {
		iounmap((void *)map_ptr->map_priv_1);
		return -ENXIO;
	}
		
	mymtd->module = THIS_MODULE;
	mymtd->point = keyTechnology_point;
	mymtd->priv = map_ptr;

	if (ram) {
		ram_mtdinfo = mymtd;
		add_mtd_partitions(mymtd, keyTechnology_romfs, NB_OF(keyTechnology_romfs));
		return(0);
	}

	flash_mtdinfo = mymtd;
	switch (size) {
	case SIZE_2MB:
		add_mtd_partitions(mymtd, keyTechnology_2mb, NB_OF(keyTechnology_2mb));
		break;
	}

	return 0;
}

/****************************************************************************/

int __init keyTechnology_mtd_init(void)
{
	int rc = -1;
	struct mtd_info *mtd;
	extern char _ebss;

#if defined(CONFIG_FLASH2MB) || defined(AUTO_PROBE)
	if (rc != 0)
		rc = keyTechnology_probe(0, 0xffe00000, SIZE_2MB, 2);
#endif

	/*
	 * Map in the filesystem from RAM last so that,  if the filesystem
	 * is not in RAM for some reason we do not change the minor/major
	 * for the flash devices
	 */
#ifndef CONFIG_ROMFS_FROM_ROM
	if (0 != keyTechnology_probe(1, (unsigned long) &_ebss,
			PAGE_ALIGN(* (unsigned long *)((&_ebss) + 8)), 4))
		printk("Failed to probe RAM filesystem\n");
#else
	{
		unsigned long start_area;
		unsigned char *sp, *ep;
		size_t len;

		start_area = (unsigned long) &_ebss;

		if (strncmp((char *) start_area, "-rom1fs-", 8) != 0) {
			mtd = get_mtd_named("Image");
			if (mtd && mtd->point) {
				if ((*mtd->point)(mtd, 0, mtd->size, &len, &sp) == 0) {
					ep = sp + len;
					while (sp < ep && strncmp(sp, "-rom1fs-", 8) != 0)
						sp++;
					if (sp < ep)
						start_area = (unsigned long) sp;
				}
			}
			if (mtd)
				put_mtd_device(mtd);
		}
		if (0 != keyTechnology_probe(1, start_area,
				PAGE_ALIGN(* (unsigned long *)(start_area + 8)), 4))
			printk("Failed to probe RAM filesystem\n");
	}
#endif
	
	mtd = get_mtd_named("romfs");
	if (mtd) {
		ROOT_DEV = MKDEV(MTD_BLOCK_MAJOR, mtd->index);
		put_mtd_device(mtd);
	}

	return(rc);
}

/****************************************************************************/

static void __exit keyTechnology_mtd_cleanup(void)
{
	if (flash_mtdinfo) {
		del_mtd_partitions(flash_mtdinfo);
		map_destroy(flash_mtdinfo);
		flash_mtdinfo = NULL;
	}
	if (ram_mtdinfo) {
		del_mtd_partitions(ram_mtdinfo);
		map_destroy(ram_mtdinfo);
		ram_mtdinfo = NULL;
	}
	if (keyTechnology_ram_map.map_priv_1) {
		iounmap((void *)keyTechnology_ram_map.map_priv_1);
		keyTechnology_ram_map.map_priv_1 = 0;
	}
	if (keyTechnology_flash_map.map_priv_1) {
		iounmap((void *)keyTechnology_flash_map.map_priv_1);
		keyTechnology_flash_map.map_priv_1 = 0;
	}
}

/****************************************************************************/

module_init(keyTechnology_mtd_init);
module_exit(keyTechnology_mtd_cleanup);

/****************************************************************************/
