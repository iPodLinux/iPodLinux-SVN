/*
 *  linux/arch/arm/mach-xscale/mm.c
 */

#include <linux/mm.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/pgtable.h>
#include <asm/page.h>
 
#include <asm/mach/map.h>

static struct map_desc adi_evb_io_desc[] __initdata = {
 /* virtual    physical    length      domain     r  w  c  b */ 
 /* On-board devices */
 { 0xff400000, 0x00400000, 0x00300000, DOMAIN_IO, 0, 1, 0, 0 },  
 LAST_DESC
};

static struct map_desc brh_io_desc[] __initdata = {
 /* virtual    physical    length      domain     r  w  c  b  */ 
 /* PCI Config Window */
 { 0xfb000000, 0x08000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 },
 /* PCI I/O Window */
 { 0xfd000000, 0x0a000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 },
 /* On board devices - UART 0 & 1, LED */
 { 0xff000000, 0x03000000, 0x00400000, DOMAIN_IO, 0, 1, 0, 0 },
 /* BECC Configurations Registers */
 { 0xff400000, 0x04000000, 0x00010000, DOMAIN_IO, 0, 1, 0, 0 },
 LAST_DESC
};

void __init adi_map_io(void)
{
	if(machine_is_adi_evb())	
		iotable_init(adi_evb_io_desc);
	else if(machine_is_brh())
		iotable_init(brh_io_desc);
}
