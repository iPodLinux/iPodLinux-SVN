/*
 * Copyright (c) 2005-06 James Jacobsson, Adam Johnston, Joshua Oreman, and David Carne.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice, this list
 * of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 * Neither the name of the organization nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior
 * written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef IPOD
	
	.section .text
	
__ASM_functions_list:
	.long	_HD_ARM_Convert16
	.long	_HD_ARM_Convert2
	.long	_HD_ARM_LowerBlit_ScaleBlend
	.long	_HD_ARM_LowerBlit_Blend
	.long	_HD_ARM_LowerBlit_Fast
	.long	_HD_ARM_Update5G
	.long	_HD_ARM_ClearScreen
	.long	_HD_ARM_UpdatePhoto
	.long	0

	.globl	_HD_ARM_Setup
_HD_ARM_Setup:
	str	r4, [sp, #-4]!
	@@ Setup stuff for code copy.
	ldr	r0, =__ASM_functions_start
	ldr	r1, =0x40000030
	ldr	r2, =__ASM_functions_end
	sub	r3, r1, r0		@ r3 = offset
	@@ Copy ASM funcs to IRAM + 0x30.
1:	ldr	r4, [r0], #4
	str	r4, [r1], #4
	cmp	r0, r2
	blo	1b
	@@ Put stubs in at the old addresses to jump to the new ones.
	adr	r0, __ASM_functions_list
1:	ldr	r1, [r0], #4		@ load a funcptr and inc funcptrptr
	cmp	r1, #0			@ is it the 0 at the end?
	beq	2f			@ if so, break out
	add	r2, r1, r3		@ r2 = address of func in iram
	ldr	r4, =0xe51ff004		@ r3 = encoding of `ldr pc, [pc, #-4]' (load pc from next word)
	str	r4, [r1], #4		@ store instr at func and inc funcptr
	str	r2, [r1], #4		@ store address of where-to-jump at func + 4 and inc funcptr
	b	1b			@ keep looping
2:	@@ Return.
	ldr	r4, [sp], #4
	bx	lr
	.pool

__ASM_functions_start:
/*
 * This file contains optimized ARM ASM versions of several common operations.
 * Cycle counts are specified for all instructions in inner loops
 * that do not complete in one cycle.
 */

	@@ Convert r2 4-byte AARRGGBB pixels in [r0] to 2-byte RGB pixels in [r1].
	@@ void _HD_ARM_Convert16 (uint32 *buffer, uint16 *fb, int npix)

	.globl	_HD_ARM_Convert16
_HD_ARM_Convert16:
	stmdb	sp!, {r4-r12, lr}
	ldr	r4, =0x0000F800		@ R mask - 5 top bits.
	ldr	r5, =0x000007E0		@ G mask - 6 middle bits.
	ldr	r6, =0x0000001F		@ B mask - 5 bottom bits.
	mov	r8, r0			@ r8 = src
	mov	r9, r1			@ r9 = dst
	mov	r10, r2			@ r10 = count

	@@ Main loop. 20 cycles per 2 pixels = 10 cycles/pix.
1:	ldmia	r8!, {r0, r11-r12, r14}	@ Load four pixels to r0, r11, r12, r14. 1N + 1I + 4S cyc.
	and	r1, r4, r0, lsr #8	@ Red pixel, shift, mask, and store in r1
	and	r2, r5, r0, lsr #5	@ Green pixel, shift, mask, store in r2
	and	r3, r6, r0, lsr #3	@ Blue pixel, shift, mask, and store in r3
	orr	r1, r2, r1		@ Red |= Green
	orr	r0, r1, r3		@ Store Red|Green|Blue in r0. r0 = first pixel, converted.

	@@ Do it again for the next pixel.
	and	r1, r4, r11, lsr #8	@ Red pixel, shift, mask, and store in r1
	and	r2, r5, r11, lsr #5	@ Green pixel, shift, mask, store in r2
	and	r3, r6, r11, lsr #3	@ Blue pixel, shift, mask, and store in r3
	orr	r1, r2, r1		@ Red |= Green
	orr	r11, r3, r1		@ r11 = Red|Green|Blue = second pixel, converted.

	@@ And again...
	and	r1, r4, r12, lsr #8
	and	r2, r5, r12, lsr #5
	and	r3, r6, r12, lsr #3
	orr	r1, r2, r1
	orr	r12, r3, r1

	@@ And again!
	and	r1, r4, r14, lsr #8
	and	r2, r5, r14, lsr #5
	and	r3, r6, r14, lsr #3
	orr	r1, r2, r1
	orr	r14, r3, r1

	@@ Combine first+second, third+fourth.
	orr	r0, r0, r11, lsl #16	@ r0 = (second << 16) | first
	orr	r12, r12, r14, lsl #16	@ r12 = (fourth << 16) | third
	subs	r10, r10, #2		@ Step counter down
	stmia	r9!, {r0, r12}		@ Save these four pixels. 1S + 2N cyc.
	bne	1b			@ Keep looping until counter = 0. 3 cyc.

	@@ Return
	ldmia	sp!, {r4-r12, pc}
	.pool


	
	@@ Convert r2 4-byte AARRGGBB pixels in [r0] to 2-bit Y pixels in [r1].
	@@ void _HD_ARM_Convert2 (uint32 *buffer, uint8 *fb2bpp, int npix)

	.globl	_HD_ARM_Convert2
_HD_ARM_Convert2:
	stmdb	sp!, {r4-r12, lr}

	@@ The algorithm we're using is Y = B/8 + G/2 + G/8 + R/4.
	@@ Register usage:
	@@   r0  Red component
	@@   r1  Green component
	@@   r2  Blue component
	@@   r3  Byte currently being built
	@@ r4-7  Four pixels we loaded
	@@   r8  #0x3f
	@@   r9  #0x1f
	@@  r10  #0x7f
	@@  r11  src pixptr
	@@  r12  dst pixptr
	@@  r14  npix
	
	mov	r8, #0x3f		@ R mask   (/4)
	mov	r9, #0x1f		@ G mask 1 (/8) and B mask
	mov	r10, #0x7f		@ G mask 2 (/2)
	mov	r11, r0			@ src pixptr
	mov	r12, r1			@ dst pixptr
	mov	r14, r2			@ npix

	@@ Main loop. 46 cycles per 4 pixels ~= 11.5 cycles/pix.
