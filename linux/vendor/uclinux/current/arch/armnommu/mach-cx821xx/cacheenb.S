/****************************************************************************
*
*	Name:			sysalib.S
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
*  $Modtime:  $
****************************************************************************/

/*
NOTE
The routines in this module don't use the "C" frame pointer %r11@ ! or
establish a stack frame.

*/


#define _ASMLANGUAGE

#include <asm/proc/assembler.h>

/* internals */


	.globl  cache_on
	.globl  cache_off

	.text
	.balign 4


/* No args */

	.code	16	
	.balign	4	
	.thumb_func	
	

cache_on:
	BX	pc	
	NOP		
	.code	32	

	mrc	p15, 0, r0, c1, c0		@ read control register
	orr	r0, r0, #0x0004			@ Enable D cache
	orr	r0, r0, #0x1000			@ I Cache on
	mcr	p15, 0, r0, c1, c0		@ update control register


#if	((CPU == ARM7TDMI_T) || (CPU == ARMARCH4_T))
	BX	lr
#else
	MOV	pc, lr
#endif	/* (CPU == ARM7TDMI_T) */




cache_off:
	BX	pc	
	NOP		
	.code	32	


	mrc	p15, 0, r0, c1, c0		@ read control register
	bic	r0, r0, #0x0004			@ DCache
	bic	r0, r0, #0x1000			@ ICache
	mcr	p15, 0, r0, c1, c0		@ disabled

	
#if	((CPU == ARM7TDMI_T) || (CPU == ARMARCH4_T))
	BX	lr
#else
	MOV	pc, lr
#endif	/* (CPU == ARM7TDMI_T) */





