; Modified for uClinux - Vic - Apr 2002
; From: 
   
;
; Assembly language portions of Nios GDB Stub
;
; Stack set up, register clearing, all kinds of
; clever things.
;

#ifndef __KERNEL__
	.include "nios.s"
#else
	#include <asm/niosasm.h>
#endif

.ifdef ETHER_DEBUG
.ifdef na_enet
	.equ	ethernet_exists, 1
.endif
.endif

; Manual additions .. - AF 9/4/2000
; Eventually these items will be in the SDK.

  GEQU na_BreakpointTrap     , 3
  GEQU na_SingleStepTrap     , 4


; End of manual additions.
	;
	; Word size (32-bit) offsets
	; for register storage
	;

	.equ	gdbRegistersGeneral,0
	.equ	gdbRegistersPC,32
.if __nios32__
	.equ	gdbRegistersCtl0Ctl1,33
	.equ	gdbRegistersCtl2Ctl3,34
	.equ	gdbTrapNumber,35		; ISR can report trap number here
.else
	.equ	gdbRegistersCtl0,34
	.equ	gdbRegistersCtl1,35
	.equ	gdbRegistersCtl2,36
	.equ	gdbRegistersCtl3,37
	.equ	gdbTrapNumber,38
.endif


	.text

	.global	StubBreakpointHandler
	.global	StubHarmlessHandler
	.global	StubButtonHandler
	.global StubHWBreakpointHandler
	.global	GDBMain

.ifdef ETHER_DEBUG
.ifdef ethernet_exists
	.comm	_gdb_stub_stack,8192,4	; Local stack, statically allocated.
	.equ	gdbStubStacktop,_gdb_stub_stack+7936
.else
	.comm	_gdb_stub_stack,1024,4	; Local stack, statically allocated.
	.equ	gdbStubStacktop,_gdb_stub_stack+992
.endif
.else
	.comm	_gdb_stub_stack,1024,4	; Local stack, statically allocated.
	.equ	gdbStubStacktop,_gdb_stub_stack+992
.endif



;
; If nios_gdb_install(0) is invoked, the stub is present,
; and linked in, but harmless. This, so that you can "turn off"
; the debugger with minimal change in your code footprint.
;
; This is the harmless breakpoint handler.

StubHarmlessHandler:
	TRET	%o7		; pretty darned minimal. Better stuff below.

; StubUart Handler checks to see if we are getting a stop requrest from the
; host.  If we are it jumps directly to the breakpoint stub. Otherwise
; it returns after clearing the interrupt

.ifdef nasys_debug_uart
	.equ	gdbBreakChar,0x3
	.global	StubUartHandler

StubUartHandler:
	;verify Rx interrupt
	MOVIA	%l0, nasys_debug_uart
	LDP		%l1, [%l0,np_uartcontrol]
	SKP0	%l1, np_uartcontrol_irrdy_bit
	BR		StubUartRx
	NOP
	TRET	%o7

StubUartRx:
	LDP		%l1, [%l0,np_uartrxdata]	;clears interrupt bit
	CMPI	%l1, gdbBreakChar
	SKPS	cc_nz
	BR		StubBreakpointHandler
	NOP
	TRET	%o7
.endif

;
; The Hardware Breakpoint needs to do a few additional thing
; before it enters the regular breakpoint handler

.if nasys_debug_core
StubHWBreakpointHandler:
	; write to stop register to halt trace
	MOVI	%l1, 9		;np_debug_stop register
	PFX	3
	WRCTL	%l1
	PFX	4
	WRCTL	%l1
	; now PASS THROUGH to the normal breakpoint handler
	; NO BRANCH HERE
	; DON'T INSERT CODE BELOW

.endif


