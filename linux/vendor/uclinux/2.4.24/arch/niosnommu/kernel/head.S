/*
 * head.S for Altera's Excalibur development board with nios processor
 *
 * (c) Vic Phillips, Microtronix Datacom Ltd., 2001
 *
 * Based on the following from the Excalibur sdk distribution:
 *	NA_MemoryMap.s, NR_JumpToStart.s, NR_Setup.s, NR_CWPManager.s
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675
 * Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <asm/niosasm.h>
#include <asm/winmacro.h>
#include <asm/psr.h>

#include <linux/config.h>
#include "nios_defs.h"

#ifdef CONFIG_CRC_CHECK
/**********************************************/
/* Define where the CRC table lives in flash. */
/* The __CRC_Sector_Size is the flash sector  */
/* size for the address range.                */
/**********************************************/

	GEQU	__CRC_Table_Begin,(na_flash)+0x4000   /* Second sector of main board flash */
	GEQU	__CRC_Sector_Size,0x2000
#endif

	GEQU	NA_ResetTrap,0
	GEQU	NA_UnderflowTrap,1
	GEQU	NA_OverflowTrap,2

	GEQU	_nios_flash_start,NIOS_FLASH_START
	GEQU	_kernel_flash_start,KERNEL_FLASH_START

	GEQU	_romfs_start,LINUX_ROMFS_START

/*
 * This global variable is used as an extension to the nios'
 * STATUS register to emulate a user/supervisor mode.
 */
	.data
	.align	2
	.global	nios_status
nios_status:
	.long	0

/*
 * First executable code - detected and jumped to by the ROM bootstrap
 * if the code resides in flash (looks for "Nios" at offset 0x0c from
 * the potential executable image).
 */
	.text
	.global _start
_start:
	PFX	8			; 0x00 (%ctl8 = CLR_IE)
	WRCTL	%g0			; 0x02 disable interrupts
	NOP				; 0x04
	MOV	%g7,%o7			; 0x06 save %o7 (return address to germs)
	BSR	NR_MoveStart		; 0x08 skip signature and load %o7 with absolute address
	NOP				; 0x0a
	.byte	'N','i','o','s'		; 0x0c - Signature for ROM bootstrap to find

NR_MoveStart:
 	MOV	%l0, %o7		;  (LMA(_start) + 4 )/2 ->l0
	LSLI	%l0, 1			;  %l0=(LMA(_start) + 4 )
	ANDNIP	%l0,0xf			;  %l0 = LMA(_start)
	
	MOVIA	%l1, _start		; VMA(_start)->l1, the dest address
	
	CMP	%l0, %l1
	IFS	cc_eq			; source and des are the same, no move
	 BR	finish_move
	NOP
	
	MOVIA	%l2, __data_end		; VMA(__data_end) -> l2, the last dest
loop_move:				; l0: src, l1: dest, l2: last dest
	LD 	%g0, [%l0]		; load a word from [%l0]
	ST 	[%l1], %g0		; stort a word to dest [%l1]
	ADDI 	%l0, 4			; inc the src addr
	ADDI	%l1, 4			; inc the dest addr
	CMP	%l1, %l2;		; if finished?
	IFS	cc_lt
	 BR loop_move
	NOP
		
	MOVIA	%l1, finish_move@h	; VMA(_start)->l1
	JMP	%l1			; jmp to _start	
	NOP
finish_move:

/*
 * Set up the stack pointer, put the register window to the top,
 * initialize CWP trap vectors, and call to "main".
 *
 * If "main" returns, TRAP 0 is executed to return to ROM monitor
 */


	;---------------------
	; Preset CWP up to top
	; 
	PFX	2
	RDCTL	%g1			; g1 = contents of WVALID reg
	LSRI	%g1,1
	ANDIP	%g1,PSR_CWP		; g1 = HI_LIMIT in position for ctl0

;;;	PFX	0
;;;	RDCTL	%g0			; g0 = contents of STATUS register
;;;	ANDNIP	%g0,PSR_CWP		; mask off old CWP
	MOVIP	%g0,PSR_IE | PSR_IPRI_3	; set IPRI to 3, interupts enabled

	OR	%g0,%g1			; put in new CWP
;;;	PFX	0
	WRCTL	%g0			; put new STATUS
	NOP

	;------------------------------------
	; Disable interrupts on known devices
	;
.ifdef na_enet
.ifdef na_enet_reset
	MOVIP	%g1, na_enet_reset	; ethernet reset address
	movi	%g0,0			; reset
	st	[%g1],%g0		;
