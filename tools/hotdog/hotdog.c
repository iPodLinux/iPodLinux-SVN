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

#ifndef MIN
#define MIN(x,y) (((x)<(y))?(x):(y))
#define MAX(x,y) (((x)>(y))?(x):(y))
#endif

#ifdef IPOD
extern void _HD_ARM_Setup();
extern void _HD_ARM_Convert2(uint32 *buffer, uint8 *fb2bpp, int npix);
#endif

hd_engine *HD_Initialize(uint32 width,uint32 height,uint8 bpp, void *framebuffer, void (*update)(struct hd_engine*, int, int, int, int)) {
	hd_engine *eng;

#ifdef IPOD
#ifndef DONT_RUN_FROM_IRAM
	_HD_ARM_Setup();
#endif
#endif

	eng = (hd_engine *)calloc( 1, sizeof(hd_engine) );
	assert(eng != NULL);

        eng->screen.__r0 = eng->screen.__r1 = 0;
	eng->screen.width  = width;
	eng->screen.height = height;

        if (bpp == 2) {
            eng->screen.fb2bpp = framebuffer;
            // The LCD update funcs assume that each line starts on a byte boundary; for the 138x110
            // Mini, this is not the case, so we "fudge" it a bit. The 2bpp converter works with the
            // FB as a whole, not line-by-line, so the 2px of padding is necessary.
            eng->screen.width  = (eng->screen.width + 3) & ~3; // round up to a multiple of 4px - IMPORTANT
            eng->screen.framebuffer = 0;
        } else {
            eng->screen.framebuffer = framebuffer;
            eng->screen.fb2bpp = 0;
        }
        eng->screen.update = update;

	eng->buffer = HD_NewSurface (eng->screen.width, eng->screen.height);
	assert(eng->buffer != NULL);

	eng->list = NULL;

	return(eng);
}

#define safe_free(x) do { if (x) free(x), x = 0; } while (0)
void HD_Deinitialize(hd_engine *eng)
{
    if (!eng) return;

    /* go ahead and free all registered objects */
    while (eng->list) {
    	hd_obj_list *ol;
	ol = eng->list;
	eng->list = eng->list->next;

	if (ol->obj->animating)
		HD_StopAnimation(ol->obj);
	ol->obj->destroy(ol->obj);
	if (!ol->obj->dontfree)
		free(ol->obj);
	free(ol);
    }

    while (eng->deregistered) {
	hd_rect *r;
	r = eng->deregistered;
	eng->deregistered = eng->deregistered->next;
	free(r);
    }


    safe_free(eng->buffer);
    safe_free(eng);
}

void HD_ClipRect (hd_rect *rect, hd_rect *clip) 
{
    if (!rect || !clip) return;
    
    if (rect->x < clip->x) rect->x = clip->x;
    if (rect->y < clip->y) rect->y = clip->y;
    if (rect->x + rect->w > clip->x + clip->w) rect->w = clip->w + clip->x - rect->x;
    if (rect->y + rect->h > clip->y + clip->h) rect->h = clip->h + clip->y - rect->y;
}

void HD_ExpandRect (hd_rect *rect, hd_rect *strut) 
{
    if (!rect || !strut) return;
    
    if (rect->x > strut->x) rect->x = strut->x;
    if (rect->y > strut->y) rect->y = strut->y;
    if (rect->x + rect->w < strut->x + strut->w) rect->w = strut->w + strut->x - rect->x;
    if (rect->y + rect->h < strut->y + strut->h) rect->h = strut->h + strut->y - rect->y;
}

void HD_Register(hd_engine *eng,hd_object *obj)
{
    if (eng->list) {
        hd_obj_list *cur = eng->list;
        while (cur->next) {
            if (cur->obj == obj) return; // already registered
            cur = cur->next;
        }
        if (cur->obj == obj) return; // maybe it was the last object
        cur->next = (hd_obj_list *)malloc (sizeof(hd_obj_list));
        cur->next->obj = obj;
        cur->next->next = 0;
    } else {
        eng->list = (hd_obj_list *)malloc (sizeof(hd_obj_list));
        eng->list->obj = obj;
        eng->list->next = 0;
    }

    obj->eng = eng;
}

