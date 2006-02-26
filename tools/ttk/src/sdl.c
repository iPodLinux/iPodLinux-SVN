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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef SDL

typedef struct Bitmap_Font
{
    char *name;
    int maxwidth;
    unsigned int height;
    int ascent;
    int firstchar;
    int size;
    const unsigned short *bits;
    const unsigned long *offset;
    const unsigned char *width;
    int defaultchar;
    long bits_size;
} Bitmap_Font;

extern ttk_screeninfo *ttk_screen;

static void palettize (SDL_Surface *srf)
{
    if (ttk_screen->bpp == 2) {
	int grp;
	SDL_Color colors[] = { {255,255,255}, {160,160,160}, {80,80,80}, {0,0,0} };
	for (grp = 0; grp < 256; grp += 4) {
	    SDL_SetColors (srf, colors, grp, 4);
	}
	SDL_FillRect (srf, 0, 0);
    }
}

void ttk_gfx_init() 
{
#ifdef IPOD
#define NOPAR 0
#else
#define NOPAR SDL_INIT_NOPARACHUTE
#endif

    int bpp = ttk_screen->bpp;
    if (bpp == 2) bpp = 8;

    if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_TIMER | NOPAR) < 0) {
	fprintf (stderr, "SDL_Init(V|T): %s\n", SDL_GetError());
	SDL_Quit();
	exit (1);
    }

    if ((ttk_screen->srf = SDL_SetVideoMode (ttk_screen->w, ttk_screen->h,
					     bpp, SDL_SWSURFACE)) == 0) {
	fprintf (stderr, "SDL_SetVideoMode(%d,%d,%d): %s\n", ttk_screen->w,
		 ttk_screen->h, ttk_screen->bpp, SDL_GetError());
	SDL_Quit();
	exit (1);
    }

#ifdef IPOD
    SDL_ShowCursor (SDL_DISABLE);
    SDL_EnableKeyRepeat (100, 100);
#else
    palettize (ttk_screen->srf);
    SDL_EnableKeyRepeat (100, 50);
    SDL_EnableUNICODE (1);
#endif
}

void ttk_gfx_quit() 
{
    SDL_Quit();
}

void ttk_gfx_update (ttk_surface srf) 
{
    SDL_Flip (srf);
}

