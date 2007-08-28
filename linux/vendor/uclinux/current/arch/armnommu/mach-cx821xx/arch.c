/****************************************************************************
*
*	Name:			arch.c
*
*	Description:	Architecture specific fixups.  This is where any
*  					parameters in the params struct are fixed up, or
*  					any additional architecture specific information
*  					is pulled from the params struct.
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
*  $Modtime: 2/28/02 7:49a $
****************************************************************************/

#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>

#include <asm/elf.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>

extern void cnxt_map_io(void);
extern void cnxt_init_irq(void);
extern void genarch_init_irq(void);


MACHINE_START(CX821XX, "CNXT CX821XX")
       BOOT_MEM(0xabcd1234, 0x56789abc, 0x12345678)
       MAINTAINER("Conexant")
       INITIRQ(genarch_init_irq)
MACHINE_END


