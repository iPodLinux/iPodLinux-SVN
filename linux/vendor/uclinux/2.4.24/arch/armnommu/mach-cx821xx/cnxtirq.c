/****************************************************************************
*
*	Name:			cnxtirq.c
*
*	Description:	
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
*  $Modtime: 9/24/02 8:43a $
****************************************************************************/

/* cnxtirq.c - CNXT interrupt stuff */

#include <asm/arch/bsptypes.h>
#include <asm/arch/bspcfg.h>
#include <asm/arch/cnxtbsp.h>
#include <asm/arch/OsTools.h>

#include <asm/system.h>
#include <linux/module.h>

void intUnlock( UINT32 oldlevel )
{
  // Enables ARM IRQ interrupt
  restore_flags( oldlevel );
}

UINT32 intLock( void )
{
  // Disables ARM IRQ interrupt

  UINT32	irqlevel;
  save_flags_cli(irqlevel);
  return irqlevel;
}

BOOL intContext( void )
{
	return FALSE;
}


void PICClearIntStatus(UINT32 IntSource)
{
   HW_REG_WRITE (PIC_TOP_ISR_IRQ, (1 <<IntSource) );
}

EXPORT_SYMBOL(intLock);
EXPORT_SYMBOL(intUnlock);
