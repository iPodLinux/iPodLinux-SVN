/****************************************************************************
*
*	Name:			irq.c
*
*	Description:	Interrupt control functions
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
*  $Modtime: 10/25/02 8:22a $
****************************************************************************/

#include <linux/module.h>
#include <linux/init.h>

#include <asm/mach/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

 
inline void cnxt_irq_ack(int irq)
{
  *CNXT_INT_STATUS |= (1 << irq);
}

  
inline void cnxt_mask_irq(unsigned int irq)
{
  /* reset cooresponding bit in the 32bit Int mask register */ 
  *CNXT_INT_MASK &= ~(1 << irq);
}


inline void cnxt_unmask_irq(unsigned int irq)
{
  /* set cooresponding bit in the 32bit Int mask register */ 
  *CNXT_INT_MASK |= (1 << irq);
}
 

inline void cnxt_mask_ack_irq(unsigned int irq)
{
  cnxt_mask_irq(irq);
  /* this is ok if int source is not hooked on GPIO */
  *CNXT_INT_STATUS |= (1 << irq);
}

int cnxt_irq_mask_state_chk( unsigned int irq_num )
{
	if( *CNXT_INT_MASK & ( 1 << irq_num ))
	{
		// IRQ is enabled
		return 1;
	}
	// IRQ is disabled
	return 0;
}


EXPORT_SYMBOL(cnxt_unmask_irq);
EXPORT_SYMBOL(cnxt_mask_irq);
EXPORT_SYMBOL(cnxt_irq_mask_state_chk);



