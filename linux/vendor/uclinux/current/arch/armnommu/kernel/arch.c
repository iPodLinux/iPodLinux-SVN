/*
 *  linux/arch/armnommu/kernel/arch.c
 *
 *  Architecture specific fixups.
 */
#include <linux/config.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>

#include <asm/elf.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <asm/hardware/dec21285.h>

extern void genarch_init_irq(void);

unsigned int vram_size;

#ifdef CONFIG_ARCH_ACORN

unsigned int memc_ctrl_reg;
unsigned int number_mfm_drives;

static int __init parse_tag_acorn(const struct tag *tag)
{
	memc_ctrl_reg = tag->u.acorn.memc_control_reg;
	number_mfm_drives = tag->u.acorn.adfsdrives;

	switch (tag->u.acorn.vram_pages) {
	case 512:
		vram_size += PAGE_SIZE * 256;
	case 256:
		vram_size += PAGE_SIZE * 256;
	default:
		break;
	}
#if 0
	if (vram_size) {
		desc->video_start = 0x02000000;
		desc->video_end   = 0x02000000 + vram_size;
	}
#endif
	return 0;
}

__tagtable(ATAG_ACORN, parse_tag_acorn);

#ifdef CONFIG_ARCH_RPC
static void __init
fixup_riscpc(struct machine_desc *desc, struct param_struct *unusd,
	    char **cmdline, struct meminfo *mi)
{
	/*
	 * RiscPC can't handle half-word loads and stores
	 */
	elf_hwcap &= ~HWCAP_HALF;
}

extern void __init rpc_map_io(void);

MACHINE_START(RISCPC, "Acorn-RiscPC")
	MAINTAINER("Russell King")
	BOOT_MEM(0x10000000, 0x03000000, 0xe0000000)
	BOOT_PARAMS(0x10000100)
	DISABLE_PARPORT(0)
	DISABLE_PARPORT(1)
	FIXUP(fixup_riscpc)
	MAPIO(rpc_map_io)
	INITIRQ(genarch_init_irq)
MACHINE_END
#endif
#ifdef CONFIG_ARCH_ARC
MACHINE_START(ARCHIMEDES, "Acorn-Archimedes")
	MAINTAINER("Dave Gilbert")
	BOOT_PARAMS(0x0207c000)
	INITIRQ(genarch_init_irq)
MACHINE_END
#endif
#ifdef CONFIG_ARCH_A5K
MACHINE_START(A5K, "Acorn-A5000")
	MAINTAINER("Russell King")
	BOOT_PARAMS(0x0207c000)
	INITIRQ(genarch_init_irq)
MACHINE_END
#endif
#endif

#ifdef CONFIG_ARCH_L7200
extern void __init l7200_map_io(void);

static void __init
fixup_l7200(struct machine_desc *desc, struct param_struct *unused,
             char **cmdline, struct meminfo *mi)
{
        mi->nr_banks      = 1;
        mi->bank[0].start = PHYS_OFFSET;
        mi->bank[0].size  = (32*1024*1024);
        mi->bank[0].node  = 0;

        ROOT_DEV = MKDEV(RAMDISK_MAJOR,0);
        setup_ramdisk( 1, 0, 0, CONFIG_BLK_DEV_RAM_SIZE);
        setup_initrd( __phys_to_virt(0xf1000000), 0x005dac7b);

        /* Serial Console COM2 and LCD */
	strcpy( *cmdline, "console=tty0 console=ttyLU1,115200");

        /* Serial Console COM1 and LCD */
	//strcpy( *cmdline, "console=tty0 console=ttyLU0,115200");

        /* Console on LCD */
	//strcpy( *cmdline, "console=tty0");
}

