/*
 * linux/arch/armnommu/mach-S3C3410/irq.c
 *
 * 2003, Thomas Eschenbacher <thomas.eschenbacher@gmx.de>
 *
 * S3C3410X interrupt flag and mask handling
 *
 */
 
#include <asm/hardware.h>
#include <asm/io.h>

void __inline__ s3c3410_mask_irq(unsigned int irq)
{
	outl( inl(S3C3410X_INTMSK) & ~( 1 << irq ), S3C3410X_INTMSK);
}

void __inline__ s3c3410_unmask_irq(unsigned int irq)
{
	outl( inl(S3C3410X_INTMSK) | ( 1 << irq ), S3C3410X_INTMSK);
}

/* Clear pending bit */
void __inline__ s3c3410_clear_pb(unsigned int irq)
{
	outl( ~(1 << irq), S3C3410X_INTPND);
}

void __inline__ s3c3410_mask_ack_irq(unsigned int irq)
{
	s3c3410_mask_irq(irq);
	s3c3410_clear_pb(irq);
}
