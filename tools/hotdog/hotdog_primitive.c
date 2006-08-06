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
#include <stdlib.h>
#include <assert.h>
#include <math.h>

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
	if (x1 > x2) SWAP(x1, x2);
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

#define BLENDPIX_WEIGHT(srf,x,y,pix,wei)			\
  do {								\
    uint32 p;							\
    _HD_ALPHA_BLEND(HD_SRF_GETPIX(srf, x, y), pix, p, wei);	\
    HD_SRF_SETPIX(srf,x,y,p);					\
  } while (0)

void HD_AALine(hd_surface srf, int x0, int y0, int x1, int y1, uint32 col)
{
	uint32 erracc, erradj;
	uint32 erracctmp, wgt;
	int dx, dy, xdir;

	if (y0 > y1) {
		SWAP(y0, y1);
		SWAP(x0, x1);
	}

	dx = x1 - x0;
	dy = y1 - y0;

	if (dx >= 0)
		xdir = 1;
	else {
		xdir = -1;
		dx = (-dx);
	}

	if (dx == 0 || dy == 0 || dx == dy) {
		HD_Line(srf, x0, y0, x1, y1, col);
		return;
	}

	erracc = 0;

	HD_SRF_BLENDPIX(srf, x0, y0, col);

	if (dy > dx) {
		erradj = ((dx << 16) / dy) << 16;

		while (--dy) {
			erracctmp = erracc;
			erracc += erradj;
			if (erracc <= erracctmp)
				x0 += xdir;
			++y0;
	
			wgt = (erracc >> 24) & 0xff;
			BLENDPIX_WEIGHT(srf, x0, y0, col, 255 - wgt);
			BLENDPIX_WEIGHT(srf, x0 + xdir, y0, col, wgt);
		}
	}
	else {
		erradj = ((dy << 16) / dx) << 16;

		while (--dx) {
			erracctmp = erracc;
			erracc += erradj;
			if (erracc <= erracctmp)
				++y0;
			x0 += xdir;
			wgt = (erracc >> 24) & 0xff;
			BLENDPIX_WEIGHT(srf, x0, y0, col, 255 - wgt);
			BLENDPIX_WEIGHT(srf, x0, y0 + 1, col, wgt);
		}
	}

	HD_SRF_BLENDPIX(srf, x1, y1, col);
}

#define DO_HD_LINES(function, srf, p, n, col)				\
  do {									\
    for (--n; n; --n)							\
      function(srf, p[n - 1].x, p[n - 1].y, p[n].x, p[n].y, col);	\
  } while (0)

void HD_Lines(hd_surface srf, hd_point *points, int n, uint32 col)
{ DO_HD_LINES(HD_Line, srf, points, n, col); }

void HD_AALines(hd_surface srf, hd_point *points, int n, uint32 col)
{ DO_HD_LINES(HD_AALine, srf, points, n, col); }

#define DO_HD_POLY(function, srf, p, n, col)			\
  do {								\
    if (n < 3) return;						\
    function(srf, p[0].x, p[0].y, p[n - 1].x, p[n - 1].y, col);	\
    DO_HD_LINES(function, srf, p, n, col);			\
  } while (0)

void HD_Poly(hd_surface srf, hd_point *points, int n, uint32 col)
{ DO_HD_POLY(HD_Line, srf, points, n, col); }

void HD_AAPoly(hd_surface srf, hd_point *points, int n, uint32 col)
{ DO_HD_POLY(HD_AALine, srf, points, n, col); }