;
; When we get here, in response to
; a breakpoint (trap 3 poked into code),
; we need to save the state of all the registers,
; every last one! So we can restore them, you see,
; and then set up to call the debugger. Major
; context switching here! Neat, eh?
;
; We arrive here with CWP down 1 from
; its authentic state (fortunately!)
;
; the %l's and %o's are all ours
; down here. again, fortunately.
;
; We save the current interruptee's
; registers into the register structure
; that the C portion uses.
;
; Then, after popping CWP up one level,
; we reenable interrupts, and jump
; to the debugger's main.
;
; With interrupts enabled, the CWP manager will
; operate normally; if an underflow occurs, registers
; from the debugged program will get stored in
; their own local stack frame, since their own %sp's
; are used for the task. The debugger's will
; go in its own frame.
;

StubBreakpointHandler:

.if nasys_debug_core
	; if there is a deubbing peripheral,
	; me must stop it on interrupt
	; write to stop register to halt trace
	MOVI	%l1, 9		;np_debug_stop register
	PFX	3
	WRCTL	%l1
	PFX	4
	WRCTL	%l1
.endif
	;
	; Save the globals first.
	; Then, when we up the CWP, they're
	; ours to play with a bit.
	;

	MOVIA	%l0,gdb		; g is the C side's globals. starts with registers.
	STP	[%l0,0],%g0
	STP	[%l0,1],%g1
	STP	[%l0,2],%g2
	STP	[%l0,3],%g3
	STP	[%l0,4],%g4
	STP	[%l0,5],%g5
	STP	[%l0,6],%g6
	STP	[%l0,7],%g7

	;
	; Save the %o registers, which
	; appear to us, down here, as
	; %i registers
	;
	STP	[%l0,8],%i0
	STP	[%l0,9],%i1
	STP	[%l0,10],%i2
	STP	[%l0,11],%i3
	STP	[%l0,12],%i4
	STP	[%l0,13],%i5
	STP	[%l0,14],%i6
	STP	[%l0,15],%i7

	;
	; Read the trap number from CTL0, store in struct
	;
	RDCTL	%g4
	LSRI	%g4,9
	ANDIP	%g4,0x3f
	PFX	gdbTrapNumber
	ST	[%l0],%g4

	;
	; Read and pack the control
	; registers. Annoyingly, they
	; are shorts.
	;
	; But we never *really* know
	; what was in CTL1. In fact,
	; we're going to report CTL1
	; as zero, and read CTL1 and
	; use it for the returned value
	; of CTL0, since thats what it was.
	; Clear? Good!

	PFX	1
	RDCTL	%g0			; will pretend to be ctl0 (low) and ctl 1(zeroes) high
.if __nios32__
	PFX	gdbRegistersCtl0Ctl1
	ST	[%l0],%g0
.else
	PFX	gdbRegistersCtl0
	ST	[%l0],%g0
	PFX	gdbRegistersCtl1
	ST	[%l0],%g0
.endif

	;
	; Silly to save these two, they're fixed, but they're
	; in the vars, so ok.
	;

	PFX	2
	RDCTL	%g1
.if __nios32__
	PFX 3
	RDCTL	%g0
	LSLI	%g0, 16
	OR		%g0, %g1
	PFX gdbRegistersCtl2Ctl3
.else
	PFX gdbRegistersCtl2
	ST	[%l0],%g1
	PFX 3
	RDCTL	%g0
	PFX gdbRegistersCtl3
