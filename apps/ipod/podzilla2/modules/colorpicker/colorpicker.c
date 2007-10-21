/*
 * Copyright (C) 2006 Rebecca G. Bettencourt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "pz.h"
#include <math.h>

static const int colorpicker_count = 7;
static const char * colorpicker_name[] = { "RGB", "CMY", "HSV", "3\xB3", "4\xB3", "5\xB3", "6\xB3" };

static int colorpicker_picker = 0;
static int colorpicker_sel = 0;
static ttk_color colorpicker_old_color;
static ttk_color colorpicker_new_color;
static int colorpicker_r, colorpicker_g, colorpicker_b;
static int colorpicker_h, colorpicker_s, colorpicker_v;
static int colorpicker_cancel = 0;

static unsigned short colorpicker_icon_down[] = { 0xF800, 0x7000, 0x2000 };
static unsigned short colorpicker_icon_right[] = { 0x4000, 0xE000, 0x7000, 0x3800, 0x7000, 0xE000, 0x4000 };

#define PICKER_BG 0
#define PICKER_FG 1
#define PICKER_SELBG 2
#define PICKER_SELFG 3
#define PICKER_LINE 4
#define PICKER_ICON 5
#define PICKER_BOX 6

#ifndef MIN
#define MIN(a,b) ( ((a)<(b))?(a):(b) )
#endif
#ifndef MAX
#define MAX(a,b) ( ((a)>(b))?(a):(b) )
#endif

/* OTHER COLOR MODELS FOR TTK */

ttk_color ttk_makecmy(int c, int m, int y)
{
	return ttk_makecol(255-c,255-m,255-y);
}

void ttk_unmakecmy(ttk_color col, int * c, int * m, int * y)
{
	int r, g, b;
	ttk_unmakecol(col, &r, &g, &b);
	(*c) = 255-r;
	(*m) = 255-g;
	(*y) = 255-b;
}

ttk_color ttk_makehsv(int h, int s, int v)
{
	int i, f, p, q, t, r, g, b;
	if (s == 0) {
		r = g = b = (v*255)/100;
	} else {
		i = h/60;
		f = h%60;
		p = (v*(100-s)*255)/10000;
		q = (v*(6000-s*f)*255)/600000;
		t = (v*(6000-s*60+s*f)*255)/600000;
		v = (v*255)/100;
		switch (i%6) {
		case 0: r = v; g = t; b = p; break;
		case 1: r = q; g = v; b = p; break;
		case 2: r = p; g = v; b = t; break;
		case 3: r = p; g = q; b = v; break;
		case 4: r = t; g = p; b = v; break;
		case 5: r = v; g = p; b = q; break;
		default: r = g = b = 0; break; /* silence a 'might be used uninitialized' warning */
		}
	}
	return ttk_makecol(r,g,b);
}

void ttk_unmakehsv(ttk_color col, int * h, int * s, int * v)
{
	int r, g, b, min, max, delta;
	ttk_unmakecol(col, &r, &g, &b);
	min = MIN(MIN(r,g),b);
	max = MAX(MAX(r,g),b);
	*v = (max*100)/255;
	delta = max-min;
	if (max == 0) {
		*s = 0;
		*h = 0;
	} else {
		*s = (delta * 100) / max;
		if (r == max) {
			*h = ((g-b) * 60)/delta;
		} else if (g == max) {
			*h = 120 + ((b-r) * 60)/delta;
		} else {
			*h = 240 + ((r-g) * 60)/delta;
		}
		if (*h < 0) *h += 360;
	}
}

/* COLOR MODELS EXCLUSIVELY FOR COLORPICKER */

static void colorpicker_unmakettk(ttk_color * t)
{
	if (t) (*t) = ttk_makecol(colorpicker_r, colorpicker_g, colorpicker_b);
}

static void colorpicker_makettk(ttk_color t)
{
	colorpicker_new_color = t;
	ttk_unmakecol(t, &colorpicker_r, &colorpicker_g, &colorpicker_b);
	ttk_unmakehsv(t, &colorpicker_h, &colorpicker_s, &colorpicker_v);
}

