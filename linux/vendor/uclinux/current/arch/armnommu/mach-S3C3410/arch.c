/*
 *  uClinux/arch/arm/mach/arch.c
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

MACHINE_START(S3C3410, "S3C3410")
       MAINTAINER("Thomas Eschenbacher")
       INITIRQ(genarch_init_irq)
MACHINE_END

void arch_hard_reset(void)
{
	/*
	 * @todo activate the hardware reset
	 */
	while (1){};
}
