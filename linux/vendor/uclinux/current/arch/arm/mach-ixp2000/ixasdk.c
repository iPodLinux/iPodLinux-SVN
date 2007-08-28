/*
 *  linux/arch/arm/mach-ixp2000/ixasdk.c
 *
 *  Copyright (C) 2002 Intel Corp
 *
 *  naeem.m.afzal@intel.com
 * API used for IXASDK only
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/mm.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>

#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/mach/map.h>
#include <asm/arch/hardware.h>

#define MAX_QDR_CHANNEL         2

struct EEPROM_CONTENT {
        unsigned char   prom_fmt; //1
    	unsigned char   dev_id;  //1
        unsigned char   man_id[12]; //[CFG_MAN_ID_LEN]; //12
        unsigned char   part_rev [2];//[CFG_PARTREV_LEN]; //2

        unsigned char   part_num[10]; //[CFG_PARTNUM_LEN]; //10
        unsigned char   serial_num[10]; //[CFG_SERIAL_NUM_LEN]; //10
        unsigned char   model_id[12]; //[CFG_MODEL_ID_LEN]; //12

        unsigned char   rsvd1[8]; //[CFG_RSVD1_LEN]; //8
        unsigned char   passwd[8]; //[CFG_PASSWD_LEN]; //8

        unsigned char   test_status;    //1
        unsigned char   date[8]; //[CFG_DATE_LEN]; //8
        unsigned char   chksum[4]; //[CFG_CHKSUM_LEN]; //4
        unsigned char   rsvd2[3]; //[CFG_RSVD2_LEN]; //3
        unsigned int    M_uart_baud; //4
        unsigned char   M_ether_mac_addr[6]; //6
        unsigned int    S_uart_baud; //4
        unsigned char   S_ether_mac_addr[6]; //6
        unsigned char   rsvd3[12]; //4
};

struct board_config
{
        struct EEPROM_CONTENT config_data;
        int sdram_size;
        int qdr_channel_size[MAX_QDR_CHANNEL];
        int flash_size;
        int config_valid;
};

#define VALID_STRING	0x12345678

struct ixdp2000_sys_info {
    int valid;
    int sys; 		/* size of sdram used by  Linux OS*/
    int sys_all;   	/* Total SDRAM size on system */
    int sdram;   	/* Size of memory used by MicroEngines  */
    int sram;      	/* Size of sram Total */
    int sram0;		/* channel 0 sram size */
    int sram1;		/* channel 1 sram size */
    int sram2;		/* channel 2 sram size */
    int sram3;		/* channel 3 sram size */
    int flash;     	/* Size of Flash */
};

struct ixdp2000_sys_info *sz_info;

static int __init ixdp2000_ixasdk_init(void)
{
	struct board_config *scratch;

	scratch = (struct board_config *)__ioremap(PHY_SCRATCH, sizeof(struct board_config), 0);

	if (!scratch) {
                printk("Failed to ioremap\n");
                goto out;
        }

	if (scratch->config_valid != VALID_STRING ) {
		printk(KERN_WARNING"ixasdk.o: Invalid board config data\n");
		printk(KERN_WARNING"ixasdk.o: Need to program I2C data for valid config data.\n");
	}
	sz_info =(struct ixdp2000_sys_info *)kmalloc(sizeof(struct ixdp2000_sys_info), GFP_KERNEL);

	if (!sz_info) {
		printk(KERN_WARNING" Cannot allocate memory for struct ixdp2000_sys_info.\n");
		return -ENOMEM;
	}

	memset(sz_info,0, sizeof(struct ixdp2000_sys_info));

	if (scratch->config_valid == VALID_STRING) {
		sz_info->valid = 1;
		sz_info->sys_all = scratch->sdram_size; /* total system sdram */
		sz_info->sdram 	= scratch->sdram_size 
				- PHYS_SDRAM_SIZE;/* sdram for uEngines */

		sz_info->sys = PHYS_SDRAM_SIZE;
		sz_info->sram = scratch->qdr_channel_size[0] 
			+ scratch->qdr_channel_size[1];
		sz_info->sram0 = scratch->qdr_channel_size[0];
		sz_info->sram1 = scratch->qdr_channel_size[1];
#if CONFIG_ARCH_IXDP2800
		sz_info->sram2 = scratch->qdr_channel_size[2];
		sz_info->sram2 = scratch->qdr_channel_size[3];
#endif
		sz_info->flash = scratch->flash_size;
	} else { /* take some default values */
		sz_info->valid = 0;
		sz_info->sys_all = 0x20000000; /* total system sdram */
		sz_info->sdram 	= 0x20000000 
					- PHYS_SDRAM_SIZE;/* sdram for uEngines */
		sz_info->sys = PHYS_SDRAM_SIZE;
		sz_info->sram = 0x800000;
		sz_info->sram0 = 0x400000;
		sz_info->sram1 = 0x400000;
#if CONFIG_ARCH_IXDP2800
		sz_info->sram2 = 0x400000; 
		sz_info->sram2 = 0x400000;
#endif
		sz_info->flash = 0x1000000;

	}

	__iounmap(scratch);

	return 0;
out:
	return -1;
 
}
void sysMemVirtSize (
	 unsigned int* sys, unsigned int* sys_all
	,unsigned int* sdram,  unsigned int* sram
	,unsigned int* sram0, unsigned int* sram1
	,unsigned int* sram2, unsigned int* sram3
	,unsigned int* flash )
{

	if (!sz_info->valid)
		printk(KERN_WARNING "sysMemVirtSize(): System sizes are invalid.\n");
	*sys = sz_info->sys;	
	*sys_all = sz_info->sys_all;	
	*sdram = sz_info->sdram;
	*sram = sz_info->sram;
	*sram0 = sz_info->sram0;
	*sram1 = sz_info->sram1;
#if CONFIG_ARCH_IXDP2800
	*sram2 = sz_info->sram2;
	*sram3 = sz_info->sram3;
#endif
	*flash = sz_info->flash;

}
__initcall(ixdp2000_ixasdk_init);

EXPORT_SYMBOL(sysMemVirtSize);