static void colorpicker_unmakergb(int * r, int * g, int * b)
{
	if (r) (*r) = colorpicker_r;
	if (g) (*g) = colorpicker_g;
	if (b) (*b) = colorpicker_b;
}

static void colorpicker_makergb(int r, int g, int b)
{
	colorpicker_new_color = ttk_makecol(r,g,b);
	colorpicker_r = r;
	colorpicker_g = g;
	colorpicker_b = b;
	ttk_unmakehsv(colorpicker_new_color, &colorpicker_h, &colorpicker_s, &colorpicker_v);
}

static void colorpicker_unmakecmy(int * c, int * m, int * y)
{
	if (c) (*c) = 255-colorpicker_r;
	if (m) (*m) = 255-colorpicker_g;
	if (y) (*y) = 255-colorpicker_b;
}

static void colorpicker_makecmy(int c, int m, int y)
{
	colorpicker_new_color = ttk_makecol(255-c,255-m,255-y);
	colorpicker_r = 255-c;
	colorpicker_g = 255-m;
	colorpicker_b = 255-y;
	ttk_unmakehsv(colorpicker_new_color, &colorpicker_h, &colorpicker_s, &colorpicker_v);
}

static void colorpicker_unmakehsv(int * h, int * s, int * v)
{
	if (h) (*h) = colorpicker_h;
	if (s) (*s) = colorpicker_s;
	if (v) (*v) = colorpicker_v;
}

static void colorpicker_makehsv(int h, int s, int v)
{
	colorpicker_new_color = ttk_makehsv(h,s,v);
	colorpicker_h = h;
	colorpicker_s = s;
	colorpicker_v = v;
	ttk_unmakecol(colorpicker_new_color, &colorpicker_r, &colorpicker_g, &colorpicker_b);
}

/* COLORPICKER WIDGET */

static TApItem * colorpicker_ap_getx(int x)
{
	TApItem * ap;
	switch (x) {
	case PICKER_BG:
		if (( ap = ttk_ap_get("picker.bg") )) return ap;
		else return ttk_ap_getx("window.bg");
		break;
	case PICKER_FG:
		if (( ap = ttk_ap_get("picker.fg") )) return ap;
		else return ttk_ap_getx("window.fg");
		break;
	case PICKER_SELBG:
		if (( ap = ttk_ap_get("picker.selbg") )) return ap;
		else return ttk_ap_getx("menu.selbg");
		break;
	case PICKER_SELFG:
		if (( ap = ttk_ap_get("picker.selfg") )) return ap;
		else return ttk_ap_getx("menu.selfg");
		break;
	case PICKER_LINE:
		if (( ap = ttk_ap_get("picker.line") )) return ap;
		else return ttk_ap_getx("window.fg");
		break;
	case PICKER_ICON:
		if (( ap = ttk_ap_get("picker.icon") )) return ap;
		else return ttk_ap_getx("window.fg");
		break;
	case PICKER_BOX:
		if (( ap = ttk_ap_get("picker.box") )) return ap;
		else return ttk_ap_getx("window.fg");
		break;
	}
	return ttk_ap_getx("window.bg");
}

static void ttk_huegradient(
	ttk_surface srf, int x1, int y1, int x2, int y2,
	int h1, int s1, int v1, int h2, int s2, int v2
) {
	/* You can't trust ttk_unmakehsv to give you back what was passed to ttk_makehsv! */
	int x, h, s, v;
	for (x=x1; x<x2; x++) {
		h = h1 + (h2-h1)*(x-x1)/(x2-1-x1);
		s = s1 + (s2-s1)*(x-x1)/(x2-1-x1);
		v = v1 + (v2-v1)*(x-x1)/(x2-1-x1);
		ttk_line(srf, x, y1, x, y2-1, ttk_makehsv(h,s,v));
	}
}