1:	ldmia	r11!, {r4-r7}		@ Load four pixels. 6 cyc.
	mov	r3, #0			@ r3 = byte we're building

	@@ First pixel (r4). 8 cyc.
	and	r0, r8, r4, lsr #18	@ r0 = red/4
	and	r1, r10, r4, lsr #9	@ r1 = green/2
	and	r2, r9, r4, lsr #3	@ r2 = blue/8
	@ We've now gotten everything we need out of r4, so we'll
	@ now use it for a scratch register.
	add	r4, r1, r1, lsr #2	@ r4 = green/2 + green/8
	add	r4, r4, r0		@    + red/4
	add	r4, r4, r2		@    + blue/8
	@ r0-r2 are free now too.
	mov	r0, r4, lsr #6		@ r0 = Y>>6
	rsb	r3, r0, #3		@ 0 is white

	@@ r4 is now freed up for temp stuff.
	@@ Let's use it to store #3, so we can do the
	@@ r0 = 3 - (Y >> 6)   in one instruction.
	mov	r4, #3

	@@ Second pixel (r5). 8 cyc.
	and	r0, r8, r5, lsr #18	@ r0 = red/4
	and	r1, r10, r5, lsr #9	@ r1 = green/2
	and	r2, r9, r5, lsr #3	@ r2 = blue/8
	@ r5 is now scratch.
	add	r5, r1, r1, lsr #2	@ r5 = green/2 + green/8
	add	r5, r5, r0		@    + red/4
	add	r5, r5, r2		@    + blue/8
	@ r0-r2 are free now too.
	sub	r0, r4, r5, lsr #6	@ r0 = 3 - (Y>>6)
	orr	r3, r3, r0, lsr #2	@ put this pixel into spot #2 of r3

	@@ Third pixel (r6). 8 cyc.
	and	r0, r8, r6, lsr #18	@ r0 = red/4
	and	r1, r10, r6, lsr #9	@ r1 = green/2
	and	r2, r9, r6, lsr #3	@ r2 = blue/8
	add	r5, r1, r1, lsr #2	@ r6 = green/2 + green/8
	add	r5, r5, r0		@    + red/4
	add	r5, r5, r2		@    + blue/8
	sub	r0, r4, r5, lsr #6	@ r0 = 3 - (Y>>6)
	orr	r3, r3, r0, lsr #4	@ put this pixel into spot #3 of r3

	@@ Fourth pixel (r7). 8 cyc.
	and	r0, r8, r7, lsr #18	@ r0 = red/4
	and	r1, r10, r7, lsr #9	@ r1 = green/2
	and	r2, r9, r7, lsr #3	@ r2 = blue/8
	add	r5, r1, r1, lsr #2	@ r6 = green/2 + green/8
	add	r5, r5, r0		@    + red/4
	add	r5, r5, r2		@    + blue/8
	sub	r0, r4, r5, lsr #6	@ r0 = 3 - (Y>>6)
	orr	r3, r3, r0, lsr #6	@ put this pixel into spot #4 of r3

	@@ Dec the counter, store the 4 pixels, and loop.
	subs	r14, r14, #1		@ dec the counter
	strb	r4, [r12], #1		@ store the byte. 2 cyc.
	bne	1b			@ loop if counter != 0. 3 cyc.

	@@ Return
	ldmia	sp!, {r4-r12, pc}



	@@ _HD_ARM_LowerBlit_AS - the core of the ScaleBlendClip function.
	@@ This is NOT intended to be called from user code!
	@@ void _HD_ARM_LowerBlit_AS (hd_surface src, uint32 fp_initial_ix, uint32 fp_initial_iy,
	@@                            uint32 fp_step_x, uint32 fp_step_y,
	@@                            hd_surface dest, uint32 startx, uint32 deltx, uint32 starty,
	@@                            uint32 delty, uint8 opacity)

	.globl	_HD_ARM_LowerBlit_ScaleBlend
