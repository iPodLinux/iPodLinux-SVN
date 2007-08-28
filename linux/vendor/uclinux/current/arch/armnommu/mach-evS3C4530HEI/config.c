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

#undef CONFIG_DEBUG

unsigned char *sn;

char S3C4530_HWADDR[]="00:ca:ca:00:00:01";
char CS8900A_HWADDR[]="00:ca:ca:00:00:02";

#if defined(CONFIG_UCBOOTSTRAP)
    int	ConTTY = 0;
    static int errno;
    _bsc0(char *, getserialnum)
    _bsc1(unsigned char *, gethwaddr, int, a)
    _bsc1(char *, getbenv, char *, a)
#endif

unsigned char *get_MAC_address(char *);

void config_BSP()
{
#if defined(CONFIG_UCBOOTSTRAP)
    printk("uCbootloader: Copyright (C) 2001,2002 Arcturus Networks Inc.\n");
    printk("uCbootloader system calls are initialized.\n");
#else
  sn = (unsigned char *)"123456789012345";
# ifdef	CONFIG_DEBUG
    printk("Serial number [%s]\n",sn);
# endif  
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
	 ret = (char *)S3C4530_HWADDR;
    else ret = (char *)CS8900A_HWADDR;
#endif    
    memset(tmphwaddr, 0, 6);
    if	(ret && strlen(ret) == 17)
	for (i=0; i<6; i++) {
	    ret[(i+1) * 3 - 1] = 0;
	    tmphwaddr[i] = simple_strtoul(ret + i*3, 0, 16); 
	    }
    return(tmphwaddr);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
