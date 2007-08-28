/*
 * Copyright (c) 2005 Joshua Oreman
 *
 * This file is a part of TTK.
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

#include "ttk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern ttk_screeninfo *ttk_screen;
extern TWindowStack *ttk_windows;

// Imperfection: this opens an extra window.
int t_GrOpen() 
{
    if (ttk_init())
	return 1;
    return -1;
}

void t_GrClose()
{
    ttk_quit();
}

void t_GrGetScreenInfo (t_GR_SCREEN_INFO *inf) 
{
    inf->rows = ttk_screen->h;
    inf->cols = ttk_screen->w;
    inf->planes = ttk_screen->bpp / 8;
    inf->bpp = ttk_screen->bpp;
    inf->ncolors = 1 << ttk_screen->bpp;
}

// Imperfection: Amalgamated scroll treated as scroll of 1; scroll gives KEYDOWN but not KEYUP.
int t_GrPeekEvent (t_GR_EVENT *ev) 
{
    int earg, e;

    if ((e = ttk_get_event (&earg)) != TTK_NO_EVENT) {
	switch (e) {
	case TTK_BUTTON_DOWN:
	    ev->type = GR_EVENT_TYPE_KEY_DOWN;
	    ev->keystroke.ch = earg;
	    break;
	case TTK_BUTTON_UP:
	    ev->type = GR_EVENT_TYPE_KEY_UP;
	    ev->keystroke.ch = earg;
	    break;
	case TTK_SCROLL:
	    ev->type = GR_EVENT_TYPE_KEY_DOWN;
	    if (earg > 0) {
		ev->keystroke.ch = 'r';
	    } else {
		ev->keystroke.ch = 'l';
	    }
	    break;
	default:
	    return 0;
	}
	return 1;
    }
    return 0;
}

int t_GrGetNextEventTimeout (t_GR_EVENT *ev, int timeout) 
{
    int start = ttk_getticks();
    int end = start + timeout;
    
    while (!t_GrPeekEvent (ev) && (ttk_getticks() < end))
	;
    if (ttk_getticks() < end)
	return 0;
    return 1;
}

t_GR_WINDOW_ID t_GrNewWindow (int _u1, int x, int y, int w, int h, int _u2, int _u3, int _u4) 
{
    t_GR_WINDOW_ID ret = ttk_new_window();
    ret->x = x - ttk_screen->wx;
    ret->y = y - ttk_screen->wy;
    ret->w = w;
    ret->h = h;
    
    if (ret->y < 0) {
	ret->y += ttk_screen->wy;
	ret->show_header = 0;
    }

    return ret;
}

t_GR_WINDOW_ID t_GrNewWindowEx (int _u1, const char *title, int _u3, int x, int y, int w, int h, int _u4) 
{
    t_GR_WINDOW_ID ret = t_GrNewWindow (0, x, y, w, h, 0, 0, 0);
    ttk_window_title (ret, title);
    return ret;
}

void t_GrDestroyWindow (t_GR_WINDOW_ID w) 
{
    ttk_free_window (w);
}

void t_GrMapWindow (t_GR_WINDOW_ID w) 
{
    ttk_show_window (w);
}

void t_GrUnmapWindow (t_GR_WINDOW_ID w) 
{
    ttk_hide_window (w);
}

void t_GrResizeWindow (t_GR_WINDOW_ID win, int w, int h) 
{
    win->w = w;
    win->h = h;
    if (win->w > (ttk_screen->w - ttk_screen->wy)) {
	win->show_header = 0;
    }
}

void t_GrMoveWindow (t_GR_WINDOW_ID win, int x, int y) 
{
    win->x = x;
    win->y = y;
    if (win->y == 20 || win->y == 21 || win->y == 19)
	win->y = ttk_screen->wy;
    if (win->y >= ttk_screen->wy)
	win->show_header = 1;
    else
	win->show_header = 0;
    ttk_dirty |= TTK_FILTHY;
}

void t_GrClearWindow (t_GR_WINDOW_ID w, int _u) 
{
    ttk_fillrect (w->srf, 0, 0, w->w, w->h, ttk_makecol (255, 255, 255));
}

void t_GrGetWindowInfo (t_GR_WINDOW_ID w, t_GR_WINDOW_INFO *inf) 
{
    inf->x = w->x;
    inf->y = w->y;
    inf->width = w->w;
    inf->height = w->h;
}

t_GR_WINDOW_ID t_GrGetFocus() 
{
    return ttk_windows->w;
}

t_GR_WINDOW_ID t_GrNewPixmap (int w, int h, void *_u)
{
    TWindow *ret = calloc (1, sizeof(TWindow));
    ret->srf = ttk_new_surface (w, h, ttk_screen->bpp);
    return ret;
}

t_GR_GC_ID t_GrNewGC() 
{
    return ttk_new_gc();
}

t_GR_GC_ID t_GrCopyGC (t_GR_GC_ID other) 
{
    return ttk_copy_gc (other);
}

void t_GrDestroyGC (t_GR_GC_ID gc) 
{
    return ttk_free_gc (gc);
}

void t_GrSetGCForeground (t_GR_GC_ID gc, t_GR_COLOR col) 
{
    ttk_gc_set_foreground (gc, ttk_makecol (RfRGB (col), GfRGB (col), BfRGB (col)));
}
void t_GrSetGCBackground (t_GR_GC_ID gc, t_GR_COLOR col) 
{
    ttk_gc_set_background (gc, ttk_makecol (RfRGB (col), GfRGB (col), BfRGB (col)));
}
void t_GrSetGCFont (t_GR_GC_ID gc, t_GR_FONT_ID font) 
{
    ttk_gc_set_font (gc, font);
}
void t_GrSetGCUseBackground (t_GR_GC_ID gc, int flag) 
{
    ttk_gc_set_usebg (gc, flag);
}
void t_GrSetGCMode (t_GR_GC_ID gc, int mode) 
{
    ttk_gc_set_xormode (gc, (mode == GR_MODE_XOR));
}
void t_GrGetGCInfo (t_GR_GC_ID gc, t_GR_GC_INFO *inf) 
{
#if defined(SDL) || !defined(MWIN)
    inf->foreground = gc->fg;
    inf->background = gc->bg;
    inf->usebackground = gc->usebg;
    inf->font = gc->font;
#else
#undef GrGetGCInfo
#undef GrGetFontInfo
#undef GR_GC_INFO
    GR_GC_INFO gi;
    GrGetGCInfo (gc, &gi);
    inf->foreground = gi.foreground;
    inf->background = gi.background;
    inf->usebackground = gi.usebackground;
    inf->font.f = gi.font;
    GrGetFontInfo (inf->font.f, &inf->font.inf);
#endif
}

void t_GrPoint (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int x, int y) { ttk_pixel_gc (dst->srf, gc, x, y); }
void t_GrLine (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int x1, int y1, int x2, int y2) { ttk_line_gc (dst->srf, gc, x1, y1, x2, y2); }
void t_GrRect (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int x, int y, int w, int h) { ttk_rect_gc (dst->srf, gc, x, y, w, h); }
void t_GrFillRect (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int x, int y, int w, int h) { ttk_fillrect_gc (dst->srf, gc, x, y, w, h); }
void t_GrPoly (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int n, t_GR_POINT *v) { ttk_polyline_gc (dst->srf, gc, n, v); }
void t_GrFillPoly (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int n, t_GR_POINT *v) { ttk_fillpoly_gc (dst->srf, gc, n, v); }
void t_GrEllipse (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int x, int y, int rx, int ry) { ttk_ellipse_gc (dst->srf, gc, x, y, rx, ry); }
void t_GrFillEllipse (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int x, int y, int rx, int ry) { ttk_fillellipse_gc (dst->srf, gc, x, y, rx, ry); }

void t_GrCopyArea (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int dx, int dy, int sw, int sh, t_GR_DRAW_ID src, int sx, int sy, int _unused1)
{
    ttk_blit_image_ex (src->srf, sx, sy, sw, sh, dst->srf, dx, dy);
}

void t_GrBitmap (t_GR_DRAW_ID srf, t_GR_GC_ID gc, int x, int y, int width, int height, t_GR_BITMAP *imagebits)
{
    int minx, maxx;
    unsigned short bitvalue = 0;
    int bitcount;
    int ubg = 0, bgcol = 0;

    t_GR_GC_INFO gci;
    t_GrGetGCInfo (gc, &gci);
    if (gci.usebackground) {
	ubg = 1;
	bgcol = gci.background;
    }

    minx = x;
    maxx = x + width - 1;
    bitcount = 0;
    while (height > 0) {
	if (bitcount <= 0) {
	    bitcount = 16;
	    bitvalue = *imagebits++;
	}
	if (bitvalue & (1<<15))
	    ttk_pixel_gc (srf->srf, gc, x, y);
	else if (ubg)
	    ttk_pixel (srf->srf, x, y, bgcol);

	bitvalue <<= 1;
	bitcount--;

	if (x++ == maxx) {
	    x = minx;
	    y++;
	    --height;
	    bitcount = 0;
	}
    }
}

t_GR_IMAGE_ID t_GrLoadImageFromFile (const char *file, int _u) 
{
    return ttk_load_image (file);
}

void t_GrDrawImageToFit (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int x, int y, int w, int h, t_GR_IMAGE_ID img) 
{
    int rw, rh;
    float xf, yf;
    ttk_surface_get_dimen (img, &rw, &rh);
    
    if (rw == w && rh == h) xf = yf = 1.0;
    else {
	xf = (float)w / (float)rw;
	yf = (float)h / (float)rh;
    }

    if (xf > 0.95 && xf < 1.05 && yf > 0.95 && yf < 1.05) {
	ttk_blit_image (img, dst->srf, x, y);
    } else {
	ttk_surface tmp = ttk_scale_surface (img, xf);
	ttk_blit_image_ex (tmp, 0, 0, w, h, dst->srf, x, y);
	ttk_free_surface (tmp);
    }
}

void t_GrFreeImage (t_GR_IMAGE_ID img) 
{
    ttk_free_surface (img);
}

void t_GrText (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int x, int y, const void *str, int count, int _u) 
{
    if (_u & GR_TFTOP)
	ttk_text_gc (dst->srf, gc, x, y, (const char *)str);
    else
	ttk_text_gc (dst->srf, gc, x, y - ttk_text_height_gc (gc) + 1, (const char *)str);
}

t_GR_FONT_ID t_GrCreateFont (const char *path, int size, struct t_GR_LOGFONT *_u) 
{
    if (strrchr (path, '/')) path = strrchr (path, '/') + 1;
    return ttk_get_font (path, size);
}

void t_GrDestroyFont (t_GR_FONT_ID font) {}
void t_GrGetGCTextSize (t_GR_GC_ID gc, const void *str, int _u, int flags, int *width, int *height, int *base)
{
    if (width) *width = ttk_text_width_gc (gc, str);
    if (height) *height = ttk_text_height_gc (gc);
    if (base) *base = 0;
}

t_GR_TIMER_ID t_GrCreateTimer (t_GR_WINDOW_ID win, int ms) 
{
    if (win->focus) {
	ttk_widget_set_timer (win->focus, ms);
    } else {
	fprintf (stderr, "No focus setting timer\n");
    }
    return win->focus;
}

void t_GrDestroyTimer (t_GR_TIMER_ID timer) 
{
    if (timer)
	ttk_widget_set_timer (timer, 0);
}