int ttk_get_rawevent (int *arg)
{
    SDL_Event ev;
    int tev = TTK_NO_EVENT;

    if (SDL_PollEvent (&ev)) {
	switch (ev.type) {
	case SDL_QUIT:
	    ttk_quit();
	    exit (0);
	    break;
	case SDL_KEYDOWN:
	case SDL_KEYUP:
	    tev = (ev.type == SDL_KEYUP)? TTK_BUTTON_UP : TTK_BUTTON_DOWN;
	    
	    if (ev.key.keysym.sym >= 32 && ev.key.keysym.sym < 128) {
		*arg = ev.key.keysym.unicode;
		return tev;
	    } else if (ev.key.keysym.sym == SDLK_RETURN) {
		*arg = TTK_INPUT_ENTER;
		return tev;
	    } else if (ev.key.keysym.sym == SDLK_BACKSPACE) {
		*arg = TTK_INPUT_BKSP;
		return tev;
	    } else if (ev.key.keysym.sym == SDLK_LEFT) {
		*arg = TTK_INPUT_LEFT;
		return tev;
	    } else if (ev.key.keysym.sym == SDLK_RIGHT) {
		*arg = TTK_INPUT_RIGHT;
		return tev;
	    } else if (ev.key.keysym.sym == SDLK_ESCAPE) {
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
    SDL_Event ev;
    int tev = TTK_NO_EVENT;
    
    *arg = 0;

    // We amalgamate adjacent scroll events to reduce lag.
    while (SDL_PollEvent (&ev)) {
	switch (ev.type) {
	case SDL_QUIT:
	    ttk_quit();
	    exit (0);
	    break;
	case SDL_KEYDOWN:
	case SDL_KEYUP:
	    if (ev.key.keysym.sym == SDLK_l || ev.key.keysym.sym == SDLK_r) {
		if (ev.type == SDL_KEYUP) continue;
		tev = TTK_SCROLL;
		if (ev.key.keysym.sym == SDLK_l) (*arg)--;
		else (*arg)++;
		break;
	    }

	    tev = (ev.type == SDL_KEYDOWN)? TTK_BUTTON_DOWN : TTK_BUTTON_UP;
	    
	    if (ev.key.keysym.sym == SDLK_w || ev.key.keysym.sym == SDLK_LEFT) *arg = TTK_BUTTON_PREVIOUS;
	    else if (ev.key.keysym.sym == SDLK_f || ev.key.keysym.sym == SDLK_RIGHT) *arg = TTK_BUTTON_NEXT;
	    else if (ev.key.keysym.sym == SDLK_m || ev.key.keysym.sym == SDLK_UP) *arg = TTK_BUTTON_MENU;
	    else if (ev.key.keysym.sym == SDLK_d || ev.key.keysym.sym == SDLK_DOWN) *arg = TTK_BUTTON_PLAY;
	    else if (ev.key.keysym.sym == SDLK_h) *arg = TTK_BUTTON_HOLD;
	    else if (ev.key.keysym.sym == SDLK_RETURN) *arg = TTK_BUTTON_ACTION;
	    
#ifndef IPOD
	    else if (ev.key.keysym.sym == SDLK_KP0) *arg = '0';
	    else if (ev.key.keysym.sym == SDLK_KP1) *arg = '1';
	    else if (ev.key.keysym.sym == SDLK_KP2) *arg = '2';
	    else if (ev.key.keysym.sym == SDLK_KP3) *arg = '3';
	    else if (ev.key.keysym.sym == SDLK_KP4) *arg = '4';
	    else if (ev.key.keysym.sym == SDLK_KP5) *arg = '5';
	    else if (ev.key.keysym.sym == SDLK_KP6) *arg = '6';
	    else if (ev.key.keysym.sym == SDLK_KP7) *arg = '7';
	    else if (ev.key.keysym.sym == SDLK_KP8) *arg = '8';
	    else if (ev.key.keysym.sym == SDLK_KP9) *arg = '9';
#endif
#ifdef WARN_UNRECOG_KEYS
	    else fprintf (stderr, "Unrecognized key\n"), *arg = ev.key.keysym.sym;
#endif
	    return tev;
	}
    }
    return tev;
}

int ttk_getticks() 
{
    return SDL_GetTicks();
}

void ttk_delay (int ms) 
{
    SDL_Delay (ms);
}

#ifndef MAX
#define MAX(x,y) (((x)>(y))?(x):(y))
#endif

ttk_color ttk_makecol (int r, int g, int b) 
{
    return ttk_makecol_ex (r, g, b, 0);
}

ttk_color ttk_makecol_ex (int r, int g, int b, ttk_surface srf) 
{
    if (ttk_screen->bpp == 2) {
	int hue;
	int col;

	// quickies
	if ((r+g+b) == 0)
	    return 3;
	if ((r+g+b) == 3*255)
	    return 0;

	hue = MAX (MAX (r,g),b);

	if (hue < 40) col = 3;
	else if (hue < 120) col = 2;
	else if (hue < 200) col = 1;
	else col = 0;
	return col;
    } else {
	if (!srf) srf = ttk_screen->srf;
	return SDL_MapRGB (srf->format, r, g, b);
    }
}

void ttk_unmakecol (ttk_color col, int *r, int *g, int *b)
{
    ttk_unmakecol_ex (col, r, g, b, 0);
}
void ttk_unmakecol_ex (ttk_color col, int *r, int *g, int *b, ttk_surface srf) 
{
    if (!srf) srf = ttk_screen->srf;
    if (ttk_screen->bpp == 2) {
	int v;
	
	col &= 3;
	
	v = (col == 3)? 0 : (col == 2)? 80 : (col == 1)? 160 : 255;
	*r = *g = *b = v;
    } else {
	Uint8 R, G, B;
	SDL_GetRGB (col, srf->format, &R, &G, &B);
	*r = R; *g = G; *b = B;
    }
}

ttk_gc ttk_new_gc() 
{
    return (ttk_gc) calloc (1, sizeof(struct _ttk_gc));
}
ttk_gc ttk_copy_gc (ttk_gc other) 
{
    ttk_gc ret = malloc (sizeof(struct _ttk_gc));
    memcpy (ret, other, sizeof(struct _ttk_gc));
    return ret;
}
ttk_color ttk_gc_get_foreground (ttk_gc gc) { return gc->fg; }
ttk_color ttk_gc_get_background (ttk_gc gc) { return gc->bg; }
ttk_font ttk_gc_get_font (ttk_gc gc) { return gc->font; }
void ttk_gc_set_foreground (ttk_gc gc, ttk_color fgcol) 
{
    gc->fg = fgcol;
}
void ttk_gc_set_background (ttk_gc gc, ttk_color bgcol) 
{
    gc->bg = bgcol;
}
void ttk_gc_set_font (ttk_gc gc, ttk_font font) 
{
    gc->font = font;
}
void ttk_gc_set_usebg (ttk_gc gc, int flag)
{
    gc->usebg = flag;
}
void ttk_gc_set_xormode (ttk_gc gc, int flag)
{
    gc->xormode = flag;
}
void ttk_free_gc (ttk_gc gc) 
{
    free (gc);
}

static int fetchcolor (ttk_color col) 
{
    int ret = 0;
    Uint8 r,g,b;

    SDL_GetRGB (col, ttk_screen->srf->format, &r,&g,&b);
    ret = (r << 24) | (g << 16) | (b << 8);
    if (col & 0xff0000) ret |= (col >> 16);
    else ret |= 0xff;
    return ret;
}

ttk_color ttk_getpixel (ttk_surface srf, int x, int y) 
{
    switch (srf->format->BytesPerPixel) {
    case 1:
        return *((Uint8 *)((Uint8 *)srf->pixels + y*srf->pitch) + x);
    case 2:
        return *((Uint16 *)((Uint8 *)srf->pixels + y*srf->pitch) + x);
    case 4:
        return *((Uint32 *)((Uint8 *)srf->pixels + y*srf->pitch) + x);
    default:
        return ttk_makecol_ex (255, 0, 255, srf);
    }
}
void ttk_pixel (ttk_surface srf, int x, int y, ttk_color col) 
{
    if (ttk_screen->bpp == 2)
	pixelByte (srf, x, y, col);
    else
	pixelColor (srf, x, y, fetchcolor (col));
}
void ttk_pixel_gc (ttk_surface srf, ttk_gc gc, int x, int y)
{
    if (ttk_screen->bpp == 2)
	pixelByte (srf, x, y, gc->fg);
    else
	pixelColor (srf, x, y, fetchcolor (gc->fg));
}

void ttk_line (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col)
{
    if (ttk_screen->bpp == 2)
	lineByte (srf, x1, y1, x2, y2, col);
    else
	lineColor (srf, x1, y1, x2, y2, fetchcolor (col));
}
void ttk_line_gc (ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2)
{
    if (ttk_screen->bpp == 2)
	lineByte (srf, x1, y1, x2, y2, gc->fg);
    else
	lineColor (srf, x1, y1, x2, y2, fetchcolor (gc->fg));
}
void ttk_aaline (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col)
{
    if (ttk_screen->bpp == 2)
	aalineByte (srf, x1, y1, x2, y2, col);
    else
	aalineColor (srf, x1, y1, x2, y2, fetchcolor (col));
}
void ttk_aaline_gc (ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2)
{
    if (ttk_screen->bpp == 2)
	aalineByte (srf, x1, y1, x2, y2, gc->fg);
    else
	aalineColor (srf, x1, y1, x2, y2, fetchcolor (gc->fg));
}

void ttk_rect (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col)
{
    if (ttk_screen->bpp == 2)
	rectangleByte (srf, x1, y1, x2, y2, col);
    else
	rectangleColor (srf, x1, y1, x2, y2, fetchcolor (col));
}
void ttk_rect_gc (ttk_surface srf, ttk_gc gc, int x, int y, int w, int h)
{
    if (ttk_screen->bpp == 2)
	rectangleByte (srf, x, y, x+w, y+h, gc->fg);
    else
	rectangleColor (srf, x, y, x+w, y+h, fetchcolor (gc->fg));
}
void ttk_fillrect (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col)
{
    if (ttk_screen->bpp == 2)
	boxByte (srf, x1, y1, x2, y2, col);
    else
	boxColor (srf, x1, y1, x2, y2, fetchcolor (col));
}
void ttk_fillrect_gc (ttk_surface srf, ttk_gc gc, int x, int y, int w, int h)
{
    if (gc->xormode) {
	int x1 = x, x2 = x + w, y1 = y, y2 = y + h;
	
	if (ttk_screen->bpp == 2) {
	    Uint8 *startp = (Uint8 *)srf->pixels + y1*srf->pitch + x1; // start of this line
	    Uint8 *endp = (Uint8 *)srf->pixels + y1*srf->pitch + x2; // end of this line
	    Uint8 *Endp = (Uint8 *)srf->pixels + y2*srf->pitch + x2; // end of it all
	    Uint8 *p;
	    
	    while (endp <= Endp) {
		p = startp;
		while (p < endp)
		    *p++ ^= 0xff;
		
		startp += srf->pitch;
		endp += srf->pitch;
	    }
	} else {
	    Uint16 *startp = (Uint16 *)((Uint8 *)srf->pixels + y1*srf->pitch) + x1; // start of this line
	    Uint16 *endp = (Uint16 *)((Uint8 *)srf->pixels + y1*srf->pitch) + x2; // end of this line
	    Uint16 *Endp = (Uint16 *)((Uint8 *)srf->pixels + y2*srf->pitch) + x2; // end of it all
	    Uint16 *p;
	    
	    while (endp <= Endp) {
		p = startp;
		while (p < endp)
		    *p++ ^= 0xffff;
		
		startp = (Uint16 *)((Uint8 *)startp + srf->pitch);
		endp = (Uint16 *)((Uint8 *)endp + srf->pitch);
	    }
	}
    } else {
	if (ttk_screen->bpp == 2)
	    boxByte (srf, x, y, x+w, y+h, gc->fg);
	else
	    boxColor (srf, x, y, x+w, y+h, fetchcolor (gc->fg));
    }
}

extern unsigned char ttk_chamfering[][10];

void ttk_do_gradient(ttk_surface srf, char horiz, int b_rad, int e_rad,
		int x1, int y1, int x2, int y2, ttk_color begin, ttk_color end)
{
	gradient_node *gn = ttk_gradient_find_or_add(begin, end);
	int steps = horiz ? x2-x1 : y2-y1;
	int line, bc, ec, i;

	if (!gn) return;

	if (steps < 0) steps *= -1;

	if (horiz) {
		for (line = 0, i = steps; line<i &&(b_rad||e_rad); line++,i--) {
			bc = (line < b_rad)? ttk_chamfering[b_rad - 1][line]:0;
			ec = (line < e_rad)? ttk_chamfering[e_rad - 1][line]:0;
			if (bc == 0 && ec == 0) break;
			ttk_line(srf, x1 + line, y1 + bc, x1 + line, y2-1 - bc,
					gn->gradient[(line * 256)/steps]);
			ttk_line(srf, x2-1 - line, y1 + ec, x2-1 - line,
					y2-1 - ec,
					gn->gradient[((i-1) * 256)/steps]);
		}
		for (; line < i; line++) {
			ttk_line(srf, x1+line, y1, x1+line, y2-1,
					gn->gradient[(line * 256)/steps]);
		}
	}
	else {
		for (line = 0, i = steps; line<i &&(b_rad||e_rad); line++,i--) {
			bc = (line < b_rad)? ttk_chamfering[b_rad - 1][line]:0;
			ec = (line < e_rad)? ttk_chamfering[e_rad - 1][line]:0;
			if (bc == 0 && ec == 0) break;
			ttk_line(srf, x1 + bc, y1 + line, x2-1 - bc, y1 + line,
					gn->gradient[(line * 256)/steps]);
			ttk_line(srf, x1 + ec, y2-1 - line, x2-1 - ec,
					y2-1 - line,
					gn->gradient[((i-1) * 256)/steps]);
		}
		for (; line < i; line++)
			ttk_line(srf, x1, y1+line, x2-1, y1+line,
					gn->gradient[(line * 256)/steps]);
	}
}

void ttk_hgradient( ttk_surface srf, int x1, int y1, int x2, int y2, 
			ttk_color left, ttk_color right )
{ ttk_do_gradient(srf, 1, 0, 0, x1, y1, x2, y2, left, right); }

void ttk_vgradient(ttk_surface srf, int x1, int y1, int x2, int y2, 
			ttk_color top, ttk_color bottom )
{ ttk_do_gradient(srf, 0, 0, 0, x1, y1, x2, y2, top, bottom); }


void ttk_poly (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col)
{
    if (ttk_screen->bpp == 2)
	polygonByte (srf, (Sint16 *)vx, (Sint16 *)vy, nv, col);
    else
	polygonColor (srf, (Sint16 *)vx, (Sint16 *)vy, nv, fetchcolor (col));
}
void ttk_poly_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v) 
{
    int i;
    short *vx = malloc (n*sizeof(short)), *vy = malloc (n*sizeof(short));
    if (!vx || !vy) {
	fprintf (stderr, "Out of memory\n");
	ttk_quit();
	exit (1);
    }

    for (i = 0; i < n; i++) {
	vx[i] = v[i].x;
	vy[i] = v[i].y;
    }
    
    if (ttk_screen->bpp == 2)
	polygonByte (srf, vx, vy, n, gc->fg);
    else
	polygonColor (srf, vx, vy, n, fetchcolor (gc->fg));
    
    free (vx);
    free (vy);
}

void ttk_aapoly (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col)
{
    if (ttk_screen->bpp == 2)
	aapolygonByte (srf, (Sint16 *)vx, (Sint16 *)vy, nv, col);
    else
	aapolygonColor (srf, (Sint16 *)vx, (Sint16 *)vy, nv, fetchcolor (col));
}
void ttk_aapoly_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v) 
{
    int i;
    short *vx = malloc (n*sizeof(short)), *vy = malloc (n*sizeof(short));
    if (!vx || !vy) {
	fprintf (stderr, "Out of memory\n");
	ttk_quit();
	exit (1);
    }

    for (i = 0; i < n; i++) {
	vx[i] = v[i].x;
	vy[i] = v[i].y;
    }
    
    if (ttk_screen->bpp == 2)
	aapolygonByte (srf, vx, vy, n, gc->fg);
    else
	aapolygonColor (srf, vx, vy, n, fetchcolor (gc->fg));
    
    free (vx);
    free (vy);
}

void ttk_polyline (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col)
{
    if (ttk_screen->bpp == 2)
	polylineByte (srf, (Sint16 *)vx, (Sint16 *)vy, nv, col);
    else
	polylineColor (srf, (Sint16 *)vx, (Sint16 *)vy, nv, fetchcolor (col));
}
void ttk_polyline_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v) 
{
    int i;
    short *vx = malloc (n*sizeof(short)), *vy = malloc (n*sizeof(short));
    if (!vx || !vy) {
	fprintf (stderr, "Out of memory\n");
	ttk_quit();
	exit (1);
    }

    for (i = 0; i < n; i++) {
	vx[i] = v[i].x;
	vy[i] = v[i].y;
    }
    
    if (ttk_screen->bpp == 2)
	polylineByte (srf, vx, vy, n, gc->fg);
    else
	polylineColor (srf, vx, vy, n, fetchcolor (gc->fg));
    
    free (vx);
    free (vy);
}