.endif
	ST	[%l0],%g0

	;
	; Save the %pc register, which
	; is now in %o7 cut in half
	;

	LSLI	%o7,1
	CMPI	%g4,na_SingleStepTrap	; %g4 = trap number we got, now
	IFS	cc_le
	 SUBI	%o7,2		; back it up one, to before the breakpoint, if breakpoint or step

	PFX	gdbRegistersPC
	ST	[%l0],%o7

	;
	; Ascend the CWP, and grab
	; the last %l's and %i's
	;

	MOV	%g0,%l0
	RESTORE
	PFX	16		; save %l0 first, so we can use it for the rest
	ST	[%g0],%l0

	MOV	%l0,%g0
	STP	[%l0,17],%l1
	STP	[%l0,18],%l2
	STP	[%l0,19],%l3
	STP	[%l0,20],%l4
	STP	[%l0,21],%l5
	STP	[%l0,22],%l6
	STP	[%l0,23],%l7

	STP	[%l0,24],%i0
	STP	[%l0,25],%i1
	STP	[%l0,26],%i2
	STP	[%l0,27],%i3
	STP	[%l0,28],%i4
	STP	[%l0,29],%i5
	STP	[%l0,30],%i6
	STP	[%l0,31],%i7

	PFX	9
	WRCTL	%g0			; enable traps (below 3, for cwp underflow action)
	NOP

	; JMB - Before execute GDBMain, we need to flush the register file to the stack
	; for call trace visibility. Do this on the user stack, cuz our stack is minimal
	; The global registers are ours, and nothing happens to them on save, so lets use them.

	;First get the CWP
	RDCTL	%g0
	LSRI	%g0, 4
	MOVI	%g2, 0x1f
	AND	%g0, %g2

	;Now get the LO_LIMIT
	PFX	2
	RDCTL	%g1
	AND	%g1, %g2

	;Now calculate how many times to save/restore
	SUB	%g0, %g1
	MOV	%g1, %g0

GDBSaveLoop:
	SAVE	%sp, -16
	nop
	SUBI	%g0, 1
	SKPS	cc_mi
	BR	GDBSaveLoop
	NOP

GDBRestoreLoop:
	RESTORE
	nop
	SUBI	%g1, 1
	SKPS	cc_mi
	BR	GDBRestoreLoop
	NOP

	;
	; Now, everything is saved.
	; If we set ctl0 and ctl1
	; from the gdb globals, we
	; can restore the registers
	; as they were, and use
	; a TRET to bump the CWP
	; at the same time that we
	; jump somewheres.
	;
	; So what we shall like to
	; do now is set up the
	; local stack and stuff
	; to call into the debugger!
	;

	MOVIA	%sp,gdbStubStacktop

	;
	; We CALL to our debugger, so
	; that we can ensure it balances
	; its calling sequence before
	; returning here. Once back, we'll
	; unwind all the trickery we've
	; done, and TRET
	;

	MOVIA	%g0,GDBMain@h
	CALL	%g0
	NOP

	;
	; Zero'th order, as we say, is to disable interrupts.
	;

	PFX	8
	WRCTL	%g0			; disable interrupts.

	;
	; We're back. We assume nothing; everything
	; may have changed. "Everything" of course
	; means the state of the memory, and the
	; state of the registers in the GDB globals.
	;
	; We'll start by instantiating CTL0 and CTL1
	; from those globals, bump down a level, tinker
	; a bit more, and TRET.
	;

	MOVIA	%g0,gdb		; g is the C side's globals. starts with registers.

.if __nios32__
	PFX	gdbRegistersCtl0Ctl1
.else
	PFX	gdbRegistersCtl0
.endif
	LD	%g1,[%g0]

	; CTL0 in the low 16 bits; the high 16 bits are CTL1 which we hate
	; And yet, we'll write them into CTL1, so that the TRET
	; puts them back for real.

.if __nios32__
	EXT16s	%g1,0	; clear the upper 16 bits