void HD_Deregister (hd_engine *eng, hd_object *obj) 
{
    if (obj->eng != eng) return;
    
    hd_obj_list *cur = eng->list, *previous = 0, *next;
    while (cur) {
        next = cur->next;
        
        if (cur->obj == obj) {
            if (previous) previous->next = cur->next;
            else          eng->list = cur->next;

            // Add it to the deregistered dirty list: first cur pos
            if (obj->x + obj->w > 0 && obj->x < obj->eng->screen.width &&
                obj->y + obj->h > 0 && obj->y < obj->eng->screen.height)
            {
                hd_rect *der = eng->deregistered;
                eng->deregistered = malloc (sizeof(hd_rect));
                HD_CopyRect (eng->deregistered, &cur->obj->x);
                HD_ClipRect (eng->deregistered, &eng->screen.rect);
                eng->deregistered->next = der;
            }

            // then old pos
            if (obj->last.x + obj->last.w > 0 && obj->last.x < obj->eng->screen.width &&
                obj->last.y + obj->last.h > 0 && obj->last.y < obj->eng->screen.height &&
                !HD_SameRect (eng->deregistered, &cur->obj->last))
            {
                hd_rect *der = eng->deregistered;
                eng->deregistered = malloc (sizeof(hd_rect));
                HD_CopyRect (eng->deregistered, &cur->obj->last);
                HD_ClipRect (eng->deregistered, &eng->screen.rect);
                eng->deregistered->next = der;
            }

            free (cur);
        } else {
            previous = cur;
        }

        cur = next;
    }

    obj->eng = 0;
}

void HD_Animate (hd_engine *eng) {
    hd_obj_list *cur = eng->list;
    while (cur) {
        hd_object *obj = cur->obj;
        hd_obj_list *next = cur->next; // in case the obj is dereg'ed at end of anim

        if (obj->animating && obj->animate)
            obj->animate (obj);
        if (!obj->animating && obj->pending_animations) {
            hd_pending_animation *pa = obj->pending_animations;
            obj->pending_animations = pa->next;
            obj->animdata = pa->animdata;
            obj->animate = pa->animate;
            if (pa->setup) pa->setup (obj);
            obj->animating = 1;
            HD_CopyRect (&obj->preanim, &obj->x);
        }
        
        cur = next;
    }
}

#ifdef IPOD
extern void _HD_ARM_Convert16 (uint32 *src, uint16 *dst, uint32 count);
extern void _HD_ARM_ClearScreen (uint32 *fb, uint32 pixels);
#endif

