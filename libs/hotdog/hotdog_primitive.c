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
#include <string.h>
#include <assert.h>
#include <math.h>
#include <limits.h>

#include "hotdog.h"

#define SWAP(a,b)	\
  do {			\
    (a) ^= (b);		\
    (b) ^= (a);		\
    (a) ^= (b);		\
  } while (0)
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define ABS(a) (((a)>0)?(a):(-(a)))
#define ROUND(a) ((double)(((a)>0)?(int)((a) + 0.5f):(int)((a) - 0.5f)))

int iround(double x)
{
	return (int)ROUND(x);
}

/* Draws from (x1, y) to (x2, y) */
static void hLine(hd_surface srf, int x1, int x2, int y, uint32 col)
{
	uint32 *p;
        if (y < 0 || y >= (int)HD_SRF_HEIGHT(srf)) return; /* off-surface */
	if (x1 > x2) SWAP(x1, x2);
        if (x1 < 0) x1 = 0;
        if (x2 >= (int)HD_SRF_WIDTH(srf)) x2 = HD_SRF_WIDTH(srf) - 1;
        if (x1 > x2) return; /* x1 off to the right, or no line to draw */
	p = HD_SRF_ROWF(srf, y);
	for (; x1 <= x2; ++x1)
		BLEND_ARGB8888_ON_ARGB8888(*(p + x1), col, 0xff)
}

void HD_Pixel(hd_surface srf, int x, int y, uint32 col)
{
	/* W & H bounding are done in HD_SRF_SETPIX, from HD_SRF_BLENDPIX */
        if (y < 0 || x < 0) return;
	HD_SRF_BLENDPIX(srf,x,y,col);
}

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
	for (x = x0; x <= x1; ++x) {
		if (steep) HD_Pixel(srf,y,x,col);
		else HD_Pixel(srf,x,y,col);
		err += dy;
		if (2*err >= dx) {
			y += yi;
			err -= dx;
		}
	}
}

