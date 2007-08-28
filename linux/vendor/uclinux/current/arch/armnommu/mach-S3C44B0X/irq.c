/*
 * linux/arch/armnommu/mach-S3C44B0X/irq.c
 *
 * Copyright (C) 2003 Thomas Eschenbacher <eschenbacher@gmx.de>
 *
 * S3C44B0X interrupt flag and mask handling
 *
 */
 
#include <asm/hardware.h>
#include <asm/io.h>

void __inline__ s3c44b0x_mask_irq(unsigned int irq)
{
	outl( inl(S3C44B0X_INTMSK) | ( 1 << irq ), S3C44B0X_INTMSK);
}

void __inline__ s3c44b0x_unmask_irq(unsigned int irq)
{
	outl( inl(S3C44B0X_INTMSK) & ~( 1 << irq ), S3C44B0X_INTMSK);
}

/* Clear pending bit */
void __inline__ s3c44b0x_clear_pb(unsigned int irq)
{
	outl( (1 << irq), S3C44B0X_I_ISPC);
}

void __inline__ s3c44b0x_mask_ack_irq(unsigned int irq)
{
	s3c44b0x_mask_irq(irq);
	s3c44b0x_clear_pb(irq);
}
