/*
 * Copyright (c) 2005-06 James Jacobsson, Joshua Oreman, and David Carne.
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
	
/*
 * This file contains optimized ARM ASM versions of several common operations.
 * Cycle counts are specified for all instructions in inner loops
 * that do not complete in one cycle.
 */

	@@ Convert r2 4-byte AARRGGBB pixels in [r0] to 2-byte RGB pixels in [r1].
	@@ void _HD_ARM_Convert16 (uint32 *buffer, uint16 *fb, int npix)

	.globl	_HD_ARM_Convert16
_HD_ARM_Convert16:
	stmdb	sp!, {r4-r11, lr}
	ldr	r4, =0x0000F800		@ R mask - 5 top bits.
	ldr	r5, =0x000007E0		@ G mask - 6 middle bits.
	ldr	r6, =0x0000001F		@ B mask - 5 bottom bits.
	mov	r8, r0			@ r8 = src
	mov	r9, r1			@ r9 = dst
	mov	r10, r2			@ r10 = count

	@@ Main loop. 20 cycles per 2 pixels = 10 cycles/pix.
1:	ldmia	r8!, {r0, r11}		@ Load two pixels to r0 and r11. 4 cyc.
	and	r1, r4, r0, lsr #8	@ Red pixel, shift, mask, and store in r1
	and	r2, r5, r0, lsr #5	@ Green pixel, shift, mask, store in r2
	and	r3, r6, r0, lsr #3	@ Blue pixel, shift, mask, and store in r3
	orr	r1, r2, r1		@ Red |= Green
	orr	r7, r1, r3		@ Store Red|Green|Blue in r7. r7 = first pixel, converted.

	@@ Do it again for the next pixel.
	and	r1, r4, r11, lsr #8	@ Red pixel, shift, mask, and store in r1
	and	r2, r5, r11, lsr #5	@ Green pixel, shift, mask, store in r2
	and	r3, r6, r11, lsr #3	@ Blue pixel, shift, mask, and store in r3
	orr	r1, r2, r1		@ Red |= Green
	orr	r1, r3, r1		@ Red|Green |= Blue. r1 = second pixel, converted.

	@@ Put the first pixel into the top 16 bits of r1, and store it.
	orr	r1, r7, r1, lsl #16
	subs	r10, r10, #1		@ Step counter down
	str	r1, [r9], #4		@ Save these two pixels. 2 cyc.
	bne	1b			@ Keep looping until counter = 0. 3 cyc.

	@@ Return
	ldmia	sp!, {r4-r11, pc}



	
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



	@@ _HD_ARM_LowerBlit - the core of the ScaleBlendClip function.
	@@ This is NOT intended to be called from user code!
	@@ void _HD_ARM_LowerBlit (hd_surface src, uint32 fp_initial_ix, uint32 fp_initial_iy,
	@@                         uint32 fp_step_x, uint32 fp_step_y,
	@@                         hd_surface dest, uint32 startx, uint32 deltx, uint32 starty,
	@@                         uint32 delty)

_HD_ARM_LowerBlit:
	@@ This function takes 51 + 48*h + 27*w*h cycles to execute.
	
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
	@@ r12 = 0xff
	@@ r14 is currently unused.

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

	@@ Set up the constants. 3 cyc.
	mov	r12, #0xff
	orr	r9, r12, #0xff0000
	bic	r10, r9, r9, lsr #1
	
	@@ Outer loop. 48 + 27*w cycles.