.endif
.ifdef na_enet_reset_n
	MOVIP	%g1, na_enet_reset_n	; ethernet reset address
	movi	%g0,0			; reset
	st	[%g1],%g0		;
.endif
	nop				; give it some time
	nop				;
	nop				;
	nop				;
.endif
.ifdef na_timer0
	MOVIP	%g1,na_timer0		; get timer address
	movi	%g0,0			; 
	pfx	np_timercontrol		;
	st	[%g1],%g0		; clear interrupt enable
	pfx	np_timerstatus		;
	st	[%g1],%g0		; clear interrupt condition
.endif
.ifdef na_uart0
	movi	%g0,0			; 
	MOVIP	%g1, na_uart0		; 
	pfx	np_uartcontrol		;
	st	[%g1],%g0		; clear interrupt enable
	pfx	np_uartstatus		;
	st	[%g1],%g0		; clear interrupt status
.endif
.ifdef na_uart1
	movi	%g0,0			; 
	MOVIP	%g1, na_uart1		; 
	pfx	np_uartcontrol		;
	st	[%g1],%g0		; clear interrupt enable
	pfx	np_uartstatus		;
	st	[%g1],%g0		; clear interrupt status
.endif
.ifdef na_uart2
	movi	%g0,0			; 
	MOVIP	%g1, na_uart2		; 
	pfx	np_uartcontrol		;
	st	[%g1],%g0		; clear interrupt enable
	pfx	np_uartstatus		;
	st	[%g1],%g0		; clear interrupt status
.endif
.ifdef na_uart3
	movi	%g0,0			; 
	MOVIP	%g1, na_uart3		; 
	pfx	np_uartcontrol		;
	st	[%g1],%g0		; clear interrupt enable
	pfx	np_uartstatus		;
	st	[%g1],%g0		; clear interrupt status
.endif
.ifdef na_ide_interface
	movi	%g0,1			; ATA reset
	MOVIP	%g1,na_ide_interface	; 
	st	[%g1],%g0		; write to control register
.endif
.ifdef na_enet
.ifdef na_enet_reset
	MOVIP	%g1, na_enet_reset	; ethernet reset address
	movi	%g0,1			; reset
	st	[%g1],%g0		;
.endif
.ifdef na_enet_reset_n
	MOVIP	%g1, na_enet_reset_n	; ethernet reset address
	movi	%g0,1			; reset
	st	[%g1],%g0		;
.endif
.endif

	;------------------------------------------------------
	; Zero out the .bss segment (uninitialized common data)
	;
	MOVIA	%g2,__bss_start		; presume nothing is between
	MOVIA	%g1,_end		; the .bss and _end.
	SUBI	%g1,1			; %g1 = last byte address
	MOVI	%r0,0			; %r0 (%g0) is always ST8d's source.
1:
	ST8d	[%g2],%r0
	CMP	%g2,%g1
	IFS	cc_ne
	 BR	1b
	ADDI	%g2,1			; (delay slot)

	;------------------------------------------------
	; Install the CWP Underflow/Overflow trap vectors
	; (assumes vectors are in RAM ...)
	;
	MOVIA	%g0,nasys_vector_table
	MOVIA	%g1,CWPUnderflowTrapHandler@h
	PFX	NA_UnderflowTrap
	ST	[%g0],%g1
	MOVIA	%g1,CWPOverflowTrapHandler@h
	PFX	NA_OverflowTrap
	ST	[%g0],%g1

	;---------------------------------------------
	; Set the reset trap (usually 0) to restart
	; the monitor.
	;
	MOV	%g1,%g7				; for returning to where the monitor called us from
;;;	MOVIA	%g1,nasys_reset_address@h	; set to the start address of the monitor
	PFX	NA_ResetTrap
	ST	[%g0],%g1

	;------------------------------------------------------
	; Call main() with interrupts enabled, priority level 3
	; This blocks all interrupts EXCEPT under/overflow traps
	;
	MOVIA	%g0,nios_status		; get the STATUS extension address
	bgen	%g1, PSR_SUPERVISOR_BIT	; set initial mode = supervisor
	st	[%g0], %g1
	
	MOVIA	%g1, init_task_union	; load the current task ptr
	MOVIA	%g0, _current_task
	st	[%g0], %g1

	ADDIP	%g1, 0x2000-0x40	; set stack at top of the task union
	mov	%sp, %g1		; and reserve some space for reg win

	MOVIA	%g0,main@h		; call main as a subroutine
	CALL	%g0
	 NOP

	PFX	8			; (%ctl8 = CLR_IE)
	WRCTL	%g0			; disable interrupts

	;------------------------------------------------------------------
	; If we return from main, do a Trap Zero to go back to the monitor.
	;
	TRAP	0
