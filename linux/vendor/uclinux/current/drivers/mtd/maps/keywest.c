/*
 * Flash memory access on SH/KeyWest based devices
 * (C) 2001 Lineo
 * (C) 2002,2003 SnapGear, davidm@snapgear.com
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>
#include <asm/delay.h>

#ifndef CONFIG_SUPERH
#error This is for SH architecture only
#endif

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

static __u8 keywest_read8(struct map_info *map, unsigned long ofs)
{
	return *(__u8 *)(map->map_priv_1 + ofs);
}

static __u16 keywest_read16(struct map_info *map, unsigned long ofs)
{
	return *(__u16 *)(map->map_priv_1 + ofs);
}

static __u32 keywest_read32(struct map_info *map, unsigned long ofs)
{
	return *(__u32 *)(map->map_priv_1 + ofs);
}

static void keywest_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void keywest_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*(__u8 *)(map->map_priv_1 + adr) = d;
}

static void keywest_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*(__u16 *)(map->map_priv_1 + adr) = d;
}

static void keywest_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*(__u32 *)(map->map_priv_1 + adr) = d;
}

static void keywest_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

static struct map_info keywest_map = {
	name:		"KeyWest/BigSur flash",
	read8:		keywest_read8,
	read16:		keywest_read16,
	read32:		keywest_read32,
	copy_from:	keywest_copy_from,
	write8:		keywest_write8,
	write16:	keywest_write16,
	write32:	keywest_write32,
	copy_to:	keywest_copy_to,
	size:       0x00400000,			/* 32 Mbytes */
};

static struct mtd_partition keywest_partitions[] = {
	{
		name: "Bootloader",
		offset: 0x00000000,
		size:   0x00080000
	},
	{
		name: "Bootargs",
		offset: 0x00080000,
		size:   0x00080000
	},
	{
		name: "Config",
		offset: 0x00180000,
		size:   0x00080000
	},
	{
		name: "Spare",
		offset: 0x00100000,
		size:   0x00080000
	},
	{
		name: "kernel/root",
		offset: 0x00200000,
		size:   0x00e00000
	},
	{
		name: "Full flash",
		offset: 0
	}
};

static struct mtd_partition *parsed_parts;
static struct mtd_info *mymtd;

static int __init keywest_probe(unsigned long addr, int buswidth)
{
	keywest_map.buswidth = buswidth;
	keywest_map.map_priv_2 = addr;

	printk(KERN_NOTICE "KeyWest flash probe(0x%x,%d): %lx at %lx\n",
			addr, buswidth, keywest_map.size, keywest_map.map_priv_2);

	keywest_map.map_priv_1 = (unsigned long)
			ioremap_nocache(keywest_map.map_priv_2, keywest_map.size);

	if (!keywest_map.map_priv_1) {
		printk("Failed to ioremap_nocache\n");
		return -EIO;
	}
	mymtd = do_map_probe("cfi_probe", &keywest_map);
	if(!mymtd)
		mymtd = do_map_probe("jedec_probe", &keywest_map);

	if (!mymtd) {
		printk("No flash/rom found !\n");
		iounmap((void *)keywest_map.map_priv_1);
		return -ENXIO;
	}
		
	mymtd->module = THIS_MODULE;
	add_mtd_partitions(mymtd, keywest_partitions, NB_OF(keywest_partitions));
	return 0;
}

int __init keywest_mtd_init(void)
{
	unsigned short r;
	int rc;

#if defined(CONFIG_SH_KEYWEST)
	/*
	 *	Need to enable flash writes to ID chips etc
	 *	Port D(2) is our target,  set it to '1'
	 */
	r = * (unsigned short *) 0xa4000106;
	r &= ~0x0030;
	r |=  0x0010;
	* (unsigned short *) 0xa4000106 = r;
	* (unsigned char *) 0xa4000126 |= 0x04;
#elif defined(CONFIG_SH_BIGSUR)
	/*
	 *	Need to enable flash writes to ID chips etc
	 */
	* (unsigned char *) 0xb1fffb00 = 0x08;
#endif

	rc = keywest_probe(0x0000000, 4); /* boot from FLASH */
	if (rc != 0)
		rc = keywest_probe(0x8000000, 4); /* otherwise boot from ROM */
	return(rc);
}

static void __exit keywest_mtd_cleanup(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
	}
	if (keywest_map.map_priv_1) {
		iounmap((void *)keywest_map.map_priv_1);
		keywest_map.map_priv_1 = 0;
	}
}

module_init(keywest_mtd_init);
module_exit(keywest_mtd_cleanup);
