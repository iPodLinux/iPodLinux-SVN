/****************************************************************************
*
*	Name:			SYSDEV.S
*
*	Description:	
*
*	Copyright:		(c) 1999 Conexant Systems Inc.
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

*****************************************************************************
*  $Author: davidm $
*  $Revision: 1.1 $
*  $Modtime:    $
****************************************************************************/

#define _ASMLANGUAGE

#include <linux/linkage.h>
#include <asm/proc/assembler.h>
#include <asm/arch/bspcfg.h>
#include <asm/arch/cnxtbsp.h>
#include <asm/arch/hstif.h>
#include "plldef.h"

        .data
    .globl  _sysCacheEnable
    .globl  _sysCacheDisable
    .globl  _sysGetDeviceType
	.globl  _sysGetBusModeForDevice
    .globl  _sysGetPLLBValForDevice
    .globl  _sysGetPLLFValForDevice
    .globl  _sysGetCacheType
	.globl	_sysSoftReset
	.text
	.balign 4

/*******************************************************************************
*
* sysGetDeviceType - determines device type
*
* This function determines the device type and stores it in R3
* NOTE:		This routine should not be called by the user.
*
* RETURNS:	N/A
*******************************************************************************/

#define P5X00_DEV_ID_MASK			0xF000000F
#define RUSH_X10_45_DEV_ID_MASK		0xFFFF0000

#define CACHE_WT 0
#if 0
	#define CACHE_WB 1
#else
	#define CACHE_WB 0
#endif

#define   DEVICE_P5200X10		0
#define   DEVICE_P5200X180		1
#define   DEVICE_P5300			2
#define   DEVICE_RUSHMORE		3
#define   DEVICE_RUSHMORE_X45	4
#define   DEVICE_RUSHMORE_X50	5
#define   DEVICE_OLDFAITHFUL    6

	/*
    ******************************************************************************
    ** This routine must have the device in the run-time map mode to work properly
    ******************************************************************************
    */

ENTRY(_sysGetDeviceType)
#ifdef DEVICE_YELLOWSTONE
	MOV R0,#DEVICE_OLDFAITHFUL		/* 6 */
#else
	LDR	r2, =DEVICE_ID_REG
	LDR	r3, [r2]

	MOV R0,#DEVICE_P5200X10			/* 0 */
	LDR r2,=P5X00_DEV_ID_MASK
	AND r2,r3,r2
	LDR R1,=P5200X10_DEV_ID
	CMP	R1,R2
	BEQ finish

	MOV R0,#DEVICE_P5200X180		/* 1 */
	LDR r2,=P5X00_DEV_ID_MASK
	AND r2,r3,r2
	LDR R1,=P5200X180_DEV_ID
	CMP	R1,R2
	BEQ finish

	MOV R0,#DEVICE_P5300		    /* 2 */
	LDR r2,=P5X00_DEV_ID_MASK
	AND r2,r3,r2
	LDR R1,=P5300_DEV_ID
	CMP	R1,R2
	BEQ finish

	LDR r2,=RUSH_X10_45_DEV_ID_MASK
	AND r2,r3,r2
	LDR R1,=RUSH_X10_45_DEV_ID
	CMP	R1,R2
	BNE checkRush50
	LDR	r1, =ROM_DEVICE_ID_REG
	LDR	r2, [r1]
	MOV R1,#0
	MOV R0,#DEVICE_RUSHMORE		    /* 3 */
	CMP	R1,R2
	BEQ finish
	MOV R0,#DEVICE_RUSHMORE_X45		/* 4 */
	B finish

checkRush50:
	MOV R0,#DEVICE_RUSHMORE_X50		/* 5 */
#endif

finish:
	MOV	pc, lr

/*********************************************************************
 *********************************************************************/
/* Device type required in R0 */
ENTRY(_sysCacheEnable)

	STMDB	R13!, { R0-R4, LR }

