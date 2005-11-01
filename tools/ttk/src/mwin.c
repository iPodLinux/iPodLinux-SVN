#define MWBACKEND
#include "ttk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

GR_GC_ID tmp_gc;

struct timeval tvstart;

void ttk_gfx_init() 
{
    if (GrOpen() < 0) {
	fprintf (stderr, "GrOpen() failed\n");
	exit (1);
    }

    tmp_gc = GrNewGC();
    GrSetGCUseBackground (tmp_gc, 0);
    ttk_screen->srf = GrNewWindowEx (GR_WM_PROPS_APPFRAME |
				     GR_WM_PROPS_CAPTION |
				     GR_WM_PROPS_CLOSEBOX,
				     "TTK Program",
				     GR_ROOT_WINDOW_ID,
				     0, 0, ttk_screen->w, ttk_screen->h, ttk_makecol (WHITE));
    
    GrSelectEvents (ttk_screen->srf, GR_EVENT_MASK_EXPOSURE |
		    GR_EVENT_MASK_CLOSE_REQ | GR_EVENT_MASK_KEY_UP |
		    GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_TIMER);

    GrMapWindow (ttk_screen->srf);

    gettimeofday (&tvstart, 0);
}

void ttk_gfx_quit() 
{
    GrClose();
}

void ttk_gfx_update (ttk_surface s)
{
    GrFlush();
}


int ttk_get_rawevent (int *arg)
{
    GR_EVENT ev;
    int tev = TTK_NO_EVENT;

    if (GrCheckNextEvent (&ev), ev.type != GR_EVENT_TYPE_NONE) {
	switch (ev.type) {
	case GR_EVENT_TYPE_CLOSE_REQ:
	    ttk_quit();
	    exit (0);
	    break;
	case GR_EVENT_TYPE_KEY_DOWN:
	case GR_EVENT_TYPE_KEY_UP:
	    tev = (ev.type == GR_EVENT_TYPE_KEY_UP)? TTK_BUTTON_UP : TTK_BUTTON_DOWN;
	    
	    if (ev.keystroke.ch >= 32 && ev.keystroke.ch < 128) {
		*arg = ev.keystroke.ch;
		return tev;
	    } else if (ev.keystroke.ch == MWKEY_ENTER) {
		*arg = TTK_INPUT_ENTER;
		return tev;
	    } else if (ev.keystroke.ch == MWKEY_BACKSPACE) {
		*arg = TTK_INPUT_BKSP;
		return tev;
	    } else if (ev.keystroke.ch == MWKEY_LEFT) {
		*arg = TTK_INPUT_LEFT;
		return tev;
	    } else if (ev.keystroke.ch == MWKEY_RIGHT) {
		*arg = TTK_INPUT_RIGHT;
		return tev;
	    } else if (ev.keystroke.ch == MWKEY_ESCAPE) {
		ttk_input_end();
		return TTK_NO_EVENT;
	    }
	    return TTK_NO_EVENT;
	}
    }
    return TTK_NO_EVENT;
}

