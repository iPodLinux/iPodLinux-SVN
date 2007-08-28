/*
 *  linux/arch/arm/mach-p52/irq.c
 *  2001 Mindspeed
 */
#include <linux/init.h>

#include <asm/mach/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

 
inline void p52_irq_ack(int irq)
{
  *((unsigned long *)P52INT_STATUS) |= (1 << irq);
}

  
void p52_mask_irq(unsigned int irq)
{
  /* reset cooresponding bit in the 32bit Int mask register */ 
  *((unsigned long *)P52INT_MASK) &= ~(1 << irq);
}


void p52_unmask_irq(unsigned int irq)
{
  /* set cooresponding bit in the 32bit Int mask register */ 
  *((unsigned long *)P52INT_MASK) |= (1 << irq);
}
 

void p52_mask_ack_irq(unsigned int irq)
{
  p52_mask_irq(irq);
  /* this is ok if int source is not hooked on GPIO */
  *((unsigned long *)P52INT_STATUS) |= (1 << irq);
}





