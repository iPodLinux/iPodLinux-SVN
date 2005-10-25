/*
 * Copyright (c) 1999, 2000, 2001 Greg Haerr <greg@censoft.com>
 * Portions Copyright (c) 2002 by Koninklijke Philips Electronics N.V.
 * Copyright (c) 2005 Bernard Leach <leachbj@bouncycastle.org> (ipod port)
 *
 * 16bpp iPod photo LCD Linear Video Driver for Microwindows
 *
 * Inspired from Ben Pfaff's BOGL <pfaffben@debian.org>
 */

#include <assert.h>
#include <stdlib.h>

/* We want to do string copying fast, so inline assembly if possible */
#ifndef __OPTIMIZE__
#define __OPTIMIZE__
#endif
#include <string.h>

extern int mw_ipod_hw_ver;
extern int mw_ipod_lcd_type;

#include "device.h"
#include "fb.h"

static void lcd_update_display(PSD psd, int sx, int sy, int mx, int my);

#define inl(p) (*(volatile unsigned long *) (p))
#define outl(v,p) (*(volatile unsigned long *) (p) = (v))

/* Calc linelen and mmap size, return 0 on fail*/
static int
linear16_init(PSD psd)
{
	if (!psd->size) {
		psd->size = psd->yres * psd->linelen;
		/* convert linelen from byte to pixel len for bpp 16, 24, 32*/
		psd->linelen /= 2;
	}
	return 1;
}

/* Set pixel at x, y, to pixelval c*/
static void
linear16_drawpixel(PSD psd, MWCOORD x, MWCOORD y, MWPIXELVAL c)
{
	ADDR16	addr = psd->addr;

	assert (addr != 0);
	assert (x >= 0 && x < psd->xres);
	assert (y >= 0 && y < psd->yres);
	assert (c < psd->ncolors);

	DRAWON;
	if(gr_mode == MWMODE_COPY)
		addr[x + y * psd->linelen] = c;
	else
		applyOp(gr_mode, c, &addr[x + y * psd->linelen], ADDR16);
	lcd_update_display(psd, x, y, x, y);
	DRAWOFF;
}

/* Read pixel at x, y*/
static MWPIXELVAL
linear16_readpixel(PSD psd, MWCOORD x, MWCOORD y)
{
	ADDR16	addr = psd->addr;

	assert (addr != 0);
	assert (x >= 0 && x < psd->xres);
	assert (y >= 0 && y < psd->yres);

	return addr[x + y * psd->linelen];
}

/* Draw horizontal line from x1,y to x2,y including final point*/
static void
linear16_drawhorzline(PSD psd, MWCOORD x1, MWCOORD x2, MWCOORD y, MWPIXELVAL c)
{
	ADDR16	addr = psd->addr;
	int	x1orig = x1;

	assert (addr != 0);
	assert (x1 >= 0 && x1 < psd->xres);
	assert (x2 >= 0 && x2 < psd->xres);
	assert (x2 >= x1);
	assert (y >= 0 && y < psd->yres);
	assert (c < psd->ncolors);

	DRAWON;
	addr += x1 + y * psd->linelen;
	if(gr_mode == MWMODE_COPY) {
		/* FIXME: memsetw(dst, c, x2-x1+1)*/
		while(x1++ <= x2)
			*addr++ = c;
	} else {
		while (x1++ <= x2) {
			applyOp(gr_mode, c, addr, ADDR16);
			++addr;
		}
	}
	lcd_update_display(psd, x1orig, y, x2, y);
	DRAWOFF;
}

/* Draw a vertical line from x,y1 to x,y2 including final point*/
static void
linear16_drawvertline(PSD psd, MWCOORD x, MWCOORD y1, MWCOORD y2, MWPIXELVAL c)
{
	ADDR16	addr = psd->addr;
	int	linelen = psd->linelen;
	int	y1orig = y1;

	assert (addr != 0);
	assert (x >= 0 && x < psd->xres);
	assert (y1 >= 0 && y1 < psd->yres);
	assert (y2 >= 0 && y2 < psd->yres);
	assert (y2 >= y1);
	assert (c < psd->ncolors);

	DRAWON;
	addr += x + y1 * linelen;
	if(gr_mode == MWMODE_COPY) {
		while(y1++ <= y2) {
			*addr = c;
			addr += linelen;
		}
	} else {
		while (y1++ <= y2) {
			applyOp(gr_mode, c, addr, ADDR16);
			addr += linelen;
		}
	}
	lcd_update_display(psd, x, y1orig, x, y2);
	DRAWOFF;
}

