/*
 *  arch/armnommu/mach/irq.c
 *  2001, Oleksandr Zhadan
 */
 
#include <linux/init.h>

#include <asm/mach/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>


void __inline__ s3c4530_mask_irq(unsigned int irq)
{
    unsigned long mask;

    mask = inl(INTMSK);
    mask |= ( 1<<irq );
    outl(mask, INTMSK);
}	

void __inline__ s3c4530_unmask_irq(unsigned int irq)
{
    unsigned long mask;
    
    mask = inl(INTMSK);
    mask &= ~(1<<(irq));
    mask &= ~(1<<(_GLOBAL));
    outl(mask, INTMSK);
}

void __inline__ s3c4530_mask_ack_irq(unsigned int irq)
{
    s3c4530_mask_irq( irq);
}

void __inline__ s3c4530_clear_pb(unsigned int irq)
{
    outl( (1<<irq), INTPND);	/* Clear pending bit */
}
