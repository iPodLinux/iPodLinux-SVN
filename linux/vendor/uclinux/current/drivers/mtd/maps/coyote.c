#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/cfi.h>
#include <linux/ioport.h>
#include <asm/io.h>

#include <linux/notifier.h>

#define WINDOW_ADDR 	0x50000000
#define BUSWIDTH 	2
#define WINDOW_SIZE	0x01000000

#ifndef __ARMEB__
#define	B_0(h)	((h) & 0xFF)
#define	B_1(h)	(((h) >> 8) & 0xFF)
#else
#define	B_0(h)	(((h) >> 8) & 0xFF)
#define	B_1(h)	((h) & 0xFF)
#endif

static __u16 coyote_read16(struct map_info *map, unsigned long ofs)
{
    return *(__u16 *)(map->map_priv_1 + ofs);
}

/*
 * The IXP425 expansion bus only allows 16-bit wide acceses
 * when attached to a 16-bit wide device (such as the 28F128J3A),
 * so we can't use a memcpy_fromio as it does byte acceses.
 */
static void coyote_copy_from(struct map_info *map, void *to,
    unsigned long from, ssize_t len)
{
	int i;
	u8 *dest = (u8*)to;
	u16 *src = (u16 *)(map->map_priv_1 + from);
	u16 data;

	for(i = 0; i < (len / 2); i++) {
		data = src[i];
		dest[i * 2] = B_0(data);
		dest[i * 2 + 1] = B_1(data);
	}

	if(len & 1)
		dest[len - 1] = B_0(src[i]);
}

static void coyote_write16(struct map_info *map, __u16 d, unsigned long adr)
{
    *(__u16 *)(map->map_priv_1 + adr) = d;
}

static struct map_info coyote_map = {
	name: 		"IXP425 Residential Gateway Demo Platform Flash",
	buswidth: 	BUSWIDTH,
	read16:		coyote_read16,
	copy_from:	coyote_copy_from,
	write16:	coyote_write16,
};

#ifdef CONFIG_MTD_REDBOOT_PARTS
static struct mtd_partition *parsed_parts;
#endif

#ifdef CONFIG_MTD_CFI_INTELEXT
/*
 *	Set the Intel flash back to read mode as MTD may leave it in command mode
 */

static int coyote_reboot_notifier(
	struct notifier_block *nb,
	unsigned long val,
	void *v)
{
	struct cfi_private *cfi = coyote_map.fldrv_priv;
	int i;
	
	for (i = 0; cfi && i < cfi->numchips; i++)
		cfi_send_gen_cmd(0xff, 0x55, cfi->chips[i].start, &coyote_map,
				cfi, cfi->device_type, NULL);

	return(NOTIFY_OK);
}

static struct notifier_block coyote_notifier_block = {
	coyote_reboot_notifier, NULL, 0
};

#endif


static struct mtd_partition coyote_partitions[] = {
    {
	name:	"image",
	offset:	0x00040000,
	size:	0x00400000, /* 4M for linux kernel + cramfs + initrd image */
    },
    {
	name:	"user",
	offset:	0x00440000,
	size:	0x00B80000, /* Rest of flash space minus redboot configuration
	                     * sectors (-256k) at the end of flash
			     */
    },
};

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

static struct mtd_info *coyote_mtd;
static struct resource *mtd_resource;

static void coyote_exit(void)
{
    if (coyote_mtd)
    {
	del_mtd_partitions(coyote_mtd);
	map_destroy(coyote_mtd);
    }
    if (coyote_map.map_priv_1)
	iounmap((void *)coyote_map.map_priv_1);
    if (mtd_resource)
	release_mem_region(WINDOW_ADDR, WINDOW_SIZE);
    
#ifdef CONFIG_MTD_REDBOOT_PARTS
    if (parsed_parts)
	kfree(parsed_parts);
#endif

    /* Disable flash write */
    *IXP425_EXP_CS0 &= ~IXP425_FLASH_WRITABLE;
}

static int __init coyote_init(void)
{
    int res, npart;

    /* Enable flash write */
    *IXP425_EXP_CS0 |= IXP425_FLASH_WRITABLE;

    coyote_map.map_priv_1 = 0;
    mtd_resource = 
	request_mem_region(WINDOW_ADDR, WINDOW_SIZE, "IXP425 Residential Gateway Demo Platform Flash");
    if (!mtd_resource)
    {
	printk(KERN_ERR "IXP425 Residential Gateway Demo Platform Flash: Could not request mem region.\n" );
	res = -ENOMEM;
	goto Error;
    }

    coyote_map.map_priv_1 =
	(unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);
    if (!coyote_map.map_priv_1)
    {
	printk("IXP425 Residential Gateway Demo Platform Flash: Failed to map IO region. (ioremap)\n");
	res = -EIO;
	goto Error;
    }
    coyote_map.size = WINDOW_SIZE;


    /* 
     * Probe for the CFI complaint chip
     * suposed to be 28F128J3A
     */
    coyote_mtd = do_map_probe("cfi_probe", &coyote_map);
    if (!coyote_mtd)
    {
	res = -ENXIO;
	goto Error;
    }
    coyote_mtd->module = THIS_MODULE;
   
    /* Initialize flash partiotions 
     * Note: Redeboot partition info table can be parsed by MTD, and used
     *       instead of hard-coded partions. TBD
     */

#ifdef CONFIG_MTD_REDBOOT_PARTS
    /* Try to parse RedBoot partitions */
    npart = parse_redboot_partitions(coyote_mtd, &parsed_parts);
    if (npart > 0)
    {
	/* found "npart" RedBoot partitions */
	
	res = add_mtd_partitions(coyote_mtd, parsed_parts, npart);
    }
    else   
	res = -EIO;

    if (res)
#endif
    {
	printk("Using predefined MTD partitions.\n");
	/* RedBoot partitions not found - use hardcoded partition table */
	res = add_mtd_partitions(coyote_mtd, coyote_partitions,
	    NB_OF(coyote_partitions));
    }

    if (res)
	goto Error;

#ifdef CONFIG_MTD_CFI_INTELEXT
	register_reboot_notifier(&coyote_notifier_block);
#endif

    return res;
Error:
    coyote_exit();
    return res;
}

module_init(coyote_init);
module_exit(coyote_exit);

MODULE_DESCRIPTION("MTD map driver for IXP425 Residential Gateway Demo Platform");