#define DO_BEZIER(function)\
    /*  Note: I don't think there is any great performance win in \
     *  translating this to fixed-point integer math, most of the time \
     *  is spent in the line drawing routine. */ \
    float x = (float)x1, y = (float)y1;\
    float xp = x, yp = y;\
    float delta;\
    float dx, d2x, d3x;\
    float dy, d2y, d3y;\
    float a, b, c;\
    int i;\
    int n = 1;\
    Sint16 xmax=x1, ymax=y1, xmin=x1, ymin=y1;\
    \
    /* compute number of iterations */\
    if(level < 1)\
	level=1;\
    if(level >= 15)\
	level=15; \
    while (level-- > 0)\
	n*= 2;\
    delta = (float)( 1.0 / (float)n );\
    \
    /* compute finite differences \
     * a, b, c are the coefficient of the polynom in t defining the \
     * parametric curve. The computation is done independently for x and y */\
    a = (float)(-x1 + 3*x2 - 3*x3 + x4);\
    b = (float)(3*x1 - 6*x2 + 3*x3);\
    c = (float)(-3*x1 + 3*x2);\
    \
    d3x = 6 * a * delta*delta*delta;\
    d2x = d3x + 2 * b * delta*delta;\
    dx = a * delta*delta*delta + b * delta*delta + c * delta;\
    \
    a = (float)(-y1 + 3*y2 - 3*y3 + y4);\
    b = (float)(3*y1 - 6*y2 + 3*y3);\
    c = (float)(-3*y1 + 3*y2);\
    \
    d3y = 6 * a * delta*delta*delta;\
    d2y = d3y + 2 * b * delta*delta;\
    dy = a * delta*delta*delta + b * delta*delta + c * delta;\
    \
    /* iterate */\
    for (i = 0; i < n; i++) {\
	x += dx; dx += d2x; d2x += d3x;\
	y += dy; dy += d2y; d2y += d3y;\
	if((Sint16)xp != (Sint16)x || (Sint16)yp != (Sint16)y){\
	    function;\
	    xmax= (xmax>(Sint16)xp)? xmax : (Sint16)xp;\
	    ymax= (ymax>(Sint16)yp)? ymax : (Sint16)yp;\
	    xmin= (xmin<(Sint16)xp)? xmin : (Sint16)xp;\
	    ymin= (ymin<(Sint16)yp)? ymin : (Sint16)yp;\
	    xmax= (xmax>(Sint16)x)? xmax : (Sint16)x;\
	    ymax= (ymax>(Sint16)y)? ymax : (Sint16)y;\
	    xmin= (xmin<(Sint16)x)? xmin : (Sint16)x;\
	    ymin= (ymin<(Sint16)y)? ymin : (Sint16)y;\
	}\
	xp = x; yp = y;\
    }
	
void ttk_bezier(ttk_surface srf, int x1, int y1, int x2, int y2,
		int x3, int y3, int x4, int y4, int level, ttk_color col)
{
    DO_BEZIER(ttk_line(srf, (short)xp, (short)yp, (short)x, (short)y, col));
}

void ttk_aabezier(ttk_surface srf, int x1, int y1, int x2, int y2,
		int x3, int y3, int x4, int y4, int level, ttk_color col)
{
    DO_BEZIER(ttk_aaline(srf, (short)xp, (short)yp, (short)x, (short)y, col));
}

void ttk_fillpoly (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col)
{
    if (ttk_screen->bpp == 2)
	filledPolygonByte (srf, (Sint16 *)vx, (Sint16 *)vy, nv, col);
    else
	filledPolygonColor (srf, (Sint16 *)vx, (Sint16 *)vy, nv, fetchcolor (col));
}
void ttk_fillpoly_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v) 
{
    int i;
    short *vx = malloc (n*sizeof(short)), *vy = malloc (n*sizeof(short));
    if (!vx || !vy) {
	fprintf (stderr, "Out of memory\n");
	ttk_quit();
	exit (1);
    }

    for (i = 0; i < n; i++) {
	vx[i] = v[i].x;
	vy[i] = v[i].y;
    }
    
    if (ttk_screen->bpp == 2)
	filledPolygonByte (srf, vx, vy, n, gc->fg);
    else
	filledPolygonColor (srf, vx, vy, n, fetchcolor (gc->fg));
    
    free (vx);
    free (vy);
}

void ttk_ellipse (ttk_surface srf, int x, int y, int rx, int ry, ttk_color col)
{
    if (rx == ry) {
	if (ttk_screen->bpp == 2)
	    circleByte (srf, x, y, rx, col);
	else
	    circleColor (srf, x, y, rx, fetchcolor (col));
    } else {
	if (ttk_screen->bpp == 2)
	    ellipseByte (srf, x, y, rx, ry, col);
	else
	    ellipseColor (srf, x, y, rx, ry, fetchcolor (col));
    }
}
void ttk_ellipse_gc (ttk_surface srf, ttk_gc gc, int x, int y, int rx, int ry) 
{
    if (rx == ry) {
	if (ttk_screen->bpp == 2)
	    circleByte (srf, x, y, rx, gc->fg);
	else
	    circleColor (srf, x, y, rx, fetchcolor (gc->fg));
    } else {
	if (ttk_screen->bpp == 2)
	    ellipseByte (srf, x, y, rx, ry, gc->fg);
	else
	    ellipseColor (srf, x, y, rx, ry, fetchcolor (gc->fg));
    }
}

void ttk_aaellipse (ttk_surface srf, int x, int y, int rx, int ry, ttk_color col)
{
    if (rx == ry) {
	if (ttk_screen->bpp == 2)
	    aacircleByte (srf, x, y, rx, col);
	else
	    aacircleColor (srf, x, y, rx, fetchcolor (col));
    } else {
	if (ttk_screen->bpp == 2)
	    aaellipseByte (srf, x, y, rx, ry, col);
	else
	    aaellipseColor (srf, x, y, rx, ry, fetchcolor (col));
    }
}
void ttk_aaellipse_gc (ttk_surface srf, ttk_gc gc, int x, int y, int rx, int ry) 
{
    if (rx == ry) {
	if (ttk_screen->bpp == 2)
	    aacircleByte (srf, x, y, rx, gc->fg);
	else
	    aacircleColor (srf, x, y, rx, fetchcolor (gc->fg));
    } else {
	if (ttk_screen->bpp == 2)
	    aaellipseByte (srf, x, y, rx, ry, gc->fg);
	else
	    aaellipseColor (srf, x, y, rx, ry, fetchcolor (gc->fg));
    }
}