#if defined(CONFIG_ARCH_CX821XX) || defined(CONFIG_ARCH_P52)

    /*
     * Cache type for device already determined at this point. R0 contains
	 * either CACHE_WB or CACHE_WT. Move it to r3
	 */
	
	MOV 	r3, r0

    ldr     R1,=0x0000003F     @ ; Cacheable 0x00000000 ~ 0xFFFFFFFF (4 GB)
    mcr     p15,0,R1,c6,c0,1   @ ; Inst Region 0 (C0)

    ldr     R1,=0x0000003F     @ ; non-Cacheable 0x00000000 ~ 0xFFFFFFFF (4Gb) Background region
    mcr     p15,0,R1,c6,c0,0   @ ; Data Region 0 (C0)

	/*
	* Control GBd bit (set to 0)
	*
	* Enable data bufferable enables write-back when cache is enabled,
	* and disable data bufferable enables write-through when cache is enabled.
	*/
	mrc     p15,0,R0,c3,c0,0
	and		r0,r0,#0xFD		   @ ; Mask off the data region 1 bit
	mov		r1,r3,LSL #1	   @ ; R3 contains WB or WT 	
	orr     R0,R0,R1		   @ ; Data region 1, enable data bufferable
	mcr     p15,0,R0,c3,c0,0

	/*
	* Region size encodings
	* 0x25 = 0.5M
	* 0x27 = 1M
	* 0x29 = 2M
	* 0x2B = 4M
	* 0x2D = 8M
	*
	* NOTE: The higher the region number the higher the priority of it's
	*		attribute settings.  For example if the entire 8Mb of SDRAM is
	* 		set to be cacheable by the region 1 settings, but the region
	*		2 settings make 512k of some address of the 8Mb non-cacheable, then
	*		those settings override those of region 1 for that specified address
	*		range.
	*/


    ldr     R1,=0x0080002D	   @ ; Cacheable 0x00800000 ~ 0x0FFFFFF (8Mb) 
    mcr     p15,0,R1,c6,c1,0   @ ; Data Region 1 (C1)

    ldr     R1,=0x00F80025     @ ; non-Cacheable 0x00F80000 ~ 0x00FFFFFF (512kb)
    mcr     p15,0,R1,c6,c2,0   @ ; Data Region 2 (C2)

    mov     R1,#0x01           @ ; (C0 means nothing here)
    mcr     p15,0,R1,c2,c0,1   @ ; enable Inst Region 0 , disable Region 1~7

	mov     R1,#0x02
    mcr     p15,0,R1,c2,c0,0   @ ; enable caching Data regions 1, disable caching regions 0,2-7

    mov     R1,#0x3
    mcr     p15,0,R1,c5,c0,1   @ ; enable full access on Inst Region 0 

    mov     R1,#0x3F
    mcr     p15,0,R1,c5,c0,0   @ ; enable full access on Data Regions 0~2

/* Read, Modify, and Write the control register now */

	mrc     p15, 0, R0, c1, c0, 0	@ get control register
	orr		r0, r0, #0x0001			@ Enable Protection Unit
		
	#ifdef CONFIG_CPU_ARM940_D_CACHE_ON
		orr	r0, r0, #0x0004			@ Enable D cache
	#endif

	#ifdef CONFIG_CPU_ARM940_I_CACHE_ON
		orr	r0, r0, #0x1000			@ I Cache on
	#endif

	mcr	p15, 0, r0, c1, c0		@ Update the control register now
								@ with all of the new settings
#endif

	LDMIA	R13!, { R0-R4, PC }

/***************************************************************************
 * sysCacheDisable - Flush and disable I and D caches
 **************************************************************************/
ENTRY(_sysCacheDisable)
	/*
	* We would like to discard the contents of the Write-Buffer
	* altogether, but there is no facility to do this. Failing that,
	* we do not want any pending writes to happen at a later stage,
	* so drain the Write-Buffer, i.e. force any pending writes to
	* happen now. Then flush, i.e. invalidate, all cache(s).
	*/
	LDR	r1, =SYSTEM_CACHE_UNCACHED_ADRS	/* R1 -> uncached area */
	LDR	r1, [r1]						/* drain write-buffer  */

    /*********************************/
	/* Clean and flush D cache       */
    /*********************************/
	MOV	r1, #0					/* Initalize line counter, r1 */
Flushouter_loop:
	MOV	r0, #0					/* Initialize segment counter, r0 */
