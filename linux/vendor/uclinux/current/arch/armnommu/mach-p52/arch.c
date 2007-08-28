/*
 *  linux/arch/arm/mach-p52/arch.c
 *
 *  Architecture specific fixups.  This is where any
 *  parameters in the params struct are fixed up, or
 *  any additional architecture specific information
 *  is pulled from the params struct.
 */
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>

#include <asm/elf.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>

extern void p52_map_io(void);
extern void p52_init_irq(void);
extern void genarch_init_irq(void);


MACHINE_START(P52, "CNXT P5200")
       BOOT_MEM(0xabcd1234, 0x56789abc, 0x12345678)
       MAINTAINER("Mindspeed")
       INITIRQ(genarch_init_irq)
MACHINE_END


