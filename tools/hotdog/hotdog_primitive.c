/*
 * Copyright (c) 2005, James Jacobsson
 * Copyright (c) 2006, Courtney Cavin
 * Parts Copyright (c) A. Schiffler
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the organization nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <assert.h>

#include "hotdog.h"

#if 0
void HD_Primitive_Render(hd_engine *eng,hd_object *obj, int cx, int cy, int cw, int ch) {
  int32 x,y;
  int32 startx,starty,endx,endy;

  if(  obj->x+cx     <                  0 ) startx = 0; else startx = obj->x + cx;
  if( (obj->x+cx+cw) >  eng->screen.width ) endx   = eng->screen.width; else endx = obj->x + cx + cw;
  if(  obj->y+cy     <                  0 ) starty = 0; else starty = obj->y + cy;
  if( (obj->y+cy+ch) > eng->screen.height ) endy   = eng->screen.height; else endy = obj->y + cy + ch;


  for(y=starty;y<endy;y++) {
    for(x=startx;x<endx;x++) {
      
      BLEND_ARGB8888_ON_ARGB8888( eng->buffer[ y * eng->screen.width + x ], obj->sub.prim->color );
      
    }
  }

#if 0
  for(y=obj->y;y<(obj->y+obj->h);y++) {
    for(x=obj->x;x<(obj->x+obj->h);x++) {
      
      BLEND_ARGB8888_ON_ARGB8888( eng->buffer[ y * eng->screen.width + x ], obj->sub.prim->color );
      
    }
  }
#endif

}
#endif

#define SWAP(a,b)	\
  do {			\
    (a) ^= (b);		\
    (b) ^= (a);		\
    (a) ^= (b);		\
  } while (0)
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define ABS(a) (((a)>0)?(a):(-(a)))

static void hLine(hd_surface srf, int x1, int x2, int y, uint32 col)
{
	uint32 *p;
	/* TODO: check bounds */
	p = HD_SRF_ROWF(srf, y);
	for (; x1 < x2; ++x1)
		BLEND_ARGB8888_ON_ARGB8888(*(p + x1), col, 0xff)
}

void HD_Pixel(hd_surface srf, int x, int y, uint32 col)
{
	HD_SRF_BLENDPIX(srf,x,y,col);
}

/* NOTE: last time I tried using this algorithm the line always was a pixel
 * short. check for inconsistancies. */
void HD_Line(hd_surface srf, int x0, int y0, int x1, int y1, uint32 col)
{
	char steep;
	int dy, dx, x, y, yi;
	int err = 0;

	if (y0 == y1) {
		hLine(srf, x0, x1, y1, col);
		return;
	}

	steep = (ABS(y1 - y0) > ABS(x1 - x0));
	if (steep) {
		SWAP(x0, y0);
		SWAP(x1, y1);
	}
	if (x0 > x1) {
		SWAP(x0, x1);
		SWAP(y0, y1);
	}
	dx = x1 - x0;
	dy = ABS(y1 - y0);
	y = y0;
	yi = (y0 < y1) ? 1 : -1;
	for (x = x0; x < x1; ++x) {
		/* TODO: extract this if/else out of the loop */
		if (steep) HD_SRF_BLENDPIX(srf,y,x,col);
		else HD_SRF_BLENDPIX(srf,x,y,col);
		err += dy;
		if (2*err >= dx) {
			y += yi;
			err -= dx;
		}
	}
#if 0 /* Bresenham's line algorithm */
function line(x0, x1, y0, y1)
     boolean steep := abs(y1 - y0) > abs(x1 - x0)
     if steep then
         swap(x0, y0)
         swap(x1, y1)
     if x0 > x1 then
         swap(x0, x1)
         swap(y0, y1)
     int deltax := x1 - x0
     int deltay := abs(y1 - y0)
     int error := 0
     int y := y0
     if y0 < y1 then ystep := 1 else ystep := -1
     for x from x0 to x1
         if steep then plot(y,x) else plot(x,y)
         error := error + deltay
         if 2*error >= deltax
             y := y + ystep
             error := error - deltax
#endif
}

void HD_FillRect(hd_surface srf, int x1, int y1, int x2, int y2, uint32 col)
{
	int xi;
	uint32 *p;

	if (x1 == x2) return;
	if (x1 > x2) SWAP(x1, x2);
	if (y1 == y2) return;
	if (y1 > y2) SWAP(y1, y2);

	y2 = MIN(HD_SRF_HEIGHT(srf), y2);
	x2 = MIN(HD_SRF_WIDTH(srf), x2);

	x1 = MAX(0, x1);
	y1 = MAX(0, y1);

	if (x2 <= x1 || y2 <= y1) return;

	for (; y1 < y2; ++y1) {
		/* TODO: increment rather than assign? */
		p = HD_SRF_ROWF(srf, y1);
		for (xi = x1; xi < x2; ++xi)
			BLEND_ARGB8888_ON_ARGB8888(*(p + xi), col, 0xff)
	}
}