// We support three "dirty modes" until we decide which one gives
// the best speed. They are:
// - Conservative dirties (default): Redraw everything, but only update() dirty areas.
// - Object dirties (-DOBJECT_DIRTIES): Redraw all dirty objects. If a dirty touches
//   a non-dirty, redraw all of the non-dirty, unless the non-dirty completely
//   envelops the dirty and the dirty is stacked on top of it, in which case
//   redraw only the covered part.
// - Precise dirties (-DPRECISE_DIRTIES): Redraw all dirty objects. If a dirty touches
//   a non-dirty, redraw only the touched part. Tends to produce subtle artifacts.
void HD_Render(hd_engine *eng) {
	hd_obj_list *cur = eng->list;
        hd_rect *dirties = 0, *rect = 0;
        int needsort = 0;
        int needrender = 0;

        rect = dirties = calloc (1, sizeof(hd_rect)); // just a NULL rect

        // Make a list of all dirty rects. Rects are clipped
        // to screen from the beginning.
#ifdef OBJECT_DIRTIES
        // Magic dirty values:
        // 1+ - dirty, not rected
        // 0 - not dirty
        // -1 - dirty and rected, will be fully redrawn
        // -2 - dirty, not rected, will be redrawn where covered
        int dirtied;
        do {
            dirtied = 0;
            cur = eng->list;
            while (cur) {
                hd_object *obj = cur->obj;

                if ((obj->last.x != obj->x || obj->last.y != obj->y ||
                     obj->last.w != obj->w || obj->last.h != obj->h ||
                     obj->last.z != obj->z || obj->dirty) && (obj->dirty >= 0)) {
                    needrender = 1;

                    if (obj->x + obj->w > 0 && obj->x < eng->screen.width &&
                        obj->y + obj->h > 0 && obj->y < eng->screen.height) {
                        // One rect: current position
                        rect->next = malloc (sizeof(hd_rect));
                        rect = rect->next;
                        rect->x = (obj->x < 0)? 0 : obj->x;
                        rect->y = (obj->y < 0)? 0 : obj->y;
                        rect->w = (obj->x + obj->w > eng->screen.width)? eng->screen.width - obj->x : obj->w;
                        rect->h = (obj->y + obj->h > eng->screen.height)? eng->screen.height-obj->y : obj->h;
                        rect->z = obj->z;
                    }
                    
                    if (obj->last.x + obj->last.w > 0 && obj->last.x < eng->screen.width &&
                        obj->last.y + obj->last.h > 0 && obj->last.y < eng->screen.height) {
                        // Next rect: old position
                        rect->next = malloc (sizeof(hd_rect));
                        rect = rect->next;
                        
                        rect->x = (obj->last.x < 0)? 0 : obj->last.x;
                        rect->y = (obj->last.y < 0)? 0 : obj->last.y;
                        rect->w = (obj->last.x + obj->last.w > eng->screen.width)? eng->screen.width - obj->last.x : obj->last.w;
                        rect->h = (obj->last.y + obj->last.h > eng->screen.height)? eng->screen.height - obj->last.y : obj->last.h;
                        rect->z = obj->last.z;
                    }
                        
                    rect->next = 0;

                    dirtied++;
                    obj->dirty = -1;

                    if (obj->last.z != obj->z) needsort = 1;

                    hd_obj_list *rlap = eng->list;
                    while (rlap) {
                        if ((rlap->obj->dirty != -1) &&
                            (obj->x + obj->w > rlap->obj->x) && (obj->y + obj->h > rlap->obj->y) &&
                            (obj->x < rlap->obj->x + rlap->obj->w) && (obj->y < rlap->obj->y + rlap->obj->h)) {
                            if ((rlap->obj->dirty != 1) &&
                                (rlap->obj->x <= MAX (obj->x, 0)) && (rlap->obj->y <= MAX (obj->y, 0)) &&
                                (rlap->obj->x + rlap->obj->w >= MIN (obj->x + obj->w, eng->screen.width)) &&
                                (rlap->obj->y + rlap->obj->h >= MIN (obj->y + obj->h, eng->screen.height)) && // fully envelops
                                (rlap->obj->z > obj->z) && // and is below
                                rlap->obj->renderpart) { // and can render parts
                                rlap->obj->dirty = -2;
                            } else {
                                rlap->obj->dirty = 1;
                                dirtied++;
                            }
                        }
                        rlap = rlap->next;
                    }
                }
                cur = cur->next;
            }
        } while (dirtied != 0);
#else
        while (cur) {
            hd_object *obj = cur->obj;
            
            if (obj->last.x != obj->x || obj->last.y != obj->y ||
                obj->last.w != obj->w || obj->last.h != obj->h ||
                obj->last.z != obj->z || obj->dirty) {
                needrender = 1;

                if (obj->x + obj->w > 0 && obj->x < eng->screen.width &&
                    obj->y + obj->h > 0 && obj->y < eng->screen.height) {
                    // One rect: current position
                    rect->next = malloc (sizeof(hd_rect));
                    rect = rect->next;
                    rect->x = (obj->x < 0)? 0 : obj->x;
                    rect->y = (obj->y < 0)? 0 : obj->y;
                    rect->w = (obj->x + obj->w > eng->screen.width)? eng->screen.width - obj->x : obj->w;
                    rect->h = (obj->y + obj->h > eng->screen.height)? eng->screen.height-obj->y : obj->h;
                    rect->z = obj->z;
                }

                if (obj->last.x + obj->last.w > 0 && obj->last.x < eng->screen.width &&
                    obj->last.y + obj->last.h > 0 && obj->last.y < eng->screen.height) {
                    // Next rect: old position
                    rect->next = malloc (sizeof(hd_rect));
                    rect = rect->next;
                    
                    rect->x = (obj->last.x < 0)? 0 : obj->last.x;
                    rect->y = (obj->last.y < 0)? 0 : obj->last.y;
                    rect->w = (obj->last.x + obj->last.w > eng->screen.width)? eng->screen.width - obj->last.x : obj->last.w;
                    rect->h = (obj->last.y + obj->last.h > eng->screen.height)? eng->screen.height - obj->last.y  : obj->last.h;
                    rect->z = obj->last.z;
                }
                
                rect->next = 0;
                
                obj->dirty = 1;
                if (obj->last.z != obj->z) needsort = 1;
            }
            cur = cur->next;
        }
#endif

        // Tack on the deregistered dirties
        if (rect) rect->next = eng->deregistered;
        else rect = eng->deregistered;
        eng->deregistered = 0;

        // Sort 'em if necessary
        if (needsort)
            eng->list = HD_StackObjects (eng->list);

        if (!needrender) {
		while (dirties) {
			rect = dirties->next;
			free(dirties);
			dirties = rect;
		}
		return;
	}

#if !defined(PRECISE_DIRTIES) && !defined(OBJECT_DIRTIES)
        // Clear FB
#ifdef IPOD
        _HD_ARM_ClearScreen (HD_SRF_PIXELS (eng->buffer), eng->screen.width * eng->screen.height);
#else
        memset (HD_SRF_PIXELS (eng->buffer), 0, eng->screen.width * eng->screen.height * 4);
#endif
#else
        // Clear dirties
        rect = dirties;
        while (rect) {
            uint32 *p = HD_SRF_PIXPTR (eng->buffer, rect->x, rect->y);
            uint32 *endp = HD_SRF_END (eng->buffer);
            while (p < endp) {
                memset (p, 0, rect->w*4);
                p += eng->screen.width;
            }
            rect = rect->next;
        }
#endif

#if defined(OBJECT_DIRTIES)
        // Draw -2 dirties
        cur = eng->list;
        while (cur) {
            if (cur->obj->dirty == -2) {
                rect = dirties;
                while (rect) {
                    if ((rect->x >= cur->obj->x) && (rect->y >= cur->obj->y) &&
                        (rect->x + rect->w <= cur->obj->x + cur->obj->w) &&
                        (rect->y + rect->h <= cur->obj->y + cur->obj->h)) {
                        cur->obj->renderpart (cur->obj, eng->buffer, rect->x - cur->obj->x,
                                              rect->y - cur->obj->y, rect->w, rect->h, 0, 0);
                    }
                    rect = rect->next;
                }
            }
            cur = cur->next;
        }

        // Draw -1 dirties
        cur = eng->list;
        while (cur) {
            if (cur->obj->dirty == -1) {
                cur->obj->render (cur->obj, eng->buffer, 0, 0);
            }
            cur->obj->last.x = cur->obj->x;
            cur->obj->last.y = cur->obj->y;
            cur->obj->last.w = cur->obj->w;
            cur->obj->last.h = cur->obj->h;
            cur->obj->last.z = cur->obj->z;
            cur->obj->dirty = 0;
            cur = cur->next;
        }
#elif defined(PRECISE_DIRTIES)
        // Draw all parts of objects that are touched by rects
        rect = dirties;
        while (rect) {
            cur = eng->list;
            while (cur) {
                hd_object *obj = cur->obj;
                if (!obj->dirty) {
                    // Just check if it overlaps at all for now. The conditional
                    // will be executed N*M times, so keep it quick.
                    if ((obj->x + obj->w > rect->x) && (obj->y + obj->h > rect->y) &&
                        (obj->x < rect->x + rect->w) && (obj->y < rect->y + rect->h)) {
                        int x, y, w, h;
                        // Rect intersection of obj and rect.
                        // In these diagrams, < > is the object and [ ] is the dirty rect.
                        if (rect->x > obj->x) {
                            // <[>] or <[]>
                            x = rect->x;
                            if (rect->x + rect->w < obj->x + obj->w) {
                                // <[]>
                                w = rect->w;
                            } else {
                                // <[>]
                                w = obj->w - rect->x + obj->x;
                            }
                        } else {
                            // [<]> or [<>]
                            x = obj->x;
                            if (obj->x + obj->w < rect->x + rect->w) {
                                // [<>]
                                w = obj->w;
                            } else {
                                // [<]>
                                w = rect->w - obj->x + rect->x;
                            }
                        }
                        
                        // Similar for the Y coordinates.
                        if (rect->y > obj->y) {
                            y = rect->y;
                            if (rect->y + rect->h < obj->y + obj->h)
                                h = rect->h;
                            else
                                h = obj->h - rect->y + obj->y;
                        } else {
                            y = obj->y;
                            if (obj->y + obj->h < rect->y + rect->h) {
                                h = obj->h;
                            } else {
                                h = rect->h - obj->y + rect->y;
                            }
                        }

                        // Draw it.
                        if (obj->renderpart)
                            obj->renderpart (obj, eng->buffer, x - obj->x, y - obj->y, w, h, 0, 0);
                        else
                            obj->render (obj, eng->buffer, 0, 0);
                    }
                }
                cur = cur->next;
            }
            rect = rect->next;
        }
        
        cur = eng->list;
        while (cur) {
            hd_object *obj = cur->obj;
            if (obj->dirty) {
                // This is the dirty one. Draw it and update last.
                obj->render (obj, eng->buffer, 0, 0);
                obj->last.x = obj->x;
                obj->last.y = obj->y;
                obj->last.w = obj->w;
                obj->last.h = obj->h;
                obj->last.z = obj->z;
                obj->dirty = 0;
            }
            cur = cur->next;
        }
#else
        // Draw everything
        cur = eng->list;
        while (cur) {
            cur->obj->render (cur->obj, eng->buffer, 0, 0);
            cur->obj->last.x = cur->obj->x;
            cur->obj->last.y = cur->obj->y;
            cur->obj->last.w = cur->obj->w;
            cur->obj->last.h = cur->obj->h;
            cur->obj->last.z = cur->obj->z;
            cur->obj->dirty = 0;
            cur = cur->next;
        }
#endif

        /** Arguable whether we should only translate dirty areas...
         ** the ASM code below might get rather slower if we had to worry about short regions,
         ** non-multiple-of-4 areas, etc.
         **/

	// Translate internal ARGB8888 to RGB565
	if (eng->screen.framebuffer) {
#ifndef IPOD
		uint32 off,sPix;
		uint16 dPix;

		for(off=0;off<(eng->screen.width*eng->screen.height);off++) {
                	sPix = HD_SRF_PIXELS(eng->buffer)[off];

			dPix  = ((sPix & 0x00FF0000) >> (16+3)) << 11; // R
			dPix |= ((sPix & 0x0000FF00) >> (8+2)) << 5;  // G
			dPix |= ((sPix & 0x000000FF) >> (3));    // B

			eng->screen.framebuffer[off] = dPix;
		}
#else
                uint32 * srcptr;
                uint16 * dstptr;
                
                srcptr = HD_SRF_PIXELS (eng->buffer);
                dstptr = eng->screen.framebuffer;
                
                uint32 count = eng->screen.width*eng->screen.height;
                _HD_ARM_Convert16 (srcptr, dstptr, count);
#endif
	}
        // Translate to Y2
        if (eng->screen.fb2bpp) {
            uint32 *sPix = HD_SRF_PIXELS (eng->buffer);
            uint8 *dPix = eng->screen.fb2bpp;
#ifndef IPOD
            uint32 *endPix = HD_SRF_END (eng->buffer);

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
            _HD_ARM_Convert2 (sPix, dPix, eng->screen.width * eng->screen.height);
#endif
        }

        rect = dirties;
        while (rect) {
            hd_rect *next;
#ifndef DARWIN /* for some reason updating rect's is incredibly slow on OS X */
#ifndef IPOD
            eng->screen.update (eng, rect->x, rect->y, rect->w, rect->h);
#endif
#endif
            next = rect->next;
            free (rect);
            rect = next;
        }
#if defined(DARWIN) || defined(IPOD)
        eng->screen.update (eng, 0, 0, eng->screen.width, eng->screen.height);
#endif
}