void ttk_fillellipse (ttk_surface srf, int x, int y, int rx, int ry, ttk_color col)
{
    if (rx == ry) {
	if (ttk_screen->bpp == 2)
	    filledCircleByte (srf, x, y, rx, col);
	else
	    filledCircleColor (srf, x, y, rx, fetchcolor (col));
    } else {
	if (ttk_screen->bpp == 2)
	    filledEllipseByte (srf, x, y, rx, ry, col);
	else
	    filledEllipseColor (srf, x, y, rx, ry, fetchcolor (col));
    }
}
void ttk_fillellipse_gc (ttk_surface srf, ttk_gc gc, int x, int y, int rx, int ry) 
{
    if (rx == ry) {
	if (ttk_screen->bpp == 2)
	    filledCircleByte (srf, x, y, rx, gc->fg);
	else
	    filledCircleColor (srf, x, y, rx, fetchcolor (gc->fg));
    } else {
	if (ttk_screen->bpp == 2)
	    filledEllipseByte (srf, x, y, rx, ry, gc->fg);
	else
	    filledEllipseColor (srf, x, y, rx, ry, fetchcolor (gc->fg));
    }
}


/**** Font stuff. Taken from Microwindows with minor modifications. ****/

static int read8 (FILE *fp, unsigned char *cp)
{
    int c;
    if ((c = getc (fp)) == EOF) return 0;
    *cp = (unsigned char) c;
    return 1;
}
static int read16 (FILE *fp, unsigned short *sp)
{
    int c;
    unsigned short s = 0;
    if ((c = getc (fp)) == EOF) return 0;
    s |= (c & 0xff);
    if ((c = getc (fp)) == EOF) return 0;
    s |= (c & 0xff) << 8;
    *sp = s;
    return 1;
}
static int read32 (FILE *fp, unsigned long *lp)
{
    int c;
    unsigned long l = 0;
    if ((c = getc (fp)) == EOF) return 0;
    l |= (c & 0xff);
    if ((c = getc (fp)) == EOF) return 0;
    l |= (c & 0xff) << 8;
    if ((c = getc (fp)) == EOF) return 0;
    l |= (c & 0xff) << 16;
    if ((c = getc (fp)) == EOF) return 0;
    l |= (c & 0xff) << 24;
    *lp = l;
    return 1;
}
static int readstr (FILE *fp, char *buf, int count)
{
    return fread (buf, 1, count, fp);
}
static int readstr_padded (FILE *fp, char *buf, int totlen)
{
    char *p;
    if (fread (buf, 1, totlen, fp) != totlen) return 0;

    p = &buf[totlen];
    *p-- = 0;
    while (*p == ' ' && p >= buf)
	*p-- = 0;

    return totlen;
}

/* Mostly copied from mwin */
#define FNT_VERSION "RB11"
static void load_fnt (Bitmap_Font *bf, const char *fname)
{
    FILE *fp = fopen (fname, "rb");
    int i;
    unsigned short maxwidth, height, ascent, pad;
    unsigned long firstchar, defaultchar, size;
    unsigned long nbits, noffset, nwidth;
    char version[5];
    char name[65];
    char copyright[257];

    if (!fp) {
	fprintf (stderr, "Couldn't find font file %s; exiting.\n", fname);
	ttk_quit();
	exit (1);
    }

    /* read magic and version #*/
    memset(version, 0, sizeof(version));
    if (readstr(fp, version, 4) != 4)
	goto errout;
    if (strcmp(version, FNT_VERSION) != 0)
	goto errout;
    
    /* internal font name*/
    if (readstr_padded(fp, name, 64) != 64)
	goto errout;
    bf->name = (char *)malloc(strlen(name)+1);
    if (!bf->name)
	goto errout;
    strcpy(bf->name, name);
    
    /* copyright, not currently stored*/
    if (readstr_padded(fp, copyright, 256) != 256)
	goto errout;
    
    /* font info*/
    if (!read16(fp, &maxwidth))
	goto errout;
    bf->maxwidth = maxwidth;
    if (!read16(fp, &height))
	goto errout;
    bf->height = height;
    if (!read16(fp, &ascent))
	goto errout;
    bf->ascent = ascent;
    if (!read16(fp, &pad))
	goto errout;
    if (!read32(fp, &firstchar))
	goto errout;
    bf->firstchar = firstchar;
    if (!read32(fp, &defaultchar))
	goto errout;
    bf->defaultchar = defaultchar;
    if (!read32(fp, &size))
	goto errout;
    bf->size = size;

    /* variable font data sizes*/
    /* # words of MWIMAGEBITS*/
    if (!read32(fp, &nbits))
	goto errout;
    bf->bits = (unsigned short *)malloc(nbits * sizeof(unsigned short));
    if (!bf->bits)
	goto errout;
    bf->bits_size = nbits;
    
    /* # longs of offset*/
    if (!read32(fp, &noffset))
	goto errout;
    if (noffset) {
	bf->offset = (unsigned long *)malloc(noffset * sizeof(unsigned long));
	if (!bf->offset)
	    goto errout;
    }
    
    /* # bytes of width*/
    if (!read32(fp, &nwidth))
	goto errout;
    if (nwidth) {
	bf->width = (unsigned char *)malloc(nwidth * sizeof(unsigned char));
	if (!bf->width)
	    goto errout;
    }
    
    /* variable font data*/
    for (i=0; i<nbits; ++i)
	if (!read16(fp, (unsigned short *)&bf->bits[i]))
	    goto errout;
    /* pad to longword boundary*/
    if (ftell(fp) & 02)
	if (!read16(fp, (unsigned short *)&bf->bits[i]))
	    goto errout;
    if (noffset)
	for (i=0; i<bf->size; ++i)
	    if (!read32(fp, (unsigned long *)&bf->offset[i]))
		goto errout;
    if (nwidth)
	for (i=0; i<bf->size; ++i)
	    if (!read8(fp, (unsigned char *)&bf->width[i]))
		goto errout;
    
    fclose (fp);
    return;	/* success!*/    

errout:
    fclose (fp);
    if (!bf)
	return;
    if (bf->name)
	free(bf->name);
    if (bf->bits)
	free((char *)bf->bits);
    if (bf->offset)
	free((char *)bf->offset);
    if (bf->width)
	free((char *)bf->width);
    fprintf (stderr, "Error in font file %s - possibly truncated.\n", fname);
    ttk_quit();
    exit (1);
}

/* These are maintained statically for ease FIXME*/
static struct toc_entry *toc;
static unsigned long toc_size;

/* Various definitions from the Free86 PCF code */
#define PCF_FILE_VERSION	(('p'<<24)|('c'<<16)|('f'<<8)|1)
#define PCF_PROPERTIES		(1 << 0)
#define PCF_ACCELERATORS	(1 << 1)
#define PCF_METRICS		(1 << 2)
#define PCF_BITMAPS		(1 << 3)
#define PCF_INK_METRICS		(1 << 4)
#define PCF_BDF_ENCODINGS	(1 << 5)
#define PCF_SWIDTHS		(1 << 6)
#define PCF_GLYPH_NAMES		(1 << 7)
#define PCF_BDF_ACCELERATORS	(1 << 8)
#define PCF_FORMAT_MASK		0xFFFFFF00
#define PCF_DEFAULT_FORMAT	0x00000000

#define PCF_GLYPH_PAD_MASK	(3<<0)
#define PCF_BYTE_MASK		(1<<2)
#define PCF_BIT_MASK		(1<<3)
#define PCF_SCAN_UNIT_MASK	(3<<4)
#define GLYPHPADOPTIONS		4

#define PCF_LSB_FIRST		0
#define PCF_MSB_FIRST		1

#if SDL_BYTE_ORDER != SDL_BIG_ENDIAN

/* little endian - no action required */
# define wswap(x)       (x)
# define dwswap(x)      (x)

#else
/** Convert little-endian 16-bit number to the host CPU format. */
# define wswap(x)       ((((x) << 8) & 0xff00) | (((x) >> 8) & 0x00ff))
/** Convert little-endian 32-bit number to the host CPU format. */
# define dwswap(x)      ((((x) << 24) & 0xff000000L) | \
                         (((x) <<  8) & 0x00ff0000L) | \
                         (((x) >>  8) & 0x0000ff00L) | \
                         (((x) >> 24) & 0x000000ffL) )
#endif

/* A few structures that define the various fields within the file */
struct toc_entry {
	int type;
	int format;
	int size;
	int offset;
};

struct prop_entry {
	unsigned int name;
	unsigned char is_string;
	unsigned int value;
};

struct string_table {
	unsigned char *name;
	unsigned char *value;
};