/* srccopy bitblt*/
static void
linear16_blit(PSD dstpsd, MWCOORD dstx, MWCOORD dsty, MWCOORD w, MWCOORD h,
	PSD srcpsd, MWCOORD srcx, MWCOORD srcy, long op)
{
	ADDR16	dst = dstpsd->addr;
	ADDR16	src = srcpsd->addr;
	int	i;
	int	dlinelen = dstpsd->linelen;
	int	slinelen = srcpsd->linelen;
	int	horig = h;
#if ALPHABLEND
	unsigned int alpha;
#endif

	assert (dst != 0);
	assert (dstx >= 0 && dstx < dstpsd->xres);
	assert (dsty >= 0 && dsty < dstpsd->yres);
	assert (w > 0);
	assert (h > 0);
	assert (src != 0);
	assert (srcx >= 0 && srcx < srcpsd->xres);
	assert (srcy >= 0 && srcy < srcpsd->yres);
	assert (dstx+w <= dstpsd->xres);
	assert (dsty+h <= dstpsd->yres);
	assert (srcx+w <= srcpsd->xres);
	assert (srcy+h <= srcpsd->yres);

	DRAWON;
	dst += dstx + dsty * dlinelen;
	src += srcx + srcy * slinelen;

#if ALPHABLEND
	if((op & MWROP_EXTENSION) != MWROP_BLENDCONSTANT)
		goto stdblit;
	alpha = op & 0xff;

	if (dstpsd->pixtype == MWPF_TRUECOLOR565) {
		while(--h >= 0) {
			for(i=0; i<w; ++i) {
				unsigned int s = *src++;
				unsigned int d = *dst;
				unsigned int t = d & 0xf800;
				unsigned int m1, m2, m3;
				m1 = (((((s & 0xf800) - t)*alpha)>>8) & 0xf800) + t;
				t = d & 0x07e0;
				m2 = (((((s & 0x07e0) - t)*alpha)>>8) & 0x07e0) + t;
				t = d & 0x001f;
				m3 = (((((s & 0x001f) - t)*alpha)>>8) & 0x001f) + t;
				*dst++ = m1 | m2 | m3;
			}
			dst += dlinelen - w;
			src += slinelen - w;
		}
	} else {
		/* 5/5/5 format*/
		while(--h >= 0) {
			for(i=0; i<w; ++i) {
				unsigned int s = *src++;
				unsigned int d = *dst;
				unsigned int t = d & 0x7c00;
				unsigned int m1, m2, m3;
				m1 = (((((s & 0x7c00) - t)*alpha)>>8) & 0x7c00) + t;
				t = d & 0x03e0;
				m2 = (((((s & 0x03e0) - t)*alpha)>>8) & 0x03e0) + t;
				t = d & 0x001f;
				m3 = (((((s & 0x001f) - t)*alpha)>>8) & 0x001f) + t;
				*dst++ = m1 | m2 | m3;
			}
			dst += dlinelen - w;
			src += slinelen - w;
		}
	}
	lcd_update_display(dstpsd, dstx, dsty, dstx+w, dsty+horig);
	DRAWOFF;
	return;
stdblit:
#endif

	if (op == MWROP_COPY) {
		/* copy from bottom up if dst in src rectangle*/
		/* memmove is used to handle x case*/
		if (srcy < dsty) {
			src += (h-1) * slinelen;
			dst += (h-1) * dlinelen;
			slinelen *= -1;
			dlinelen *= -1;
		}
		while (--h >= 0) {
			/* a _fast_ memcpy is a _must_ in this routine*/
			memmove(dst, src, w<<1);
			dst += dlinelen;
			src += slinelen;
		}
	} else {
		while (--h >= 0) {
			for (i=0; i<w; i++) {
				applyOp(MWROP_TO_MODE(op), *src, dst, ADDR16);
				++src;
				++dst;
			}
			dst += dlinelen - w;
			src += slinelen - w;
		}
	}
	lcd_update_display(dstpsd, dstx, dsty, dstx+w, dsty+horig);
	DRAWOFF;
}

SUBDRIVER fblinear16 = {
	linear16_init,
	linear16_drawpixel,
	linear16_readpixel,
	linear16_drawhorzline,
	linear16_drawvertline,
	gen_fillrect,
	linear16_blit,
};

/* get current usec counter */
static int timer_get_current(void)
{
	return inl(0x60005010);
}