#ifdef IPOD
extern void _HD_ARM_LowerBlit_ScaleBlend (hd_surface, uint32, uint32, uint32, uint32,
                                          hd_surface, uint32, uint32, uint32, uint32, uint8);
extern void _HD_ARM_LowerBlit_Blend (hd_surface, uint32, uint32,
                                     hd_surface, uint32, uint32, uint32, uint32, uint8);
extern void _HD_ARM_LowerBlit_Fast (hd_surface, uint32, uint32,
                                    hd_surface, uint32, uint32, uint32, uint32);
#endif

void HD_ScaleBlendClip (hd_surface ssrf, int sx, int sy, int sw, int sh,
                        hd_surface dsrf, int dx, int dy, int dw, int dh, int speed, uint8 opacity)
{
  int stw = HD_SRF_WIDTH (ssrf), sth = HD_SRF_HEIGHT (ssrf),
      dtw = HD_SRF_WIDTH (dsrf), dth = HD_SRF_HEIGHT (dsrf);
  uint32 *sbuf = HD_SRF_PIXELS (ssrf), *dbuf = HD_SRF_PIXELS (dsrf);
  int32 fp_step_x,fp_step_y,fp_ix,fp_iy,fp_initial_ix,fp_initial_iy;
  int32 x,y;
  int32 startx,starty,endx,endy;
  uint32 buffOff, imgOff;

  // Source clipping
  if (sx < 0) { sw += sx; sx = 0; }
  if (sy < 0) { sh += sy; sy = 0; }
  if (sx+sw > stw) { sw = stw - sx; }
  if (sy+sh > sth) { sh = sth - sy; }
  
  if (!dw || !dh) return;
  fp_step_x = (sw << 16) / dw;
  fp_step_y = (sh << 16) / dh;

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
        fp_initial_ix = -(startx) * fp_step_x;
        startx = 0;
    }
    if( endx > dtw ) {
        endx = dtw - 1;
    }
    if( starty < 0 ) {
        fp_initial_iy = -(starty) * fp_step_y;
        starty = 0;
    }
    if( endy > dth ) {
        endy = dth - 1;
    }

    fp_initial_ix += (sx << 16);
    fp_initial_iy += (sy << 16);
  }

