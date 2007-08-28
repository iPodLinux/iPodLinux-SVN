/*
 *
 * Normal mappings of chips in physical memory
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/config.h>


#define WINDOW_ADDR 0xffe00000
#define WINDOW_SIZE 0x200000
#define BUSWIDTH 2

extern char* ppcboot_getenv(char* v);

static struct mtd_info *mymtd;

__u8 m5272c3_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

__u16 m5272c3_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

__u32 m5272c3_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

void m5272c3_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

void m5272c3_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

void m5272c3_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

void m5272c3_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

void m5272c3_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

struct map_info m5272c3_map = {
	name: "MCF5272C3 flash device",
	size: WINDOW_SIZE,
	buswidth: BUSWIDTH,
	read8: m5272c3_read8,
	read16: m5272c3_read16,
	read32: m5272c3_read32,
	copy_from: m5272c3_copy_from,
	write8: m5272c3_write8,
	write16: m5272c3_write16,
	write32: m5272c3_write32,
	copy_to: m5272c3_copy_to
};

/*
 * MTD 'PARTITIONING' STUFF 
 */

#ifndef CONFIG_PPCBOOT

static struct mtd_partition m5272c3_partitions[] = {
        {
                name: "kernel (768K)",
                size: 0xc0000,
                offset: 0x40000
        },
        {
                name: "rootfs (1024K)",
                size: 0x100000,
                offset: 0x100000
        },
	{
                name: "environment (8K)",
                size: 0x2000,
                offset: 0x4000
        }
};

#else

#define M5272C3_MAX_PARTITIONS 8
#define M5272C3_MAX_PARTARGENTRIES 3
#define M5272C3_PTABLE_BUFSIZE 256
static struct mtd_partition m5272c3_partitions[M5272C3_MAX_PARTITIONS];
static int m5272c3_partitions_cnt=0;
static char* m5272c3_emptyentry = "";

static void read_m5272c3_partitiontable(void) {
  char* envptable=ppcboot_getenv("ptable");
  if(envptable) {
    char ptable[M5272C3_PTABLE_BUFSIZE],*p,*pt;
    int pcnt,acnt,i;
    char *ptableargentry[M5272C3_MAX_PARTITIONS][M5272C3_MAX_PARTARGENTRIES];
    
    strncpy(ptable,envptable,M5272C3_PTABLE_BUFSIZE-1);
    ptable[M5272C3_PTABLE_BUFSIZE-1]=0;

    for(pcnt=0;pcnt<M5272C3_MAX_PARTITIONS;pcnt++) {
      for(acnt=0;acnt<M5272C3_MAX_PARTARGENTRIES;acnt++) {
	ptableargentry[pcnt][acnt]=m5272c3_emptyentry;
      };
    };

    pcnt=acnt=0;
    p=pt=ptable;
    while(1) {
      if((*p == ',') || (*p == ':') || (*p == 0)) {
	if(pcnt<M5272C3_MAX_PARTITIONS) {      
	  if(acnt<M5272C3_MAX_PARTARGENTRIES)
	    ptableargentry[pcnt][acnt++]=pt;
	  if((*p == ':') || ( *p == 0 ))
	    { acnt=0; pcnt++; };
	  if( *p == 0 )
	    break;
	  pt=p+1;
	};
	*p=0;
      };
      p++;
    };

    for(i=0;i<pcnt;i++) {
      m5272c3_partitions[i].name=
	(char*)kmalloc(strlen(ptableargentry[i][0])+1,GFP_KERNEL);
      strcpy(m5272c3_partitions[i].name,ptableargentry[i][0]);
      m5272c3_partitions[i].size=
	simple_strtoul(ptableargentry[i][1],0,16)*0x1000;
      m5272c3_partitions[i].offset=
	simple_strtoul(ptableargentry[i][2],0,16)*0x1000;
    };
    
    m5272c3_partitions_cnt=pcnt;
  };
};

#endif

#if LINUX_VERSION_CODE < 0x20212 && defined(MODULE)
#define init_m5272c3 init_module
#define cleanup_m5272c3 cleanup_module
#endif

int __init init_m5272c3(void)
{
       	printk(KERN_NOTICE "m5272c3 flash device: %x at %x\n", WINDOW_SIZE, WINDOW_ADDR);
	m5272c3_map.map_priv_1 = (unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);

	if (!m5272c3_map.map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}
	mymtd = do_map_probe("cfi_probe", &m5272c3_map);
	if (mymtd) {
		mymtd->module = THIS_MODULE;

#ifdef CONFIG_PPCBOOT
                read_m5272c3_partitiontable();
		if(m5272c3_partitions_cnt) {
		  return add_mtd_partitions(mymtd, m5272c3_partitions, m5272c3_partitions_cnt);
		}
		else {
		  return  -ENXIO;
		};
#else
		return add_mtd_partitions(mymtd, m5272c3_partitions,
					  sizeof(m5272c3_partitions) /
					  sizeof(struct mtd_partition));
#endif
	}

	iounmap((void *)m5272c3_map.map_priv_1);
	return -ENXIO;
}

static void __exit cleanup_m5272c3(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
	}
	if (m5272c3_map.map_priv_1) {
		iounmap((void *)m5272c3_map.map_priv_1);
		m5272c3_map.map_priv_1 = 0;
	}
}

module_init(init_m5272c3);
module_exit(cleanup_m5272c3);

