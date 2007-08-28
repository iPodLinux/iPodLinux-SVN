/*
 *  linux/arch/armnommu/$(MACHINE)/config.c
 *
 *  Copyright (C) 1993 Hamish Macdonald
 *  Copyright (C) 1999-2003 D. Jeff Dionne <jeff@uClinux.org>
 *  Copyright (C) 2001-2003 Arcturus Networks Inc. 
 *                          by Oleksandr Zhadan <www.ArcturusNetworks.com> 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <stdarg.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/init.h>
#include <asm/uCbootstrap.h>
#include <asm/arch/hardware.h>

#undef CONFIG_DEBUG

unsigned char *sn;

#if defined(CONFIG_UCBOOTSTRAP)
    int	ConTTY = 0;
    static int errno;
    _bsc0(char *, getserialnum)
    _bsc1(unsigned char *, gethwaddr, int, a)
    _bsc1(char *, getbenv, char *, a)
#endif

extern unsigned int arm940_get_creg_0(void);
extern unsigned int arm940_get_creg_1(void);
extern unsigned int arm940_get_creg_2(unsigned int);
extern unsigned int arm940_get_creg_3(void);
extern unsigned int arm940_get_creg_5(unsigned int);
extern unsigned int arm940_get_creg_6_0(void);
extern unsigned int arm940_get_creg_6_1(void);
extern unsigned int arm940_get_creg_6_2(void);
extern unsigned int arm940_get_creg_6_3(void);
extern unsigned int arm940_get_creg_6_4(void);
extern unsigned int arm940_get_creg_6_5(void);
extern unsigned int arm940_get_creg_6_6(void);
extern unsigned int arm940_get_creg_6_7(void);
extern void arm940_uncache_region_4(unsigned int, unsigned int);
extern void arm940_uncache_region_5(unsigned int, unsigned int);
extern void arm940_uncache_region_6(unsigned int, unsigned int);
extern void arm940_uncache_region_7(unsigned int, unsigned int);

unsigned char *get_MAC_address(char *);

unsigned int CPU_CLOCK=0;  		/* CPU clock	*/
unsigned int SYS_CLOCK=0;  		/* System clock	*/
unsigned int CONFIG_ARM_CLK=0;

char S3C2500_HWADDR0[]="00:ca:ca:00:00:01";
char S3C2500_HWADDR1[]="00:ca:ca:00:00:02";

void	get_system_clock(void)
{
    unsigned int status = *(unsigned int *)CLKST;
    unsigned int pll = *(unsigned int *)SYSCFG;

    if	 ( pll & 0x80000000 ) {
	 CPU_CLOCK = (*(unsigned int *)CPLL & 0xff)+8;
         switch (status>>30) {
		case 0:	    SYS_CLOCK = CPU_CLOCK;
			    break;
		case 1:
		case 2:	    SYS_CLOCK = CPU_CLOCK >> 1;
			    break;
		case 3:	    SYS_CLOCK = (*(unsigned int *)SPLL & 0xff)+8;
			    break;
		}
	 }
    else {
	 pll = (status & 0x00000fff);
	 CPU_CLOCK += ((pll&0xF00)>>8)*100;
	 CPU_CLOCK += ((pll&0x0F0)>>4)*10;
	 CPU_CLOCK += ((pll&0x00F)>>0)*1;

	 switch (status>>30) {
		case 0:	    SYS_CLOCK = CPU_CLOCK;
			    break;
		case 1:
		case 2:	    SYS_CLOCK = CPU_CLOCK >> 1;
			    break;
		case 3:	    pll = ((status & 0x00fff000)>>12);
			    SYS_CLOCK += ((pll&0xF00)>>8)*100;
			    SYS_CLOCK += ((pll&0x0F0)>>4)*10;
			    SYS_CLOCK += ((pll&0x00F)>>0)*1;
			    break;
		}
	}

    CONFIG_ARM_CLK = (SYS_CLOCK*1000000);
    asm("nop");
}

void config_BSP()
{
#if defined(CONFIG_UCBOOTSTRAP)
    printk("uCbootloader: Copyright (C) 2001-2003 Arcturus Networks Inc.\n");
    printk("uCbootloader system calls are initialized.\n");
#else
  sn = (unsigned char *)"123456789012345";
# ifdef	CONFIG_DEBUG
    printk("Serial number [%s]\n",sn);
# endif
#endif

#ifdef CONFIG_UNCACHED_MEM
    {
    extern void set_top_uncached_region(void);
    set_top_uncached_region();
    }
#endif
}

