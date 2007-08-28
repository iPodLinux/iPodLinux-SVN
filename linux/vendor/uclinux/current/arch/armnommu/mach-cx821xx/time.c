/****************************************************************************
*
*	Name:			time.c
*
*	Description:	Timer ISR and query functions
*
*	Copyright:		(c) 2002 Conexant Systems Inc.
*
*****************************************************************************

  This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option)
any later version.

  This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
more details.

  You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc., 59
Temple Place, Suite 330, Boston, MA 02111-1307 USA

****************************************************************************
*  $Author: davidm $
*  $Revision: 1.1 $
*  $Modtime: 2/28/02 8:22a $
****************************************************************************/

#include <linux/time.h>
#include <linux/timex.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>

extern unsigned long cnxt_gettimeoffset(void)
{
#if 0
	u16 elapsed;

	/* 
	 * Compute the elapsed count. The current count tells us how
	 * many counts remain until the next interrupt. LATCH tells us
	 * how many counts total occur between interrupts. Subtract to
	 * get the counts since the last interrupt.
	 */
	elapsed = LATCH - inb(CX821xx_TIMBase_Addr);

	/* 
	 * Convert the elapsed count to usecs. I guess there are 'tick' usecs
	 * between every interrupt. 
	 */
	return (unsigned long)((elapsed * tick) / LATCH);
#endif
	return (unsigned long)(inl(CX821xx_TIMBase_Addr));
}

void cnxt_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
  *CNXT_INT_STATUS |= (1 << irq);
  do_timer(regs);
}