#define BLENDPIX_WEIGHT(srf,x,y,pix,wei)			\
  do {								\
    uint32 p;							\
    if (x < 0 || y < 0) break;					\
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

	HD_Pixel(srf, x0, y0, col);

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

	HD_Pixel(srf, x1, y1, col);
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
			HD_Pixel(srf, x, y, col);
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

typedef struct _hd_bucket {
	int n;
	int allocd;
	hd_point *x;
} hd_bucket;
typedef struct _hd_hsls {
	int y;
	int xl, xr;
} hd_hsls;
typedef struct _hd_poly {
	int ymax, ymin, n;
	hd_bucket *net;	
	hd_bucket *ppoly;	
} hd_poly;

static void record_point(hd_bucket *b, hd_point *p)
{
	if (b->n + 1 > b->allocd) {
		b->allocd += 256;
		/* allocate for hd_hsls so we don't have to realloc later */
		b->x = xrealloc(b->x, b->allocd * sizeof(hd_hsls));
	}
	/* in order for us to be able to translate this to hd_hsls, we have
	 * to swap x and y for now, don't panic. */
	b->x[b->n].x = p->y;
	b->x[b->n++].y = p->x;
}

/* simple bresenham algo */
static void record_line(hd_bucket *ppoly, hd_point *a, hd_point *b)
{
#define RECORD(xp,yp)                \
do { hd_point r;                     \
     if (steep) r.x = yp, r.y = xp;  \
     else r.x = xp, r.y = yp;        \
     record_point(ppoly, &r); } while (0);
	char steep;
	int dy, dx, yi, xi;
	int x0, y0, x1, y1;
	int err = 0;

	x0 = a->x; x1 = b->x;
	y0 = a->y; y1 = b->y;

	steep = (ABS(y1 - y0) > ABS(x1 - x0));
	if (steep) {
		SWAP(x0, y0);
		SWAP(x1, y1);
	}

	dx = ABS(x1 - x0);
	dy = ABS(y1 - y0);
	yi = (y0 < y1) ? 1 : -1;
	xi = (x0 > x1) ? -1 : 1;
	while (x0 != x1) {
		RECORD(x0,y0);
		err += dy;
		if (2*err >= dx) {
			y0 += yi;
			err -= dx;
		}
		x0 += xi;
	}
#undef RECORD
}

static int cmp_coord(const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
}

/* converts an array of hd_points (2 ints) to an array of hd_hsls (3 ints)
 * x and y are already swapped for fast translation ex: YXYXYXYX => YXXYXX */
static inline void hsls(hd_point *d, int **dest)
{
	int i;
	hd_hsls h;

	for (i = 1, h.xr = h.xl = d->y;
			(int *)(d + i) < *dest &&
			(d + i)->x == d->x; ++i) {
		h.xl = MIN(h.xl, (d+i)->y);
		h.xr = MAX(h.xr, (d+i)->y);
	}
	if (!(--i)) { /* zero consecutive, shortest hsls: YX => YXX */
		memmove(d + 1, (int *)d + 1,
				(*dest - ((int *)d + 1)) * sizeof(int));
		*dest += 1;
	}
	else { /* we have -i- consecutive pixels: YXYXYX => YXX */
		memmove((int *)d + 3, d + i + 1,
				(*dest - (int *)(d + i + 1)) * sizeof(int));
		*((int *)d + 1) = h.xl;
		*((int *)d + 2) = h.xr;
		*dest -= 2 * i - 1;
	}
}

/* take preconverted hsls array and merge end hsls if needed */
static inline void end_hsls(hd_hsls *h, int *n)
{
	if (h->y != (h + *n - 1)->y) return;

	--(*n);
	h->xl = MIN(h->xl, (h + *n)->xl);
	h->xr = MAX(h->xr, (h + *n)->xr);
}

/* simplify an array of hd_points into hd_hsls, remember x and y are swapped */
static void simplify(hd_bucket *b)
{
	int *d;
	int *dest;

	if (!b->x || !b->n) return;

	dest = (int *)(b->x + b->n);
	for (d = (int *)b->x; d < dest; d += 3)
		hsls((hd_point *)d, &dest);
	b->n = (dest - (int *)b->x) / 3;

	end_hsls((hd_hsls *)b->x, &b->n);
}

/* back to a two int set of values, but in fresh memory this time */
static void insert_node(hd_bucket *b, hd_hsls *l)
{
	if (b->n + 2 > b->allocd) {
		b->allocd += 32;
		b->x = xrealloc(b->x, b->allocd * sizeof(hd_point));
	}
	b->x[b->n].x = l->xl;
	b->x[b->n++].y = l->xr;
}

/* Liu's Criterion for even number decision:
 * 1. If ( Py > Cy && Ny < Cy || Py < Cy && Ny > Cy ) then sort and insert a
 *  node with field values xL and xR of Cy into a proper position(indexed by y)
 *  in the y Nbucket.
 *
 * 2. If ( Py > Cy && Ny > Cy || Py < Cy && Ny < Cy ) then sort and insert a
 *  pair of identical nodes with field values xL and xR of Cy into two proper
 *  consecutive positions in the y Nbucket respectively.
 **/
static void lius_criterion(hd_poly *poly)
{
	int i;
	hd_bucket *b = poly->ppoly;
	hd_bucket *net = poly->net;
	hd_hsls *p, *c, *n;

	for (i = 0; i < b->n; ++i) {
		c = &((hd_hsls *)b->x)[i];
		p = &((hd_hsls *)b->x)[(i == 0) ? (b->n - 1) : (i - 1)];
		n = &((hd_hsls *)b->x)[(i == b->n - 1) ? 0 : (i + 1)];

		if ((p->y>c->y && n->y<c->y) || (p->y<c->y && n->y>c->y))
			insert_node(&net[c->y - poly->ymin], c);
		else if ((p->y>c->y && n->y>c->y) || (p->y<c->y && n->y<c->y)) {
			insert_node(&net[c->y - poly->ymin], c);
			insert_node(&net[c->y - poly->ymin], c);
		}
	}
}

void HD_FillPoly(hd_surface srf, hd_point *points, int n, uint32 col)
{
	hd_poly *poly;
	int i, y;

	if (n < 3) return;
	poly = xcalloc(1, sizeof(hd_poly));
	poly->ymin = points[0].y;
	for (i = n; i--;) {
		poly->ymax = MAX(poly->ymax, points[i].y);
		poly->ymin = MIN(poly->ymin, points[i].y);
	}
	poly->n = (poly->ymax - poly->ymin) + 1;
	poly->net = xcalloc(poly->n + 1, sizeof(hd_bucket));
	poly->ppoly = poly->net++;

	for (i = 0; i < n - 1; ++i)
		record_line(poly->ppoly, &points[i], &points[i + 1]);
	record_line(poly->ppoly, &points[n - 1], &points[0]);

	simplify(poly->ppoly);
	lius_criterion(poly);
	for (y = poly->ymin; y <= poly->ymax; ++y) {
		hd_bucket *p = &poly->net[y - poly->ymin];
		hd_point *s;
		qsort(p->x, p->n, sizeof(int) * 2, cmp_coord);
		for (s = (hd_point * )p->x; p->n > 1; s += 2, p->n -= 2)
			hLine(srf, s->x, (s + 1)->y, y, col);
		safe_free(p->x);
	}
	safe_free(poly->ppoly->x);
	safe_free(poly->ppoly);
	safe_free(poly);
}

void HD_FillRect(hd_surface srf, int x1, int y1, int x2, int y2, uint32 col)
{
	int xi;
	uint32 *p, alpha;

	if (x1 == x2 || y1 == y2) return;
	if (x1 > x2) SWAP(x1, x2);
	if (y1 > y2) SWAP(y1, y2);

	y2 = MIN((int)HD_SRF_HEIGHT(srf), y2);
	x2 = MIN((int)HD_SRF_WIDTH(srf), x2);

	x1 = MAX(0, x1);
	y1 = MAX(0, y1);

	if (x2 <= x1 || y2 <= y1) return;

	if (col == HD_CLEAR) {
		alpha = 0xff;
		col = 0x00000000;
	}
	else alpha = (col & 0xff000000) >> 24;

	if (alpha == 0) return;
	else if (alpha == 0xff) {
		for (; y1 < y2; ++y1) {
			p = HD_SRF_ROWF(srf, y1);
			for (xi = x1; xi < x2; ++xi)
				*(p + xi) = col;
		}
	}
	else for (; y1 < y2; ++y1) {
		p = HD_SRF_ROWF(srf, y1);
		for (xi = x1; xi < x2; ++xi)
			BLEND_ARGB8888_ON_ARGB8888(*(p + xi), col, 0xff)
	}
}

void HD_Rect(hd_surface srf, int x1, int y1, int x2, int y2, uint32 col)
{
	char t, b, l, r;
	int xi;
	uint32 *s, *e;

	if (x1 == x2 || y1 == y2) return;
	if (x1 > x2) SWAP(x1, x2);
	if (y1 > y2) SWAP(y1, y2);

	t = b = l = r = 1;

	if (y1 < 0) {
		t = 0;
		y1 = 0;
	}
	if (y2 > (int)HD_SRF_HEIGHT(srf)) {
		b = 0;
		y2 = HD_SRF_HEIGHT(srf);
	}
	if (x1 < 0) {
		l = 0;
		x1 = 0;
	}
	if (x2 > (int)HD_SRF_WIDTH(srf)) {
		r = 0;
		x2 = HD_SRF_WIDTH(srf);
	}
	if (x2 <= x1 || y2 <= y1) return;

	s = HD_SRF_ROWF(srf, y1);
	e = HD_SRF_ROWF(srf, y2 - 1);
	if (l && r) {
		for (xi = x1 + 1; xi < x2 - 1; ++xi) {
			BLEND_ARGB8888_ON_ARGB8888(*(s + xi), col, 0xff)
			BLEND_ARGB8888_ON_ARGB8888(*(e + xi), col, 0xff)
		}
	} else if (l || r) {
		s = (l) ? s : e;
		for (xi = x1 + 1; xi < x2 - 1; ++xi)
			BLEND_ARGB8888_ON_ARGB8888(*(s + xi), col, 0xff)
	}
	if (t && b) {
		for (; y1 < y2; ++y1) {
			s = HD_SRF_ROWF(srf, y1);
			BLEND_ARGB8888_ON_ARGB8888(*(s + x1), col, 0xff)
			BLEND_ARGB8888_ON_ARGB8888(*(s + x2 - 1), col, 0xff)
		}
	} else if (t || b) {
		xi = (t) ? x1 : x2 - 1;
		for (; y1 < y2; ++y1) {
			s = HD_SRF_ROWF(srf, y1);
			BLEND_ARGB8888_ON_ARGB8888(*(s + xi), col, 0xff)
		}
	}
}

void HD_Circle(hd_surface srf, int x, int y, int r, uint32 col)
{
	int16 cx = 0;
	int16 cy = r;
	int16 ocx = SHRT_MAX;
	int16 ocy = SHRT_MAX;
	int16 df = 1 - r;
	int16 d_e = 3;
	int16 d_se = -2 * r + 5;
	int16 xpcx, xpcy, xmcx, xmcy;
	int16 ypcx, ypcy, ymcx, ymcy;

	if (r < 0) return;
	if (r == 0) {
		HD_Pixel(srf, x, y, col);
		return;
	}

	do {
		if (ocy != cy || ocx != cx) {
			xpcx = x + cx;
			xmcx = x - cx;
			if (cy > 0) {
				ypcy = y + cy;
				ymcy = y - cy;
				HD_Pixel(srf, xmcx, ypcy, col);
				HD_Pixel(srf, xpcx, ypcy, col);
				HD_Pixel(srf, xmcx, ymcy, col);
				HD_Pixel(srf, xpcx, ymcy, col);
			}
			else {
				HD_Pixel(srf, xmcx, y, col);
				HD_Pixel(srf, xpcx, y, col);
			}
			ocy = cy;
			xpcy = x + cy;
			xmcy = x - cy;
			if (cx > 0) {
				ypcx = y + cx;
				ymcx = y - cx;
				HD_Pixel(srf, xmcy, ypcx, col);
				HD_Pixel(srf, xpcy, ypcx, col);
				HD_Pixel(srf, xmcy, ymcx, col);
				HD_Pixel(srf, xpcy, ymcx, col);
			}
			else {
				HD_Pixel(srf, xmcy, y, col);
				HD_Pixel(srf, xpcy, y, col);
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

void HD_FillCircle(hd_surface srf, int x, int y, int r, uint32 col)
{
	int16 cx = 0;
	int16 cy = r;
	int16 ocx = SHRT_MAX;
	int16 ocy = SHRT_MAX;
	int16 df = 1 - r;
	int16 d_e = 3;
	int16 d_se = -2 * r + 5;
	int16 xpcx, xpcy, xmcx, xmcy;

	if (r < 0) return;
	if (r == 0) {
		HD_Pixel(srf, x, y, col);
		return;
	}
	if (x + r < 0 || y + r < 0 ||
			x - r > (int)HD_SRF_WIDTH(srf) ||
			y - r > (int)HD_SRF_HEIGHT(srf))
		return;

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
		else if (cx > ocx) {
			xmcx = x - ocx - 1;
			xpcx = x + ocx + 1;
			hLine(srf, x - cx, xmcx, y - cy, col);
			hLine(srf, x - cx, xmcx, y + cy, col);
			hLine(srf, xpcx, x + cx, y - cy, col);
			hLine(srf, xpcx, x + cx, y + cy, col);
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
					HD_Pixel(srf, xmh, ypk, col);
					HD_Pixel(srf, xph, ypk, col);
					HD_Pixel(srf, xmh, ymk, col);
					HD_Pixel(srf, xph, ymk, col);
				}
				else {
					HD_Pixel(srf, xmh, y, col);
					HD_Pixel(srf, xph, y, col);
				}
				ok = k;
				xpi = x + i;
				xmi = x - i;
				if (j > 0) {
					ypj = y + j;
					ymj = y - j;
					HD_Pixel(srf, xmi, ypj, col);
					HD_Pixel(srf, xpi, ypj, col);
					HD_Pixel(srf, xmi, ymj, col);
					HD_Pixel(srf, xpi, ymj, col);
				}
				else {
					HD_Pixel(srf, xmi, y, col);
					HD_Pixel(srf, xpi, y, col);
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
					HD_Pixel(srf, xmj, ypi, col);
					HD_Pixel(srf, xpj, ypi, col);
					HD_Pixel(srf, xmj, ymi, col);
					HD_Pixel(srf, xpj, ymi, col);
				}
				else {
					HD_Pixel(srf, xmj, y, col);
					HD_Pixel(srf, xpj, y, col);
				}
				oi = i;
				xmk = x - k;
				xpk = x + k;
				if (h > 0) {
					yph = y + h;
					ymh = y - h;
					HD_Pixel(srf, xmk, yph, col);
					HD_Pixel(srf, xpk, yph, col);
					HD_Pixel(srf, xmk, ymh, col);
					HD_Pixel(srf, xpk, ymh, col);
				}
				else {
					HD_Pixel(srf, xmk, y, col);
					HD_Pixel(srf, xpk, y, col);
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
	int32 x, y, xs, ys, dyt, od, xx, yy, xc2, yc2;
	float cp;
	double sab;
	unsigned char weight, iweight;

	if (rx < 0 || ry < 0)
		return;

	if (rx == 0 || ry == 0) {
		HD_Line(srf, xc - rx, yc - ry, xc + ry, yc + ry, col);
		return;
	}

	if (xc + rx < 0 || yc + ry < 0 ||
			xc - rx > (int)HD_SRF_WIDTH(srf) ||
			yc - ry > (int)HD_SRF_HEIGHT(srf))
		return;

	a2 = rx * rx;
	b2 = ry * ry;

	ds = a2 << 1;
	dt = b2 << 1;

	xc2 = xc << 1;
	yc2 = yc << 1;

	sab = sqrt(a2 + b2);
	od = (int)iround(sab*0.01) + 1;
	dxt = (int)iround((double)a2 / sab) + od;

	t = 0;
	s = -2 * a2 * ry;
	d = 0;

	x = xc;
	y = yc - ry;

	HD_Pixel(srf, x, y, col);
	HD_Pixel(srf, xc2 - x, y, col);
	HD_Pixel(srf, x, yc2 - y, col);
	HD_Pixel(srf, xc2 - x, yc2 - y, col);

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

	dyt = (int)iround((double)b2 / sab) + od;

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
	if (x + rx < 0 || y + ry < 0 ||
			x - rx > (int)HD_SRF_WIDTH(srf) ||
			y - ry > (int)HD_SRF_HEIGHT(srf))
		return;

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

void HD_AAFillEllipse(hd_surface srf, int xc, int yc, int rx, int ry, uint32 col)
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
	if (xc + rx < 0 || yc + ry < 0 ||
			xc - rx > (int)HD_SRF_WIDTH(srf) ||
			yc - ry > (int)HD_SRF_HEIGHT(srf))
		return;

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

	HD_Pixel(srf, x, y, col);
	HD_Pixel(srf, xc2 - x, y, col);
	HD_Pixel(srf, x, yc2 - y, col);
	HD_Pixel(srf, xc2 - x, yc2 - y, col);

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
				hLine(srf, x, xx, y, col);
			if (styy != yy)
				hLine(srf, x, xx, yy, col);
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
		hLine(srf, x+1, xx-1, y-1, col);

		/* Right half */
		xx = xc2 - xs;
		BLENDPIX_WEIGHT(srf, xs, y, col, weight);
		BLENDPIX_WEIGHT(srf, xx, y, col, weight);

		BLENDPIX_WEIGHT(srf, xs, yy, col, weight);
		BLENDPIX_WEIGHT(srf, xx, yy, col, weight);
		hLine(srf, x+1, xc2 - x - 1, yy+1, col);
	}
}

static double lpow(double x, signed char p)
{
	double i = x;
	if (p > 0) while (--p > 0) x *= i;
	else if (p-- < 0) while (p++ < 0) x /= i;
	else return 1;
	return x;
}

static long factorial(long d)
{
	if (d > 12) return factorial(d - 1) * d;
	switch (d) {
	case 0:
	case 1: return          1;
	case 2: return          2;
	case 3: return          6;
	case 4: return         24;
	case 5: return        120;
	case 6: return        720;
	case 7: return       5040;
	case 8: return      40320;
	case 9: return     362880;
	case 10: return   3628800;
	case 11: return  39916800;
	case 12: return 479001600;
	}
	return 0; /* never happens */
}

/**
 * 2D bezier function:
 * u is the parametric value on interval [0,1], where u is 0 
 * 	at the first point in 'points', and u is 1 at the last. 
 * retval is the bezier value at u.
 * points is the vector of points
 **/
static void b2dim(float u,  int order, hd_point *points, hd_point *retval)
{
	uint32 ui;
	float mult;
	retval->x = 0;
	retval->y = 0;
	for (ui = 0; (int)ui <= order; ++ui) {
		mult = factorial(order) * lpow(u, ui) * lpow(1-u, order-ui) /
			(factorial(ui) * factorial(order - ui));
		retval->x += mult * points[ui].x;
		retval->y += mult * points[ui].y;
	}
}

#define DO_HD_BEZIER(function)			\
  float u, fi;					\
  hd_point *hs, *p;				\
  int i = (order + 1) * resolution;		\
  u = (float)(1.0f / (float)i);			\
  p = hs = xmalloc(++i * sizeof(hd_point));	\
						\
  for (fi = 0.0f; p - hs < i; fi += u)		\
    b2dim(fi, order, points, p++);		\
  function(srf, hs, p - hs, col);		\
  free(hs);

void HD_Bezier(hd_surface srf, int order, hd_point *points,
		int resolution, uint32 col)
{ DO_HD_BEZIER(HD_Lines) }
void HD_AABezier(hd_surface srf, int order, hd_point *points,
		int resolution, uint32 col)
{ DO_HD_BEZIER(HD_AALines) }

void HD_Chrome(hd_surface srf, int x, int y, int w, int h)
{
	int lx, ly, a, c;
	uint32 col;

	if (w <= 0 || h <= 0)
		return;
	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}

	if (x + w > (int)HD_SRF_WIDTH(srf))
		w = HD_SRF_WIDTH(srf) - x;
	if (y + h > (int)HD_SRF_HEIGHT(srf))
		h = HD_SRF_HEIGHT(srf) - y;

	for (ly = 0; ly < h; ++ly) {
		for (lx = 0; lx < w; ++lx) {
			col = HD_SRF_PIXF(srf, x + lx, y + ly);
			a = ((col & 0xff000000) >> 24);
			c = (((col & 0xff0000) >> 16) +
			     ((col & 0x00ff00) >>  8) +
			      (col & 0x0000ff)         ) / 3;
			HD_SRF_PIXF(srf, x + lx, y + ly) = HD_RGBA(c, c, c, a);
		}
	}
}

void HD_Blur(hd_surface srf, int x, int y, int w, int h, int rad)
{
	int kx, ky;
	int lx, ly;
	int asum, rsum, gsum, bsum, amt;
	uint32 col;
	int tx, ty;
	int diam = (rad << 1) + 1;

	if (rad <= 0 || w <= 0 || h <= 0)
		return;
	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}

	for (ly = 0; ly < h; ++ly) {
		for (lx = 0; lx < w; ++lx) {
			amt = asum = rsum = gsum = bsum = 0;
			for (ky = 0; ky < diam; ++ky) {
				for (kx = 0; kx < diam; ++kx) {
					tx = x + lx + kx - rad;
					ty = y + ly + ky - rad;
					if (tx < 0 || ty < 0 ||
						tx >= (int)HD_SRF_WIDTH(srf) ||
						ty >= (int)HD_SRF_HEIGHT(srf))
						continue;
					col = HD_SRF_PIXF(srf, tx, ty);
					asum += (col & 0xff000000) >> 24;
					rsum += (col & 0x00ff0000) >> 16;
					gsum += (col & 0x0000ff00) >> 8;
					bsum += (col & 0x000000ff);
					++amt;
				}
			}
			HD_SRF_PIXF(srf, x + lx, y + ly) =
					HD_RGBA(rsum/amt, gsum/amt, bsum/amt, asum/amt);
		}
	}
}