int ttk_get_event (int *arg) 
{
    GR_EVENT ev;
    int tev = TTK_NO_EVENT;
    
    *arg = 0;
    
    // We amalgamate adjacent scroll events to reduce lag.
    while (GrCheckNextEvent (&ev), ev.type != GR_EVENT_TYPE_NONE) {
	switch (ev.type) {
	case GR_EVENT_TYPE_CLOSE_REQ:
	    ttk_quit();
	    exit (0);
	    break;
	case GR_EVENT_TYPE_KEY_DOWN:
	case GR_EVENT_TYPE_KEY_UP:
	    if (ev.keystroke.ch == 'l' || ev.keystroke.ch == 'r') {
		if (ev.type == GR_EVENT_TYPE_KEY_DOWN) continue;
		tev = TTK_SCROLL;
		if (ev.keystroke.ch == 'l') (*arg)--;
		else (*arg)++;
		break;
	    }

	    tev = (ev.type == GR_EVENT_TYPE_KEY_DOWN)? TTK_BUTTON_DOWN : TTK_BUTTON_UP;
	    
	    if (ev.keystroke.ch == 'w' || ev.keystroke.ch == MWKEY_LEFT) *arg = TTK_BUTTON_PREVIOUS;
	    else if (ev.keystroke.ch == 'f' || ev.keystroke.ch == MWKEY_RIGHT) *arg = TTK_BUTTON_NEXT;
	    else if (ev.keystroke.ch == 'm' || ev.keystroke.ch == MWKEY_UP) *arg = TTK_BUTTON_MENU;
	    else if (ev.keystroke.ch == 'd' || ev.keystroke.ch == MWKEY_DOWN) *arg = TTK_BUTTON_PLAY;
	    else if (ev.keystroke.ch == 'h') *arg = TTK_BUTTON_HOLD;
	    else if (ev.keystroke.ch == MWKEY_ENTER) *arg = TTK_BUTTON_ACTION;
	    
#ifndef IPOD
	    else if (ev.keystroke.ch == MWKEY_KP0) *arg = '0';
	    else if (ev.keystroke.ch == MWKEY_KP1) *arg = '1';
	    else if (ev.keystroke.ch == MWKEY_KP2) *arg = '2';
	    else if (ev.keystroke.ch == MWKEY_KP3) *arg = '3';
	    else if (ev.keystroke.ch == MWKEY_KP4) *arg = '4';
	    else if (ev.keystroke.ch == MWKEY_KP5) *arg = '5';
	    else if (ev.keystroke.ch == MWKEY_KP6) *arg = '6';
	    else if (ev.keystroke.ch == MWKEY_KP7) *arg = '7';
	    else if (ev.keystroke.ch == MWKEY_KP8) *arg = '8';
	    else if (ev.keystroke.ch == MWKEY_KP9) *arg = '9';
#endif
	    else fprintf (stderr, "Unrecognized key\n"), *arg = ev.keystroke.ch;
	    return tev;
	}
    }
    return tev;    
}

int ttk_getticks() 
{
    struct timeval tv;
    gettimeofday (&tv, 0);
    return (tv.tv_usec - tvstart.tv_usec) / 1000 + (tv.tv_sec - tvstart.tv_sec) * 1000;
}

void ttk_delay (int ms) 
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;

    while (nanosleep (&ts, &ts) < 0 && errno == EINTR)
	;
}

ttk_color ttk_makecol (int r, int g, int b) 
{
    return (0xff000000 | (b << 16) | (g << 8) | r);
}

ttk_color ttk_makecol_ex (int r, int g, int b, ttk_surface srf) 
{
    return (0xff000000 | (b << 16) | (g << 8) | r);
}

void ttk_unmakecol (ttk_color col, int *r, int *g, int *b) 
{
    *b = (col >> 16) & 0xff;
    *g = (col >> 8)  & 0xff;
    *r =  col        & 0xff;
}

void ttk_unmakecol_ex (ttk_color col, int *r, int *g, int *b, ttk_surface srf) 
{
    return ttk_unmakecol (col, r, g, b);
}

ttk_gc ttk_new_gc() 
{
    return GrNewGC();
}
ttk_gc ttk_copy_gc (ttk_gc other) 
{
    return GrCopyGC (other);
}
void ttk_gc_set_foreground (ttk_gc gc, ttk_color fgcol) 
{
    GrSetGCForeground (gc, fgcol);
}
void ttk_gc_set_background (ttk_gc gc, ttk_color fgcol) 
{
    GrSetGCBackground (gc, fgcol);
}
void ttk_gc_set_font (ttk_gc gc, ttk_font font) 
{
    GrSetGCFont (gc, font.f);
}
void ttk_gc_set_usebg (ttk_gc gc, int flag)
{
    GrSetGCUseBackground (gc, flag);
}
void ttk_gc_set_xormode (ttk_gc gc, int flag) 
{
    if (flag)
	GrSetGCMode (gc, GR_MODE_XOR);
    else
	GrSetGCMode (gc, GR_MODE_COPY);
}
void ttk_free_gc (ttk_gc gc) 
{
    GrDestroyGC (gc);
}

void ttk_pixel (ttk_surface srf, int x, int y, ttk_color col) 
{
    GrSetGCForeground (tmp_gc, col);
    GrPoint (srf, tmp_gc, x, y);
}
void ttk_pixel_gc (ttk_surface srf, ttk_gc gc, int x, int y) 
{
    GrPoint (srf, gc, x, y);
}