struct metric_entry {
	short leftBearing;
	short rightBearing;
	short width;
	short ascent;
	short descent;
	short attributes;
};

struct encoding_entry {
	unsigned short min_byte2;	/* min_char or min_byte2 */
	unsigned short max_byte2;	/* max_char or max_byte2 */
	unsigned short min_byte1;	/* min_byte1 (hi order) */
	unsigned short max_byte1;	/* max_byte1 (hi order) */
	unsigned short defaultchar;
	unsigned long count;		/* count of map entries */
	unsigned short *map;		/* font index -> glyph index */
};

/* This is used to quickly reverse the bits in a field */
static unsigned char _reverse_byte[0x100] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

/*
 *	Invert bit order within each BYTE of an array.
 */
static void
bit_order_invert(unsigned char *buf, int nbytes)
{
	for (; --nbytes >= 0; buf++)
		*buf = _reverse_byte[*buf];
}

/*
 *	Invert byte order within each 16-bits of an array.
 */
static void
two_byte_swap(unsigned char *buf, int nbytes)
{
	unsigned char c;

	for (; nbytes > 0; nbytes -= 2, buf += 2) {
		c = buf[0];
		buf[0] = buf[1];
		buf[1] = c;
	}
}

/*
 *	Invert byte order within each 32-bits of an array.
 */
static void
four_byte_swap(unsigned char *buf, int nbytes)
{
	unsigned char c;

	for (; nbytes > 0; nbytes -= 4, buf += 4) {
		c = buf[0];
		buf[0] = buf[3];
		buf[3] = c;
		c = buf[1];
		buf[1] = buf[2];
		buf[2] = c;
	}
}

#define FILEP FILE*
#define FREAD(a,b,c) fread(b,1,c,a)

/* read an 8 bit byte*/
static unsigned short
readINT8(FILEP file)
{
	unsigned char b;

	FREAD(file, &b, sizeof(b));
	return b;
}

/* read a 16-bit integer LSB16 format*/
static unsigned short
readLSB16(FILEP file)
{
	unsigned short s;

	FREAD(file, &s, sizeof(s));
	return wswap(s);
}

/* read a 32-bit integer LSB32 format*/
static unsigned long
readLSB32(FILEP file)
{
	unsigned long n;

	FREAD(file, &n, sizeof(n));
	return dwswap(n);
}

/* Get the offset of the given field */
static int
pcf_get_offset(int item)
{
	int i;

	for (i = 0; i < toc_size; i++)
		if (item == toc[i].type)
			return toc[i].offset;
	return -1;
}

/* Read the actual bitmaps into memory */
static int
pcf_readbitmaps(FILE *file, unsigned char **bits, int *bits_size,
	unsigned long **offsets)
{
	long offset;
	unsigned long format;
	unsigned long num_glyphs;
	unsigned long pad;
	unsigned int i;
	int endian;
	unsigned long *o;
	unsigned char *b;
	unsigned long bmsize[GLYPHPADOPTIONS];

	if ((offset = pcf_get_offset(PCF_BITMAPS)) == -1)
		return -1;
	fseek (file, offset, SEEK_SET);

	format = readLSB32(file);
	endian = (format & PCF_BIT_MASK)? PCF_LSB_FIRST: PCF_MSB_FIRST;

	num_glyphs = readLSB32(file);

	o = *offsets = (unsigned long *)malloc(num_glyphs * sizeof(unsigned long));
	for (i=0; i < num_glyphs; ++i)
		o[i] = readLSB32(file);

	for (i=0; i < GLYPHPADOPTIONS; ++i)
		bmsize[i] = readLSB32(file);

	pad = format & PCF_GLYPH_PAD_MASK;
	*bits_size = bmsize[pad]? bmsize[pad] : 1;

	/* alloc and read bitmap data*/
	b = *bits = (unsigned char *) malloc(*bits_size);
	FREAD(file, b, *bits_size);

	/* convert bitmaps*/
	bit_order_invert(b, *bits_size);
#if SDL_BYTE_ORDER == SDL_BIG_ENDIAN
	if (endian == PCF_LSB_FIRST)
		two_byte_swap(b, *bits_size);
#else
	if (endian == PCF_MSB_FIRST)
		two_byte_swap(b, *bits_size);
#endif
	return num_glyphs;
}

/* read character metric data*/
static int
pcf_readmetrics(FILE * file, struct metric_entry **metrics)
{
	long i, size, offset;
	unsigned long format;
	struct metric_entry *m;

	if ((offset = pcf_get_offset(PCF_METRICS)) == -1)
		return -1;
	fseek (file, offset, SEEK_SET);

	format = readLSB32(file);

	if ((format & PCF_FORMAT_MASK) == PCF_DEFAULT_FORMAT) {
		size = readLSB32(file);		/* 32 bits - Number of metrics*/

		m = *metrics = (struct metric_entry *) malloc(size *
			sizeof(struct metric_entry));

		for (i=0; i < size; i++) {
			m[i].leftBearing = readLSB16(file);
			m[i].rightBearing = readLSB16(file);
			m[i].width = readLSB16(file);
			m[i].ascent = readLSB16(file);
			m[i].descent = readLSB16(file);
			m[i].attributes = readLSB16(file);
		}
	} else {
		size = readLSB16(file);		/* 16 bits - Number of metrics*/

		m = *metrics = (struct metric_entry *) malloc(size *
			sizeof(struct metric_entry));

		for (i = 0; i < size; i++) {
			m[i].leftBearing = readINT8(file) - 0x80;
			m[i].rightBearing = readINT8(file) - 0x80;
			m[i].width = readINT8(file) - 0x80;
			m[i].ascent = readINT8(file) - 0x80;
			m[i].descent = readINT8(file) - 0x80;
		}
	}
	return size;
}

/* read encoding table*/
static int
pcf_read_encoding(FILE * file, struct encoding_entry **encoding)
{
	long offset, n;
	unsigned long format;
	struct encoding_entry *e;

	if ((offset = pcf_get_offset(PCF_BDF_ENCODINGS)) == -1)
		return -1;
	fseek (file, offset, SEEK_SET);

	format = readLSB32(file);

	e = *encoding = (struct encoding_entry *)
		malloc(sizeof(struct encoding_entry));
	e->min_byte2 = readLSB16(file);
	e->max_byte2 = readLSB16(file);
	e->min_byte1 = readLSB16(file);
	e->max_byte1 = readLSB16(file);
	e->defaultchar = readLSB16(file);
	e->count = (e->max_byte2 - e->min_byte2 + 1) *
		(e->max_byte1 - e->min_byte1 + 1);
	e->map = (unsigned short *) malloc(e->count * sizeof(unsigned short));

	for (n = 0; n < e->count; ++n) {
		e->map[n] = readLSB16(file);
		/*DPRINTF("ncode %x (%c) %x\n", n, n, e->map[n]);*/
	}
	return e->count;
}

static int
pcf_read_toc(FILE * file, struct toc_entry **toc, unsigned long *size)
{
	long i;
	unsigned long version;
	struct toc_entry *t;

	fseek (file, 0, SEEK_SET);

	/* Verify the version */
	version = readLSB32(file);
	if (version != PCF_FILE_VERSION)
		return -1;

	*size = readLSB32(file);
	t = *toc = (struct toc_entry *) calloc(sizeof(struct toc_entry), *size);
	if (!t)
		return -1;

	/* Read in the entire table of contents */
	for (i=0; i<*size; ++i) {
		t[i].type = readLSB32(file);
		t[i].format = readLSB32(file);
		t[i].size = readLSB32(file);
		t[i].offset = readLSB32(file);
	}

	return 0;
}

