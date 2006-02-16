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



	@@ _HD_ARM_LowerBlit_AS - the core of the ScaleBlendClip function.
	@@ This is NOT intended to be called from user code!
	@@ void _HD_ARM_LowerBlit_AS (hd_surface src, uint32 fp_initial_ix, uint32 fp_initial_iy,
	@@                            uint32 fp_step_x, uint32 fp_step_y,
	@@                            hd_surface dest, uint32 startx, uint32 deltx, uint32 starty,
	@@                            uint32 delty, uint8 opacity)

	.globl	_HD_ARM_LowerBlit_ScaleBlend
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
	@@ r12 = 0xff
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
	mov	r12, #0xff
	orr	r9, r12, #0xff0000
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
	sub	r7, r7, #8		@ compensate for the +2
	add	r5, r5, r7, lsl #2	@ r5 = pointer to row in dest

	@@ Load fp_iy, figure out imgOff, increment fp_iy and store it back. 18 cyc.
	ldr	r1, [sp, #0]		@ r1 = src + 2
	ldr	r7, [sp, #8]		@ r7 = fp_iy
	mov	r8, r7, lsr #16		@ r8 = row in image
	ldr	r8, [r1, r8, lsl #2]	@ r8 = offset of row in src
	sub	r8, r8, #8		@ compensate for the +2
	add	r1, r1, r8, lsl #2	@ r1 = pointer to row in src
	ldr	r8, [sp, #56]		@ r8 = fp_step_y
	add	r7, r7, r8		@ fp_iy += fp_step_y
	str	r7, [sp, #8]		@ and store it

	@@ Set up some stuff for the inner loop. 9 cyc.
	ldr	r3, [sp, #4]		@ r3 = fp_initial_ix
	ldr	r6, [sp, #64]		@ r6 = startx
	ldr	r2, [sp, #68]		@ r2 = deltx
	add	r5, r5, r6, lsl #2	@ r5 += startx

	@@ Pick the no-opacity or the opacity version. 2-4 cyc.
	cmp	r14, #0xff
	bne	3f
	
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
	ldr	r6,  [r5]		@ r6 = idst
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
	b	4f			@ skip the other version - 3 cyc.

	@@ Inner loop 2 - opacity version. 4-5 extra cycles per loop iteration.
	@@ We skip the adjustment if A=0 because it results in A=1, which is wrong.
3:	
	ldr	r6,  [r5]		@ r6 = idst
	mov	r7,  r3, lsr #16	@ r7 = ix
	ldr	r7,  [r1, r7, lsl #2]	@ r7 = isrc
	movs	r8,  r7, lsr #24	@ r8 = A
	mulne	r8,  r14, r8		@ multiply by the opacity (unless A=0)
	movne	r8,  r8,  lsr #8	@ divide by 256 (unless A=0)
	addne	r8,  r8,  #1		@ and add 1 (unless A=0)
	sub	r8,  r7,  r8		@ r8 = 255 - adjusted A
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
	@@ End of inner loop, option 2.
	
	@@ Test. 9 cyc.
4:	ldr	r6, [sp, #76]		@ r6 = delty
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
	add	r7,  r7,  r11, lsr#8	@ r7 += that whole thing >> 8
	and	r11, r9,  r6		@ r11 = idst & 0x00ff00ff
	mla	r6,  r11, r8, r10	@ r6 = r11 * alpha + 0x00800080
	bic	r6,  r6,  r9		@ r6 &= 0xff00ff00
	add	r7,  r7,  r6		@ r7 += that whole thing
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
	add	r7,  r7,  r11, lsr#8	@ r7 += that whole thing >> 8
	and	r11, r9,  r6		@ r11 = idst & 0x00ff00ff
	mla	r6,  r11, r8, r10	@ r6 = r11 * alpha + 0x00800080
	bic	r6,  r6,  r9		@ r6 &= 0xff00ff00
	add	r7,  r7,  r6		@ r7 += that whole thing
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

	ldmia	sp!, {r4-r12, lr}	@ return, some inordinately large number of cycles
	
	
	@@ _HD_ARM_LowerBlit_f - The core of blit, non-alpha non-scaling.
	@@ Nice and fast.
	@@ void _HD_ARM_LowerBlit_f (hd_surface src, uint32 sx, uint32 sy,
	@@                           hd_surface dest, uint32 dx, uint32 dy, uint32 dw, uint32 dh)
	
	.globl	_HD_ARM_LowerBlit_Fast
_HD_ARM_LowerBlit_Fast:
	stmdb	sp!, {r4-r12, lr}	@ save 19 regs, 2N + 9S cyc.

	@@ Stack layout:
	@@    dx at [sp, #40]
	@@    dy at [sp, #44]
	@@    dw at [sp, #48]
	@@    dh at [sp, #52]

	@@ Register usage:
	@@     r0  current row + sx in src
	@@     r1  current row + dx in dest
	@@     r2  current pixel in src
	@@     r3  current pixel in dest
	@@     r4  width of the blitted region (dw)
	@@     r5  address of dest(dx,dy+dh)
	@@     r6  width of src (src[0])
	@@     r7  width of dest (dest[0])
	@@ r8-r11  transfer regs
	@@    r12  number of pixels left to transfer in this row
	@@    r14  scratch

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

	@@ Ok, r0-r7 are set up. Let's transfer some pixels.
1:
	@@ Inits - 3S.
	mov	r2, r0
	mov	r3, r1
	mov	r12, r4
	
	@@ First copy as many 16-byte units as we can. 6S if <16 bytes left,
	@@ 10S + 3N + 1I if =16, 12S + 4N + 1I if >16.
2:	cmp	r12, #16		@ at least 16 bytes left?
	ldmhsia	r2!, {r8-r11}		@ If it was >= 16, transfer 16 bytes. Read (1N + 1I + 4S),
	stmhsia	r3!, {r8-r11}		@ write (2N + 3S),
	subhs	r12, r12, #16		@ and update r12.
	bhi	2b			@ If there's more, go back now and skip the rest of this stuff.
	@@ Then copy 4-byte units till we're done. 4S if done, 3N + 2I + 5S if not done
3:	ldrne	r8, [r2], #4		@ If r8 was != 16, and we're still here (so it's < 16),
	strne	r8, [r3], #4		@ transfer 4 bytes. 2N+2I+2S for these two.
	subnes	r12, r12, #4		@ and update r12.
	bne	3b			@ If we're now <16 bytes, just repeat the word-copying part till
					@ we're done.
	@@ Update r0, r1. 2S.
	add	r0, r0, r6, lsl #2
	add	r1, r1, r7, lsl #2

	@@ Test and branch. 3S + 1N.
	cmp	r1, r5
	blo	1b

	ldmia	sp!, {r4-r12, lr}	@ return - 11S + 1I + 2N

	
	
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
	orr	r11, r12, #0x30000	@ r11 = 0x30030000
	mov	r10, #0x31
	strh	r10, [r11]		@ store the 0x31 thing
	mvn	r5, #0			@ r5 = 0xFFFFFFFF
	and	r5, r5, r5, lsr #16	@ r5 = 0x0000FFFF
	mvn	r6, #0x50000		@ r6 = 0xFFFAFFFF
	bic	r6, r6, r6, lsr #16	@ r6 = 0xFFFA0005
	mov	r7, #0x1F8
	mov	r8, #0x1FC
	bcmr32	r8, r9
2:	bcmr32	r7, r9			@ read some reg
	cmp	r9, r6			@ is it 0xFFFA0005?
	cmpne	r9, r5			@ is it 0xFFFF?
	beq	2b			@ if either, keep looping
	bcmr32	r8, r9			@ and read 0x1fc again
	
	@@ We've stored 10 registers, so h is at [sp, #40].
1:	ldmia	sp!, {r4-r12, lr}	@ return - DEBUG
	ldr	r4, [sp, #40]		@ 3 cyc

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
	mov	r10, #640		@ r8 = lcd width * 2 [bytes per pixel]
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
	subs	r2, r2, #1		@ dec y
	bne	1b			@ and keep looping if it's nonzero

	@@ Store a value to say we need a finishup next time we're called.
	mov	r0, #1
	ldr	r1, =need_finishup
	str	r0, [r1]
	
	ldmia	sp!, {r4-r12, lr}	@ return



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