void ttk_line (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col) 
{
    GrSetGCForeground (tmp_gc, col);
    GrLine (srf, tmp_gc, x1, y1, x2, y2);
}
void ttk_line_gc (ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2) 
{
    GrLine (srf, gc, x1, y1, x2, y2);
}

void ttk_rect (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col) 
{
    GrSetGCForeground (tmp_gc, col);
    GrRect (srf, tmp_gc, x1, y1, x2-x1+1, y2-y1+1);
}
void ttk_rect_gc (ttk_surface srf, ttk_gc gc, int x, int y, int w, int h) 
{
    GrRect (srf, gc, x, y, w, h);
}
void ttk_fillrect (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col) 
{
    GrSetGCForeground (tmp_gc, col);
    GrFillRect (srf, tmp_gc, x1, y1, x2-x1+1, y2-y1+1);
}
void ttk_fillrect_gc (ttk_surface srf, ttk_gc gc, int x, int y, int w, int h) 
{
    GrFillRect (srf, gc, x, y, w, h);
}

void ttk_poly (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col) 
{
    int i;
    ttk_point *v = malloc (nv*sizeof(ttk_point));
    if (!v) {
	fprintf (stderr, "Out of memory\n");
	ttk_quit();
	exit (1);
    }

    for (i = 0; i < nv; i++) {
	v[i].x = vx[i];
	v[i].y = vy[i];
    }

    GrSetGCForeground (tmp_gc, col);
    GrPoly (srf, tmp_gc, nv, v);

    free (v);
}
void ttk_poly_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v) 
{
    GrPoly (srf, gc, n, v);
}
void ttk_fillpoly (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col) 
{
    int i;
    ttk_point *v = malloc (nv*sizeof(ttk_point));
    if (!v) {
	fprintf (stderr, "Out of memory\n");
	ttk_quit();
	exit (1);
    }

    for (i = 0; i < nv; i++) {
	v[i].x = vx[i];
	v[i].y = vy[i];
    }

    GrSetGCForeground (tmp_gc, col);
    GrFillPoly (srf, tmp_gc, nv, v);

    free (v);
}
void ttk_fillpoly_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v) 
{
    GrFillPoly (srf, gc, n, v);
}

void ttk_ellipse (ttk_surface srf, int x, int y, int rx, int ry, ttk_color col) 
{
    GrSetGCForeground (tmp_gc, col);
    GrEllipse (srf, tmp_gc, x, y, rx, ry);
}
void ttk_ellipse_gc (ttk_surface srf, ttk_gc gc, int x, int y, int rx, int ry) 
{
    GrEllipse (srf, gc, x, y, rx, ry);
}
void ttk_fillellipse (ttk_surface srf, int x, int y, int rx, int ry, ttk_color col) 
{
    GrSetGCForeground (tmp_gc, col);
    GrFillEllipse (srf, tmp_gc, x, y, rx, ry);
}
void ttk_fillellipse_gc (ttk_surface srf, ttk_gc gc, int x, int y, int rx, int ry) 
{
    GrFillEllipse (srf, gc, x, y, rx, ry);
}


void ttk_text (ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col, const char *str) 
{
    GrSetGCForeground (tmp_gc, col);
    GrSetGCFont (tmp_gc, fnt.f);
    GrText (srf, tmp_gc, x, y, (void *)str, strlen (str), GR_TFASCII | GR_TFTOP);
}
void ttk_text_gc (ttk_surface srf, ttk_gc gc, int x, int y, const char *str)
{
    GrText (srf, gc, x, y, (void *)str, strlen (str), GR_TFASCII | GR_TFTOP);
}
int ttk_text_width_gc (ttk_gc gc, const char *str) 
{
    int w, h, b;
    GrGetGCTextSize (gc, (void *)str, strlen (str), GR_TFASCII, &w, &h, &b);
    return w;
}
int ttk_text_width (ttk_font fnt, const char *str)
{
    int w = 0;
    const char *p;

    if (!str) return 0;
    
    for (p = str; *p; p++) {
	if (fnt.inf.fixed) w += fnt.inf.maxwidth;
	else               w += fnt.inf.widths[*p];
    }

    return w;
}
int ttk_text_height_gc (ttk_gc gc) 
{
    int w, h, b;
    GrGetGCTextSize (gc, "Ay", 2, GR_TFASCII, &w, &h, &b);
    return h + b;
}
int ttk_text_height (ttk_font fnt)
{
    return fnt.inf.height;
}
void ttk_load_font (ttk_fontinfo *fi, const char *fnbase, int size) 
{
    char *fname = alloca (strlen (fnbase) + 5); /* +5: . f n t \0 */
    struct stat st;
    
    strcpy (fname, fnbase);
    strcat (fname, ".fnt");
    if ((fi->f.f = GrCreateFont ((GR_CHAR *)fname, fi->size, 0)) == 0) {
	strcpy (fname, fnbase);
	strcat (fname, ".pcf");
	if ((fi->f.f = GrCreateFont ((GR_CHAR *)fname, fi->size, 0)) == 0) {
	    fprintf (stderr, "Unable to load font\n");
	    fi->good = 0;
	    return;
	}
    }

    GrGetFontInfo (fi->f.f, &fi->f.inf);
}
void ttk_unload_font (ttk_fontinfo *fi) 
{
    GrDestroyFont (fi->f.f);
    fi->good = 0;
    fi->loaded = 0;
}