;vic;	MOVIP	%g0,nasys_reset_address>>1	; this will be safer than 'trap 0'
;vic;	JMP	%g0				;  (vector 0 may be corrupt)
;vic;	 NOP					;

	/* End of startup code */



	.align	2
	.global CWPUnderflowTrapHandler
CWPUnderflowTrapHandler:

; Current state:
; CWP=LO_LIMIT has just performed a SAVE, which completes normally
; except that now CWP=LO_LIMIT-1, which is not allowed.
; So, we wind up here, and we shall make CWP=HI_LIMIT-1 seem like CWP=LO_LIMIT-1.

;	CWP = LO_LIMIT-1 (usually 0) - new %l's and %o's
;	%o6 (%sp) points to interrupted process's new and virgin stack frame
;	%o7 (%r15) is the interrupted process's return address

; As we march up the CWP frames, only the %g's can be relied on, as they're fixed.

; Save the %g registers in the interrupted process's new stack frame.
; Don't need to save the %l's because none have been used yet

	sts	[%sp,0],%g0		; save the %g's where the %l's usually go
	sts	[%sp,1],%g1
	sts	[%sp,2],%g2
	sts	[%sp,3],%g3
	sts	[%sp,4],%g4
	sts	[%sp,5],%g5
	sts	[%sp,6],%g6
	sts	[%sp,7],%g7

	sts	[%sp, 8],%i0		; save the %i's (these are restored later)
	sts	[%sp, 9],%i1
	sts	[%sp,10],%i2
	sts	[%sp,11],%i3
	sts	[%sp,12],%i4
	sts	[%sp,13],%i5
	sts	[%sp,14],%i6
	sts	[%sp,15],%i7

	mov	%g6,%sp			; %g6 = stack ptr (%o6)
	mov	%g7,%o7			; %g7 = return address

	; Figure out HI- and LO-limit...

	pfx	2			; read WVALID 
	rdctl	%g2
	lsri	%g2,1
	ANDIP	%g2,PSR_CWP		; %g2 = CWP HI_LIMIT, in CTL 0 position

	; Store the %l's and %i's on the stack in the 16 words that every routine provides.
1:
	restore				; move to previous reg window (CWP += 1)
	STORE_WINDOW			; save the %l's and %i's at this CWP's stack ptr
;;;	pfx	0			; read STATUS
	rdctl	%g0
	ANDIP	%g0,PSR_CWP		; isolate CWP
	cmp	%g0,%g2			; compare to HI_LIMIT
	BNE	1b			; not reached yet
	 nop

	; Now, get LO_LIMIT's %o's (LO_LIMIT-1'S %i's) into HI_LIMIT's %i's.
	; CTL 0 is at HI_LIMIT and %g6 still points to the stashed registers.

	; Modify CTL 1, which is the saved status register, so that
	; the only difference upon return that instead of CWP = 0, we're
	; at CWP = HI_LIMIT...
	pfx	1			; read ISTATUS
	rdctl	%g0			; %g0 has status stored before this rude interrupt
	ANDNIP	%g0,PSR_CWP		; CTL 0 with old CWP masked out
	or	%g0,%g2			; Insert HI_LIMIT
	pfx	1			; write ISTATUS
	wrctl	%g0			; Now, a TRET will land here.

	; Restore the stack pointer and return address from CWP=0...
	mov	%sp,%g6
	mov	%o7,%g7

	lds	%g0,[%sp,0]		; restore the %g's
	lds	%g1,[%sp,1]
	lds	%g2,[%sp,2]
	lds	%g3,[%sp,3]
	lds	%g4,[%sp,4]
	lds	%g5,[%sp,5]
	lds	%g6,[%sp,6]
	lds	%g7,[%sp,7]

	lds	%i0,[%sp, 8]		; restore the %i's
	lds	%i1,[%sp, 9]
	lds	%i2,[%sp,10]
	lds	%i3,[%sp,11]
	lds	%i4,[%sp,12]
	lds	%i5,[%sp,13]
	lds	%i6,[%sp,14]
	lds	%i7,[%sp,15]

	tret	%o7			; return to interrupted process


	.align	2
	.global CWPOverflowTrapHandler
