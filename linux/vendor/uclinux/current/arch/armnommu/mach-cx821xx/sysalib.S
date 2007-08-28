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

#include <linux/linkage.h>
#include <linux/config.h>
#include <asm/proc/assembler.h>
#include <asm/proc/ptrace.h>
#include <asm/constants.h>
#include <asm/procinfo.h>
#include <asm/hardware.h>
#include <asm/arch/bspcfg.h>
#include <asm/arch/cnxtbsp.h>
#include <asm/arch/hstif.h>
#include "plldef.h"

/* internals */


	.globl	_sysInit		/* start of system code */
    .globl  _sysCacheEnable
    .globl  _sysCacheDisable
    .globl  _sysGetDeviceType

	.text
	.balign 4


/*******************************************************************************
*
* sysInit 
*
* NOTE:
*
* RETURNS:	N/A
*
*******************************************************************************/

ENTRY(sysInit)

	STMDB	R13!, { R0-R4, LR }

	/* Disable device interrupts */
	LDR	r2, =PIC_TOP_IMR_IRQ       /* R2->interrupt controller */
	LDR	r1, =PIC_DISABLE_MASK
	STR	r1, [r2]

	/* Return value is in R0, do not care if R14 is changed */

#ifndef DEVICE_YELLOWSTONE
	BL _sysGetDeviceType
	MOV R3,R0
	BL _sysGetBusModeForDevice

	/* Initial MMU value is fastbus */
	MOV	r2, #MMU_INIT_VALUE

	/* Setup MMU Control Register */
	/* r0 contains the bus mode (async or fast bus) */
	ORR	r2, r2, r0					@ asynchronous clock mode
	MCR	p15, 0, r2, c1, c0, 0		@ Write to MMU CR
#endif

#ifndef DEVICE_YELLOWSTONE
	BL  _sysCacheDisable

	#if PLL_CONTROL
	    MOV R0, R3						@ Pass in device type
	    BL _sysGetPLLFValForDevice

	 	LDR	r2, =PLL_F
		STR	r0, [r2]
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP

	    MOV R0, R3					@ Pass in device type
	    BL _sysGetPLLBValForDevice
	    LDR	r2, =PLL_B
		STR	r0, [r2]

		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
    #endif  /* PLL_CONTROL */

	#if HOSTINTERFACE_CONTROL
		/* Switch boot map to run map (RAM at address 0000),*/
	    /* and set SDRAM size appropriately. */
		LDR	r2, =HST_CTRL
	    
		#if (PHYS_SDRAM_SIZE == 8)
			MOV	r1, #HSTCTRL_RUN_MAP | HSTCTRL_8MEG
		#elif (PHYS_SDRAM_SIZE == 4)
			MOV	r1, #HSTCTRL_RUN_MAP | HSTCTRL_4MEG
		#else
			MOV	r1, #HSTCTRL_RUN_MAP
	    #endif

	    STR r1, [r2]
		NOP								@ just in case
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
		NOP
	#endif /* end of HOSTINTERFACE_CONTROL */

	BL  _sysGetCacheType	@ R0 contains cache type when function returns
    BL  _sysCacheEnable

#endif	/* end of DEVICE_YELLOWSTONE */
	LDMIA	R13!, { R0-R4, PC }


