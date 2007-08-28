/*
 *  linux/arch/arm/mach-snds100/arch.c
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

extern void genarch_init_irq(void);

MACHINE_START(SNDS100, "SNDS100")
       MAINTAINER("Mac Wang")
       INITIRQ(genarch_init_irq)
MACHINE_END