// image funcs
ttk_surface ttk_load_image (const char *path)
{
    GR_WINDOW_ID pixmap;
    GR_IMAGE_ID image;
    GR_IMAGE_INFO inf;
    
    if ((image = GrLoadImageFromFile ((char *)path, 0)) == 0) {
	return 0;
    }

    GrGetImageInfo (image, &inf);
    if ((pixmap = GrNewPixmap (inf.width, inf.height, 0)) == 0) {
	GrFreeImage (image);
	return 0;
    }

    GrSetGCForeground (tmp_gc, GR_RGB (0, 0, 0));
    GrDrawImageToFit (pixmap, tmp_gc, 0, 0, inf.width, inf.height, image);
    GrFreeImage (image);
    
    return pixmap;
}
void ttk_free_image (ttk_surface img) 
{
    GrDestroyWindow (img);
}
void ttk_blit_image (ttk_surface src, ttk_surface dst, int dx, int dy) 
{
    int w, h;
    
    ttk_surface_get_dimen (src, &w, &h);
    ttk_blit_image_ex (src, 0, 0, w, h, dst, dx, dy);
}
void ttk_blit_image_ex (ttk_surface src, int sx, int sy, int sw, int sh,
			ttk_surface dst, int dx, int dy) 
{
    int rsw, rsh, rdw, rdh;
    ttk_surface_get_dimen (src, &rsw, &rsh);
    ttk_surface_get_dimen (dst, &rdw, &rdh);
    
    if (sx+sw > rsw)
	sw = rsw - sx;
    if (sy+sh > rsh)
	sh = rsh - sy;
    if (dx+sw > rdw)
	sw = rdw - dx;
    if (dy+sh > rdh)
	sh = rdh - dy;
    if (sw>0 && sh>0) {
	GrSetGCForeground (tmp_gc, GR_RGB (0, 0, 0));
	GrCopyArea (dst, tmp_gc, dx, dy, sw, sh, src, sx, sy, MWROP_SRCCOPY);
    }
}

// surface funcs
ttk_surface ttk_new_surface (int w, int h, int bpp) 
{
    GR_WINDOW_ID ret = GrNewPixmap (w, h, 0);
    GrSetGCForeground (tmp_gc, GR_RGB (255, 255, 255));
    GrFillRect (ret, tmp_gc, 0, 0, w, h);
    return ret;
}

ttk_surface ttk_scale_surface (ttk_surface srf, float factor) 
{
    GR_WINDOW_ID ret;
    int w, h;
    
    ttk_surface_get_dimen (srf, &w, &h);
    ret = GrNewPixmap ((int)((float)w * factor), (int)((float)h * factor), 0);
    GrStretchArea (ret, tmp_gc, 0, 0, w * factor, h * factor,
		   srf, 0, 0, w, h, MWROP_SRCCOPY);
    return ret;
}

void ttk_surface_get_dimen (ttk_surface srf, int *w, int *h) 
{
    GR_WINDOW_INFO winf;
    GrGetWindowInfo (srf, &winf);
    if (w) *w = winf.width;
    if (h) *h = winf.height;
}

void ttk_free_surface (ttk_surface srf)
{
    GrDestroyWindow (srf);
}
