; Modified for uClinux - Vic - Dec 2001
; From: 
  
; File: nios_gdb_stub_isr.s
; Date: 2000 June 20
; Author dvb \ Altera Santa Cruz

;
; Assembly language portions of Nios GDB Stub
;
; Stack set up, register clearing, all kinds of
; clever things.
;

#include <asm/niosasm.h>

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
	.equ	gdbRegistersCtl2,34
	.equ	gdbTrapNumber,35		; ISR can report trap number here
.else
	.equ	gdbRegistersCtl0,34
	.equ	gdbRegistersCtl1,35
	.equ	gdbRegistersCtl2,36
	.equ	gdbTrapNumber,38
.endif


	.text

	.global	StubBreakpointHandler
	.global	StubHarmlessHandler
//	.global	StubButtonHandler
	.global	GDBMain

	.comm	_gdb_stub_stack,1024,4	; Local stack, statically allocated.
	.equ	gdbStubStacktop,_gdb_stub_stack+992


;
; If nios_gdb_install(0) is invoked, the stub is present,
; and linked in, but harmless. This, so that you can "turn off"
; the debugger with minimal change in your code footprint.
;
; This is the harmless breakpoint handler.

StubHarmlessHandler:
	TRET	%o7		; pretty darned minimal. Better stuff below.




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

	;
	; Save the globals first.
	; Then, when we up the CWP, they're
	; ours to play with a bit.
	;

	MOVIA	%l0,g		; g is the C side's globals. starts with registers.
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
	; Silly to save this one, it's fixed, but it's
	; in the vars, so ok.
	;

	PFX	2
	RDCTL	%g0
	PFX	gdbRegistersCtl2
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
	PFX	9
	WRCTL	%g0			; enable traps (below 3, for cwp underflow action)

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

	MOVIA	%g0,g		; g is the C side's globals. starts with registers.

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

	;
	; I'm so glad we had this time together,
	; Just to have a laugh or sing a song.
	; Seems we just get started and before you know it
	; Comes the time we have to say, "So long."
	;   Carol Burnett, 1967
	;

	TRET %o7		; so long!

; end of file
