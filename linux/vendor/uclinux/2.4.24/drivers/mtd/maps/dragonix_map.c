#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>


#define WINDOW_ADDR 0x02200000
#define WINDOW_SIZE 0x00200000
#define BUSWIDTH 2

#define __raw_readb readb
#define __raw_readl readl
#define __raw_readw readw

static struct mtd_info *mymtd;


static __u8 dragonix_read8(struct map_info *map,unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

static __u16 dragonix_read16(struct map_info *map,unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

static __u32 dragonix_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

static void dragonix_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void dragonix_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*(__u8 *)(map->map_priv_1 + adr) = d;
}

static void dragonix_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*(__u16 *)(map->map_priv_1 + adr) = d;
}

static void dragonix_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*(__u32 *)(map->map_priv_1 + adr) = d;
}

static void dragonix_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

/*
static struct mtd_partition dragonix_partition[]={
    { 
	    name: "Dragonix Bootloader", 
	    offset: 0, 
	    size:   0x20000,
    },
    {
	   name:"Dragonix Kernel",
	   offset:0x20000,
	   size:0x7e0000,
    },
    { 
	    name: "dragonix RamDisk", /////
	    offset: 0x800000,
	    size:0x800000, 
    }
};
*/

// Test partition
static struct mtd_partition dragonix_partition[]={
    { 
	    name: "Dragonix Bootloader", 
	    offset: 0, 
	    size:   0x200000,
    }
};

#define NUM_PARTITIONS (sizeof(dragonix_partition)/sizeof(dragonix_partition[0]))


struct map_info dragonix_map = {
	name: "Dragonix VZ Flash map",
	size: WINDOW_SIZE,
	buswidth: BUSWIDTH,
	read8: dragonix_read8,
	read16: dragonix_read16,
	read32: dragonix_read32,
	copy_from: dragonix_copy_from,
	write8: dragonix_write8,
	write16: dragonix_write16,
	write32: dragonix_write32,
	copy_to: dragonix_copy_to,
};


static  int __init init_dragonix(void)
{
	printk(KERN_NOTICE "Dragonix VZ mapping:size %x at %x\n", WINDOW_SIZE, WINDOW_ADDR);
	dragonix_map.map_priv_1 = (unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);
	printk("ZEUS:dragonix_map.map_priv_1=%x\n",dragonix_map.map_priv_1);
	if (!dragonix_map.map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}
	
	mymtd = do_map_probe("cfi_probe", &dragonix_map);
	if (mymtd) {
		mymtd->module = THIS_MODULE;
		add_mtd_partitions( mymtd, dragonix_partition, NUM_PARTITIONS );

		add_mtd_device(mymtd);
		printk("ZEUS:right end of init_dragonix(I mean flash)\n");
		return 0;
	}
	
	iounmap((void *)dragonix_map.map_priv_1);
	printk("ZEUS:so its the wrong end of init_dragonix(flash)\n");
	return -ENXIO;

}

mod_exit_t cleanup_dragonix(void)
{
	if (mymtd) {
		del_mtd_device(mymtd);
		map_destroy(mymtd);
	}
	if (dragonix_map.map_priv_1) {
		iounmap((void *)dragonix_map.map_priv_1);
		dragonix_map.map_priv_1 = 0;
	}
}

module_init(init_dragonix);
module_exit(cleanup_dragonix);