static void load_pcf (Bitmap_Font *bf, const char *fname)
{
    FILE *file = 0;
    int offset, i, count, bsize, bwidth, err = 0;
    struct metric_entry *metrics = 0;
    struct encoding_entry *encoding = 0;
    unsigned short *output;
    unsigned char *glyphs = 0;
    unsigned long *glyphs_offsets = 0;
    int max_width = 0, max_descent = 0, max_ascent = 0, max_height = 0;
    int glyph_count;
    unsigned long *goffset = 0;
    unsigned char *gwidth = 0;
    int uc16;
    
    file = fopen (fname, "rb");
    if (!file) {
	fprintf (stderr, "Could not find font file %s. Exiting.\n", fname);
    }

    /* Read the table of contents */
    if (pcf_read_toc(file, &toc, &toc_size) == -1) {
	err = -1;
	goto leave_func;
    }
    
    /* Now, read in the bitmaps */
    glyph_count = pcf_readbitmaps(file, &glyphs, &bsize, &glyphs_offsets);
    
    if (glyph_count == -1) {
	err = -1;
	goto leave_func;
    }
    
    if (pcf_read_encoding(file, &encoding) == -1) {
	err = -1;
	goto leave_func;
    }
    
    bf->firstchar = encoding->min_byte2 * (encoding->min_byte1 + 1);
    
    /* Read in the metrics */
    count = pcf_readmetrics(file, &metrics);
    
    /* Calculate various maximum values */
    for (i = 0; i < count; i++) {
	if (metrics[i].width > max_width)
	    max_width = metrics[i].width;
	if (metrics[i].ascent > max_ascent)
	    max_ascent = metrics[i].ascent;
	if (metrics[i].descent > max_descent)
	    max_descent = metrics[i].descent;
    }
    max_height = max_ascent + max_descent;
    
    bf->maxwidth = max_width;
    bf->height = max_height;
    bf->ascent = max_ascent;
    
    /* Allocate enough room to hold all of the bits and the offsets */
    bwidth = (max_width + 15) / 16;
    
    bf->bits = (unsigned short *) calloc((max_height *
				       (sizeof(unsigned short) * bwidth)), glyph_count);
    goffset = (unsigned long *) malloc(glyph_count *
				       sizeof(unsigned long));
    gwidth = (unsigned char *) malloc(glyph_count * sizeof(unsigned char));
    
    output = (unsigned short *) bf->bits;
    offset = 0;
    
    /* copy and convert from packed BDF format to MWCFONT format*/
    for (i = 0; i < glyph_count; i++) {
	int h, w;
	int y = max_height;
	unsigned long *ptr =
	    (unsigned long *) (glyphs + glyphs_offsets[i]);
	
	/* # words image width*/
	int lwidth = (metrics[i].width + 15) / 16;
	
	/* # words image width, corrected for bounding box problem*/
	int xwidth = (metrics[i].rightBearing - metrics[i].leftBearing + 15) / 16;
	
	gwidth[i] = (unsigned char) metrics[i].width;
	goffset[i] = offset;
	
	offset += (lwidth * max_height);
	
	for (h = 0; h < (max_ascent - metrics[i].ascent); h++) {
	    for (w = 0; w < lwidth; w++)
		*output++ = 0;
	    y--;
	}
	
	for (h = 0; h < (metrics[i].ascent + metrics[i].descent); h++) {
	    unsigned short *val = (unsigned short *) ptr;
	    int            bearing, carry_shift;
	    unsigned short carry = 0;
	    
	    /* leftBearing correction*/
	    bearing = metrics[i].leftBearing;
	    if (bearing < 0)	/* negative bearing not handled yet*/
		bearing = 0;
	    carry_shift = 16 - bearing;
	    
	    for (w = 0; w < lwidth; w++) {
		*output++ = (val[w] >> bearing) | carry;
		carry = val[w] << carry_shift;
	    }
	    ptr += (xwidth + 1) / 2;
	    y--;
	}
	
	for (; y > 0; y--)
	    for (w = 0; w < lwidth; w++)
		*output++ = 0;
    }
    
    /* reorder offsets and width according to encoding map */
    bf->offset = (unsigned long *) malloc(encoding->count *
					  sizeof(unsigned long));
    bf->width = (unsigned char *) malloc(encoding->count *
					 sizeof(unsigned char));
    for (i = 0; i < encoding->count; ++i) {
	unsigned short n = encoding->map[i];
	if (n == 0xffff)	/* map non-existent chars to default char */
	    n = encoding->map[encoding->defaultchar];
	((unsigned long *)bf->offset)[i] = goffset[n];
	((unsigned char *)bf->width)[i] = gwidth[n];
    }
    bf->size = encoding->count;
    

leave_func:
    if (goffset)
	free(goffset);
    if (gwidth)
	free(gwidth);
    if (encoding) {
	if (encoding->map)
	    free(encoding->map);
	free(encoding);
    }
    if (metrics)
	free(metrics);
    if (glyphs)
	free(glyphs);
    if (glyphs_offsets)
	free(glyphs_offsets);
    
    if (toc)
	free(toc);
    toc = 0;
    toc_size = 0;
    
    if (file)
	fclose (file);
    
    if (err == 0 && bf)
	return;

    fprintf (stderr, "Error loading PCF font file %s (err=%d); exiting\n", fname, err);
    ttk_quit();
    exit (1);
}

/**** End MW copied font handling stuff. ****/

static int h2d(char *s, int len)
{
	int i, ret = 0;
	for (i = 0; i < len; i++) {
		ret = ret << 4;
		if (*s > 0x40 && *s < 0x47) ret |= *s - 0x37;
		if (*s > 0x2f && *s < 0x3a) ret |= *s - 0x30;
		if (*s > 0x60 && *s < 0x67) ret |= *s - 0x57;
		s++;
	}
	return ret;
}

static void load_fff(Bitmap_Font *bf, const char *fname)
{
	char tmp[1024];
	FILE *ip;
	long n_bits = 0;
	unsigned short *bits = NULL;

	bf->firstchar = 0xffff;

	if ((ip = fopen(fname, "r")) == NULL) {
		perror(fname);	
		ttk_quit();
		exit(1);
	}

	fgets(tmp, 1024, ip);
	if (tmp[0] != '\\') {
		fputs("Invalid fff\n", stderr);
		ttk_quit();
		exit(1);
	}
	sscanf(tmp, "\\size %dx%d", &bf->maxwidth, (int *)&bf->height);
	while (fgets(tmp, 1024, ip)) {
		unsigned short index;
		int i, len = bf->height * 2;

		index = h2d(tmp, 4);
		if (index < bf->firstchar) bf->firstchar = index;
		if (n_bits < index + 1) {
			n_bits = (index + 0x100) & ~0xff;
			bits = realloc(bits, n_bits * len);
		}
		for (i = 0; i < (int)bf->height; i++)
			*((unsigned short *)bits + index*bf->height + i) =
                            h2d(tmp + 5 + (i * 2), 2) << 8;
		bf->size++;
	}
	bf->bits = bits;
	fclose(ip);
}

#ifndef NO_SF
static void draw_sf (ttk_font *f, ttk_surface srf, int x, int y, ttk_color col, const char *str)
{
    Uint8 r, g, b;
    SDL_GetRGB (col, srf->format, &r, &g, &b);

    if (!f->sf) return;

    if ((r+g+b) > 600)
	SFont_Write (srf, f->sfi, x, y, str);
    else
	SFont_Write (srf, f->sf, x, y, str);
}
static void draw16_sf (ttk_font *f, ttk_surface srf, int x, int y, ttk_color col, const uc16 *str) 
{
    int len = 0;
    const uc16 *sp = str;
    char *dst, *dp;
    while (*sp++) len++;
    dp = dst = malloc (len);
    sp = str;
    while (*sp) *dp++ = (*sp++ & 0xff);
    *dp = 0;
    draw_sf (f, srf, x, y, col, dst);
    free (dst);
}
static int width_sf (ttk_font *f, const char *str)
{
    if (f->sf)
	return SFont_TextWidth (f->sf, str);
    return 0;
}
static int width16_sf (ttk_font *f, const uc16 *str)
{
    int len = 0;
    const uc16 *sp = str;
    char *dst, *dp;
    int ret;
    
    while (*sp++) len++;
    dp = dst = malloc (len);
    sp = str;
    while (*sp) *dp++ = (*sp++ & 0xff);
    *dp = 0;
    
    ret = width_sf (f, dst);
    free (dst);
    return ret;
}
static void free_sf (ttk_font *f) 
{
    SFont_FreeFont (f->sf);
    SFont_FreeFont (f->sfi);
    f->sf = 0;
}
#endif

#ifndef NO_TF
#define TF_FUNC(name,type,func) \
static void name ## _tf (ttk_font *f, ttk_surface srf, int x, int y, ttk_color col, const type *str) \
{ \
    SDL_Surface *textsrf; \
    SDL_Rect dr; \
    \
    if (ttk_screen->bpp == 2) \
	textsrf = TTF_Render ## func ## _Solid(f->tf, str, col);
#if 0
    else if (ttk_last_gc->usebg)
	textsrf = TTF_Render ## func ## _Shaded(f->tf, str, col, ttk_last_gc->bg);
#endif
    else
	textsrf = TTF_Render ## func ## _Blended(f->tf, str. col);

    dr.x = x; dr.y = y;
    SDL_BlitSurface (textsrf, 0, srf, &dr);
}
TF_FUNC (draw, char, UTF8);
TF_FUNC (lat1, char, Text);
TF_FUNC (uc16, Uint16, UNICODE);

static int width_tf (ttk_font *f, const char *str)
{
    int w, h;
    TTF_SizeUTF8 (f->tf, str, &w, &h);
    return w;
}
static int widthL_tf (ttk_font *f, const char *str)
{
    int w, h;
    TTF_SizeText (f->tf, str, &w, &h);
    return w;
}
static int widthU_tf (ttk_font *f, const uc16 *str)
{
    int w, h;
    TTF_SizeUNICODE (f->tf, str, &w, &h);
    return w;
}
static void free_tf (ttk_font *f) 
{
    TTF_CloseFont (f->tf);
}
#endif