Flushinner_loop:
	ORR	r2, r1, r0				/* Make segement and line address */
	MCR	p15, 0, r2, c7, c14, 1	/* Clean and flash that line */
	ADD	r0, r0, #0x10			/* Increment segment counter */
	CMP	r0, #0x40				/* Completed all four segments? */
	BNE	Flushinner_loop
	ADD	r1, r1, #0x04000000		/* Increment line counter */
	CMP	r1, #0x0				/* Completed all lines? */
	BNE	Flushouter_loop

    /*********************************/
	/* Disable D cache			     */
    /*********************************/
	MRC     p15, 0, R0, c1, c0, 0
	BIC     R0, R0, #0x4
	MCR     p15, 0, R0, c1, c0, 0

    /*********************************/
	/* Flush I cache			     */
    /*********************************/
	MOV	r1, #0
	MCR	p15, 0, r1, c7, c5, 0

    /*********************************/
	/* Disable I cache			     */
    /*********************************/
	MRC     p15, 0, R0, c1, c0, 0
	BIC     R0, R0, #0x1000
	MCR     p15, 0, R0, c1, c0, 0 

    NOP
    NOP
    NOP
    NOP
    NOP
    NOP

    /*********************************/
	/* Disable Protection unit       */
    /*********************************/
	MRC     p15, 0, R0, c1, c0, 0
	BIC     R0, R0, #0x1
	MCR     p15, 0, R0, c1, c0, 0

    /*********************************/
	/* Disable all memory areas	     */
    /*********************************/
	MOV	r0, #0
	MCR     p15,0,R0,c6,c0,1		/* instruction, area 0 */
	MCR     p15,0,R0,c6,c0,0		/* data, area 0 */
	MCR     p15,0,R0,c6,c1,1		/* instruction, area 1 */
	MCR     p15,0,R0,c6,c1,0		/* data, area 1 */
	MCR     p15,0,R0,c6,c2,1		/* instruction, area 2 */
	MCR     p15,0,R0,c6,c2,0		/* data, area 2 */
	MCR     p15,0,R0,c6,c3,1		/* instruction, area 3 */
	MCR     p15,0,R0,c6,c3,0		/* data, area 3 */
	MCR     p15,0,R0,c6,c4,1		/* instruction, area 4 */
	MCR     p15,0,R0,c6,c4,0		/* data, area 4 */
	MCR     p15,0,R0,c6,c5,1		/* instruction, area 5 */
	MCR     p15,0,R0,c6,c5,0		/* data, area 5 */
	MCR     p15,0,R0,c6,c6,1		/* instruction, area 6 */
	MCR     p15,0,R0,c6,c6,0		/* data, area 6 */
	MCR     p15,0,R0,c6,c7,1		/* instruction, area 7 */
	MCR     p15,0,R0,c6,c7,0		/* data, area 7 */

	MOV	pc, lr

/***************************************************************************
 * sysGetCacheType - Gets the data cache type (Writethrough or Writeback)
 *					 for the device in R0
 **************************************************************************/
ENTRY(_sysGetCacheType)

/*
 * !!!!W A R N I N G!!!!!
 * DO NOT TRY TO OPTIMIZE THIS BY MAKING IT INTO A TABLE
 * VARIABLES WILL NOT WORK HERE!
 */

   /*
    * Add 1 to device type in R0. Keep subtracting 1 later, when difference
	* reaches zero, we have found our device.
	*/
   ADD r1, r0, #1

   /* Assume Cache Writethrough as the default type */
   MOV r0, #CACHE_WT

#ifdef BUILD_BOOTROM
   /* If we are building the boot rom, then return writethrough no matter which device */
   B GetCacheTypeExit
#else

   SUBS r1, r1, #1				/* Device == 0, P52 CACHE_WT */
   BEQ GetCacheTypeExit

   SUBS r1, r1, #1				/* Device == 1, P52-X18 CACHE_WT */
   BEQ GetCacheTypeExit

   SUBS r1, r1, #1				/* Device == 2, P53 CACHE_WB*/
   MOVEQ r0, #CACHE_WB
   BEQ GetCacheTypeExit

   SUBS r1, r1, #1				/* Device == 3, RUSHMORE CACHE_WB */
   MOVEQ r0, #CACHE_WB
   BEQ GetCacheTypeExit

   SUBS r1, r1, #1				/* Device == 4, RUSHMORE X4.5 CACHE_WB */
   MOVEQ r0, #CACHE_WB
   BEQ GetCacheTypeExit

   SUBS r1, r1, #1				/* Device == 5, RUSHMORE X5 CACHE_WB */
   MOVEQ r0, #CACHE_WB
   BEQ GetCacheTypeExit

#endif /* BUILD_BOOTROM */
   
GetCacheTypeExit:
	MOV	pc, lr


/********************************************************************************
 * sysGetBusModeForDevice: Pass in device type, this translates it and returns
 *                         the correspoding bus mode for that device
 *******************************************************************************/
ENTRY(_sysGetBusModeForDevice)

/*
 * !!!!W A R N I N G!!!!!
 * DO NOT TRY TO OPTIMIZE THIS BY MAKING IT INTO A TABLE 
 * VARIABLES WILL NOT WORK HERE!
 */
 
   /*
    * Add 1 to device type in R0. Keep subtracting 1 later, when difference
	* reaches zero, we have found our device.
	*/

   ADD r1,r0,#1

   /* Assume MMUCR_ASYNC mode since that is the majority. Only Rushmore uses Fastbus for now */
   MOV r0,#MMUCR_ASYNC

   SUBS r1,r1,#1		    /* Was Device Type equal to 0? */
   BEQ GetBusModeExit

   SUBS r1,r1,#1     		/* Was Device Type equal to 1? */
   BEQ GetBusModeExit

   SUBS r1,r1,#1    		/* Was Device Type equal to 2? */
   BEQ GetBusModeExit

   SUBS r1,r1,#1
   MOVEQ r0,#MMUCR_FB		/* Was Device Type equal to 3? */
   BEQ GetBusModeExit

   SUBS r1,r1,#1    		/* Was Device Type equal to 4? */
   BEQ GetBusModeExit

   SUBS r1,r1,#1    		/* Must've been 5 then		   */
   BEQ GetBusModeExit

   /* Default to FastBus */
   MOV r0, #MMUCR_FB