/* check if number of useconds has past */
static int timer_check(int clock_start, int usecs)
{
	unsigned long clock;
	clock = inl(0x60005010);
	
	if ( (clock - clock_start) >= usecs ) {
		return 1;
	} else {
		return 0;
	}
}

/* wait for LCD with timeout */
static void lcd_wait_write(void)
{
	if ((inl(0x70008A0C) & 0x80000000) != 0) {
		int start = timer_get_current();
			
		do {
			if ((inl(0x70008A0C) & 0x80000000) == 0) 
				break;
		} while (timer_check(start, 1000) == 0);
	}
}

static void lcd_send_lo(int v)
{
	lcd_wait_write();
	outl(v | 0x80000000, 0x70008A0C);
}

static void lcd_send_hi(int v)
{
	lcd_wait_write();
	outl(v | 0x81000000, 0x70008A0C);
}

static void lcd_cmd_data(int cmd, int data)
{
	if (mw_ipod_lcd_type == 0) {
		lcd_send_lo(cmd);
		lcd_send_lo(data);
	} else {
		lcd_send_lo(0);
		lcd_send_lo(cmd);
		lcd_send_hi((data >> 8) & 0xff);
		lcd_send_hi(data & 0xff);
	}
}

static void lcd_update_display(PSD psd, int sx, int sy, int mx, int my)
{
	int height = (my - sy) + 1;
	int width = (mx - sx) + 1;
	int rect1, rect2, rect3, rect4;

	ADDR16	addr = psd->addr;
	assert (addr != 0);

	/* only update the ipod if we are writing to the screen */
	if (!(psd->flags & PSF_SCREEN)) return;

	if (width & 1) width++;

	/* calculate the drawing region */
	if (mw_ipod_hw_ver != 0x6) {
		rect1 = sx;
		rect2 = sy;
		rect3 = (sx + width) - 1;
		rect4 = (sy + height) - 1;
	} else {
		rect1 = sy;
		rect2 = (psd->xres - 1) - sx;
		rect3 = (sy + height) - 1;
		rect4 = (rect2 - width) + 1;
	}

	/* setup the drawing region */
	if (mw_ipod_lcd_type == 0) {
		lcd_cmd_data(0x12, rect1 & 0xff);
		lcd_cmd_data(0x13, rect2 & 0xff);
		lcd_cmd_data(0x15, rect3 & 0xff);
		lcd_cmd_data(0x16, rect4 & 0xff);
	} else {
		if (rect3 < rect1) {
			int t;
			t = rect1;
			rect1 = rect3;
			rect3 = t;
		}

		if (rect4 < rect2) {
			int t;
			t = rect2;
			rect2 = rect4;
			rect4 = t;
		}

		/* max horiz << 8 | start horiz */
		lcd_cmd_data(0x44, (rect3 << 8) | rect1);
		/* max vert << 8 | start vert */
		lcd_cmd_data(0x45, (rect4 << 8) | rect2);

		if (mw_ipod_hw_ver == 0x6) {
			rect2 = rect4;
		}

		/* position cursor (set AD0-AD15) */
		/* start vert << 8 | start horiz */
		lcd_cmd_data(0x21, (rect2 << 8) | rect1);

		/* start drawing */
		lcd_send_lo(0x0);
		lcd_send_lo(0x22);
	}

	addr += sx + sy * psd->linelen;

	while (height > 0) {
		int h, x, y, pixels_to_write;

		pixels_to_write = (width * height) * 2;

		/* calculate how much we can do in one go */
		h = height;
		if (pixels_to_write > 64000) {
			h = (64000/2) / width;
			pixels_to_write = (width * h) * 2;
		}

		outl(0x10000080, 0x70008A20);
		outl((pixels_to_write - 1) | 0xC0010000, 0x70008A24);
		outl(0x34000000, 0x70008A20);

		/* for each row */
		for (x = 0; x < h; x++)
		{
			/* for each column */
			for (y = 0; y < width; y += 2) {
				unsigned two_pixels;

				two_pixels = addr[0] | (addr[1] << 16);
				addr += 2;

				while ((inl(0x70008A20) & 0x1000000) == 0);

				if (mw_ipod_lcd_type != 0) {
					unsigned t = (two_pixels & ~0xFF0000) >> 8;
					two_pixels = two_pixels & ~0xFF00;
					two_pixels = t | (two_pixels << 8);
				}


				/* output 2 pixels */
				outl(two_pixels, 0x70008B00);
			}

			addr += psd->xres - width;
		}

		while ((inl(0x70008A20) & 0x4000000) == 0);

		outl(0x0, 0x70008A24);

		height = height - h;
	}
}

