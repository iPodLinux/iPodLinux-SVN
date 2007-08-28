/*
*  linux/arch/armnommu/mach-snds100/irq.c
*  2001 Mac Wang <mac@os.nctu.edu.tw>
*/
#include <linux/init.h>

#include <asm/mach/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

void s3c4510b_mask_irq(unsigned int irq)
{
	INT_DISABLE(irq);
}

void s3c4510b_unmask_irq(unsigned int irq)
{
	INT_ENABLE(irq);
}

void s3c4510b_mask_ack_irq(unsigned int irq)
{
	INT_DISABLE(irq);
}

void s3c4510b_int_init()
{
	IntPend = 0x1FFFFF;
	IntMode = INT_MODE_IRQ;
	INT_ENABLE(INT_GLOBAL);
}