MACHINE_START(L7200, "LinkUp Systems L7200")
	MAINTAINER("Steve Hill / Scott McConnell")
	BOOT_MEM(0xf0000000, 0x80040000, 0xd0000000)
	FIXUP(fixup_l7200)
	MAPIO(l7200_map_io)
	INITIRQ(genarch_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_NEXUSPCI

extern void __init nexuspci_map_io(void);

MACHINE_START(NEXUSPCI, "FTV/PCI")
	MAINTAINER("Philip Blundell")
	BOOT_MEM(0x40000000, 0x10000000, 0xe0000000)
	MAPIO(nexuspci_map_io)
	INITIRQ(genarch_init_irq)
MACHINE_END
#endif
#ifdef CONFIG_ARCH_TBOX

extern void __init tbox_map_io(void);

MACHINE_START(TBOX, "unknown-TBOX")
	MAINTAINER("Philip Blundell")
	BOOT_MEM(0x80000000, 0x00400000, 0xe0000000)
	MAPIO(tbox_map_io)
	INITIRQ(genarch_init_irq)
MACHINE_END
#endif
#ifdef CONFIG_ARCH_CLPS7110
MACHINE_START(CLPS7110, "CL-PS7110")
	MAINTAINER("Werner Almesberger")
	INITIRQ(genarch_init_irq)
MACHINE_END
#endif
#ifdef CONFIG_ARCH_ETOILE
MACHINE_START(ETOILE, "Etoile")
	MAINTAINER("Alex de Vries")
	INITIRQ(genarch_init_irq)
MACHINE_END
#endif
#ifdef CONFIG_ARCH_LACIE_NAS
MACHINE_START(LACIE_NAS, "LaCie_NAS")
	MAINTAINER("Benjamin Herrenschmidt")
	INITIRQ(genarch_init_irq)
MACHINE_END
#endif
#ifdef CONFIG_ARCH_CLPS7500

extern void __init clps7500_map_io(void);

MACHINE_START(CLPS7500, "CL-PS7500")
	MAINTAINER("Philip Blundell")
	BOOT_MEM(0x10000000, 0x03000000, 0xe0000000)
	MAPIO(clps7500_map_io)
	INITIRQ(genarch_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_NETARM
#include <asm/arch/netarm_registers.h>
#include <asm/arch/netarm_mmap.h>
#include <asm/arch/netarm_nvram.h>
static void __init
fixup_netarm(struct machine_desc *desc, struct param_struct *params,
             char **cmdline, struct meminfo *mi)
{
	extern void __ramdisk_data, __ramdisk_data_end, _text;

	/* code from 2.0.38 by J.deBlaquiere:  --rp */
	printk("fixup_netarm: Kernel memory start 0x%08lx end 0x%08lx\n",
	       (unsigned long)&_text, (unsigned long)&_end_kernel);

#ifdef CONFIG_ETHER_NETARM
#ifdef CONFIG_NETARM_EEPROM
	/* copy MAC address from NVRAM to Ethernet controller */
	{
		NA_dev_board_params_t *pParams;
		unsigned int *pUint;
		unsigned int uItmp;
		unsigned int serno;
		unsigned int csum;
		
		pParams = (NA_dev_board_params_t *)(NETARM_MMAP_EEPROM_BASE) ;
		csum = compute_checksum(pParams) ;
		
		if (csum == 0)
		{
			/* valid checksum... parse chars */
			int i,j ;
		
			serno = 0;
			for ( i = 0 ; i < 8 ; i++ )
			{
				j = parse_decimal(pParams->serialNumber[i]);
				if (j < 0)
				{
					/* invalid digits - pretend csum bad */
					csum = 1 ;
					break;
				}
				serno *= 10 ;
				serno += j;
			}
		} else {
			/* (csum != 0): use default serial number */
			serno = 99335 ;
		}
		/* multiply serno by 8 to calc MAC */
		serno <<= 3 ;
		
		uItmp = serno & 0xFF ;
		uItmp <<= 8 ;
		uItmp += ( serno >> 8 ) & 0xFF ;
		pUint = (unsigned int *)get_eth_reg_addr(NETARM_ETH_SAL_STATION_ADDR_3);
		*pUint = uItmp;

		uItmp = ( serno >> 16 ) & 0xFF ;
		uItmp <<= 8 ;
		uItmp += NETARM_OUI_BYTE3 ;
		pUint = (unsigned int *)get_eth_reg_addr(NETARM_ETH_SAL_STATION_ADDR_2);
		*pUint = uItmp;

		uItmp = NETARM_OUI_BYTE2 ;
		uItmp <<= 8 ;
		uItmp += NETARM_OUI_BYTE1 ;
		pUint = (unsigned int *)get_eth_reg_addr(NETARM_ETH_SAL_STATION_ADDR_1);
		*pUint = uItmp;
	}
#else
	/* no EEPROM: generate MAC address from GEN_ID */
	{
		unsigned int *pUint;
		unsigned int uItmp;
		unsigned int serno;
		
		pUint = (unsigned int *)get_gen_reg_addr(NETARM_GEN_STATUS_CONTROL);
		serno = *pUint & 0x7FF;

		uItmp = serno & 0xFF ;
		uItmp <<= 8 ;
		uItmp += ( serno >> 8 ) & 0xFF ;
		pUint = get_eth_reg_addr(NETARM_ETH_SAL_STATION_ADDR_3);
		*pUint = uItmp;

		uItmp = ( serno >> 16 ) & 0xFF ;
		uItmp <<= 8 ;
		uItmp += NETARM_OUI_BYTE3 ;
		pUint = get_eth_reg_addr(NETARM_ETH_SAL_STATION_ADDR_2);
		*pUint = uItmp;

		uItmp = NETARM_OUI_BYTE2 ;
		uItmp <<= 8 ;
		uItmp += NETARM_OUI_BYTE1 ;
		pUint = get_eth_reg_addr(NETARM_ETH_SAL_STATION_ADDR_1);
		*pUint = uItmp;
	}
#endif
#endif

#ifdef CONFIG_BLK_DEV_RAMDISK_DATA
	setup_ramdisk(1, 0, 0, CONFIG_BLK_DEV_RAM_SIZE);
        setup_initrd((int)&__ramdisk_data, 
	             (int)&__ramdisk_data_end - (int)&__ramdisk_data);
#endif
} /* fixup_netarm() */

MACHINE_START(NETARM, "NET+ARM")
	MAINTAINER("Rolf Peukert")
	FIXUP(fixup_netarm)
	INITIRQ(genarch_init_irq)
MACHINE_END
#endif

#if defined( CONFIG_ARCH_TA7S ) || defined( CONFIG_ARCH_TA7V )
#include <asm/arch/arch.h>

#ifdef CONFIG_BLK_DEV_RAMDISK_DATA
extern void __ramdisk_data, __ramdisk_data_end;
#endif

extern void _text;

static void __init
fixup_ta7(struct machine_desc *desc, struct param_struct *params,
             char **cmdline, struct meminfo *mi)
{
	ta7_arch_init();

#ifdef CONFIG_BLK_DEV_RAMDISK_DATA
	strcpy( *cmdline, "root=/dev/ram rw");
	setup_ramdisk(1, 0, 0, CONFIG_BLK_DEV_RAM_SIZE);
        setup_initrd((int)&__ramdisk_data, 
	             (int)&__ramdisk_data_end - (int)&__ramdisk_data);
#endif
} /* fixup_ta7s() */

#if defined(CONFIG_ARCH_TA7S)
MACHINE_START(TA7S, "Triscend-A7S")
	MAINTAINER("Craig Hackney (craig@triscend.com)")
	FIXUP(fixup_ta7)
	INITIRQ(genarch_init_irq)
MACHINE_END
#elif defined(CONFIG_ARCH_TA7V)
MACHINE_START(TA7V, "Triscend-A7V")
	MAINTAINER("Craig Hackney (craig@triscend.com)")
	FIXUP(fixup_ta7)
	INITIRQ(genarch_init_irq)
MACHINE_END
#endif


#endif



#ifdef CONFIG_ARCH_SWARM

static void __init
fixup_swarm(struct machine_desc *desc, struct param_struct *params,
             char **cmdline, struct meminfo *mi)
{
	
	printk("ARM for SWARM by C Hanish Menon [www.hanishkvc.com] - 9 Sep 2001 \n");				
	mi->nr_banks      = 1;
	/* Note: PHYS_OFFSET set to start after the kernel code and data */
	mi->bank[0].start = PHYS_OFFSET; 
	mi->bank[0].size  = (10*1024*1024);
	mi->bank[0].node  = 0;
				
#if 0
	ROOT_DEV = MKDEV(RAMDISK_MAJOR,0);
	setup_ramdisk( 1, 0, 0, CONFIG_BLK_DEV_RAM_SIZE);
	/* Initial ramdisk in the last 2 MB of Memory */
	setup_initrd( __phys_to_virt(0x00800000), 0x00200000);
#endif

	/* Serial Console COM2 and LCD */
	strcpy( *cmdline, "console=tty0");

}

MACHINE_START(SWARM, "SWARM")
	MAINTAINER("C Hanish Menon [www.hanishkvc.com]")
	BOOT_MEM(0x00000000, 0x90000000, 0x90000000)
	FIXUP(fixup_swarm)
	INITIRQ(genarch_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_C5471
#include <linux/sched.h>       /* For union task_union */
#include <linux/personality.h> /* For PER_LINUX_32BIT */

static void __init
fixup_c5471(struct machine_desc *desc, struct param_struct *params,
            char **cmdline, struct meminfo *mi)
{
  /* This is probably not necessary for the init process. But, all
   * normal C5471 processes must be marked as 32-bit processes. */

  init_task_union.task.personality = PER_LINUX_32BIT;
}

MACHINE_START(C5471, "C5471")
        MAINTAINER("Todd Fischer <todd.fischer@cadenux.com>")
        FIXUP(fixup_c5471)
        INITIRQ(genarch_init_irq)
MACHINE_END
#endif