void colorpicker_draw(TWidget * wid, ttk_surface srf)
{
	int th = ttk_text_height(ttk_textfont);
	int tw;
	int tx = wid->x;
	int ty = wid->y;
	ttk_color tc;
	int r, g, b;
	int ry, gy, bx, by;
	int cw, ch, cx, cy, cm;
	int i, x, y, z;
	char buf[5];
	
	ttk_ap_fillrect(srf, colorpicker_ap_getx(PICKER_BG), wid->x, wid->y, wid->x+wid->w, wid->y+wid->h);
	
	for (i=0; i<colorpicker_count; i++) {
		tw = ttk_text_width_lat1(ttk_textfont, colorpicker_name[i]);
		if (i == colorpicker_picker) {
			ttk_ap_fillrect(srf, colorpicker_ap_getx(PICKER_SELBG), tx, ty, tx+tw+4, ty+th+2);
			tc = colorpicker_ap_getx(PICKER_SELFG)->color;
		} else {
			tc = colorpicker_ap_getx(PICKER_FG)->color;
		}
		ttk_text_lat1(srf, ttk_textfont, tx+2, ty+1, tc, colorpicker_name[i]);
		tx += tw + 4;
	}
	ty += th+2;
	ttk_ap_hline(srf, colorpicker_ap_getx(PICKER_LINE), wid->x, wid->x+wid->w, ty);
	tx = wid->x;
	ty += 3;
	tw = wid->w;
	th = wid->h - (ty-wid->y);
	ttk_fillrect(srf, tx+tw-20, ty, tx+tw, ty+12, colorpicker_old_color);
	ttk_fillrect(srf, tx+tw-20, ty+14, tx+tw, ty+26, colorpicker_new_color);
	tw -= 24;
	
	switch (colorpicker_picker) {
	case 0: /* rgb */
		colorpicker_unmakergb(&r, &g, &b);
		ry = ty + th/4;
		gy = ty + th/2;
		by = ty + th*3/4;
		
		tw -= ttk_text_width(ttk_textfont, "257");
		snprintf(buf, 5, "%d", r);
		ttk_text(srf, ttk_textfont, tx+tw, ry-ttk_text_height(ttk_textfont)/2, colorpicker_ap_getx(PICKER_FG)->color, buf);
		snprintf(buf, 5, "%d", g);
		ttk_text(srf, ttk_textfont, tx+tw, gy-ttk_text_height(ttk_textfont)/2, colorpicker_ap_getx(PICKER_FG)->color, buf);
		snprintf(buf, 5, "%d", b);
		ttk_text(srf, ttk_textfont, tx+tw, by-ttk_text_height(ttk_textfont)/2, colorpicker_ap_getx(PICKER_FG)->color, buf);
		tw -= 4;
		
		ttk_hgradient(srf, tx+6, ry-3, tx+tw-1, ry+4, ttk_makecol(0, g, b), ttk_makecol(255, g, b));
		ttk_rect(srf, tx+5, ry-4, tx+tw, ry+5, colorpicker_ap_getx(PICKER_BOX)->color);
		ttk_bitmap(srf, tx+5+((tw-6)*r/255)-2, ry-7, 5, 3, colorpicker_icon_down, colorpicker_ap_getx(PICKER_ICON)->color);
		
		ttk_hgradient(srf, tx+6, gy-3, tx+tw-1, gy+4, ttk_makecol(r, 0, b), ttk_makecol(r, 255, b));
		ttk_rect(srf, tx+5, gy-4, tx+tw, gy+5, colorpicker_ap_getx(PICKER_BOX)->color);
		ttk_bitmap(srf, tx+5+((tw-6)*g/255)-2, gy-7, 5, 3, colorpicker_icon_down, colorpicker_ap_getx(PICKER_ICON)->color);
		
		ttk_hgradient(srf, tx+6, by-3, tx+tw-1, by+4, ttk_makecol(r, g, 0), ttk_makecol(r, g, 255));
		ttk_rect(srf, tx+5, by-4, tx+tw, by+5, colorpicker_ap_getx(PICKER_BOX)->color);
		ttk_bitmap(srf, tx+5+((tw-6)*b/255)-2, by-7, 5, 3, colorpicker_icon_down, colorpicker_ap_getx(PICKER_ICON)->color);
		
		ttk_bitmap(srf, tx, ty + (th*(colorpicker_sel+1)/4) - 3, 5, 7, colorpicker_icon_right, colorpicker_ap_getx(PICKER_ICON)->color);
		break;
	case 1: /* cmy */
		colorpicker_unmakecmy(&r, &g, &b);
		ry = ty + th/4;
		gy = ty + th/2;
		by = ty + th*3/4;
		
		tw -= ttk_text_width(ttk_textfont, "257");
		snprintf(buf, 5, "%d", r);
		ttk_text(srf, ttk_textfont, tx+tw, ry-ttk_text_height(ttk_textfont)/2, colorpicker_ap_getx(PICKER_FG)->color, buf);
		snprintf(buf, 5, "%d", g);
		ttk_text(srf, ttk_textfont, tx+tw, gy-ttk_text_height(ttk_textfont)/2, colorpicker_ap_getx(PICKER_FG)->color, buf);
		snprintf(buf, 5, "%d", b);
		ttk_text(srf, ttk_textfont, tx+tw, by-ttk_text_height(ttk_textfont)/2, colorpicker_ap_getx(PICKER_FG)->color, buf);
		tw -= 4;
		
		ttk_hgradient(srf, tx+6, ry-3, tx+tw-1, ry+4, ttk_makecmy(0, g, b), ttk_makecmy(255, g, b));
		ttk_rect(srf, tx+5, ry-4, tx+tw, ry+5, colorpicker_ap_getx(PICKER_BOX)->color);
		ttk_bitmap(srf, tx+5+((tw-6)*r/255)-2, ry-7, 5, 3, colorpicker_icon_down, colorpicker_ap_getx(PICKER_ICON)->color);
		
		ttk_hgradient(srf, tx+6, gy-3, tx+tw-1, gy+4, ttk_makecmy(r, 0, b), ttk_makecmy(r, 255, b));
		ttk_rect(srf, tx+5, gy-4, tx+tw, gy+5, colorpicker_ap_getx(PICKER_BOX)->color);
		ttk_bitmap(srf, tx+5+((tw-6)*g/255)-2, gy-7, 5, 3, colorpicker_icon_down, colorpicker_ap_getx(PICKER_ICON)->color);
		
		ttk_hgradient(srf, tx+6, by-3, tx+tw-1, by+4, ttk_makecmy(r, g, 0), ttk_makecmy(r, g, 255));
		ttk_rect(srf, tx+5, by-4, tx+tw, by+5, colorpicker_ap_getx(PICKER_BOX)->color);
		ttk_bitmap(srf, tx+5+((tw-6)*b/255)-2, by-7, 5, 3, colorpicker_icon_down, colorpicker_ap_getx(PICKER_ICON)->color);
		
		ttk_bitmap(srf, tx, ty + (th*(colorpicker_sel+1)/4) - 3, 5, 7, colorpicker_icon_right, colorpicker_ap_getx(PICKER_ICON)->color);
		break;
	case 2: /* hsv */
		colorpicker_unmakehsv(&r, &g, &b);
		ry = ty + th/4;
		gy = ty + th/2;
		by = ty + th*3/4;
		
		tw -= ttk_text_width(ttk_textfont, "257");
		snprintf(buf, 5, "%d", r);
		ttk_text(srf, ttk_textfont, tx+tw, ry-ttk_text_height(ttk_textfont)/2, colorpicker_ap_getx(PICKER_FG)->color, buf);
		snprintf(buf, 5, "%d", g);
		ttk_text(srf, ttk_textfont, tx+tw, gy-ttk_text_height(ttk_textfont)/2, colorpicker_ap_getx(PICKER_FG)->color, buf);
		snprintf(buf, 5, "%d", b);
		ttk_text(srf, ttk_textfont, tx+tw, by-ttk_text_height(ttk_textfont)/2, colorpicker_ap_getx(PICKER_FG)->color, buf);
		tw -= 4;
		
		ttk_huegradient(srf, tx+6, ry-3, tx+tw-1, ry+4, 0, g, b, 359, g, b);
		ttk_rect(srf, tx+5, ry-4, tx+tw, ry+5, colorpicker_ap_getx(PICKER_BOX)->color);
		ttk_bitmap(srf, tx+5+((tw-6)*r/360)-2, ry-7, 5, 3, colorpicker_icon_down, colorpicker_ap_getx(PICKER_ICON)->color);
		
		ttk_hgradient(srf, tx+6, gy-3, tx+tw-1, gy+4, ttk_makehsv(r, 0, b), ttk_makehsv(r, 100, b));
		ttk_rect(srf, tx+5, gy-4, tx+tw, gy+5, colorpicker_ap_getx(PICKER_BOX)->color);
		ttk_bitmap(srf, tx+5+((tw-6)*g/100)-2, gy-7, 5, 3, colorpicker_icon_down, colorpicker_ap_getx(PICKER_ICON)->color);
		
		ttk_hgradient(srf, tx+6, by-3, tx+tw-1, by+4, ttk_makehsv(r, g, 0), ttk_makehsv(r, g, 100));
		ttk_rect(srf, tx+5, by-4, tx+tw, by+5, colorpicker_ap_getx(PICKER_BOX)->color);
		ttk_bitmap(srf, tx+5+((tw-6)*b/100)-2, by-7, 5, 3, colorpicker_icon_down, colorpicker_ap_getx(PICKER_ICON)->color);
		
		ttk_bitmap(srf, tx, ty + (th*(colorpicker_sel+1)/4) - 3, 5, 7, colorpicker_icon_right, colorpicker_ap_getx(PICKER_ICON)->color);
		break;
	case 3: /* 3x3 */
		cw = ch = MIN((tw-1)/9,(th-1)/3);
		bx = tx + (tw - cw*9)/2;
		by = ty + (th - ch*3)/2;
		cm = 128;
		for (i=0; i<27; i++) {
			x = i % 3;
			y = (i % 9) / 3;
			z = i / 9;
			cx = bx + (z*3 + x)*cw;
			cy = by + (y)*cw;
			x *= 128;
			y *= 128;
			z *= 128;
			if (x>255) x=255;
			if (y>255) y=255;
			if (z>255) z=255;
			ttk_fillrect(srf, cx+1, cy+1, cx+cw, cy+ch, ttk_makecol(x,y,z));
			if (i == colorpicker_sel) {
				ttk_rect(srf, cx, cy, cx+cw+1, cy+ch+1, colorpicker_ap_getx(PICKER_BOX)->color);
			}
		}
		break;
	case 4: /* 4x4 */
		cw = ch = MIN((tw-1)/8,(th-1)/8);
		bx = tx + (tw - cw*8)/2;
		by = ty + (th - ch*8)/2;
		cm = 85;
		for (i=0; i<64; i++) {
			x = i % 4;
			y = (i % 16) / 4;
			z = i / 16;
			cx = bx + ((z%2)*4 + x)*cw;
			cy = by + ((z/2)*4 + y)*cw;
			x *= 85;
			y *= 85;
			z *= 85;
			if (x>255) x=255;
			if (y>255) y=255;
			if (z>255) z=255;
			ttk_fillrect(srf, cx+1, cy+1, cx+cw, cy+ch, ttk_makecol(x,y,z));
			if (i == colorpicker_sel) {
				ttk_rect(srf, cx, cy, cx+cw+1, cy+ch+1, colorpicker_ap_getx(PICKER_BOX)->color);
			}
		}
		break;
	case 5: /* 5x5 */
		cw = ch = MIN((tw-1)/15,(th-1)/10);
		bx = tx + (tw - cw*15)/2;
		by = ty + (th - ch*10)/2;
		cm = 64;
		for (i=0; i<125; i++) {
			x = i % 5;
			y = (i % 25) / 5;
			z = i / 25;
			cx = bx + ((z%3)*5 + x)*cw;
			cy = by + ((z/3)*5 + y)*cw;
			x *= 64;
			y *= 64;
			z *= 64;
			if (x>255) x=255;
			if (y>255) y=255;
			if (z>255) z=255;
			ttk_fillrect(srf, cx+1, cy+1, cx+cw, cy+ch, ttk_makecol(x,y,z));
			if (i == colorpicker_sel) {
				ttk_rect(srf, cx, cy, cx+cw+1, cy+ch+1, colorpicker_ap_getx(PICKER_BOX)->color);
			}
		}
		break;
	case 6: /* 6x6 */
		cw = ch = MIN((tw-1)/18,(th-1)/12);
		bx = tx + (tw - cw*18)/2;
		by = ty + (th - ch*12)/2;
		cm = 51;
		for (i=0; i<216; i++) {
			x = i % 6;
			y = (i % 36) / 6;
			z = i / 36;
			cx = bx + ((z%3)*6 + x)*cw;
			cy = by + ((z/3)*6 + y)*cw;
			x *= 51;
			y *= 51;
			z *= 51;
			if (x>255) x=255;
			if (y>255) y=255;
			if (z>255) z=255;
			ttk_fillrect(srf, cx+1, cy+1, cx+cw, cy+ch, ttk_makecol(x,y,z));
			if (i == colorpicker_sel) {
				ttk_rect(srf, cx, cy, cx+cw+1, cy+ch+1, colorpicker_ap_getx(PICKER_BOX)->color);
			}
		}
		break;
	}
}