.endif

	PFX	1
	WRCTL	%g1

	;
	; And to start, we shall set CWP to where
	; we'll return to, and restore what we can
	; the one we're gonna return to
	; with interrupts carefully disabled
	;

	ANDIP	%g1,0x00007fff	; mask off TE bit
	WRCTL	%g1

	MOV	%l0,%g0		; %l0 --> C globals now, for restoring registers

	; restore %i registers

	LDP	%i0,[%l0,24]
	LDP	%i1,[%l0,25]
	LDP	%i2,[%l0,26]
	LDP	%i3,[%l0,27]
	LDP	%i4,[%l0,28]
	LDP	%i5,[%l0,29]
	LDP	%i6,[%l0,30]
	LDP	%i7,[%l0,31]

	; restore %l registers


	LDP	%l7,[%l0,23]
	LDP	%l6,[%l0,22]
	LDP	%l5,[%l0,21]
	LDP	%l4,[%l0,20]
	LDP	%l3,[%l0,19]
	LDP	%l2,[%l0,18]
	LDP	%l1,[%l0,17]
	LDP	%l0,[%l0,16]	; %l0 kills itself!

	;
	; Bump CWP down 1, so we have some %l to play with
	;

	SAVE	%sp,0
	MOV	%l0,%g0

	; Restore %o's (visible from down here as %i's)

	LDP	%i0,[%l0,8]
	LDP	%i1,[%l0,9]
	LDP	%i2,[%l0,10]
	LDP	%i3,[%l0,11]
	LDP	%i4,[%l0,12]
	LDP	%i5,[%l0,13]
	LDP	%i6,[%l0,14]
	LDP	%i7,[%l0,15]

	; restore %g's

	LDP	%g0,[%l0,0]
	LDP	%g1,[%l0,1]
	LDP	%g2,[%l0,2]
	LDP	%g3,[%l0,3]
	LDP	%g4,[%l0,4]
	LDP	%g5,[%l0,5]
	LDP	%g6,[%l0,6]
	LDP	%g7,[%l0,7]

	;
	; Set up local %o7, from PC, for TRET
	; (Could use any register, but
	; %o7 is ours, and it's traditional.)
	;

	PFX	gdbRegistersPC
	LD	%o7,[%l0]
	LSRI	%o7,1

.if nasys_debug_core
	; if there is a deubbing peripheral,
	; me must start it before returning
	; write to stop register to halt trace
	MOVI	%l1, 8		;np_debug_start register
	PFX	3
	WRCTL	%l1
	PFX	4
	WRCTL	%l1
.endif

	;
	;
	; I'm so glad we had this time together,
	; Just to have a laugh or sing a song.
	; Seems we just get started and before you know it
	; Comes the time we have to say, "So long."
	;   Carol Burnett, 1967
	;

	TRET %o7		; so long!

; end of file

#ifdef __KERNEL__
;----------------------------------------
;         Name: nr_uart_rxchar
;  Description: Read character if available
;        Input: %o0: UART base to use
;       Output: %o0 = character 0-0xff, or -1 if none present
; Side Effects: %g0 & %g1 altered
;    CWP Depth: 0
;

	.global	nr_uart_rxchar
nr_uart_rxchar:

.ifdef nasys_printf_uart
	IFRnz	%o0		; if %o0 is zero, use "default" uart
	BR	rxCharA
	NOP
	MOVIA   %o0,nasys_printf_uart
.endif ; nasys_printf_uart

rxCharA:

	MOV	%g0,%o0		; %g0 -> uart
	MOVI	%o0,1
	NEG	%o0		; %o0 gets -1 by default
	PFX	#2
	LD	%g1,[%g0]	; status register
	SKP0	%g1,7		; rx buffer empty? if no, keep the -1
	LD	%o0,[%g0]	; something for us? read into %o0.

	JMP	%o7
	CMPI	%o0,0		; Set condition flags as per char.


;----------------------------------------
;         Name: nr_uart_txchar
;  Description: Send a single byte out the UART
;        Input: %o0 = A character
;               %o1 = the UART to use, 0 for default
;       Output: none
; Side Effects: %g0 & %g1 altered, CPU waits for UART
;    CWP Depth: 0
;

; nr_uart_txchar
		.global	nr_uart_txchar
nr_uart_txchar:

.ifdef nasys_printf_uart
	IFRnz	%o1
	 BR	txCharWait
	NOP
	MOVIA	%o1,nasys_printf_uart
.endif ; nasys_printf_uart

txCharWait:
	PFX	np_uartstatus			; status register for uart
	LD	%g1,[%o1]
	SKP1	%g1,np_uartstatus_trdy_bit	; check the ready bit
	BR	txCharWait			; loop until empty
	NOP

	; Put character to out & return

	PFX	np_uarttxdata
	ST	[%o1],%o0	; put char into UART
	JMP	%o7
	NOP

#endif