unsigned char *get_MAC_address(char *devname)
{
    char *devnum  = devname + (strlen(devname)-1);
    char varname[sizeof("HWADDR0")];
    char *varnum  = varname + (strlen(varname)-1);
    static unsigned char tmphwaddr[6] = {0, 0, 0, 0, 0, 0};
    char *ret;
    int	 i;

    strcpy ( varname, "HWADDR" );
    strcat ( varname, devnum );

#if defined(CONFIG_UCBOOTSTRAP)
    ret = getbenv(varname);
#else
    if	 ( !strcmp(varname, "HWADDR0") )
	 ret = (char *)S3C2500_HWADDR0;
    else ret = (char *)S3C2500_HWADDR1;
#endif    
    memset(tmphwaddr, 0, 6);
    if	(ret && strlen(ret) == 17)
	for (i=0; i<6; i++) {
	    ret[(i+1) * 3 - 1] = 0;
	    tmphwaddr[i] = simple_strtoul(ret + i*3, 0, 16); 
	    }
    return(tmphwaddr);
}

unsigned int reg_map[8] = { 1, 1, 1, 1, 0, 0, 0, 0 };

int	ask_s3c2500_uncache_region( void *addr, unsigned int size, unsigned int flag)
{
    static unsigned int reg_map[8] = { 1, 1, 1, 1, 0, 0, 0, 0 };
    unsigned int i, j, n;
    unsigned int baseandsize=0;
    
    if	((unsigned int)addr & (size-1))
	return	-1;				/* addr is not alligned */
	
    for ( n = 4; n < 8; n++ )
	if  ( !reg_map[n] ) {
	    for	( i = 11, j=0x1000; i < 31; i++, j<<=1 )
		if  ( j == size ) {
		    baseandsize = (i << 1);		/* Area size	 */
		    baseandsize |= ((unsigned int)addr);/* Base address  */
		    baseandsize |= 1;			/* Enable region */
		    switch ( n ) {
			    case 4 :	arm940_uncache_region_4(baseandsize, flag);
					break;
			    case 5 :	arm940_uncache_region_5(baseandsize, flag);
					break;
			    case 6 :	arm940_uncache_region_6(baseandsize, flag);
					break;
			    case 7 :	arm940_uncache_region_7(baseandsize, flag);
					break;
			    default :	return -1;
			    }
		    reg_map[n] = 1;
		    return 0;
		    }
	    return -2;	/* Size is not correct */
	    }
    return -3;		/* No free region found */
}

#ifdef CONFIG_UNCACHED_MEM

void	set_top_uncached_region()
{
#ifdef CONFIG_UNCACHED_256
unsigned int val = 0x23 | (END_MEM-0x40000);
#endif

#ifdef CONFIG_UNCACHED_512
unsigned int val = 0x25 | (END_MEM-0x80000);
#endif

#ifdef CONFIG_UNCACHED_1024
unsigned int val = 0x27 | (END_MEM-0x100000);
#endif

#ifdef CONFIG_UNCACHED_2048
unsigned int val = 0x29 | (END_MEM-0x200000);
#endif

#ifdef CONFIG_UNCACHED_4096
unsigned int val = 0x2B | (END_MEM-0x400000);
#endif

#ifdef CONFIG_UNCACHED_8192
unsigned int val = 0x2D | (END_MEM-0x800000);
#endif

    arm940_uncache_region_7(val, 0);
    reg_map[7] = 1;
}

#endif	/* CONFIG_UNCACHED_MEM */


void crinfo(void)
{
    unsigned int tmp, size, i;
    printk("\nCoprocessor register info:\n");
    
    tmp = arm940_get_creg_0();
    printk("ID code                  : %8x\n", tmp);
    
    tmp = arm940_get_creg_1();
    printk("Control register         : %8x\n", tmp);

    tmp = arm940_get_creg_2(0);
    printk("Data cacheable register  : %8x\n", tmp);

    tmp = arm940_get_creg_2(1);
    printk("Inst cacheable register  : %8x\n", tmp);

    tmp = arm940_get_creg_3();
    printk("Write buffer register    : %8x\n", tmp);

    tmp = arm940_get_creg_5(0);
    printk("Data access register     : %8x\n", tmp);

    tmp = arm940_get_creg_5(1);
    printk("Inst access register     : %8x\n", tmp);

    for	(i=0; i < 8; i++ ) {
	switch (i) {
	    case 0 :	tmp = arm940_get_creg_6_0();	break;
	    case 1 :	tmp = arm940_get_creg_6_1();	break;
	    case 2 :	tmp = arm940_get_creg_6_2();	break;
	    case 3 :	tmp = arm940_get_creg_6_3();	break;
	    case 4 :	tmp = arm940_get_creg_6_4();	break;
	    case 5 :	tmp = arm940_get_creg_6_5();	break;
	    case 6 :	tmp = arm940_get_creg_6_6();	break;
	    case 7 :	tmp = arm940_get_creg_6_7();	break;
	    }
	printk("Region%d: addr=%8x, ", i, (tmp & 0xfffff000));
	size = tmp>>1;
	size &= 0x1f;
	size -= 0xb;
	printk("size=%dK, ", (0x4<<size));
	if 	( tmp & 1 )	printk("enabled\n");
	else			printk("disabled\n");
	}
}

EXPORT_SYMBOL(ask_s3c2500_uncache_region);

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