int colorpicker_scroll(TWidget * wid, int dir)
{
	int r, g, b;
	TTK_SCROLLMOD(dir, 4);
	
	switch (colorpicker_picker) {
	case 0: /* rgb */
		colorpicker_unmakergb(&r, &g, &b);
		switch (colorpicker_sel) {
		case 0: r += dir; if (r<0) r=0; if (r>255) r=255; break;
		case 1: g += dir; if (g<0) g=0; if (g>255) g=255; break;
		case 2: b += dir; if (b<0) b=0; if (b>255) b=255; break;
		}
		colorpicker_makergb(r,g,b);
		wid->dirty = 1;
		break;
	case 1: /* cmy */
		colorpicker_unmakecmy(&r, &g, &b);
		switch (colorpicker_sel) {
		case 0: r += dir; if (r<0) r=0; if (r>255) r=255; break;
		case 1: g += dir; if (g<0) g=0; if (g>255) g=255; break;
		case 2: b += dir; if (b<0) b=0; if (b>255) b=255; break;
		}
		colorpicker_makecmy(r,g,b);
		wid->dirty = 1;
		break;
	case 2: /* hsv */
		colorpicker_unmakehsv(&r, &g, &b);
		switch (colorpicker_sel) {
		case 0: r += dir; if (r<0) r=0; if (r>359) r=359; break;
		case 1: g += dir; if (g<0) g=0; if (g>100) g=100; break;
		case 2: b += dir; if (b<0) b=0; if (b>100) b=100; break;
		}
		colorpicker_makehsv(r,g,b);
		wid->dirty = 1;
		break;
	case 3: /* 3x3 */
		colorpicker_sel += dir;
		while (colorpicker_sel < 0) colorpicker_sel += 27;
		while (colorpicker_sel >= 27) colorpicker_sel -= 27;
		wid->dirty = 1;
		break;
	case 4: /* 4x4 */
		colorpicker_sel += dir;
		while (colorpicker_sel < 0) colorpicker_sel += 64;
		while (colorpicker_sel >= 64) colorpicker_sel -= 64;
		wid->dirty = 1;
		break;
	case 5: /* 5x5 */
		colorpicker_sel += dir;
		while (colorpicker_sel < 0) colorpicker_sel += 125;
		while (colorpicker_sel >= 125) colorpicker_sel -= 125;
		wid->dirty = 1;
		break;
	case 6: /* 6x6 */
		colorpicker_sel += dir;
		while (colorpicker_sel < 0) colorpicker_sel += 216;
		while (colorpicker_sel >= 216) colorpicker_sel -= 216;
		wid->dirty = 1;
		break;
	}
	return TTK_EV_CLICK;
}

