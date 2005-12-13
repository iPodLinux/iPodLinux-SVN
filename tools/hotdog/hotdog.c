/*
 * Copyright (c) 2005, James Jacobsson
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
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "hotdog.h"
#include "hotdog_primitive.h"
#include "hotdog_png.h"
#include "hotdog_font.h"

hd_engine *HD_Initialize(uint32 width,uint32 height,uint8 bpp, void *framebuffer, void (*update)(struct hd_engine*, int, int, int, int)) {
	hd_engine *eng;

	eng = (hd_engine *)malloc( sizeof(hd_engine) );
	assert(eng != NULL);

	eng->buffer = (uint32 *)malloc( width * height * 4 );
	assert(eng->buffer != NULL);

	eng->screen.width  = width;
	eng->screen.height = height;
        if (bpp == 2) {
            eng->screen.fb2bpp = framebuffer;
            eng->screen.framebuffer = 0;
        } else {
            eng->screen.framebuffer = framebuffer;
            eng->screen.fb2bpp = 0;
        }
        eng->screen.update = update;

	eng->list = NULL;

	return(eng);
}

void HD_Register(hd_engine *eng,hd_object *obj) {
	hd_obj_list *curr;

        obj->eng = eng;

	// Special case for first entry
	if( eng->list == NULL ) {
		eng->list = (hd_obj_list *)malloc( sizeof(hd_obj_list) );
		eng->list->obj = obj;
		eng->list->next = NULL;

		return;
	}

	curr = eng->list;
	while(curr->next != NULL) curr = curr->next;

	curr->next = (hd_obj_list *)malloc( sizeof(hd_obj_list) );
	assert(curr->next);

	curr->next->obj = obj;
	curr->next->next = NULL;
}

void HD_Render(hd_engine *eng) {
	hd_obj_list *cur = eng->list;
        hd_rect *dirties = 0, *rect = 0;
        int needsort = 0;

        // Make a list of all dirty rects. Rects are clipped
        // to screen from the beginning.
        while (cur) {
            hd_object *obj = cur->obj;
            if (obj->last.x != obj->x || obj->last.y != obj->y ||
                obj->last.w != obj->w || obj->last.h != obj->h ||
                obj->last.z != obj->z) {
                // One rect: current position
                if (!rect) {
                    rect = dirties = malloc (sizeof(hd_rect));
                } else {
                    rect->next = malloc (sizeof(hd_rect));
                    rect = rect->next;
                }
                rect->x = (obj->x < 0)? 0 : obj->x;
                rect->y = (obj->y < 0)? 0 : obj->y;
                rect->w = (obj->x + obj->w > eng->screen.width)? eng->screen.width - obj->x : obj->w;
                rect->h = (obj->y + obj->h > eng->screen.height)? eng->screen.height-obj->y : obj->h;
                rect->z = obj->z;
                // Next rect: old position
                rect->next = malloc (sizeof(hd_rect));
                rect = rect->next;
                
                rect->x = (obj->last.x < 0)? 0 : obj->last.x;
                rect->y = (obj->last.y < 0)? 0 : obj->last.y;
                rect->w = (obj->last.x + obj->last.w > eng->screen.width)? eng->screen.width - obj->last.x : obj->last.w;
                rect->h = (obj->last.y + obj->last.h > eng->screen.height)? eng->screen.height - obj->last.y : obj->last.h;
                rect->z = obj->last.z;

                rect->next = 0;
                obj->dirty = 1;
                if (obj->last.z != obj->z) needsort = 1;
            }
            cur = cur->next;
        }

        // Sort 'em if necessary
        if (needsort)
            eng->list = HD_StackObjects (eng->list);

        // Clear dirty areas
        rect = dirties;
        while (rect) {
            uint32 *p = eng->buffer + rect->y*eng->screen.width + rect->x;
            uint32 *endp = p + rect->h*eng->screen.width;
            while (p < endp) {
                memset (p, 0, rect->w*4);
                p += eng->screen.width;
            }
            rect = rect->next;
        }

        // Draw objects, and parts of any others that overlap/underlap
        cur = eng->list;
        while (cur) {
            hd_object *obj = cur->obj;
            if (!obj->dirty) {
                rect = dirties;
                while (rect) {
                    // Just check if it overlaps at all for now. The conditional
                    // will be executed N*M times, so keep it quick.
                    if ((obj->x + obj->w > rect->x) && (obj->y + obj->h > rect->y) &&
                        (obj->x < rect->x + rect->w) && (obj->y < rect->y + rect->h)) {
                        int x, y, w, h;
                        // Rect intersection of obj and rect.
                        // In these diagrams, < > is the object and [ ] is the dirty rect.
                        if (rect->x > obj->x) {
                            // <[>] or <[]>
                            x = rect->x - obj->x;
                            if (rect->x + rect->w < obj->x + obj->w) {
                                // <[]>
                                w = rect->w;
                            } else {
                                // <[>]
                                w = obj->w - x;
                            }
                        } else {
                            // [<]> or [<>]
                            x = obj->x - rect->x;
                            if (obj->x + obj->w < rect->x + rect->w) {
                                // [<>]
                                w = obj->w;
                            } else {
                                // [<]>
                                w = rect->w - x;
                            }
                        }
                        
                        // Similar for the Y coordinates.
                        if (rect->y > obj->y) {
                            y = rect->y - obj->y;
                            if (rect->y + rect->h < obj->y + obj->h)
                                h = rect->h;
                            else
                                h = obj->h - y;
                        } else {
                            y = obj->y - rect->y;
                            if (obj->y + obj->h < rect->y + rect->h) {
                                h = obj->h;
                            } else {
                                h = rect->h - y;
                            }
                        }

                        // Draw it.
                        if (obj->natw + obj->nath) {
                            int32 fx = (obj->natw << 16) / obj->w, fy = (obj->nath << 16) / obj->h;
                            obj->render (eng, obj, (x * fx) >> 16, (y * fy) >> 16, (w * fx) >> 16, (h * fy) >> 16);
                        } else {
                            obj->render (eng, obj, x, y, w, h);
                        }
                    }
                    rect = rect->next;
                }
            } else {
                // This is the dirty one. Draw it and update last.
                obj->render (eng, obj, 0, 0, obj->natw, obj->nath);
                obj->last.x = obj->x;
                obj->last.y = obj->y;
                obj->last.w = obj->w;
                obj->last.h = obj->h;
                obj->dirty = 0;
            }
            cur = cur->next;
        }

	// Translate internal ARGB8888 to RGB565
	if (eng->screen.framebuffer) {
#ifndef IPOD
		uint32 off,sPix;
		uint16 dPix;

		for(off=0;off<(eng->screen.width*eng->screen.height);off++) {
			sPix = eng->buffer[off];

			dPix  = ((sPix & 0x00FF0000) >> (16+3)) << 11; // R
			dPix |= ((sPix & 0x0000FF00) >> (8+2)) << 5;  // G
			dPix |= ((sPix & 0x000000FF) >> (3));    // B

			eng->screen.framebuffer[off] = dPix;
		}
#else
                uint32 * srcptr;
                uint16 * dstptr;
                
                srcptr = eng->buffer;
                dstptr = eng->screen.framebuffer;
                
                uint32 count = eng->screen.width*eng->screen.height / 2 + 1;
                
                
                // %0 = counter
                // %1 = dst ptr
                // %2 = src ptr
                
                // R0 = src val
                // R1 = dst accum
                // R2 = dst tmp
                // R3 = dst tmp 2
                // R4, R5, R6 - Various mask constants
                // R7 another temp
                asm volatile(
                "               LDR     R4, =0x0000F800     \n"  // R is 5 Bits
                "               LDR     R5, =0x000007E0     \n"  // G is 6 Bits
                "               LDR     R6, =0x0000001F     \n"  // B is 5 bits
                
                "LOOP_START:                                \n"
                "               LDR     R0, [%2], #4        \n" // Get the source value, Skip the src ptr to the next pixel
                "               AND     R1, R4, R0, LSR #8  \n" // Red pixel, shift, mask + store in dst
                "               AND     R2, R5, R0, LSR #5  \n" // Green pixel, shift, mask, store in tmp1
                "               AND     R3, R6, R0, LSR #3  \n" // Blue pixel, shift, mask, store in tmp2
                "               ORR     R1, R2, R1          \n" // Combine Red + Green
                
                "               LDR     R0, [%2], #4        \n" // Load the next pixel, this saves up a partial pipeline stall
                
                "               ORR     R7, R3, R1          \n" // Combine RedGreen_pix1 + Blue_pix1 - Save in R7
            
                "               AND     R1, R4, R0, LSR #8  \n" // Red pixel, shift, mask + store in dst
                "               AND     R2, R5, R0, LSR #5  \n" // Green pixel, shift, mask, store in tmp1
                "               AND     R3, R6, R0, LSR #3  \n" // Blue pixel, shift, mask, store in tmp2
                "               ORR     R1, R2, R1          \n" // Combine Red + Green
                "               ORR     R1, R3, R1          \n" // Combine RedGreen + Blue
                
                "               ORR     R1, R7, R1, LSL #16 \n" // This pixel goes into the upper half of our writeback register
                
                "               SUBS    %0, %0, #1          \n" // Step counter down                
                
                "               STR    R1, [%1], #4         \n" // Save back
                

                "               BNE     LOOP_START          \n"
                
                :                                               // No return
                : "r" (count), "r" (dstptr), "r" (srcptr)
                : "r0","r1","r2","r3","r4","r5","r6","r7");
#endif
	}
        // Translate to Y2
        if (eng->screen.fb2bpp) {
            uint32 *sPix = eng->buffer;
            uint32 *endPix = eng->buffer + eng->screen.width * eng->screen.height;
            uint8 *dPix = eng->screen.fb2bpp;
            
#ifndef IPOD
            while (sPix < endPix) {
                uint8 pix0, pix1, pix2, pix3;
                
#define CONV_ARGB8888_TO_Y2(y,argb) ({ uint32 p32 = argb; \
    y = ~((((p32 & 0xff) >> 3) + ((p32 & 0xff00) >> 9) + ((p32 & 0xff00) >> 11) + ((p32 & 0xff0000) >> 18)) >> 6); \
    y &= 3; })
                CONV_ARGB8888_TO_Y2 (pix0, *sPix++);
                CONV_ARGB8888_TO_Y2 (pix1, *sPix++);
                CONV_ARGB8888_TO_Y2 (pix2, *sPix++);
                CONV_ARGB8888_TO_Y2 (pix3, *sPix++);
                *dPix++ = (pix3 << 6) | (pix2 << 4) | (pix1 << 2) | pix0;
            }
#else
            // Y = 1/8*B + (1/2+1/8)*G + 1/4*R
            asm volatile (""
                          "1:\t"
                          "ldmia %[spix]!, { r5, r6, r7, r8 }\n\t"
                          "mov   r4,     #0\n\t" /* r4 = byte we're building */

                          "mov   r0,     #0xff\n\t" /* mask to extract colors */
                          "and   r1, r5, r0, lsl #16\n\t" /* extract (red << 16) -> r1 */
                          "and   r2, r5, r0, lsl #8\n\t" /* (green << 8) -> r2 */
                          "and   r3, r5, r0\n\t"         /* blue -> r3 */
                          "mov   r0,     r1, lsr #18\n\t" /* gray = red/4 */
                          "add   r0, r0, r2, lsr #11\n\t" /*      + green/8 */
                          "add   r0, r0, r2, lsr #9\n\t"  /*      + green/2 */
                          "add   r0, r0, r3, lsr #3\n\t" /*       + blue/8 */
                          "mov   r5,     #3\n\t" /* r5 dead, will use in latter blocks too */
                          "sub   r0, r5, r0, lsr #6\n\t" /* gray >>= 6 (becomes 2bpp val),
                                                       2bppval = 3 - 2bppval (0=white) */
                          "orr   r4, r4, r0, lsl #0\n\t" /* put in pix 0 */

                          "mov   r0,     #0xff\n\t"
                          "and   r1, r6, r0, lsl #16\n\t"
                          "and   r2, r6, r0, lsl #8\n\t"
                          "and   r3, r6, r0\n\t"
                          "mov   r6,     r0\n\t" /* r6 dead, use to store mask for next two -
                                                    saves having to reload it */
                          "mov   r0,     r1, lsr #18\n\t"
                          "add   r0, r0, r2, lsr #11\n\t"
                          "add   r0, r0, r2, lsr #9\n\t"
                          "add   r0, r0, r3, lsr #3\n\t"
                          "sub   r0, r5, r0, lsr #6\n\t"
                          "orr   r4, r4, r0, lsl #2\n\t" /* put in pix 1 */

                          "and   r1, r7, r6, lsl #16\n\t"
                          "and   r2, r7, r6, lsl #8\n\t"
                          "and   r3, r7, r6\n\t"
                          "mov   r0,     r1, lsr #18\n\t"
                          "add   r0, r0, r2, lsr #11\n\t"
                          "add   r0, r0, r2, lsr #9\n\t"
                          "add   r0, r0, r3, lsr #3\n\t"
                          "sub   r0, r5, r0, lsr #6\n\t"
                          "orr   r4, r4, r0, lsl #4\n\t" /* put in pix 2 */

                          "and   r1, r8, r6, lsl #16\n\t"
                          "and   r2, r8, r6, lsl #8\n\t"
                          "and   r3, r8, r6\n\t"
                          "mov   r0,     r1, lsr #18\n\t"
                          "add   r0, r0, r2, lsr #11\n\t"
                          "add   r0, r0, r2, lsr #9\n\t"
                          "add   r0, r0, r3, lsr #3\n\t"
                          "sub   r0, r5, r0, lsr #6\n\t"
                          "orr   r4, r4, r0, lsl #6\n\t" /* put in pix 3 */

                          "strb  r4, [%[dpix]], #1\n\t"
                          "cmp   %[spix], %[endpix]\n\t"
                          "bcc   1b"
                          :: [spix] "r" (sPix), [endpix] "r" (endPix), [dpix] "r" (dPix)
                          : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8");