/* XXX: resizes rectangle to surface; should clip and remove clipped sides */
void HD_Rect(hd_surface srf, int x1, int y1, int x2, int y2, uint32 col)
{
	int xi;
	uint32 *b, *e;

	if (x1 == x2) return;
	if (x1 > x2) SWAP(x1, x2);
	if (y1 == y2) return;
	if (y1 > y2) SWAP(y1, y2);

	y2 = MIN(HD_SRF_HEIGHT(srf), y2);
	x2 = MIN(HD_SRF_WIDTH(srf), x2);

	x1 = MAX(0, x1);
	y1 = MAX(0, y1);

	if (x2 <= x1 || y2 <= y1) return;

	b = HD_SRF_ROWF(srf, y1);
	e = HD_SRF_ROWF(srf, y2 - 1);
	for (xi = x1; xi < x2; ++xi) {
		BLEND_ARGB8888_ON_ARGB8888(*(b + xi), col, 0xff)
		BLEND_ARGB8888_ON_ARGB8888(*(e + xi), col, 0xff)
	}
	for (; y1 < y2; ++y1) {
		b = HD_SRF_ROWF(srf, y1);
		BLEND_ARGB8888_ON_ARGB8888(*(b + x1), col, 0xff)
		BLEND_ARGB8888_ON_ARGB8888(*(b + x2 - 1), col, 0xff)
	}
}

/* XXX: add clipping */
void HD_Circle(hd_surface srf, int x, int y, int r, uint32 col)
{
	int16 cx = 0;
	int16 cy = r;
	int16 ocx = 0xffff;
	int16 ocy = 0xffff;
	int16 df = 1 - r;
	int16 d_e = 3;
	int16 d_se = -2 * r + 5;
	int16 xpcx, xpcy, xmcx, xmcy;
	int16 ypcx, ypcy, ymcx, ymcy;

	if (r < 0) return;
	if (r == 0) {
		HD_SRF_BLENDPIX(srf, x, y, col);
		return;
	}

	do {
		if (ocy != cy || ocx != cx) {
			xpcx = x + cx;
			xmcx = x - cx;
			if (cy > 0) {
				ypcy = y + cy;
				ymcy = y - cy;
				HD_SRF_BLENDPIX(srf, xmcx, ypcy, col);
				HD_SRF_BLENDPIX(srf, xpcx, ypcy, col);
				HD_SRF_BLENDPIX(srf, xmcx, ymcy, col);
				HD_SRF_BLENDPIX(srf, xpcx, ymcy, col);
			}
			else {
				HD_SRF_BLENDPIX(srf, xmcx, y, col);
				HD_SRF_BLENDPIX(srf, xpcx, y, col);
			}
			ocy = cy;
			xpcy = x + cy;
			xmcy = x - cy;
			if (cx > 0) {
				ypcx = y + cx;
				ymcx = y - cx;
				HD_SRF_BLENDPIX(srf, xmcy, ypcx, col);
				HD_SRF_BLENDPIX(srf, xpcy, ypcx, col);
				HD_SRF_BLENDPIX(srf, xmcy, ymcx, col);
				HD_SRF_BLENDPIX(srf, xpcy, ymcx, col);
			}
			else {
				HD_SRF_BLENDPIX(srf, xmcy, y, col);
				HD_SRF_BLENDPIX(srf, xpcy, y, col);
			}
			ocx = cx;
		}	
		if (df < 0) {
			df += d_e;
			d_e += 2;
			d_se += 2;
		}
		else {
			df += d_se;
			d_e += 2;
			d_se += 4;
			--cy;
		}
		++cx;
	} while (cx <= cy);
}

/* XXX: add clipping */
void HD_FillCircle(hd_surface srf, int x, int y, int r, uint32 col)
{
	int16 cx = 0;
	int16 cy = r;
	int16 ocx = 0xffff;
	int16 ocy = 0xffff;
	int16 df = 1 - r;
	int16 d_e = 3;
	int16 d_se = -2 * r + 5;
	int16 xpcx, xpcy, xmcx, xmcy;

	if (r < 0) return;
	if (r == 0) {
		HD_SRF_BLENDPIX(srf, x, y, col);
		return;
	}

	do {
		xpcx = x + cx;
		xmcx = x - cx;
		xpcy = x + cy;
		xmcy = x - cy;
		if (ocy != cy) {
			if (cy > 0) {
				hLine(srf, xmcx, xpcx, y + cy, col);
				hLine(srf, xmcx, xpcx, y - cy, col);
			}
			else
				hLine(srf, xmcx, xpcx, y, col);
			ocy = cy;
		}
		if (ocx != cx) {
			if (cx != cy) {
				if (cx > 0) {
					hLine(srf, xmcy, xpcy, y - cx, col);
					hLine(srf, xmcy, xpcy, y + cx, col);
				}
				else
					hLine(srf, xmcy, xpcy, y, col);
			}
			ocx = cx;
		}	
		if (df < 0) {
			df += d_e;
			d_e += 2;
			d_se += 2;
		}
		else {
			df += d_se;
			d_e += 2;
			d_se += 4;
			--cy;
		}
		++cx;
	} while (cx <= cy);
}