int colorpicker_down(TWidget * wid, int btn)
{
	int i,x,y,z;
	switch (btn) {
	case TTK_BUTTON_MENU:
		colorpicker_cancel = 1;
		colorpicker_new_color = colorpicker_old_color;
		return TTK_EV_DONE;
		break;
	case TTK_BUTTON_PLAY:
		colorpicker_cancel = 0;
		return TTK_EV_DONE;
		break;
	case TTK_BUTTON_PREVIOUS:
		colorpicker_picker--;
		colorpicker_sel = 0;
		if (colorpicker_picker < 0) colorpicker_picker = colorpicker_count-1;
		wid->dirty = 1;
		break;
	case TTK_BUTTON_NEXT:
		colorpicker_picker++;
		colorpicker_sel = 0;
		if (colorpicker_picker >= colorpicker_count) colorpicker_picker = 0;
		wid->dirty = 1;
		break;
	case TTK_BUTTON_ACTION:
		switch (colorpicker_picker) {
		case 0: /* rgb */
		case 1: /* cmy */
		case 2: /* hsv */
			colorpicker_sel++;
			if (colorpicker_sel >= 3) colorpicker_sel = 0;
			wid->dirty = 1;
			break;
		case 3: /* 3x3 */
			i = colorpicker_sel;
			x = i % 3;
			y = (i % 9) / 3;
			z = i / 9;
			x *= 128;
			y *= 128;
			z *= 128;
			if (x>255) x=255;
			if (y>255) y=255;
			if (z>255) z=255;
			colorpicker_makergb(x,y,z);
			wid->dirty = 1;
			break;
		case 4: /* 4x4 */
			i = colorpicker_sel;
			x = i % 4;
			y = (i % 16) / 4;
			z = i / 16;
			x *= 85;
			y *= 85;
			z *= 85;
			if (x>255) x=255;
			if (y>255) y=255;
			if (z>255) z=255;
			colorpicker_makergb(x,y,z);
			wid->dirty = 1;
			break;
		case 5: /* 5x5 */
			i = colorpicker_sel;
			x = i % 5;
			y = (i % 25) / 5;
			z = i / 25;
			x *= 64;
			y *= 64;
			z *= 64;
			if (x>255) x=255;
			if (y>255) y=255;
			if (z>255) z=255;
			colorpicker_makergb(x,y,z);
			wid->dirty = 1;
			break;
		case 6: /* 6x6 */
			i = colorpicker_sel;
			x = i % 6;
			y = (i % 36) / 6;
			z = i / 36;
			x *= 51;
			y *= 51;
			z *= 51;
			if (x>255) x=255;
			if (y>255) y=255;
			if (z>255) z=255;
			colorpicker_makergb(x,y,z);
			wid->dirty = 1;
			break;
		}
		break;
	default:
		return TTK_EV_UNUSED;
		break;
	}
	return TTK_EV_CLICK;
}