/**** More stuff from microwin. src/drivers/genfont.c ****/
static void gen_gettextsize(Bitmap_Font *bf, const void *text, int cc,
			    int *pwidth, int *pheight, int *pbase)
{
    const unsigned char *	str = text;
    unsigned int		c;
    int			width;
    
    if(bf->width == NULL)
	width = cc * bf->maxwidth;
    else {
	width = 0;
	while(--cc >= 0) {
	    c = *str++;
	    if(c >= bf->firstchar && c < bf->firstchar+bf->size)
		width += bf->width[c - bf->firstchar];
	}
    }
    *pwidth = width;
    *pheight = bf->height;
    *pbase = bf->ascent;
}

static void gen16_gettextsize (Bitmap_Font *bf, const unsigned short *str, int cc,
                               int *pwidth, int *pheight, int *pbase)
{
    unsigned int	c;
    int			width;
    
    if(bf->width == NULL)
	width = cc * bf->maxwidth;
    else {
	width = 0;
	while(--cc >= 0) {
	    c = *str++;
	    if(c >= bf->firstchar && c < bf->firstchar+bf->size)
		width += bf->width[c - bf->firstchar];
	}
    }
    *pwidth = width;
    *pheight = bf->height;
    *pbase = bf->ascent;
}

static void
gen_gettextbits(Bitmap_Font *bf, int ch, const unsigned short **retmap,
	int *pwidth, int *pheight, int *pbase)
{
    int 			count, width;
    const unsigned short *	bits;

    /* if char not in font, map to first character by default*/
    if(ch < bf->firstchar || ch >= bf->firstchar+bf->size)
	ch = bf->firstchar;

    ch -= bf->firstchar;

    /* get font bitmap depending on fixed pitch or not*/
    /* GB: automatically detect if offset is 16 or 32 bit */
    if( bf->offset ) {
	if( ((unsigned long*)bf->offset)[0] >= 0x00010000 )
	    bits = bf->bits + ((unsigned short*)bf->offset)[ch];
	else
	    bits = bf->bits + ((unsigned long*)bf->offset)[ch];
    } else
	bits = bf->bits + (bf->height * ch);
    
    width = bf->width ? bf->width[ch] : bf->maxwidth;
    count = ((width+15)/16) * bf->height; 

    *retmap = bits;

    /* return width depending on fixed pitch or not*/
    *pwidth = width; 
    *pheight = bf->height;
    *pbase = bf->ascent; 
}