_HD_ARM_LowerBlit_ScaleBlend_StoreSP:	.long 0
_HD_ARM_LowerBlit_ScaleBlend:
	@@ This function takes 51 + 53*h + (27+(5*(opacity != 0xff)))*w*h cycles to execute.
	
	stmfd	sp!, {r0-r12, lr}	@ 14 registers, 15 cyc.
	
	@@           src at [sp,  #0]
	@@ fp_initial_ix at [sp,  #4]
	@@ fp_initial_iy at [sp,  #8] <-- updated as fp_iy
	@@     fp_step_x at [sp, #12]
	@@     fp_step_y at [sp, #56]
	@@          dest at [sp, #60]
	@@        startx at [sp, #64]
	@@         deltx at [sp, #68]
	@@        starty at [sp, #72] <-- updated as y
	@@         delty at [sp, #76] <-- updated
	@@       opacity at [sp, #80]

	@@ fp_initial_iy and starty are only accessed once.
	@@ fp_iy is only accessed once per row, so we'll
	@@ keep it on the stack at [sp, #8] (replacing fp_initial_iy).

	@@  r1 = src+imgOff
	@@  r2 = deltx
	@@  r3 = fp_ix
	@@  r4 = fp_step_x
	@@  r5 = dest+buffOff+x
	@@  r0, r6, r7, r8, r11 (5 regs) = scratch
	@@  r9 = 0x00ff00ff
	@@ r10 = 0x00800080
	@@ r14 = opacity

	@@ Increment the src and dest pointers on the stack by 8,
	@@ so you can do e.g. [<srf>, <row>, lsl #2] to load a
	@@ row offset in one instruction. 12 cyc.
	ldr	r1, [sp, #0]
	add	r1, r1, #8
	str	r1, [sp, #0]
	ldr	r5, [sp, #60]
	add	r5, r5, #8
	str	r5, [sp, #60]
	
	ldr	r4, [sp, #12]		@ r4 = fp_step_x. 3 cyc.

	@@ Setup. 7 cyc.
	mov	r9, #0xff
	orr	r9, r9, #0xff0000
	bic	r10, r9, r9, lsr #1
	ldr	r14, [sp, #80]
	
	@@ Outer loop. 48 + 27*w cycles.
1:	
	@@ Load y, figure out buffOff, increment y and store it back. 13 cyc.
	ldr	r5, [sp, #60]		@ r5 = dest + 2
	ldr	r7, [sp, #72]		@ r7 = y
	add	r6, r7, #1		@ inc it -> r6
	str	r6, [sp, #72]		@ and store back
	ldr	r7, [r5, r7, lsl #2]	@ r7 = offset of starty'th row
	sub	r7, r7, #2		@ compensate for the +2
	add	r5, r5, r7, lsl #2	@ r5 = pointer to row in dest

	@@ Load fp_iy, figure out imgOff, increment fp_iy and store it back. 18 cyc.
	ldr	r1, [sp, #0]		@ r1 = src + 2
	ldr	r7, [sp, #8]		@ r7 = fp_iy
	mov	r8, r7, lsr #16		@ r8 = row in image
	ldr	r8, [r1, r8, lsl #2]	@ r8 = offset of row in src
	sub	r8, r8, #2		@ compensate for the +2
	add	r1, r1, r8, lsl #2	@ r1 = pointer to row in src
	ldr	r8, [sp, #56]		@ r8 = fp_step_y
	add	r7, r7, r8		@ fp_iy += fp_step_y
	str	r7, [sp, #8]		@ and store it

	@@ Set up some stuff for the inner loop. 9 cyc.
	ldr	r3, [sp, #4]		@ r3 = fp_initial_ix
	ldr	r6, [sp, #64]		@ r6 = startx
	ldr	r2, [sp, #68]		@ r2 = deltx
	add	r5, r5, r6, lsl #2	@ r5 += startx

	@@ Store SP so we can use it for calculations. 2N cyc.
	str	sp, _HD_ARM_LowerBlit_ScaleBlend_StoreSP
	
	@@ Pick the no-opacity or the opacity version. 2-4 cyc.
	cmp	r14, #0xff
	bne	7f
	
	@@ Inner loop. 28 cycles.
	@@ Local labels:
	@@   1:	beginning of outer loop
	@@   2:	beginning of no-opacity varied-alpha inner loop
	@@   3:	jump point into no-opacity varied-alpha inner loop from simple-alpha inner loop
	@@   4:	beginning of simple-alpha inner loop
	@@   5:	jump point into simple-alpha inner loop from no-opacity varied-alpha inner loop
	@@   6:	update of simple-alpha inner loop
	@@   7:	beginning of opacity inner loop
	@@   6: update of opacity inner loop (yes, two 6's)
	@@   8:	after end of opacity inner loop
	@@   9:	used by ablend macro

.macro	ablend	src, dst, opac=0
	@@ Blends src on dst and stores the result in dst.
	@@ Clobbers src, r0, r8, r11.
	@@ Requires r14 = opacity unless opac=0, r9 = 0xff00ff, r10 = 0x800080.
	@@ Time:  4 cyc for alpha=0 opac=0,
	@@        7 cyc for alpha=255 opac=0,
	@@       15 cyc for alpha=0 opac=1,
	@@       17 cyc for all other alphas opac=0,
	@@       26 cyc for any nonzero alpha opac=1.
	@@ On exit from the macro, Z flag is set iff alpha was 0 or 255.
	
.if \opac
	@@ If we have opacity to deal with, we need to scale down *all* parts of src by opac/256.
	@@ This is almost exactly like the blend loop below.
	and	r0, r9, \src, lsr #8	@ r0 = (src >> 8) & 0x00ff00ff
	mla	r11, r0, r14, r10	@ r11 = r0 * opac + 0x00800080
	bic	r11, r11, r9		@ r11 &= 0xff00ff00
	and	r0, r9, \src		@ r0 = src & 0x00ff00ff
	mla	r8, r0, r14, r10	@ r8 = r0 * opac + 0x00800080
	bic	r8, r8, r9		@ r8 &= 0xff00ff00
	add	\src, r11, r8, lsr #8	@ src = (r8 >> 8) + r11
.endif
	movs	r8, \src, lsr #24	@ r8 = alpha
	beq	9f			@ skip everything if alpha is 0
	rsbs	r8, r8, #0xff		@ r8 = 255 - alpha
.if !\opac
	@@ Don't do this for non-0xff opacity, because it will scale 0xff alphas
	@@ down to something non-0xff.
	moveq	\dst, \src		@ if alpha was 255, just copy the pixel
	beq	9f			@ and skip the rest
.endif
					@ r7 is now accumulator
	@@ Use r0, so we can put r8 last in mla so it can take only 3 cycles.
	and	r0, r9, \dst, lsr #8	@ r0 = (idst >> 8) & 0x00ff00ff
	mla	r11, r0, r8, r10	@ r11 = r0 * alpha + 0x00800080
	bic	r11, r11, r9		@ r11 &= 0xff00ff00
	add	\src, \src, r11		@ r7 += that whole thing.
	and	r11, r9, \dst		@ r11 = idst & 0x00ff00ff
	mlas	\dst, r11, r8, r10	@ r6 = r11 * alpha + 0x00800080. S so Z=0.
	bic	\dst, \dst, r9		@ r6 &= 0xff00ff00
	add	\dst, \src, \dst, lsr#8	@ r7 += that whole thing >> 8
9:
.endm

.macro	ablndne	src, dst
	@@ Blends src on dst and stores the result in dst.
	@@ All instructions are conditionalized `ne'.
	@@ Does not support opacity.
	@@ Requirements and clobbers are the same as for ablend.
	@@ Always takes 14 S+I cyc.
	movne	r8, \src, lsr #24
	rsbne	r8, r8, #0xff
	andne	r0, r9, \dst, lsr #8
	mlane	r11, r0, r8, r10
	bicne	r11, r11, r9
	addne	\src, \src, r11
	andne	r11, r9, \dst
	mlane	\dst, r11, r8, r10
	bicne	\dst, \dst, r9
	addne	\dst, \src, \dst, lsr #8
.endm
	
	@@ Simple no-overall-opacity 4x unrolled blend loop.
2:	ldmia	r5, {r6, r12, r13, r14}	@ load dst #1, #2, #3, #4	 4S 1N 1I
	mov	r7, r3, lsr #16		@ figure src #1 offset		 1S
	ldr	r7, [r1, r7, lsl #2]	@ load src #1			 1S 1N 1I
	ablend	r7, r6			@ blend #1			17S
	beq	5f			@ go to the easy-stuff loop	 1S (unless taken)
3:	add	r3, r3, r4		@ update r3			 1S
	subs	r2, r2, #1		@ update counter		 1S
	
	movne	r7, r3, lsr #16		@ figure src #2 offset		 1S
	ldrne	r7, [r1, r7, lsl #2]	@ load src #2			 1S 1N 1I
	ablndne r7, r12			@ blend #2			14S
	addne	r3, r3, r4		@ update r3			 1S
	subnes	r2, r2, #1		@ update counter		 1S
	
	movne	r7, r3, lsr #16		@ figure src #3 offset		 1S
	ldrne	r7, [r1, r7, lsl #2]	@ load src #3			 1S 1N 1I
	ablndne	r7, r13			@ blend #3			14S
	addne	r3, r3, r4		@ update r3			 1S
	subnes	r2, r2, #1		@ update counter		 1S
	
	movne	r7, r3, lsr #16		@ figure src #4 offset		 1S
	ldrne	r7, [r1, r7, lsl #2]	@ load src #4			 1S 1N 1I
	ablndne	r7, r14			@ blend #4			14S
	addne	r3, r3, r4		@ update r3			 1S
	subnes	r2, r2, #1		@ update counter		 1S

	@@ We store all 4 pixels, even if there weren't 4 left,
	@@ because the ones we shouldn't be touching weren't
	@@ touched - they're the same as they were when we
	@@ loaded them above, and it's much quicker to do a
	@@ block store than 4 compares and individual stores.
	stmia	r5!, {r6, r12, r13, r14} @ store the four pixels	 3S 2N
	bne	2b			@ keep looping while some left	 2S 1N
	@@ Cycle total per 4px:						83S 8N 5I	104
	@@ 26 cycles per pixel.
	@@ End of first inner loop.
	mov	r14, #0xff		@ restore the 0xff opacity, since r14 got clobbered.
	b	8f			@ skip the other 2 versions - 3 cyc.

	@@ This is the `easy-stuff' loop, taken if the first pixel was blended easily,
	@@ because probably the others will be too. This speeds up long runs of
	@@ all fully-opaque or fully-transparent pixels.
4:	ldmia	r5, {r6, r12, r13, r14}	@ load dst #1, #2, #3, #4	 4S 1N 1I
	mov	r7, r3, lsr #16		@ figure src #1 offset		 1S
	ldr	r7, [r1, r7, lsl #2]	@ load src #1			 1S 1N 1I
	ablend	r7, r6			@ blend #1		       4-7S
	bne	3b			@ go to the general loop if nec	 1S (unless taken)
5:	add	r3, r3, r4		@ update r3			 1S
	subs	r2, r2, #1		@ update counter		 1S
	beq	6f			@ skip if we're done		 1S
	
	mov	r7, r3, lsr #16		@ figure src #2 offset		 1S
	ldr	r7, [r1, r7, lsl #2]	@ load src #2			 1S 1N 1I
	ablend	r7, r12			@ blend #2		       4-7S
	add	r3, r3, r4		@ update r3			 1S
	subs	r2, r2, #1		@ update counter		 1S
	beq	6f			@ skip if we're done		 1S
	
	mov	r7, r3, lsr #16		@ figure src #3 offset		 1S
	ldr	r7, [r1, r7, lsl #2]	@ load src #3			 1S 1N 1I
	ablend	r7, r13			@ blend #3		       4-7S
	add	r3, r3, r4		@ update r3			 1S
	subs	r2, r2, #1		@ update counter		 1S
	beq	6f			@ skip if we're done
	
	mov	r7, r3, lsr #16		@ figure src #4 offset		 1S
	ldr	r7, [r1, r7, lsl #2]	@ load src #4			 1S 1N 1I
	ablend	r7, r14			@ blend #4		       4-7S
	add	r3, r3, r4		@ update r3			 1S
	subs	r2, r2, #1		@ update counter		 1S

6:	stmia	r5!, {r6, r12, r13, r14} @ store the four pixels	 3S 2N
	bne	4b			@ keep looping while some left	 2S 1N
	@@ Cycle total per 4px:					     40-52S 8N 5I	53-65
	@@ 13 cycles per fully transparent pixel, 16 per fully opaque one.	
	@@ End of second inner loop.
	mov	r14, #0xff		@ restore the 0xff opacity, since r14 got clobbered.
	b	8f			@ skip the other version - 3 cyc.

	@@ Inner loop 2 - opacity version. 4-5 extra cycles per loop iteration.
7:	ldmia	r5, {r6, r12, r13}	@ load 3 pixels			 3S 1N 1I
	mov	r7, r3, lsr #16		@ figure src #1 offset		 1S
	ldr	r7, [r1, r7, lsl #2]	@ load src #1			 1S 1N 1I
	ablend	r7, r6, 1		@ blend #1		        26S
	add	r3, r3, r4		@ update r3			 1S
	subs	r2, r2, #1		@ update counter		 1S
	beq	6f			@ skip if we're done		 1S
	
	mov	r7, r3, lsr #16		@ figure src #2 offset		 1S
	ldr	r7, [r1, r7, lsl #2]	@ load src #2			 1S 1N 1I
	ablend	r7, r12, 1		@ blend #2		        26S
	add	r3, r3, r4		@ update r3			 1S
	subs	r2, r2, #1		@ update counter		 1S
	beq	6f			@ skip if we're done		 1S
	
	mov	r7, r3, lsr #16		@ figure src #3 offset		 1S
	ldr	r7, [r1, r7, lsl #2]	@ load src #3			 1S 1N 1I
	ablend	r7, r13, 1		@ blend #3		        26S
	add	r3, r3, r4		@ update r3			 1S
	subs	r2, r2, #1		@ update counter		 1S
	
6:	stmia	r5!, {r6, r12, r13}	@ store the three pixels	 2S 2N
	bne	7b			@ keep looping while some left	 2S 1N
	@@ Cycle total per 3px:						99S 7N 4I	117
	@@ 39 cycles per pixel.
	@@ End of third inner loop.

	@@ Load SP. 1S+1N+1I cyc.
8:	ldr	sp, _HD_ARM_LowerBlit_ScaleBlend_StoreSP
	
	@@ Test. 9 cyc.
	ldr	r6, [sp, #76]		@ r6 = delty
	subs	r6, r6, #1		@ dec it
	str	r6, [sp, #76]		@ and store it back
	bne	1b			@ loop while != 0
	@@ End of outer loop.

	ldmia	sp!, {r0-r12, pc}	@ return, 18 cyc.

	

	@@ _HD_ARM_LowerBlit_Blend - Non-scaling blend blit.
	@@ Kinda-sorta fast. Not really, compared to _Fast.
	@@ void _HD_ARM_LowerBlit_Blend (hd_surface src, uint32 sx, uint32 sy,
	@@                               hd_surface dest, uint32 dx, uint32 dy, uint32 dw, uint32 dh,
	@@                               uint8 opacity)
	@@ ->NOTE<-: This function is actually SLOWER than ScaleBlend! It needs some optims!

	.globl	_HD_ARM_LowerBlit_Blend
_HD_ARM_LowerBlit_Blend:
	stmdb	sp!, {r4-r12, lr}	@ save 10 regs, 2N + 9S cyc.

	@@ Stack layout:
	@@    dx at [sp, #40]
	@@    dy at [sp, #44]
	@@    dw at [sp, #48]
	@@    dh at [sp, #52]
	@@ opaci at [sp, #56]

	@@ Register usage:
	@@     r0  current pixel in src
	@@     r1  current pixel in dest
	@@     r2  width of the blitted region (dw)
	@@     r3  address of dest(dx,dy+dh)
	@@     r4  width of src (src[0])
	@@     r5  width of dest (dest[0])
	@@     r6  idst (alpha blit)
	@@     r7  isrc, accumulator (alpha blit)
	@@     r8  alpha (alpha blit)
	@@     r9  0x00ff00ff (preloaded above)
	@@    r10  0x00800080 (preloaded above)
	@@    r11  scratch
	@@    r12  number of pix left in this row
	@@    r14  opacity

	@@ r0 is already loaded with src - set it up to point to (sx,sy). 2N + 5S + 2I cyc.
	ldr	r4, [r0]		@ load width. 1N+1I+1S
	add	r8, r0, #8		@ r8 = beginning of row offset pointers
	ldr	r8, [r8, r2, lsl #2]	@ load offset of sy'th row. 1N+1I+1S
	add	r0, r0, r8, lsl #2	@ r0 = beginning of sy'th row
	add	r0, r0, r1, lsl #2	@ r0 = address of pixel (sx,sy)

	@@ set r1 up to point to (dx,dy) in dest - currently r3=dest. 3N + 10S + 3I cyc.
	ldr	r5, [r3]		@ load width. 1N+1I+1S
	add	r8, sp, #40		@ r8 = beginning of dx,dy,dw,dh on stack
	ldmia	r8, {r1, r2, r6, r7}	@ r1=dx, r2=dy, (r3=dest), r6=dw, r7=dh. 1N + 1I + 4S.
	add	r8, r3, #8		@ r8 = beginning of row offset pointers
	ldr	r8, [r8, r2, lsl #2]	@ load offset of dy'th row. 1N+1I+1S
	add	r3, r3, r8, lsl #2	@ r3 = beginning of dy'th row
	add	r1, r3, r1, lsl #2	@ r1 = address of pixel (dx,dy)

	@@ set r3 up - this one's easy, it's quicker to do a mla here than
	@@ load the row offset ptr. 2S + 2I cyc.
	@@ also setup r2 (1 cyc) and r14 (3 cyc incl 1N) and constants (3 cyc)
	mov	r7, r7, lsl #2
	mla	r3, r7, r5, r1		@ r3 = (dtw * dh * 4) + r1
	mov	r2, r6
	ldr	r14, [sp, #56]
	mov	r9, #0xff		@ r9 = 0xff
	orr	r9, r9, r9, lsl #16	@ r9 = 0xff00ff
	bic	r10, r9, r9, lsr #1	@ r10 = 0x800080

	@@ Outer loop.
1:	mov	r12, r2
	
	@@ Pick the no-opacity or the opacity version. 2-4 cyc.
	cmp	r14, #0xff
	bne	3f
	
	@@ Inner loop. 28 cycles.
2:
	@@ Do the blend. 23 cyc. Thanks aegray!
	@ r6 = idst
	@ r7 = isrc, accumulator
	@ r8 = alpha
	@ r9 = 0x00ff00ff (preloaded above)
	@ r10 = 0x00800080 (preloaded above)
	@ r11 = scratch
	ldr	r6,  [r1]		@ r6 = idst, x++
	ldr	r7,  [r0], #4		@ r7 = isrc
	sub	r8,  r12, r7, lsr #24	@ r8 = 255-AA
					@ r7 is now accumulator
	and	r11, r9,  r6, lsr#8	@ r11 = (idst >> 8) & 0x00ff00ff
	mla	r11, r8,  r11, r10	@ r11 = r11 * alpha + 0x00800080. This is non-optimal;
					@ it should be mla r11,r11,r8,r10 (2 cyc quicker) but
					@ ARM requires Rd!=Rs and we don't have a reg to spare.
	bic	r11, r11, r9		@ r11 &= 0xff00ff00
	add	r7,  r7,  r11		@ r7 += that whole thing
	and	r11, r9,  r6		@ r11 = idst & 0x00ff00ff
	mla	r6,  r11, r8, r10	@ r6 = r11 * alpha + 0x00800080
	bic	r6,  r6,  r9		@ r6 &= 0xff00ff00
	add	r7,  r7,  r6, lsr #8	@ r7 += that whole thing >> 8
	str	r7,  [r1], #4		@ store the pixel
	
	@@ Increment and test. 4 cyc.
	subs	r12, r12, #1		@ dec the counter
	bne	2b			@ loop while != 0
	@@ End of inner loop.
	b	4f			@ skip the other version - 3 cyc.

	@@ Inner loop 2 - opacity version. 4-5 extra cycles per loop iteration.
	@@ We skip the adjustment if A=0 because it results in A=1, which is wrong.
3:	
	ldr	r6,  [r1]		@ r6 = idst, x++
	ldr	r7,  [r0], #4		@ r7 = isrc
	movs	r8,  r7, lsr #24	@ r8 = A
	mulne	r8,  r14, r8		@ multiply by the opacity (unless A=0)
	movne	r8,  r8,  lsr #8	@ divide by 256 (unless A=0)
	addne	r8,  r8,  #1		@ and add 1 (unless A=0)
	sub	r8,  r7,  r8		@ r8 = 255 - adjusted A
					@ r7 is now accumulator
	and	r11, r9,  r6, lsr#8	@ r11 = (idst >> 8) & 0x00ff00ff
	mla	r11, r8,  r11, r10	@ r11 = r11 * alpha + 0x00800080. This is non-optimal;
					@ it should be mla r11,r11,r8,r10 (2 cyc quicker) but
					@ ARM requires Rd!=Rs and we don't have a reg to spare.
	bic	r11, r11, r9		@ r11 &= 0xff00ff00
	add	r7,  r7,  r11		@ r7 += that whole thing
	and	r11, r9,  r6		@ r11 = idst & 0x00ff00ff
	mla	r6,  r11, r8, r10	@ r6 = r11 * alpha + 0x00800080
	bic	r6,  r6,  r9		@ r6 &= 0xff00ff00
	add	r7,  r7,  r6, lsr #8	@ r7 += that whole thing >> 8
	str	r7,  [r1], #4		@ store the pixel
	
	@@ Increment and test. 4 cyc.
	subs	r12, r12, #1		@ dec the counter
	bne	2b			@ loop while != 0
	@@ End of inner loop, option 2.
	
	@@ Update, test, jump.
4:	sub	r0, r0, r2, lsl #2	@ back to sx
	sub	r1, r1, r2, lsl #2	@ back to dx
	add	r0, r0, r4, lsl #2	@ on to next row
	add	r1, r1, r5, lsl #2	@ ditto
	cmp	r1, r3			@ are we there yet?
	blo	1b			@ nope - keep looping

	ldmia	sp!, {r4-r12, pc}	@ return, some inordinately large number of cycles
	
	
	@@ _HD_ARM_LowerBlit_f - The core of blit, non-alpha non-scaling.
	@@ Nice and fast. (or not.. see ->NOTE<- below)
	@@ void _HD_ARM_LowerBlit_f (hd_surface src, uint32 sx, uint32 sy,
	@@                           hd_surface dest, uint32 dx, uint32 dy, uint32 dw, uint32 dh)
	@@ ->NOTE<-: This func could do with some optimization, loop unrolling, etc. It's too slow.
	
	.globl	_HD_ARM_LowerBlit_Fast
_HD_ARM_LowerBlit_Fast_StoreSP:	.long 0
_HD_ARM_LowerBlit_Fast:
	stmdb	sp!, {r4-r12, lr}	@ save 19 regs, 2N + 9S cyc.

	@@ Stack layout:
	@@    dx at [sp, #40]
	@@    dy at [sp, #44]
	@@    dw at [sp, #48]
	@@    dh at [sp, #52]

	@@ Register usage:
	@@     r0  current pixel in src
	@@     r1  current pixel in dest
	@@  r2-r3  currently unused
	@@     r4  width of the blitted region (dw)
	@@     r5  address of dest(dx,dy+dh)
	@@     r6  width of src (src[0])
	@@     r7  width of dest (dest[0])
	@@ r8-r11  transfer regs
	@@    r12  number of pixels left to transfer in this row
	@@    r14  currently unused

	@@ r0 is already loaded with src - set it up to point to (sx,sy). 2N + 5S + 2I cyc.
	ldr	r6, [r0]		@ load width. 1N+1I+1S
	add	r8, r0, #8		@ r8 = beginning of row offset pointers
	ldr	r8, [r8, r2, lsl #2]	@ load offset of sy'th row. 1N+1I+1S
	add	r0, r0, r8, lsl #2	@ r0 = beginning of sy'th row
	add	r0, r0, r1, lsl #2	@ r0 = address of pixel (sx,sy)

	@@ set r1 up to point to (dx,dy) in dest - currently r3=dest. 3N + 10S + 3I cyc.
	ldr	r7, [r3]		@ load width. 1N+1I+1S
	add	r8, sp, #40		@ r8 = beginning of dx,dy,dw,dh on stack
	ldmia	r8, {r1, r2, r4, r5}	@ r1=dx, r2=dy, (r3=dest), r4=dw, r5=dh. 1N + 1I + 4S.
	add	r8, r3, #8		@ r8 = beginning of row offset pointers
	ldr	r8, [r8, r2, lsl #2]	@ load offset of dy'th row. 1N+1I+1S
	add	r3, r3, r8, lsl #2	@ r3 = beginning of dy'th row
	add	r1, r3, r1, lsl #2	@ r1 = address of pixel (dx,dy)

	@@ set r5 up - this one's easy, it's quicker to do a mla here than
	@@ load the row offset ptr. 2S + 2I cyc.
	mov	r5, r5, lsl #2
	mla	r5, r7, r5, r1		@ r5 = (dtw * dh * 4) + r1

	@@ save sp
	str	sp, _HD_ARM_LowerBlit_Fast_StoreSP

	@@ Ok, r0-r7 are set up. Let's transfer some pixels.
1:
	@@ Inits - 3S.
	mov	r12, r4
	
	cmp	r12, #4			@ At least 4 pix left?		1S
2:	ldmhsia	r0!, {r8-r11}		@ Load four, inc, ...		4S 1N 1I
	stmhsia	r1!, {r8-r11}		@ save four, inc, ...		3S 2N
	subhs	r12, r12, #4		@ Update r12.			1S
	cmphs	r12, #4			@ At least 4 left?		1S
	ldmhsia	r0!, {r8-r11}		@ Load four, inc, ...		4S 1N 1I
	stmhsia	r1!, {r8-r11}		@ save four, inc, ...		3S 2N
	subhs	r12, r12, #4		@ Update r12.			1S
	cmphs	r12, #4			@ At least 4 left?		1S
	ldmhsia	r0!, {r8-r11}		@ Load four, inc, ...		4S 1N 1I
	stmhsia	r1!, {r8-r11}		@ save four, inc, ...		3S 2N
	subhs	r12, r12, #4		@ Update r12.			1S
	cmphs	r12, #4			@ At least 4 left?		1S
	ldmhsia	r0!, {r8-r11}		@ Load four, inc, ...		4S 1N 1I
	stmhsia	r1!, {r8-r11}		@ save four, inc, ...		3S 2N
	subhs	r12, r12, #4		@ Update r12.			1S
	cmphs	r12, #4			@ At least 4 left?		1S
	bhs	2b			@ If so, keep looping.		2S 1N
	@@ Total for 16 pixels:					    -> 38S 13N 4I (+1S first time)

	@@ Copy 4-byte units till we're done. 4S if done, 3N + 2I + 5S if not done
	cmp	r12, #0
	beq	4f
3:	ldrne	r8, [r2], #4		@ If r8 was != 16, and we're still here (so it's < 16),
	strne	r8, [r3], #4		@ transfer 4 bytes. 2N+2I+2S for these two.
	subnes	r12, r12, #1		@ and update r12.
	bne	3b			@ If we're now <16 bytes, just repeat the word-copying part till
					@ we're done.
	@@ Update r0, r1. 2S.
4:	sub	r8, r6, r4
	add	r0, r0, r8, lsl #2
	sub	r8, r7, r4
	add	r1, r1, r8, lsl #2

	@@ Test and branch. 3S + 1N.
	cmp	r1, r5
	blo	1b

	ldr	sp, _HD_ARM_LowerBlit_Fast_StoreSP
	ldmia	sp!, {r4-r12, pc}	@ return - 11S + 1I + 2N

	
	
	@@ Update the LCD on the 5g (video) iPod.
	@@ We ASM this because gcc is braindead and emits one function
	@@ call per outl etc., and we need every cycle we can get here.
	@@ void _HD_ARM_Update5G (uint16 *fb, int x, int y, int w, int h)

	@@ Write a 32-bit value to the BCM. Uses r10-r11 for temp, needs r12 to be loaded with 0x30000000.
	@@ Takes 8S + 10N + 1I cycles.
.macro	bcmw32	addr, value
	orr	r11, r12, #0x10000	@ r11 = 0x30010000
	strh	\addr, [r11]		@ store the low hword
	mov	r10, \addr, lsr #16	@ get the high one in r10
	strh	r10, [r11]		@ and store it
	orr	r11, r12, #0x30000	@ r11 = 0x30030000
9:	ldrh	r10, [r11]		@ read status
	tst	r10, #2			@ check the ready bit
	beq	9b			@ loop until it's set
	strh	\value, [r12]		@ store low hword of value to 0x30000000
	mov	r10, \value, lsr #16	@ get the high hword
	strh	r10, [r12]		@ and store it
.endm

.macro	bcmr32	addr, value
	orr	r11, r12, #0x20000	@ r11 = 0x30020000
9:	ldrh	r10, [r11]		@ load address reg
	tst	r10, #1			@ test bit 0
	beq	9b			@ loop until set
	strh	\addr, [r11]		@ store the low hword
	mov	r10, \addr, lsr #16	@ get the high hword
	strh	r10, [r11]		@ and store it
	orr	r11, r12, #0x30000	@ r12 = 0x30030000
9:	ldrh	r10, [r11]		@ load status reg
	tst	r10, #16		@ test bit 4
	beq	9b			@ loop until set
	ldrh	\value, [r12]		@ read the low hword
	ldrh	r10, [r12]		@ and the high one
	orr	\value, \value, r10, lsl #16	@ put the high hword in place
.endm
	
	.globl	_HD_ARM_Update5G
_HD_ARM_Update5G:
	stmdb	sp!, {r4-r12, lr}

	@@ Do a finishup if we need one.
	ldr	r4, =need_finishup
	ldr	r4, [r4]
	cmp	r4, #0
	beq	1f
	@@ OK, we need it. Do it.
	mov	r12, #0x30000000	@ r12 = 0x30000000
	mvn	r5, #0			@ r5 = 0xFFFFFFFF
	and	r5, r5, r5, lsr #16	@ r5 = 0x0000FFFF
	mvn	r6, #0x50000		@ r6 = 0xFFFAFFFF
	bic	r6, r6, r6, lsr #16	@ r6 = 0xFFFA0005
	mov	r7, #0x1F8
2:	bcmr32	r7, r9			@ read some reg
	cmp	r9, r6			@ is it 0xFFFA0005?
	cmpne	r9, r5			@ is it 0xFFFF?
	beq	2b			@ if either, keep looping
	bcmr32	r8, r9			@ and read 0x1fc again
	
	@@ We've stored 10 registers, so h is at [sp, #40].
1:	ldr	r4, [sp, #40]		@ 3 cyc

	@@ Register usage: r0 = fb, r1 = x, r2 = y, r3 = w, r4 = h, r5 = count (for now)
	@@ r12 = 0x30000000, r8-r11 = temp, r5 = beginning of current row (later on)
	@@ r6 = x+w-1, r7 = y+h-1
	
	@@ Align x and w to a 2-byte boundary. 5S cyc.
	tst	r1, #1			@ is x aligned?
	subne	r1, r1, #1		@ align it down if so
	addne	r3, r3, #1		@ and inc the width to compensate
	tst	r3, #1			@ is w aligned?
	addne	r3, r3, #1		@ inc it if not

	@@ Set up the drawing region.
	mul	r5, r4, r3		@ r5 (count) = width * height
	add	r5, r5, r5		@ r5 *= 2
	add	r6, r1, r3		@ r6 = x + width
	sub	r6, r6, #1		@ minus 1
	add	r7, r2, r4		@ r7 = y + height
	sub	r7, r7, #1		@ minus 1

	@@ Send the commands to set up the rect.
	mov	r12, #0x30000000	@ load r12 for the bcmw32 macro
	mov	r8, #0x1F8
	mvn	r9, #0x50000		@ r9 = 0xFFFAFFFF
	bic	r9, r9, r9, lsr #16	@ r9 = 0xFFFAFFFF & 0xFFFF0005 = 0xFFFA0005 = what we want
	bcmw32	r8, r9
	mov	r8, #0xE0000		@ load first addr
	mov	r9, #0x34
	bcmw32	r8, r9			@ send command (addr = 0xE0000)
	add	r8, r8, #4
	bcmw32	r8, r1			@ start_horiz (@0xE0004)
	add	r8, r8, #4
	bcmw32	r8, r2			@ start_vert (@0xE0008)
	add	r8, r8, #4
	bcmw32	r8, r6			@ max_horiz (@0xE000C)
	add	r8, r8, #4
	bcmw32	r8, r7			@ max_vert (@0xE0010)
	add	r8, r8, #4
	bcmw32	r8, r5			@ count (@0xE0014)
	add	r8, r8, #4
	bcmw32	r8, r5			@ count (@0xE0018)
	add	r8, r8, #4
	mov	r9, #0
	bcmw32	r8, r9			@ zero (@0xE001C)

	@@ Write the destination address out as two 16-bit values.
	mov	r9, #0x20		@ r9 = low hword of address (0xE0020)
	orr	r8, r12, #0x10000	@ r8 = 0x30010000
	strh	r9, [r8]		@ and store it
	mov	r9, #0xE		@ high hword of 0xE0020
	strh	r9, [r8]		@ and store it
	orr	r8, r12, #0x30000	@ r8 = 0x30030000
1:	ldrh	r9, [r8]		@ read status
	tst	r9, #2			@ test ready bit
	beq	1b			@ loop until set
	
	@@ Set up pointers for the write.
	mov	r8, #320*2		@ r8 = lcd width * 2 [bytes per pixel]
	mla	r5, r2, r8, r0		@ r5 = fb + 320*y
	add	r0, r5, r1, lsl #1	@    + x
	mov	r5, r0			@ r5 = r0 = pointer to first pixel
					@ we'll increment r0 and keep r5 pointing to the first
	@@ In the inner loop we'll use r11 as a counter (number of pixels on this row).
1:	mov	r11, r3			@ r11 = width
2:	cmp	r11, #8			@ at least eight pixels left?
	ldmhsia	r0!, {r6, r7, r8, r9}	@ if so: load 8,
	strhsh	r6, [r12]		@ store #1
	movhs	r6, r6, lsr #16		@ shift in #2
	strhsh	r6, [r12]		@ store #2
	strhsh	r7, [r12]		@ store #3
	movhs	r7, r7, lsr #16		@ shift in #4
	strhsh	r7, [r12]		@ store #4
	strhsh	r8, [r12]		@ store #5
	movhs	r8, r8, lsr #16		@ shift in #6
	strhsh	r8, [r12]		@ store #6
	strhsh	r9, [r12]		@ store #7
	movhs	r9, r9, lsr #16		@ shift in #8
	strhsh	r9, [r12]		@ store #8
	subhs	r11, r11, #8		@ update count
	bhi	2b			@ keep going if there's more
3:	ldrne	r8, [r0], #4		@ if not (at least eight left): load 2,
	strneh	r8, [r12]		@ store #1
	movne	r8, r8, lsr #16		@ shift in #2
	strneh	r8, [r12]		@ store #2
	subnes	r11, r11, #2		@ update the counter. r11 had better be even!
	bne	3b			@ and keep going (in two-at-a-time mode)
	@@ Ok, we're done. Advance the pointer to the next row.
	add	r0, r5, #320*2		@ next row
	mov	r5, r0			@ for r5 too
	subs	r4, r4, #1		@ dec h
	bne	1b			@ and keep looping if it's nonzero

	@@ Start the finishup, and remember to finish it next tme.
	mov	r12, #0x30000000	@ r12 = 0x30000000
	orr	r11, r12, #0x30000	@ r11 = 0x30030000
	mov	r10, #0x31
	strh	r10, [r11]		@ store the 0x31 thing
	mov	r8, #0x1FC
	bcmr32	r8, r9			@ read 0x1FC to start it
		
	mov	r0, #1
	ldr	r1, =need_finishup
	str	r0, [r1]
	
	ldmia	sp!, {r4-r12, pc}	@ return
	.pool


	@@ Really fast memset 0 for screen clearing.
	@@ void _HD_ARM_ClearScreen (uint32 *fb, uint32 pixels)
	@@ Equivalent to memset (fb, 0, pixels*4)
	.globl	_HD_ARM_ClearScreen
_HD_ARM_ClearScreen:
	stmdb	sp!, {r4-r12, lr}	@				9S 2N
	mov	r2, #0			@ Zero				1S
	mov	r3, #0			@ twelve			1S
	mov	r4, #0			@ registers			1S
	mov	r5, #0			@ for				1S
	mov	r6, #0			@ stmia				1S
	mov	r7, #0			@ and				1S
	mov	r8, #0			@ really			1S
	mov	r9, #0			@ fast				1S
	mov	r10, #0			@ block				1S
	mov	r11, #0			@ memsetting.			1S
	mov	r12, #0			@				1S
	mov	r14, #0			@				1S
	@@ Total for setup:					    -> 21S 2N
	
	cmp	r1, #12			@ At least 12 pix left?		1S
1:	stmhsia	r0!, {r2-r12, r14}	@ Zero twelve px and inc.      11S 2N
	subhs	r1, r1, #12		@ Update r1.			1S
	cmphs	r1, #12			@ At least 12 left?		1S
	stmhsia	r0!, {r2-r12, r14}	@ Zero twelve and inc.	       11S 2N
	subhs	r1, r1, #12		@ Update r1.			1S
	cmphs	r1, #12			@ At least 12 left?		1S
	stmhsia	r0!, {r2-r12, r14}	@ Zero twelve and inc.	       11S 2N
	subhs	r1, r1, #12		@ Update r1.			1S
	cmphs	r1, #12			@ At least 12 left?		1S
	stmhsia	r0!, {r2-r12, r14}	@ Zero twelve and inc.	       11S 2N
	subhs	r1, r1, #12		@ Update r1.			1S
	cmphs	r1, #12			@ At least 12 left?		1S
	bhs	1b			@ If so, keep looping.		2S 1N
	@@ Total for 48 pixels:					    -> 54S 9N (+1S first time)

	cmp	r1, #0			@ Only do the loop if r1 != 0.	1S
2:	strne	r2, [r0], #4		@ Zero one pixel and inc.	   2N
	subnes	r1, r1, #1		@ Update r1.			1S
	bne	2b			@ Keep looping while r1 != 0.	2S 1N
	@@ Total for each odd pixel:				     -> 3S 3N (+1S first time)

	ldmia	sp!, {r4-r12, pc}	@ Return.


	@@ Update the LCD on a Photo, Color, or Nano.
	@@ Possible values for `type':
	@@    0  220x176, type 0 LCD (on a Photo)
	@@    1  220x176, type 1 LCD (on a Color)
	@@    2  176x132, type 1 LCD with weird rect specifications (on a Nano)
	@@ void _HD_ARM_UpdatePhoto (uint16 *fb, int x, int y, int w, int h, int type)

	@@ wwait - waits for the LCD to be write ready. clobbers r10, r11. assumes r12 = 0x70008A0C.
.macro	wwait
	mov	r10, #0x10000		@ r10 ~= 64 thousand ~= 1ms
9:	ldr	r11, [r12]		@ r11 = *0x70008A0C
	subs	r10, r10, #1		@ timer--
	beq	9f
	tst	r11, #0x80000000	@ is high bit set?
	beq	9b			@ loop until it is
9:
.endm

	@@ sendlo - sends a low value (?) clobbers r10, r11, expects r12 = 0x70008A0C.
	@@ val *cannot* be r10 or r11
.macro	sendlo	val
	wwait
	orr	r11, \val, #0x80000000
	str	r11, [r12]
.endm
	@@ sendhi - sends a high value (?) clobbers r10, r11, expects r12 = 0x70008A0C.
	@@ val *cannot* be r10 or r11
.macro	sendhi	val
	wwait
	orr	r11, \val, #0x81000000
	str	r11, [r12]
.endm
	@@ cmdat0 - sends a command + data to a type-0 LCD. clobbers r10, r11, expects r12 = addr.
	@@ cmd or data *cannot* be r10 or r11
.macro	cmdat0	cmd, data
	sendlo	\cmd
	sendlo	\data
.endm
	@@ cmdat1 - sends a command + data to a type-1 LCD. clobbers r10, r11, r13 (!), expects r12 = addr.
	@@ cmd or data *cannot* be r10, r11 or r13
.macro	cmdat1	cmd, data
	mov	r13, #0
	sendlo	r13
	sendlo	\cmd
	mov	r13, \data, lsr #8
	sendhi	r13
	and	r13, \data, #0xff
	sendhi	r13
.endm

	.globl	_HD_ARM_UpdatePhoto
_HD_ARM_UpdatePhoto_StoreSP:	.long 0
_HD_ARM_UpdatePhoto:
	stmdb	sp!, {r4-r12, lr}	@ save regs

	@@ Align x and w to a 2-pixel boundary. 5S cyc.
	tst	r1, #1			@ is x aligned?
	subne	r1, r1, #1		@ align it down if so
	addne	r3, r3, #1		@ and inc the width to compensate
	tst	r3, #1			@ is w aligned?
	addne	r3, r3, #1		@ inc it if not

	@@ Load h, type -> r4, r5.
	add	r4, sp, #40		@ r4[0] = h, r4[1] = type
	ldmia	r4, {r4, r5}		@ load 'em to r4=h, r5=type

	@@ Save sp so we can use r13 for computations.
	str	sp, _HD_ARM_UpdatePhoto_StoreSP
	
	@@ Load 0x70008A0C -> r12.
	mov	r12,  #0x70000000
	orr	r12, r12, #0x8A00
	orr	r12, r12, #0x000C

	@@ Figure out rect1, rect2, rect3, rect4 in r6,r7,r8,r9.
	cmp	r5, #2			@ nano?
	bne	1f			@ if not, jump ahead
	mov	r6, r1			@ if so: rect1 = sx,
	mov	r7, r2			@        rect2 = sy,
	add	r8, r1, r3		@        rect3 = sx + width
	sub	r8, r8, #1		@              - 1,
	add	r9, r2, r4		@        rect4 = sy + height
	sub	r9, r9, #1		@              - 1
	b	2f			@        skip the code for photo
1:	mov	r6, r2			@ if not: rect1 = sy,
	rsb	r7, r1, #219		@         rect2 = (lcd_width - 1) - sx
	add	r8, r2, r4		@         rect3 = sy + height
	sub	r8, r8, #1		@               - 1
	sub	r9, r7, r3		@         rect4 = rect2 - width
	add	r9, r9, #1		@               + 1
2:	cmp	r5, #0			@ is it a type-0?
	bne	type1			@ if not, -> type 1.
type0:	mov	r14, #0x12
	cmdat0	r14, r6			@ lcd_cmd_data (0x12, rect1 [& 0xff])
	mov	r14, #0x13
	cmdat0	r14, r7			@ lcd_cmd_data (0x13, rect2 [& 0xff])
	mov	r14, #0x15
	cmdat0	r14, r8			@ lcd_cmd_data (0x15, rect3 [& 0xff])
	mov	r14, #0x16
	cmdat0	r14, r9			@ lcd_cmd_data (0x16, rect4 [& 0xff])
	b	doit
type1:	cmp	r8, r6			@ if (rect3 < rect1) {
	moveq	r14, r6			@     int t = rect1
	moveq	r6, r8			@     rect1 = rect3
	moveq	r8, r14			@     rect3 = t
	cmp	r9, r7			@ }    if (rect4 < rect2) { similar }
	moveq	r14, r7
	moveq	r7, r9
	moveq	r9, r14
	orr	r8, r6, r8, lsl #8	@ r8 = (rect3 << 8) | rect1, rect3 is clobbered
	mov	r14, #0x44		@ r14 = command
	cmdat1	r14, r8			@ lcd_cmd_data (0x44, (rect3 << 8) | rect1)
	orr	r8, r7, r9, lsl #8
	mov	r14, #0x45
	cmdat1	r14, r8			@ lcd_cmd_data (0x45, (rect4 << 8) | rect2)
	cmp	r5, #2			@ nano?
	movne	r7, r9			@ if not, rect2 = rect4.
	orr	r8, r6, r7, lsl #8
	mov	r14, #0x21
	cmdat1	r14, r8			@ lcd_cmd_data (0x21, (rect2 << 8) | rect1)
	mov	r14, #0
	sendlo	r14			@ lcd_send_lo (0x0)
	mov	r14, #0x22
	sendlo	r14			@ lcd_send_lo (0x22)
	@@ r6-r9 are now free for other uses.
doit:	cmp	r5, #2
	moveq	r6, #176*2		@ if nano, r6 (line width) = 176 pix,
	movne	r6, #220*2		@ else it's 220. Remember, 2-byte pix.
	mla	r0, r2, r6, r0		@ addr += sy * lcd_width
	add	r0, r0, r1, lsl #1	@       + sx
	mov	r10, r6			@ store r10 = scanline width in bytes
	
	@@ For this loop:
	@@   r6 = number of bytes we can write at once
	@@   r7 = number of bytes total
	@@   r8 = X
	mov	r8, #0			@ X = 0 to start
	add	r7, r3, r3		@ r7 = width * 2
	mul	r7, r4, r7		@    * height

	@@ Some initial figurings-outs.
1:	mov	r6, r7			@ write all that are left, if we can
	cmp	r6, #64000		@ if (pixels_to_write > 64000)   - too much?
	movhi	r6, #64000		@     pixels_to_write = 64000      limit it to 64k
	sub	r7, r7, r6		@ update r7 -> number left for the *next* iteration
	mov	r14, #0x10000000
	orr	r14, r14,  #0x80
	str	r14, [r12, #0x14]	@ outl (0x10000080, 0x70008A20)
	sub	r14, r6, #1		@ r14 = (pixels_to_write - 1)
	orr	r14, r14, #0xC0000000	@     | 0xC0000000
	orr	r14, r14, #0x10000	@     | 0x00010000
	str	r14, [r12, #0x18]	@ outl ((pixels_to_write - 1) | 0xC0010000, 0x70008A24)
	mov	r14, #0x34000000
	str	r14, [r12, #0x14]	@ outl (0x34000000, 0x70008A20)

	@@ Write that many pixels, one row at a time.
	@@ r9 holds the pix we're building
2:
	@@ two_pixels = ( ((addr[0] & 0xff) << 8) | ((addr[0] & 0xff00) >> 8)) |
	@@              ((((addr[1] & 0xff) << 8) | ((addr[1] & 0xff00) >> 8)) << 16)
	@@ Basically, swab pix0 and pix1 and put pix1 in the high bits.
	ldr	r9, [r0], #4		@ load two pix into r9		1S 1N 1I
	@@ Current layout of r9: HHLLhhll. We want LLHHllhh.
	mov	r11, r9, lsl #8		@ r11 = LLhhll00		1S
	bic	r11, r11, #0xff0000	@ r11 = LL00ll00		1S
	mov	r9, r9, lsr #8		@ r9  = 00HHLLhh		1S
	bic	r9, r9, #0xff00		@ r9  = 00HH00hh		1S
	orr	r9, r9, r11		@ r9  = LLHHllhh = what we want	1S
	@@ Wait for the controller:
3:	ldr	r11, [r12, #0x14]	@ read status reg		1S 1N 1I
	tst	r11, #0x01000000	@ test bit 24			1S
	beq	3b			@ loop until set		1S unless taken
	@@ Write the pixels.
	str	r9, [r12, #0xF4]	@ store two pix -> 0x70008B00	   2N    (F4 = B00 - A0C)
	@@ Are we at X = width? If so, skip to next row.
	add	r8, r8, #2		@ x += 2 (we just stored 2 pix)
	cmp	r8, r3			@ is it = width yet?
	addcs	r0, r0, r10		@ if it's >=, addr += lcd_width
	subcs	r0, r0, r3, lsl #1	@                   - width
	@@ Did we just write the last pixel?
	subs	r6, r6, #2		@ r6 -= 2 (# pix)
	bhi	2b			@ if nonzero and nonnegative, keep going
	@@ Guess so. Update some stuff.
3:	ldr	r11, [r12, #0x14]	@ read status reg
	tst	r11, #0x04000000	@ test bit 26
	beq	3b			@ loop until set
	mov	r11, #0
	str	r11, [r12, #0x18]	@ store 0 -> 0x70008A24
	cmp	r7, #0			@ still some pixels left to write?
	bge	1b			@ guess so. keep going.

	ldr	sp, _HD_ARM_UpdatePhoto_StoreSP	@ restore sp
	ldmia	sp!, {r4-r12, pc}	@ return


	@@ Calculates the sine/cosine of an angle.
	@@ Return a 1.16 fixp number.
	@@ Timings: fsin(x) takes 11S 4N 2I + (quadrant of x)S cycles
	@@          fcos(x) takes 12S 4N 2I + (quadrant of (x + 1024))S cycles
	@@ 
	@@ int32 fsin (int32 angle)	int32 fcos (int32 angle)
	@@
	.extern	sine_table
	.globl	fcos
	.globl	fsin
fcos:	add	r0, r0, #1024		@ cos(x) = sin(x + pi/2)
fsin:	mov	r1, #0xff
	adds	r1, r1, #0xf00		@ r1 = 0xfff. "adds" instead of "orr" so C flag gets cleared - used below
	and	r0, r0, r1		@ angle &= r1 (angle %= 4096)
	mov	r1, #3			@ r1 = 3
	sub	r1, r1, r0, lsr #10	@ r1 = 3 - top two bits of angle (3 for <1024, 2 for 1024<a<2048, etc.)
	add	pc, pc, r1, lsl #2	@ jumptable based on (3 - top two bits of angle)
	bx	lr			@ dummy
quad4:	rsbs	r0, r0, #4096		@ angle = 4096 - angle, carry flag gets set since no overflow
quad3:	subccs	r0, r0, #2048		@ angle = angle - 2048 if not in q4, C set since no overflow
quad2:	rsbcc	r0, r0, #2048		@ angle = 2048 - angle if not in q3 or q4, carry flag stays clear (no S)
quad1:	ldr	r1, =sine_table		@ r0 = sine_table
	ldr	r0, [r1, r0, lsl #2]	@ r0 = sin(corresp)
	rsbcs	r0, r0, #0		@ -sin(corresp), if angle was > 2048 (if carry flag got set a few instrs ago)
	bx	lr


__ASM_functions_end:
	
	.section .data
need_finishup:	.long 0
	
@ Local Variables:
@ asm-comment-char: ?@
@ comment-start: "@ "
@ block-comment-start: "/*"
@ block-comment-end: "*/"
@ indent-tabs-mode: t
@ End:

#endif