void HD_Bitmap(hd_surface srf, int x, int y, int w, int h,
		const unsigned short *bits, uint32 col)
{
	int minx, maxx;
	unsigned short value = 0;
	int count;

	minx = x;
	maxx = x + w - 1;
	count = 0;
	while (h > 0) {
		if (count <= 0) {
			count = 16;
			value = *bits++;
		}
		if (value & (1 << 15))
			HD_SRF_BLENDPIX(srf, x, y, col);
		value <<= 1;
		--count;

		if (x++ == maxx) {
			x = minx;
			++y;
			--h;
			count = 0;
		}
	}
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

void HD_AACircle(hd_surface srf, int xc, int yc, int r, uint32 col)
{ HD_AAEllipse(srf, xc, yc, r, r, col); }

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
		if (ocy != cy) {
			xpcx = x + cx;
			xmcx = x - cx;
			if (cy > 0) {
				hLine(srf, xmcx, xpcx, y + cy, col);
				hLine(srf, xmcx, xpcx, y - cy, col);
			}
			else
				hLine(srf, xmcx, xpcx, y, col);
			ocy = cy;
		}
		if (ocx != cx) {
			xpcy = x + cy;
			xmcy = x - cy;
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

void HD_AAFillCircle(hd_surface srf, int xc, int yc, int r, uint32 col)
{ HD_AAFillEllipse(srf, xc, yc, r, r, col); }

void HD_Ellipse(hd_surface srf, int x, int y, int rx, int ry, uint32 col)
{
	int ix, iy;
	int h, i, j, k;
	int oh, oi, oj, ok;
	int xmh, xph, ypk, ymk;
	int xmi, xpi, ymj, ypj;
	int xmj, xpj, ymi, ypi;
	int xmk, xpk, ymh, yph;

	if (rx < 0 || ry < 0)
		return;

	if (rx == 0) {
		HD_Line(srf, x, y - ry, x, y + ry, col);
		return;
	}
	if (ry == 0) {
		hLine(srf, x - rx, x + rx, y, col);
		return;
	}
	oh = oi = oj = ok = 0xffff;

	if (rx > ry) {
		ix = 0;
		iy = rx * 64;

		do {
			h = (ix + 32) >> 6;
			i = (iy + 32) >> 6;
			j = (h * ry) / rx;
			k = (i * ry) / rx;

			if (((ok != k) && (oj != k)) ||
					((oj != j) && (ok != j)) || (k != j)) {
				xph = x + h;
				xmh = x - h;
				if (k > 0) {
					ypk = y + k;
					ymk = y - k;
					HD_SRF_BLENDPIX(srf, xmh, ypk, col);
					HD_SRF_BLENDPIX(srf, xph, ypk, col);
					HD_SRF_BLENDPIX(srf, xmh, ymk, col);
					HD_SRF_BLENDPIX(srf, xph, ymk, col);
				}
				else {
					HD_SRF_BLENDPIX(srf, xmh, y, col);
					HD_SRF_BLENDPIX(srf, xph, y, col);
				}
				ok = k;
				xpi = x + i;
				xmi = x - i;
				if (j > 0) {
					ypj = y + j;
					ymj = y - j;
					HD_SRF_BLENDPIX(srf, xmi, ypj, col);
					HD_SRF_BLENDPIX(srf, xpi, ypj, col);
					HD_SRF_BLENDPIX(srf, xmi, ymj, col);
					HD_SRF_BLENDPIX(srf, xpi, ymj, col);
				}
				else {
					HD_SRF_BLENDPIX(srf, xmi, y, col);
					HD_SRF_BLENDPIX(srf, xpi, y, col);
				}
				oj = j;
			}

			ix = ix + iy / rx;
			iy = iy - ix / rx;

		} while (i > h);
	}
	else {
		ix = 0;
		iy = ry * 64;

		do {
			h = (ix + 32) >> 6;
			i = (iy + 32) >> 6;
			j = (h * rx) / ry;
			k = (i * rx) / ry;

			if (((oi != i) && (oh != i)) ||
					((oh != h) && (oi != h) && (i != h))) {
				xmj = x - j;
				xpj = x + j;
				if (i > 0) {
					ypi = y + i;
					ymi = y - i;
					HD_SRF_BLENDPIX(srf, xmj, ypi, col);
					HD_SRF_BLENDPIX(srf, xpj, ypi, col);
					HD_SRF_BLENDPIX(srf, xmj, ymi, col);
					HD_SRF_BLENDPIX(srf, xpj, ymi, col);
				}
				else {
					HD_SRF_BLENDPIX(srf, xmj, y, col);
					HD_SRF_BLENDPIX(srf, xpj, y, col);
				}
				oi = i;
				xmk = x - k;
				xpk = x + k;
				if (h > 0) {
					yph = y + h;
					ymh = y - h;
					HD_SRF_BLENDPIX(srf, xmk, yph, col);
					HD_SRF_BLENDPIX(srf, xpk, yph, col);
					HD_SRF_BLENDPIX(srf, xmk, ymh, col);
					HD_SRF_BLENDPIX(srf, xpk, ymh, col);
				}
				else {
					HD_SRF_BLENDPIX(srf, xmk, y, col);
					HD_SRF_BLENDPIX(srf, xpk, y, col);
				}
				oh = h;
			}
			ix = ix + iy / ry;
			iy = iy - ix / ry;

		} while (i > h);
	}
}

void HD_AAEllipse(hd_surface srf, int xc, int yc, int rx, int ry, uint32 col)
{
	int i;
	int a2, b2, ds, dt, dxt, t, s, d;
	int32 x, y, xs, ys, dyt, xx, yy, xc2, yc2;
	float cp;
	unsigned char weight, iweight;

	if (rx < 0 || ry < 0)
		return;

	if (rx == 0 || ry == 0) {
		HD_Line(srf, xc - rx, yc - ry, xc + ry, yc + ry, col);
		return;
	}

	a2 = rx * rx;
	b2 = ry * ry;

	ds = a2 << 1;
	dt = b2 << 1;

	xc2 = xc << 1;
	yc2 = yc << 1;

	dxt = (int)(a2 / sqrt(a2 + b2));

	t = 0;
	s = -2 * a2 * ry;
	d = 0;

	x = xc;
	y = yc - ry;

	HD_SRF_BLENDPIX(srf, x, y, col);
	HD_SRF_BLENDPIX(srf, xc2 - x, y, col);
	HD_SRF_BLENDPIX(srf, x, yc2 - y, col);
	HD_SRF_BLENDPIX(srf, xc2 - x, yc2 - y, col);

	for (i = 1; i <= dxt; ++i) {
		--x;
		d += t - b2;

		if (d >= 0)
			ys = y - 1;
		else if ((d - s - a2) > 0) {
			if ((2 * d - s - a2) >= 0)
				ys = y + 1;
			else {
				ys = y++;
				d -= s + a2;
				s += ds;
			}
		}
		else {
			ys = ++y + 1;
			d -= s + a2;
			s += ds;
		}

		t -= dt;

		/* Calculate alpha */
		if (s != 0.0) {
			cp = (float)ABS(d) / (float)ABS(s);
			if (cp > 1.0)
				cp = 1.0;
		}
		else
			cp = 1.0;

		/* Calculate weights */
		weight = (unsigned char)(cp * 255);
		iweight = 255 - weight;

		/* Upper half */
		xx = xc2 - x;
		BLENDPIX_WEIGHT(srf, x, y, col, iweight);
		BLENDPIX_WEIGHT(srf, xx, y, col, iweight);

		BLENDPIX_WEIGHT(srf, x, ys, col, weight);
		BLENDPIX_WEIGHT(srf, xx, ys, col, weight);

		/* Lower half */
		yy = yc2 - y;
		BLENDPIX_WEIGHT(srf, x, yy, col, iweight);
		BLENDPIX_WEIGHT(srf, xx, yy, col, iweight);

		yy = yc2 - ys;
		BLENDPIX_WEIGHT(srf, x, yy, col, weight);
		BLENDPIX_WEIGHT(srf, xx, yy, col, weight);
	}

	dyt = ABS(y - yc);

	for (i = 1; i <= dyt; ++i) {
		++y;
		d -= s + a2;

		if (d <= 0)
			xs = x + 1;
		else if ((d + t - b2) < 0) {
			if ((2 * d + t - b2) <= 0)
				xs = x - 1;
			else {
				xs = x--;
				d += t - b2;
				t -= dt;
			}
		} else {
			xs = --x - 1;
			d += t - b2;
			t -= dt;
		}

		s += ds;

		/* Calculate alpha */
		if (t != 0.0) {
			cp = (float) abs(d) / (float) abs(t);
			if (cp > 1.0)
				cp = 1.0;
		}
		else
			cp = 1.0;

		/* Calculate weight */
		weight = (unsigned char)(cp * 255);
		iweight = 255 - weight;

		/* Left half */
		xx = xc2 - x;
		yy = yc2 - y;
		BLENDPIX_WEIGHT(srf, x, y, col, iweight);
		BLENDPIX_WEIGHT(srf, xx, y, col, iweight);

		BLENDPIX_WEIGHT(srf, x, yy, col, iweight);
		BLENDPIX_WEIGHT(srf, xx, yy, col, iweight);

		/* Right half */
		xx = 2 * xc - xs;
		BLENDPIX_WEIGHT(srf, xs, y, col, weight);
		BLENDPIX_WEIGHT(srf, xx, y, col, weight);

		BLENDPIX_WEIGHT(srf, xs, yy, col, weight);
		BLENDPIX_WEIGHT(srf, xx, yy, col, weight);
	}
}

void HD_FillEllipse(hd_surface srf, int x, int y, int rx, int ry, uint32 col)
{
	int ix, iy;
	int h, i, j, k;
	int oh, oi, oj, ok;
	int xmh, xph;
	int xmi, xpi;
	int xmj, xpj;
	int xmk, xpk;

	if (rx < 0 || ry < 0)
		return;

	if (rx == 0 || ry == 0) {
		HD_Line(srf, x - rx, y - ry, x + rx, y + ry, col);
		return;
	}

	oh = oi = oj = ok = 0xffff;

	if (rx > ry) {
		ix = 0;
		iy = rx * 64;

		do {
			h = (ix + 32) >> 6;
			i = (iy + 32) >> 6;
			j = (h * ry) / rx;
			k = (i * ry) / rx;

			if ((ok != k) && (oj != k)) {
				xph = x + h;
				xmh = x - h;
				if (k > 0) {
					hLine(srf, xmh, xph, y + k, col);
					hLine(srf, xmh, xph, y - k, col);
				}
				else
					hLine(srf, xmh, xph, y, col);
				ok = k;
	  		}
			if ((oj != j) && (ok != j) && (k != j)) {
				xmi = x - i;
				xpi = x + i;
				if (j > 0) {
					hLine(srf, xmi, xpi, y + j, col);
					hLine(srf, xmi, xpi, y - j, col);
				}
				else
					hLine(srf, xmi, xpi, y, col);
				oj = j;
			}

			ix = ix + iy / rx;
			iy = iy - ix / rx;

		} while (i > h);
	} else {
		ix = 0;
		iy = ry * 64;

		do {
			h = (ix + 32) >> 6;
			i = (iy + 32) >> 6;
			j = (h * rx) / ry;
			k = (i * rx) / ry;

			if ((oi != i) && (oh != i)) {
				xmj = x - j;
				xpj = x + j;
				if (i > 0) {
					hLine(srf, xmj, xpj, y + i, col);
					hLine(srf, xmj, xpj, y - i, col);
				}
				else
					hLine(srf, xmj, xpj, y, col);
				oi = i;
			}
			if ((oh != h) && (oi != h) && (i != h)) {
				xmk = x - k;
				xpk = x + k;
				if (h > 0) {
					hLine(srf, xmk, xpk, y + h, col);
					hLine(srf, xmk, xpk, y - h, col);
				}
				else
					hLine(srf, xmk, xpk, y, col);
				oh = h;
			}

			ix = ix + iy / ry;
			iy = iy - ix / ry;

		} while (i > h);
	}
}

void HD_AAFillEllipse(hd_surface srf, int xc, int yc, int rx, int ry,uint32 col)
{
	int i, sty, styy;
	int a2, b2, ds, dt, dxt, t, s, d;
	int32 x, y, xs, ys, dyt, xx, yy, xc2, yc2;
	float cp;
	unsigned char weight, iweight;

	if (rx < 0 || ry < 0) return;

	if (rx == 0 || ry == 0) {
		HD_Line(srf, xc - rx, yc - ry, xc + rx, yc + ry, col);
		return;
	}

	a2 = rx * rx;
	b2 = ry * ry;

	ds = a2 << 1;
	dt = b2 << 1;

	xc2 = xc << 1;
	yc2 = yc << 1;

	dxt = (int)(a2 / sqrt(a2 + b2));

	t = 0;
	s = -2 * a2 * ry;
	d = 0;

	x = xc;
	y = yc - ry;

	HD_SRF_BLENDPIX(srf, x, y, col);
	HD_SRF_BLENDPIX(srf, xc2 - x, y, col);
	HD_SRF_BLENDPIX(srf, x, yc2 - y, col);
	HD_SRF_BLENDPIX(srf, xc2 - x, yc2 - y, col);

	hLine(srf, xc - rx, xc + rx, yc, col);
	sty = styy = 0;

	for (i = 1; i <= dxt; i++) {
		--x;
		d += t - b2;

		if (d >= 0)
			ys = y - 1;
		else if ((d - s - a2) > 0) {
			if ((2 * d - s - a2) >= 0)
				ys = y + 1;
			else {
				ys = y++;
				d -= s + a2;
				s += ds;
			}
		}
		else {
			ys = ++y + 1;
			d -= s + a2;
			s += ds;
		}

		t -= dt;

		/* Calculate alpha */
		if (s != 0.0) {
			cp = (float) abs(d) / (float) abs(s);
			if (cp > 1.0)
				cp = 1.0;
		}
		else
			cp = 1.0;

		/* Calculate weights */
		weight = (unsigned char)(cp * 255);
		iweight = 255 - weight;

		/* Upper half */
		xx = xc2 - x;
		BLENDPIX_WEIGHT(srf, x, y, col, iweight);
		BLENDPIX_WEIGHT(srf, xx, y, col, iweight);

		BLENDPIX_WEIGHT(srf, x, ys, col, weight);
		BLENDPIX_WEIGHT(srf, xx, ys, col, weight);

		/* Lower half */
		yy = yc2 - ys;
		BLENDPIX_WEIGHT(srf, x, yy, col, weight);
		BLENDPIX_WEIGHT(srf, xx, yy, col, weight);

		yy = yc2 - y;
		BLENDPIX_WEIGHT(srf, x, yy, col, iweight);
		BLENDPIX_WEIGHT(srf, xx, yy, col, iweight);


		/* Fill */
		if (i < dxt) {
			if (sty != y)
				hLine(srf, x, xx+1, y, col);
			if (styy != yy)
				hLine(srf, x, xx+1, yy, col);
			sty = y;
			styy = yy;
		}
	}

	dyt = abs(y - yc);

	for (i = 1; i <= dyt; i++) {
		++y;
		d -= s + a2;

		if (d <= 0)
			xs = x + 1;
		else if ((d + t - b2) < 0) {
			if ((2 * d + t - b2) <= 0)
				xs = x - 1;
			else {
				xs = x--;
				d += t - b2;
				t -= dt;
			}
		}
		else {
			xs = --x - 1;
			d += t - b2;
			t -= dt;
		}

		s += ds;

		/* Calculate alpha */
		if (t != 0.0) {
			cp = (float) abs(d) / (float) abs(t);
			if (cp > 1.0)
				cp = 1.0;
		}
		else
			cp = 1.0;

		/* Calculate weight */
		weight = (unsigned char) (cp * 255);
		iweight = 255 - weight;

		/* Left half */
		xx = xc2 - x;
		yy = yc2 - y;
		BLENDPIX_WEIGHT(srf, x, y, col, iweight);
		BLENDPIX_WEIGHT(srf, xx, y, col, iweight);
	
		BLENDPIX_WEIGHT(srf, x, yy, col, iweight);
		BLENDPIX_WEIGHT(srf, xx, yy, col, iweight);
		hLine(srf, x+1, xx, y-1, col);

		/* Right half */
		xx = xc2 - xs;
		BLENDPIX_WEIGHT(srf, xs, y, col, weight);
		BLENDPIX_WEIGHT(srf, xx, y, col, weight);

		BLENDPIX_WEIGHT(srf, xs, yy, col, weight);
		BLENDPIX_WEIGHT(srf, xx, yy, col, weight);
		hLine(srf, x+1, xc2 - x, yy+1, col);
	}
}



#define DO_HD_BEZIER(function)						\
  /*  Note: I don't think there is any great performance win in		\
   *  translating this to fixed-point integer math, most of the time	\
   *  is spent in the line drawing routine. */				\
  float x = (float)x1, y = (float)y1;					\
  float xp = x, yp = y;							\
  float delta;								\
  float dx, d2x, d3x;							\
  float dy, d2y, d3y;							\
  float a, b, c;							\
  int i;								\
  int n = 1;								\
  int16 xmax=x1, ymax=y1, xmin=x1, ymin=y1;				\
  									\
  /* compute number of iterations */					\
  if(level < 1) level = 1;						\
  if(level >= 15) level=15;						\
  while (level-- > 0)							\
    n*= 2;								\
  delta = (float)( 1.0 / (float)n );					\
									\
  /* compute finite differences						\
   * a, b, c are the coefficient of the polynom in t defining the	\
   * parametric curve. The computation is done independently for x and y */\
  a = (float)(-x1 + 3*x2 - 3*x3 + x4);					\
  b = (float)(3*x1 - 6*x2 + 3*x3);					\
  c = (float)(-3*x1 + 3*x2);						\
									\
  d3x = 6 * a * delta*delta*delta;					\
  d2x = d3x + 2 * b * delta*delta;					\
  dx = a * delta*delta*delta + b * delta*delta + c * delta;		\
									\
  a = (float)(-y1 + 3*y2 - 3*y3 + y4);					\
  b = (float)(3*y1 - 6*y2 + 3*y3);					\
  c = (float)(-3*y1 + 3*y2);						\
									\
  d3y = 6 * a * delta*delta*delta;					\
  d2y = d3y + 2 * b * delta*delta;					\
  dy = a * delta*delta*delta + b * delta*delta + c * delta;		\
									\
  /* iterate */								\
  for (i = 0; i < n; i++) {						\
    x += dx; dx += d2x; d2x += d3x;					\
    y += dy; dy += d2y; d2y += d3y;					\
      if((int16)xp != (int16)x || (int16)yp != (int16)y){		\
        function(srf, xp, yp, x, y, col);				\
        xmax= (xmax>(int16)xp)? xmax : (int16)xp;			\
        ymax= (ymax>(int16)yp)? ymax : (int16)yp;			\
        xmin= (xmin<(int16)xp)? xmin : (int16)xp;			\
        ymin= (ymin<(int16)yp)? ymin : (int16)yp;			\
        xmax= (xmax>(int16)x)? xmax : (int16)x;				\
        ymax= (ymax>(int16)y)? ymax : (int16)y;				\
        xmin= (xmin<(int16)x)? xmin : (int16)x;				\
        ymin= (ymin<(int16)y)? ymin : (int16)y;				\
      }									\
    xp = x; yp = y;							\
  }

void HD_Bezier(hd_surface srf, int x1, int y1, int x2, int y2, int x3, int y3,
		int x4, int y4, int level, uint32 col)
{ DO_HD_BEZIER(HD_Line) }
void HD_AABezier(hd_surface srf, int x1, int y1, int x2, int y2, int x3, int y3,
		int x4, int y4, int level, uint32 col)
{ DO_HD_BEZIER(HD_AALine) }


void HD_Blur(hd_surface srf, int x, int y, int w, int h, int rad)
{
	int  kx, ky;
	int lx, ly;
	int rsum, gsum, bsum, amt;
	uint32 col;
	int tx, ty;
	int diam = (rad << 1) + 1;

	if (rad <= 0 || w <= 0 || h <= 0)
		return;
	x = MIN(MAX(x, 0), HD_SRF_WIDTH(srf));
	y = MIN(MAX(y, 0), HD_SRF_HEIGHT(srf));
	w = MIN(w, HD_SRF_WIDTH(srf) - x);
	h = MIN(h, HD_SRF_HEIGHT(srf) - y);

	for (ly = 0; ly < h; ++ly) {
		for (lx = 0; lx < w; ++lx) {
			amt = rsum = gsum = bsum = 0;
			for (ky = 0; ky < diam; ++ky) {
				for (kx = 0; kx < diam; ++kx) {
					tx = x + lx + kx - rad;
					ty = y + ly + ky - rad;
					if (tx < 0 || tx > HD_SRF_WIDTH(srf) ||
					    ty < 0 || ty > HD_SRF_HEIGHT(srf))
						continue;
					col = HD_SRF_PIXF(srf, tx, ty);
					rsum += (col & 0xff0000) >> 16;
					gsum += (col & 0x00ff00) >> 8;
					bsum += (col & 0x0000ff);
					++amt;
				}
			}
			HD_SRF_PIXF(srf, x + lx, y + ly) = 0xff000000 |
				(rsum/amt) << 16 | (gsum/amt) << 8 | (bsum/amt);
		}
	}
}