int pz_colorpicker(const char * title, ttk_color * color)
{
	TWindow * picker = ttk_new_window();
	TWidget * widget = ttk_new_widget(0,0);
	
	picker->w = ttk_screen->w - 20;
	picker->h = ttk_screen->h - 40;
	picker->x = 10;
	picker->y = 30;
	widget->w = ttk_screen->w - 22;
	widget->h = ttk_screen->h - 42;
	widget->x = 1;
	widget->y = 1;
	
	widget->focusable = 1;
	widget->draw = colorpicker_draw;
	widget->down = colorpicker_down;
	widget->scroll = colorpicker_scroll;
	
	colorpicker_old_color = (*color);
	colorpicker_makettk(*color);
	
	ttk_window_title(picker, title);
	ttk_add_widget(picker, widget);
	ttk_show_window(picker);
	ttk_run();
	ttk_hide_window(picker);
	ttk_free_window(picker);
	if (ttk_windows) {
		ttk_draw_window(ttk_windows->w);
	} else {
		ttk_fillrect(ttk_screen->srf, ttk_screen->wx, ttk_screen->wy, ttk_screen->w, ttk_screen->h, ttk_makecol(255,255,255));
	}
	ttk_gfx_update(ttk_screen->srf);
	
	if (!colorpicker_cancel) {
		colorpicker_unmakettk(color);
	}
	return (!colorpicker_cancel);
}

/* DEMO */

PzWindow * new_colormixer_window(void)
{
	ttk_color col = ttk_makecol(0,0,0);
	pz_colorpicker("Color Mixer", &col);
	return TTK_MENU_DONOTHING;
}

void colorpicker_mod_init(void)
{
	pz_menu_add_action_group("/Extras/Demos/Color Mixer", "Tech", new_colormixer_window);
}

PZ_MOD_INIT(colorpicker_mod_init)