#ifdef IPOD
  (void)sbuf; (void)dbuf; (void)fp_ix; (void)fp_iy;
  (void)x; (void)y; (void)buffOff; (void)imgOff;
  if (speed & HD_SPEED_NOALPHA)
      _HD_ARM_LowerBlit_Fast (ssrf, fp_initial_ix >> 16, fp_initial_iy >> 16,
                              dsrf, startx, starty, endx - startx, endy - starty);
  else if (speed & HD_SPEED_NOSCALE)
      _HD_ARM_LowerBlit_Blend (ssrf, fp_initial_ix >> 16, fp_initial_iy >> 16,
                               dsrf, startx, starty, endx - startx, endy - starty, opacity);
  else
      _HD_ARM_LowerBlit_ScaleBlend (ssrf, fp_initial_ix, fp_initial_iy, fp_step_x, fp_step_y,
                                    dsrf, startx, endx - startx, starty, endy - starty, opacity);
#else
  buffOff = starty * dtw;// + startx;
  
  fp_iy = fp_initial_iy;
  for(y=starty;y<endy;y++) {
    fp_ix = fp_initial_ix;
    imgOff = (fp_iy>>16) * stw;
    
    for(x=startx;x<endx;x++) {
      
      BLEND_ARGB8888_ON_ARGB8888( dbuf[ buffOff + x ], sbuf[ imgOff + (fp_ix>>16) ], opacity );
      
      fp_ix += fp_step_x;
    }
    buffOff += dtw;
    fp_iy += fp_step_y;
  }
#endif
}

void HD_Destroy (hd_object *obj) 
{
    if (obj->eng) HD_Deregister (obj->eng, obj);
    
    obj->destroy (obj);
    if (!obj->dontfree)
        free (obj);
}

static int newobj_lz = 65535;

static void donothing_O (hd_object *obj) {}
static void call_render (hd_object *obj, hd_surface srf, int x, int y, int w, int h, int dxo, int dyo) 
{
    obj->render (obj, srf, dxo, dyo);
}
static void call_renderpart_if (hd_object *obj, hd_surface srf, int dxo, int dyo) 
{
    if (obj->renderpart != call_render) obj->renderpart (obj, srf, 0, 0, obj->w, obj->h, dxo, dyo);
}

void HD_NewObjectAt (hd_object *obj) 
{
    memset (obj, 0, sizeof(hd_object));
    obj->last.z = obj->z = newobj_lz--;
    obj->opacity = 0xff;
    obj->render = call_renderpart_if;
    obj->renderpart = call_render;
    obj->destroy = donothing_O;
}

hd_object *HD_NewObject() 
{
    hd_object *ret = malloc (sizeof(hd_object));
    assert (ret != NULL);
    HD_NewObjectAt (ret);
    return ret;
}