GetBusModeExit:
	MOV	pc, lr

/********************************************************************************
 * sysGetPLLFValForDevice: Pass in device type, this translates it and returns
 *                         the correspoding PLL_F Register divisor for that device
 *******************************************************************************/
ENTRY(_sysGetPLLFValForDevice)

/*
 * !!!!W A R N I N G!!!!!
 * DO NOT TRY TO OPTIMIZE THIS BY MAKING IT INTO A TABLE
 * VARIABLES WILL NOT WORK HERE!
 */

   /*
    * Add 1 to device type in R0. Keep subtracting 1 later, when difference
	* reaches zero, we have found our device.
	*/
   ADD r1,r0,#1
   SUBS r1,r1,#1

   LDREQ r0,=P52_PLL_FAST_144MHZ			/* Was Device Type equal to 0? */
   BEQ GetPLLFExit

   SUBS r1,r1,#1
   LDREQ r0,=P52_PLL_FAST_168MHZ			/* Was Device Type equal to 1? */
   BEQ GetPLLFExit

   SUBS r1,r1,#1
   LDREQ r0,=P52_PLL_FAST_168MHZ			/* Was Device Type equal to 2? */
   BEQ GetPLLFExit

   SUBS r1,r1,#1
   LDREQ r0,=RUSHMORE_PLL_FAST_96MHZ		/* Was Device Type equal to 3? */
   BEQ GetPLLFExit

   SUBS r1,r1,#1
   LDREQ r0,=RUSHMORE_PLL_FAST_168MHZ		/* Was Device Type equal to 4? */
   BEQ GetPLLFExit

   SUBS r1,r1,#1
   LDREQ r0,=RUSHMORE_PLL_FAST_168MHZ		/* Must've been 5 then		   */
   BEQ GetPLLFExit

   /* Default = ERROR!!*/
   MOV r0, #0x0 
GetPLLFExit:
	MOV	pc, lr

/********************************************************************************
 * sysGetPLLBValForDevice: Pass in device type, this translates it and returns
 *                         the correspoding PLL_B Register divisor for that device
 *******************************************************************************/
ENTRY(_sysGetPLLBValForDevice)

/*
 * !!!!W A R N I N G!!!!!
 * DO NOT TRY TO OPTIMIZE THIS BY MAKING IT INTO A TABLE
 * VARIABLES WILL NOT WORK HERE!
 */
 
   /*
    * Add 1 to device type in R0. Keep subtracting 1 later, when difference
	* reaches zero, we have found our device.
	*/

   ADD r1,r0,#1

   /* For each P5x device, we set to 100MHZ */
   LDR r0,=P52_PLL_FAST_100MHZ

   SUBS r1,r1,#1                			/* Was Device Type equal to 0? */
   BEQ GetPLLBExit

   SUBS r1,r1,#1							/* Was Device Type equal to 1? */
   BEQ GetPLLBExit

   SUBS r1,r1,#1							/* Was Device Type equal to 2? */
   BEQ GetPLLBExit

   /* Wasn't P5x, so it's Rushmore. All Rushmores are also 100MHZ */
   LDR r0,=RUSHMORE_PLL_FAST_100MHZ

   SUBS r1,r1,#1							/* Was Device Type equal to 3? */
   BEQ GetPLLBExit

   SUBS r1,r1,#1							/* Was Device Type equal to 4? */
   BEQ GetPLLBExit

   SUBS r1,r1,#1							/* Must've been 5 then		   */
   BEQ GetPLLBExit

   /* Default = ERROR!!*/
   MOV r0, #0x0
  
GetPLLBExit:
	MOV	pc, lr

/***************************************************************************
 * sysSoftReset - Switch from Run Map to Boot Map then force SW reset
 *
 **************************************************************************/
ENTRY(_sysSoftReset)
	/* Switch run map to boot map */
	LDR	r2, =HST_CTRL
    
	#if (PHYS_SDRAM_SIZE == 8)
		MOV	r1, #HSTCTRL_8MEG
	#elif (PHYS_SDRAM_SIZE == 4)
		MOV	r1, #HSTCTRL_4MEG
	#else
		MOV	r1, #0x00
    #endif

    STR r1, [r2]

    /* Wait for system to stabilize to be safe */
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

    BL _sysCacheDisable

    /* Force jump back to Reset vector */
	MOV r0, #0x0
  	MOV	pc, r0