CWPOverflowTrapHandler:

	; After RESTORE-ing too many times, we get
	; a trap that puts us right here.
	;
	; We get here with CWP=HI_LIMIT and ISTATUS CWP=HI_LIMIT+1.
	; We should leave here with CWP=LO_LIMIT, and lots of registers
	; restored from how we stashed em on the stack.
	;
	; Since %i6 was the stack pointer at CWP=HI_LIMIT,
	; it will now be %o6 here at CWP=HI_LIMIT+1.
	;
	; Up at CWP=HI_LIMIT+1, there are no %l or %i registers.
	; The %l's alias to the %g's, in fact.
	;

	; Start by saving the globals, since that's our only safe space...

	; Interrupted process has done a RESTORE which means they're done
	; with %l's, most of %o's (except o6,o7), and the stack frame to which
	; %sp (%o6) points.  We can use this abandoned stack frame to save
	; the %g's and %i's which need to be restored.

; Save the %g registers in the interrupted process's old stack frame.
; Don't need to save the %l's because all have been abandoned.

	sts	[%sp,0],%g0		; save the %g's (where %l's usually go)
	sts	[%sp,1],%g1
	sts	[%sp,2],%g2
	sts	[%sp,3],%g3
	sts	[%sp,4],%g4
	sts	[%sp,5],%g5
	sts	[%sp,6],%g6
	sts	[%sp,7],%g7

	sts	[%sp, 8],%i0		; save the %i's (these are restored later)
	sts	[%sp, 9],%i1
	sts	[%sp,10],%i2
	sts	[%sp,11],%i3
	sts	[%sp,12],%i4
	sts	[%sp,13],%i5
	sts	[%sp,14],%i6
	sts	[%sp,15],%i7

	mov	%g6,%sp			; save stack ptr (%o6)
	mov	%g7,%o7			; save return address

	; Figure out HI- and LO-limit...
	pfx	2			; read WVALID
	rdctl	%g0
	mov	%g1,%g0
	lsli	%g1,4			; %g1 in position to mask into CTL 0
	ANDIP	%g1,PSR_CWP		; %g1 = CWP LO_LIMIT, in CTL 0 position
	mov	%g2,%g0
	lsri	%g2,1
	ANDIP	%g2,PSR_CWP		; %g2 = CWP HI_LIMIT, in CTL 0 position

	; Move to CWP=LO_LIMIT-1 and start climbing...
;;;	pfx	0
	rdctl	%g0			; read STATUS
	ANDNIP	%g0,PSR_CWP		; mask off CWP
	subi	%g1,0x0010		; %g1 = LO_LIMIT-1
	or	%g0,%g1			; put into CWP
;;;	pfx	0
	wrctl	%g0
	nop

	mov	%sp,%g6			; Get stack pointer
	lds	%i7,[%sp,15]		; load up the %i's from CWP=HI_LIMIT
	lds	%i6,[%sp,14]
	lds	%i5,[%sp,13]
	lds	%i4,[%sp,12]
	lds	%i3,[%sp,11]
	lds	%i2,[%sp,10]
	lds	%i1,[%sp, 9]
	lds	%i0,[%sp, 8]


	; The first  RESTORE puts CWP=LO_LIMIT and the %o's ready for action.
	; This includes %sp, so CWP=LO_LIMIT can load up its %l's and %i's, and so on.
1:
	restore				; CWP += 1, next %sp
	LOAD_WINDOW			; Restore %l's & %i's from where %sp points.
;;;	pfx	0
	rdctl	%g0			; read STATUS
	ANDIP	%g0,PSR_CWP		; isolate CWP
	cmp	%g2,%g0			; reached HI_LIMIT ?
	BNE	1b			; not yet - keep going
	 nop

	; Set CWP = LO_LIMIT-1 so we can load up his %i's
;;;	pfx	0			; read STATUS
	rdctl	%g0
	ANDNIP	%g0,PSR_CWP		; mask out CWP
	or	%g0,%g1			; put LO_LIMIT-1 into CWP
;;;	pfx	0			; write STATUS
	wrctl	%g0
	nop

	; modify CTL 1 (ISTATUS) so that TRET takes us back to CWP=LO_LIMIT
	pfx	1			; read saved status (ISTATUS)
	rdctl	%g0
	ANDNIP	%g0,PSR_CWP		; mask off saved CWP
	addi	%g1,0x0010		; get LO_LIMIT
	or	%g0,%g1			; put into saved CWP
	pfx	1			; write ISTATUS
	wrctl	%g0

	mov	%o7,%g7			; retrieve return address
	mov	%sp,%g6			; retrieve saved stack pointer

	lds	%g7,[%sp,7]		; restore %g's from where %l's are usually stored
	lds	%g6,[%sp,6]
	lds	%g5,[%sp,5]
	lds	%g4,[%sp,4]
	lds	%g3,[%sp,3]
	lds	%g2,[%sp,2]
	lds	%g1,[%sp,1]
	lds	%g0,[%sp,0]

	tret	%o7