#endif
        }

        rect = dirties;
        while (rect) {
            eng->screen.update (eng, rect->x, rect->y, rect->w, rect->h);
            rect = rect->next;
            // FREE RECTS! xxx
        }
}

void HD_ScaleBlendClip (uint32 *sbuf, int stw, int sth, int sx, int sy, int sw, int sh,
                        uint32 *dbuf, int dtw, int dth, int dx, int dy, int dw, int dh) 
{
  int32 fp_step_x,fp_step_y,fp_ix,fp_iy,fp_initial_ix,fp_initial_iy;
  int32 x,y;
  int32 startx,starty,endx,endy;
  uint32 buffOff, imgOff;
  
  dw = dw * sw / stw;
  dh = dh * sh / sth;
  if (!dw || !dh) return;
  fp_step_x = (sw << 16) / dw;
  fp_step_y = (sh << 16) / dh;
  dx += ((sx << 16) / fp_step_x);
  dy += ((sy << 16) / fp_step_y);

  // 1st, check if we need to do anything at all
  if( (dx > dtw) || (dy > dth) ||
	  ((dx+dw) < 0) || ((dy+dh) < 0) ) {

	// Nope, we're entirely off-screen, so there's nothing for us to do
	return;

  // 2nd, check if we're completely inside the screen
  } else if( (dx >= 0) && ((dx+dw) < dtw) &&
             (dy >= 0) && ((dy+dh) < dth)) {
	
      startx = dx;
      starty = dy;
    endx   = startx+dw;
    endy   = starty+dh;

    fp_initial_ix = (sx << 16);
    fp_initial_iy = (sy << 16);
  } else {
      startx = dx;
      starty = dy;
    endx   = startx+dw;
    endy   = starty+dh;
    fp_initial_ix = 0;
    fp_initial_iy = 0;

    // Let the clipping commence
    if( startx < 0 ) {
        startx = 0;
        fp_initial_ix = -(startx) * fp_step_x;
    }
    if( endx > dtw ) {
        endx = dtw - 1;
    }
    if( starty < 0 ) {
        starty = 0;
        fp_initial_iy = -(starty) * fp_step_y;
    }
    if( endy > dth ) {
        endy = dth - 1;
    }

    fp_initial_ix += (sx << 16);
    fp_initial_iy += (sy << 16);
  }
  
  buffOff = starty * dtw;// + startx;
  
  fp_iy = fp_initial_iy;
  for(y=starty;y<endy;y++) {
    fp_ix = fp_initial_ix;
    imgOff = (fp_iy>>16) * stw;
    
    for(x=startx;x<endx;x++) {
      
      BLEND_ARGB8888_ON_ARGB8888( dbuf[ buffOff + x ], sbuf[ imgOff + (fp_ix>>16) ] );
      
      fp_ix += fp_step_x;
    }
    buffOff += dtw;
    fp_iy += fp_step_y;
  }
}

void HD_Destroy (hd_object *obj) 
{
    hd_obj_list *cur, *last = 0;
    
    obj->destroy (obj);
    free (obj);

    cur = obj->eng->list;
    if (cur->obj == obj) {
        obj->eng->list = cur->next;
        free (cur);
    } else {
        last = cur;
        cur = cur->next;
        while (cur) {
            if (cur->obj == obj) {
                last->next = cur->next;
                free (cur);
                cur = last->next;
            } else {
                cur = cur->next;
            }
        }
    }
}

hd_object *HD_New_Object() 
{
    static int lz = 65535;
    hd_object *ret = calloc (1, sizeof(hd_object));
    assert (ret != NULL);
    ret->last.z = ret->z = lz--;
    ret->opacity = 0xff;
    return ret;
}