/** src/engine/devdraw.c, modified for SDL **/
static void draw_bitmap (ttk_surface srf, int x, int y, int width, int height,
			 const unsigned short *imagebits, ttk_color color)
{
    int minx, maxx;
    unsigned short bitvalue = 0;
    int bitcount;
    int (*pixelfunc)(SDL_Surface *srf, Sint16 x, Sint16 y, Uint32 color);
    if (ttk_screen->bpp == 2)
	pixelfunc = fastPixelByte;
    else
	pixelfunc = fastPixelColor;

    minx = x;
    maxx = x + width - 1;
    bitcount = 0;
    while (height > 0) {
	if (bitcount <= 0) {
	    bitcount = 16;
	    bitvalue = *imagebits++;
	}
	if (bitvalue & (1<<15))
	    pixelfunc (srf, x, y, color);
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

void ttk_bitmap (ttk_surface srf, int x, int y, int w, int h, unsigned short *imagebits, ttk_color col)
{
    draw_bitmap (srf, x, y, w, h, imagebits, col);
}
void ttk_bitmap_gc (ttk_surface srf, ttk_gc gc, int x, int y, int w, int h, unsigned short *imagebits) 
{
    draw_bitmap (srf, x, y, w, h, imagebits, gc->fg);
}

/** src/engine/devfont.c **/
static void corefont_drawtext (Bitmap_Font *bf, ttk_surface srf, int x, int y,
			       const void *text, int cc, ttk_color col)
{
    const unsigned char *str = text;
    int width, height, base, startx, starty;
    const unsigned short *bitmap;
    int bgstate;
    int clip;
    
    startx = x;
    starty = y;
    bgstate = 0; //xxx

    while (--cc >= 0 && x < srf->w) {
	int ch = *str++;
	gen_gettextbits (bf, ch, &bitmap, &width, &height, &base);
	draw_bitmap (srf, x, y, width, height, bitmap, col);
	x += width;
    }
}

static void corefont16_drawtext (Bitmap_Font *bf, ttk_surface srf, int x, int y,
                                 const unsigned short *str, int cc, ttk_color col) 
{
    int width, height, base, startx, starty;
    const unsigned short *bitmap;
    int bgstate, clip;

    startx = x;
    starty = y;
    bgstate = 0; //xxx

    while (--cc >= 0 && x < srf->w) {
        int ch = *str++;
        gen_gettextbits (bf, ch, &bitmap, &width, &height, &base);
        draw_bitmap (srf, x, y, width, height, bitmap, col);
        x += width;
    }
}

/**** end mwin copied stuff ****/

static int IsASCII (const char *str) 
{
    const char *p = str;
    while (*p) {
        if (*p & 0x80) return 0;
        p++;
    }
    return 1;
}

static int ConvertUTF8 (const unsigned char *src, unsigned short *dst)
{
    const unsigned char *sp = src;
    unsigned short *dp = dst;
    int len = 0;
    while (*sp) {
        *dp = 0;
        if (*sp < 0x80) *dp = *sp++;
        else if (*sp >= 0xC0 && *sp < 0xE0) {
            *dp |= (*sp++ - 0xC0) << 6;  if (!*sp) goto err;
            *dp |= (*sp++ - 0x80);
        }
        else if (*sp >= 0xE0 && *sp < 0xF0) {
            *dp |= (*sp++ - 0xE0) << 12; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 6;  if (!*sp) goto err;
            *dp |= (*sp++ - 0x80);
        }
        else if (*sp >= 0xF0 && *sp < 0xF8) {
            *dp |= (*sp++ - 0xF0) << 18; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 12; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 6;  if (!*sp) goto err;
            *dp |= (*sp++ - 0x80);
        }
        else if (*sp >= 0xF8 && *sp < 0xFC) {
            *dp |= (*sp++ - 0xF8) << 24; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 18; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 12; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 6;  if (!*sp) goto err;
            *dp |= (*sp++ - 0x80);
        }
        else if (*sp >= 0xFC && *sp < 0xFE) {
            *dp |= (*sp++ - 0xF8) << 30; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 24; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 18; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 12; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 6;  if (!*sp) goto err;
            *dp |= (*sp++ - 0x80);
        }
        else goto err;
        
        dp++;
        len++;
        continue;
        
    err:
        *dp++ = '?';
        sp++;
        len++;
    }
    *dp = 0;
    return len;
}

static void draw_bf (ttk_font *f, ttk_surface srf, int x, int y, ttk_color col, const char *str)
{
    const void *text = (const void *)str;
    int cc = strlen (str);
    if (!f->bf) return;

    if (IsASCII (str))
        corefont_drawtext (f->bf, srf, x, y, text, cc, col);
    else {
        unsigned short *buf = malloc (strlen (str) * 2);
        int len = ConvertUTF8 ((unsigned char *)str, buf);
        corefont16_drawtext (f->bf, srf, x, y, buf, len, col);
        free (buf);
    }
}

static void lat1_bf (ttk_font *f, ttk_surface srf, int x, int y, ttk_color col, const char *str)
{
    const void *text = (const void *)str;
    int cc = strlen (str);
    if (!f->bf) return;

    corefont_drawtext (f->bf, srf, x, y, text, strlen (str), col);
}

static void uc16_bf (ttk_font *f, ttk_surface srf, int x, int y, ttk_color col, const uc16 *str) 
{
    int cc = 0;
    const uc16 *p = str;
    if (!f->bf) return;

    while (*p++) cc++;
    corefont16_drawtext (f->bf, srf, x, y, str, cc, col);
}

static int width_bf (ttk_font *f, const char *str)
{
    int width, height, base;
    if (!f->bf) return -1;

    if (IsASCII (str))
        gen_gettextsize (f->bf, str, strlen (str), &width, &height, &base);
    else {
        unsigned short *buf = malloc (strlen (str) * 2);
        int len = ConvertUTF8 ((unsigned char *)str, buf);
        gen16_gettextsize (f->bf, buf, len, &width, &height, &base);
        free (buf);
    }
    return width;
}
static int widthL_bf (ttk_font *f, const char *str) 
{
    int width, height, base;
    if (!f->bf) return -1;
    gen_gettextsize (f->bf, str, strlen (str), &width, &height, &base);
    return width;
}
static int widthU_bf (ttk_font *f, const uc16 *str) 
{
    int width, height, base;
    int cc = 0;
    const uc16 *p = str;
    if (!f->bf) return -1;

    while (*p++) cc++;
    gen16_gettextsize (f->bf, str, cc, &width, &height, &base);
    return width;
}

static void free_bf (ttk_font *f) 
{
    if (f->bf->name)   free (f->bf->name);
    if (f->bf->bits)   free ((char *)f->bf->bits);
    if (f->bf->offset) free ((char *)f->bf->offset);
    if (f->bf->width)  free ((char *)f->bf->width);
    f->bf = 0;
}

void ttk_text (ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col, const char *str)
{
    fnt.draw (&fnt, srf, x, y + fnt.ofs, col, str);
}
void ttk_text_lat1 (ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col, const char *str)
{
    fnt.draw_lat1 (&fnt, srf, x, y + fnt.ofs, col, str);
}
void ttk_text_uc16 (ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col, const uc16 *str)
{
    fnt.draw_uc16 (&fnt, srf, x, y + fnt.ofs, col, str);
}
int ttk_text_width (ttk_font fnt, const char *str)
{
    if (!str) return 0;
    return fnt.width (&fnt, str);
}
int ttk_text_width_lat1 (ttk_font fnt, const char *str)
{
    if (!str) return 0;
    return fnt.width_lat1 (&fnt, str);
}
int ttk_text_width_uc16 (ttk_font fnt, const uc16 *str)
{
    if (!str) return 0;
    return fnt.width_uc16 (&fnt, str);
}
int ttk_text_width_gc (ttk_gc gc, const char *str) 
{
    return gc->font.width (&gc->font, str);
}
int ttk_text_height (ttk_font fnt) 
{
    return fnt.height;
}
int ttk_text_height_gc (ttk_gc gc)
{
    return gc->font.height;
}
void ttk_load_font (ttk_fontinfo *fi, const char *fnbase, int size)
{
    char *fname = alloca (strlen (fnbase) + 7);	/* +7: - i . p n g \0 */
    struct stat st;

    if (ttk_screen->bpp != 2) {
#ifndef NO_SF
	strcpy (fname, fnbase);
	strcat (fname, ".png");
	if (stat (fname, &st) >= 0) {
	    fi->f.sf = SFont_InitFont (IMG_Load (fname));
	    strcpy (fname, fnbase);
	    strcat (fname, "-i.png");
	    fi->f.sfi = SFont_InitFont (IMG_Load (fname));
	    fi->f.draw = fi->f.draw_lat1 = draw_sf; fi->f.draw_uc16 = draw16_sf;
	    fi->f.width = fi->f.width_lat1 = width_sf; fi->f.width_uc16 = width16_sf;
	    fi->f.free = free_sf;
	    fi->f.height = SFont_TextHeight (fi->f.sf);
	    return;
	}
#endif

#ifndef NO_TF
	strcpy (fname, fnbase);
	strcat (fname, ".ttf");
	if (stat (fname, &st) >= 0) {
	    fi->f.tf = TTF_OpenFont (fname, size);
	    fi->f.draw = draw_tf;
            fi->f.draw_lat1 = lat1_tf;
            fi->f.draw_uc16 = uc16_tf;
	    fi->f.width = width_tf;
            fi->f.width_lat1 = widthL_tf;
            fi->f.width_uc16 = widthU_tf;
	    fi->f.free = free_tf;
	    fi->f.height = TTF_FontHeight (fi->f.tf);
	    return;
	}
#endif
    }

    strcpy (fname, fnbase);
    strcat (fname, ".fnt");
    if (stat (fname, &st) >= 0) {
	fi->f.bf = calloc (1, sizeof(Bitmap_Font));
	load_fnt (fi->f.bf, fname);
	fi->f.draw = draw_bf;
        fi->f.draw_lat1 = lat1_bf;
        fi->f.draw_uc16 = uc16_bf;
	fi->f.width = width_bf;
        fi->f.width_lat1 = widthL_bf;
        fi->f.width_uc16 = widthU_bf;
	fi->f.free = free_bf;
	fi->f.height = fi->f.bf->height;
	return;
    }
    
    strcpy (fname, fnbase);
    strcat (fname, ".pcf");
    if (stat (fname, &st) >= 0) {
	fi->f.bf = calloc (1, sizeof(Bitmap_Font));
	load_pcf (fi->f.bf, fname);
	fi->f.draw = draw_bf;
        fi->f.draw_lat1 = lat1_bf;
        fi->f.draw_uc16 = uc16_bf;
	fi->f.width = width_bf;
        fi->f.width_lat1 = widthL_bf;
        fi->f.width_uc16 = widthU_bf;
	fi->f.free = free_bf;
	fi->f.height = fi->f.bf->height;
	return;
    }

    strcpy (fname, fnbase);
    strcat (fname, ".fff");
    if (stat (fname, &st) >= 0) {
	fi->f.bf = calloc (1, sizeof(Bitmap_Font));
	load_fff (fi->f.bf, fname);
	fi->f.draw = draw_bf;
	fi->f.draw_lat1 = lat1_bf;
	fi->f.draw_uc16 = uc16_bf;
	fi->f.width = width_bf;
	fi->f.width_lat1 = widthL_bf;
	fi->f.width_uc16 = widthU_bf;
	fi->f.free = free_bf;
	fi->f.height = fi->f.bf->height;
	return;
    }

    fi->good = 0;
    return;
}
void ttk_unload_font (ttk_fontinfo *fi) 
{
    fi->f.free (&fi->f);
    fi->loaded = 0;
    fi->good = 0;
}

void ttk_text_gc (ttk_surface srf, ttk_gc gc, int x, int y, const char *str)
{
    if (gc->usebg) {
	ttk_fillrect (srf, x, y, x + ttk_text_width_gc (gc, str), y + ttk_text_height_gc (gc), gc->bg);
    }
    gc->font.draw (&gc->font, srf, x, y, gc->fg, str);
}

ttk_surface ttk_load_image (const char *path)
{
    return IMG_Load (path);
}
void ttk_free_image (ttk_surface img)
{
    SDL_FreeSurface (img);
}
void ttk_blit_image (ttk_surface src, ttk_surface dst, int dx, int dy)
{
    SDL_Rect dr;
    dr.x = dx;
    dr.y = dy;
    SDL_BlitSurface (src, 0, dst, &dr);
}
void ttk_blit_image_ex (ttk_surface src, int sx, int sy, int sw, int sh,
			       ttk_surface dst, int dx, int dy) 
{
    SDL_Rect sr, dr;
    sr.x = sx; sr.y = sy; sr.w = sw; sr.h = sh;
    dr.x = dx; dr.y = dy;
    SDL_BlitSurface (src, &sr, dst, &dr);
}

ttk_surface ttk_new_surface (int w, int h, int bpp)
{
    SDL_Surface *ret;
    int Rmask, Gmask, Bmask, Amask = 0, BPP;

    BPP = bpp;
    if (BPP == 2) BPP = 8;
    
    if (BPP == 8) {
	Rmask = Gmask = Bmask = 0;
    } else if (BPP == 16) {
        Rmask = ttk_screen->srf->format->Rmask;
        Gmask = ttk_screen->srf->format->Gmask;
        Bmask = ttk_screen->srf->format->Bmask;
    } else if (BPP == 24) {
	BPP = 32;
	Rmask = 0x0000FF;
	Gmask = 0x00FF00;
	Bmask = 0xFF0000;
    } else {
	BPP = 32;
	Rmask = 0x000000FF;
	Gmask = 0x0000FF00;
	Bmask = 0x00FF0000;
	Amask = 0xFF000000;
    }

    ret = SDL_CreateRGBSurface (SDL_SWSURFACE,
				w, h, BPP,
				Rmask, Gmask, Bmask, Amask);
    if (bpp == 2) {
	palettize (ret);
	SDL_FillRect (ret, 0, 0); // 0 = white
    } else {
	SDL_FillRect (ret, 0, ttk_makecol_ex (WHITE, ret));
    }
    return ret;
}

ttk_surface ttk_scale_surface (ttk_surface srf, float factor) 
{
    if (ttk_screen->bpp >= 16) {
	return zoomSurface (srf, factor, factor, 1);
    } else {
	return zoomSurface (srf, factor, factor, 0);
    }
}

void ttk_surface_get_dimen (ttk_surface srf, int *w, int *h) 
{
    if (w) *w = srf->w;
    if (h) *h = srf->h;
}

void ttk_free_surface (ttk_surface srf) 
{
    SDL_FreeSurface (srf);
}
#endif