1:	
	@@ Load y, figure out buffOff, increment y and store it back. 13 cyc.
	ldr	r5, [sp, #60]		@ r5 = dest + 2
	ldr	r7, [sp, #72]		@ r7 = y
	add	r6, r7, #1		@ inc it -> r6
	str	r6, [sp, #72]		@ and store back
	ldr	r7, [r5, r7, lsl #2]!	@ r7 = offset of starty'th row
	add	r5, r5, r7, lsl #2	@ r5 = pointer to row in dest

	@@ Load fp_iy, figure out imgOff, increment fp_iy and store it back. 17 cyc.
	ldr	r1, [sp, #0]		@ r1 = src + 2
	ldr	r7, [sp, #8]		@ r7 = fp_iy
	mov	r8, r7, lsr #16		@ r8 = row in image
	ldr	r8, [r1, r8, lsl #2]	@ r8 = offset of row in src
	add	r1, r1, r8, lsl #2	@ r1 = pointer to row in src
	ldr	r8, [sp, #56]		@ r8 = fp_step_y
	add	r7, r7, r8		@ fp_iy += fp_step_y
	str	r7, [sp, #8]		@ and store it

	@@ Set up some stuff for the inner loop. 9 cyc.
	ldr	r3, [sp, #4]		@ r3 = fp_initial_ix
	ldr	r6, [sp, #64]		@ r6 = startx
	ldr	r2, [sp, #68]		@ r2 = deltx
	add	r5, r5, r6, lsl #2	@ r5 += startx

	@@ Inner loop. 28 cycles.
2:
	@@ Blend [r1, r3, lsr #16 lsl #2] on [r5].
	@@ The C code:
	@ uint32 alpha, dst[2];
	@ uint32 idst = (dst_argb), isrc = (src_argb);
	@ alpha = 255 - (isrc >> 24);
	@ dst[0] = ((idst & 0x00ff00ff) * alpha + 0x00800080) & 0xff00ff00;
	@ dst[1] = (((idst>>8) & 0x00ff00ff) * alpha + 0x00800080) & 0xff00ff00;
	@ (dst_argb) = (dst[0]>>8) + dst[1] + isrc;

	@@ Do the blend. 22 cyc. Thanks aegray!
	@ r0 = scratch
	@ r6 = idst
	@ r7 = isrc, accumulator
	@ r8 = alpha
	@ r9 = 0x00ff00ff (preloaded above)
	@ r10 = 0x00800080 (preloaded above)
	@ r11 = scratch
	ldr	r6,  [r5]		@ r6 = idst, x++
	mov	r7,  r3, lsr #16	@ r7 = ix
	ldr	r7,  [r1, r7, lsl #2]	@ r7 = isrc
	sub	r8,  r12, r7, lsr #24	@ r8 = 255-AA
					@ r7 is now accumulator
	and	r0,  r9,  r6, lsr#8	@ r0 = (idst >> 8) & 0x00ff00ff
	mla	r11, r0,  r8, r10	@ r11 = r0 * alpha + 0x00800080
	bic	r11, r11, r9		@ r11 &= 0xff00ff00
	add	r7,  r7,  r11, lsr#8	@ r7 += that whole thing >> 8
	and	r11, r9,  r6		@ r11 = idst & 0x00ff00ff
	mla	r6,  r11, r8, r10	@ r6 = r11 * alpha + 0x00800080
	bic	r6,  r6,  r9		@ r6 &= 0xff00ff00
	add	r7,  r7,  r6		@ r7 += that whole thing
	str	r7,  [r5], #4		@ store the pixel
	
	@@ Increment and test. 5 cyc.
	add	r3, r3, r4		@ fp_ix += fp_step_x
	subs	r2, r2, #1		@ dec the counter
	bne	2b			@ loop while != 0
	@@ End of inner loop.
	
	@@ Test. 9 cyc.
	ldr	r6, [sp, #76]		@ r6 = delty
	subs	r6, r6, #1		@ dec it
	str	r6, [sp, #76]		@ and store it back
	bne	1b			@ loop while != 0
	@@ End of outer loop.

	ldmia	sp!, {r0-r12, pc}	@ return, 18 cyc.
	
@ Local Variables:
@ asm-comment-char: ?@
@ comment-start: "@ "
@ block-comment-start: "/*"
@ block-comment-end: "*/"
@ indent-tabs-mode: t
@ End:

#endif
