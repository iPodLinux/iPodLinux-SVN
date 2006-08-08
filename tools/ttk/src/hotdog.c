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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <termios.h>
#if !defined(SDL) && !defined(MWIN)
#include "hotdog.h"

static void *framebuffer;
#ifndef IPOD
#include "SDL.h"
static SDL_Surface *SDLscreen;
#else
static int ikbfd;
#endif

extern ttk_screeninfo *ttk_screen;
hd_engine *ttk_engine;

#ifdef IPOD

// Update the LCD.
static void update (hd_engine *e, int x, int y, int w, int h) 
{
    HD_LCD_Update (framebuffer, 0, 0, ttk_screen->w, ttk_screen->h);
}

static struct termios stored_settings;

// Set keyboard mode appropriately - ~(ICANON|ECHO|ISIG) etc.
void set_keypress(void)
{
	struct termios new_settings;
	tcgetattr(0,&stored_settings);
	new_settings = stored_settings;
	new_settings.c_lflag &= ~(ICANON | ECHO | ISIG);
	new_settings.c_iflag &= ~(ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON);
	new_settings.c_cc[VTIME] = 0;
	new_settings.c_cc[VMIN] = 1;
	tcsetattr(0,TCSANOW,&new_settings);
}

// Set the keyboard mode back to what it should be.
void reset_keypress(void)
{
	tcsetattr(0,TCSANOW,&stored_settings);
}

#else /* for desktop: */

// Update the screen from a 2-bpp framebuffer.
static void update2 (hd_engine *e, int x, int y, int w, int h) /* translates from 2bpp */
{
    static Uint16 colors[] = {
        /* White   */ 0xFFFF,
        /* Lt.Grey */ 0xA514,
        /* Dk.Grey */ 0x528A,
        /* Black   */ 0x0000
    };
    
    SDL_LockSurface (SDLscreen);

    int fbpitch = (ttk_screen->w + 3) / 4;
    Uint8 *p = (Uint8 *)framebuffer + (fbpitch * y);
    Uint16 *q = (Uint16 *)((Uint8 *)SDLscreen->pixels + (SDLscreen->pitch * y));
    int line;
    for (line = y; line < y + h; line++) {
        int pi = x / 4, pp = x % 4; // pi is the byte, pp is the pixel within the byte, in the FB
        int qi;
        for (qi = x; qi < x + w; qi++) {
            q[qi] = colors[(p[pi] >> 2*pp) & 3];
            pp++; pi += pp/4; pp %= 4;
        }
        p += fbpitch;
        q = (Uint16 *)((Uint8 *)q + SDLscreen->pitch);
    }

    SDL_UnlockSurface (SDLscreen);

    SDL_UpdateRect (SDLscreen, x, y, w, h);
}

// Update the screen from a 16bpp framebuffer.
static void update16 (hd_engine *e, int x, int y, int w, int h)
{
    SDL_UpdateRect (SDLscreen, x, y, w, h);
}

#endif

void ttk_gfx_init() 
{
#ifndef IPOD /* desktop: */
    if (SDL_Init (SDL_INIT_VIDEO) < 0) {
        fprintf (stderr, "Unable to init SDL: %s\n", SDL_GetError());
        exit (1);
    }
    atexit (SDL_Quit);
    
    SDLscreen = SDL_SetVideoMode (ttk_screen->w, ttk_screen->h, 16, SDL_SWSURFACE);
    if (!SDLscreen) {
        fprintf (stderr, "Unable to init SDL video: %s\n", SDL_GetError());
        exit (1);
    }

    SDL_EnableKeyRepeat (100, 50);
    SDL_EnableUNICODE (1);

    if (ttk_screen->bpp == 2) {
        framebuffer = malloc (ttk_screen->w * (ttk_screen->h + 3 / 4));
    } else {
        framebuffer = SDLscreen->pixels;
    }

    ttk_engine = HD_Initialize (ttk_screen->w, ttk_screen->h, ttk_screen->bpp, framebuffer,
                                (ttk_screen->bpp == 2)? update2 : update16);
#else /* iPod: */
    HD_LCD_Init();
    if (ttk_screen->bpp == 2)
        framebuffer = malloc (ttk_screen->w * (ttk_screen->h + 3 / 4));
    else
        framebuffer = malloc (ttk_screen->w * ttk_screen->h * 2);
    
    ttk_engine = HD_Initialize (ttk_screen->w, ttk_screen->h, ttk_screen->bpp, framebuffer, update);

    set_keypress();

    ikbfd = open ("/dev/misc/wheel", O_RDONLY | O_NONBLOCK);
#endif

    ttk_screen->srf = ttk_engine->buffer;
}

void ttk_gfx_quit() 
{
#ifdef IPOD
    close (ikbfd);
    reset_keypress();
    HD_LCD_Quit();
#else /* desktop: */
    SDL_Quit();
#endif
}

void ttk_gfx_update (ttk_surface srf) 
{
    HD_Render (ttk_engine);
}

//////////////// Events ///////////////////

#ifdef IPOD

// Defines for /dev/wheel:
#define EV_SCROLL        0x1000
#define EV_TAP           0x2000
#define EV_PRESS         0x4000
#define EV_RELEASE       0x8000
#define EV_TOUCH         0x0100
#define EV_LIFT          0x0200
#define EV_MASK          0xff00

#define BTN_ACTION       0x0001
#define BTN_NEXT         0x0002
#define BTN_PREVIOUS     0x0004
#define BTN_PLAY         0x0008
#define BTN_MENU         0x0010
#define BTN_HOLD         0x0020
#define BTN_MASK         0x00ff

#define SCROLL_LEFT      0x0080
#define SCROLL_RIGHT     0x0000
#define SCROLL_MASK      0x007f
#define TAP_MASK         0x007f
#define TOUCH_MASK       0x007f
#define LIFT_MASK        0x007f

int ttk_get_event (int *arg)
{
    // IKB can report multiple button events at once, we only want one at a time, so save the rest.
    static int pending_presses, pending_releases;
    if (pending_presses || pending_releases) {
    handle_button: ; // Control is transferred here after receiving a button event, too.
        char *buttons = "\nfwdmh"; // button 0 is (1 << 0) = 1 in the mask and buttons[0] = '\n' in TTK, etc.
        int i;
        for (i = 0; i < 6; i++) {
            if (pending_presses & (1 << i)) {
                pending_presses &= ~(1 << i);
                *arg = buttons[i];
                return TTK_BUTTON_DOWN;
            }
            if (pending_releases & (1 << i)) {
                pending_releases &= ~(1 << i);
                *arg = buttons[i];
                return TTK_BUTTON_UP;
            }
        }
    }

    unsigned short ev;
    if (read (ikbfd, &ev, 2) < 2) return TTK_NO_EVENT;

    switch (ev & EV_MASK) {
    case EV_SCROLL:
        *arg = ev & SCROLL_MASK;
        if (ev & SCROLL_LEFT) *arg = -*arg;
        return TTK_SCROLL;
    case EV_TAP:
        *arg = ev & TAP_MASK;
        return TTK_TAP;
    case EV_PRESS:
        pending_presses |= ev & BTN_MASK;
        goto handle_button;
    case EV_RELEASE:
        pending_releases |= ev & BTN_MASK;
        goto handle_button;
    case EV_TOUCH:
        *arg = ev & TOUCH_MASK;
        return TTK_TOUCH;
    case EV_LIFT:
        *arg = ev & LIFT_MASK;
        return TTK_LIFT;
    default:
        return TTK_NO_EVENT;
    }
}

int ttk_get_rawevent (int *arg) 
{
    int ev;
    while ((ev = ttk_get_event (arg)) != TTK_NO_EVENT) {
        if (ev == TTK_BUTTON_UP || ev == TTK_BUTTON_DOWN) return ev;
    }
    return TTK_NO_EVENT;
}
#else /* desktop: (same as code in sdl.c) */
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
	    return tev;
	}
    }
    return tev;
}
#endif

int ttk_getticks()
{
#ifdef IPOD
    static int ipod_rtc = 0;
    if (!ipod_rtc) {
        if (ttk_get_podversion() & TTK_POD_PP5002)
            ipod_rtc = 0xcf001110;
        else
            ipod_rtc = 0x60005010;
    }
    return inl(ipod_rtc) / 1000;
#else
    return SDL_GetTicks();
#endif
}

void ttk_delay (int ms) 
{
    poll (0, 0, ms); // nice, blocking, yielding delay for `ms' msec.
}

ttk_color ttk_makecol (int r, int g, int b) 
{
    if (r < 0) return (uint32)b;
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

ttk_color ttk_makecol_ex (int r, int g, int b, ttk_surface srf) 
{
    if (r < 0) return (uint32)b;
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

void ttk_unmakecol (ttk_color col, int *r, int *g, int *b)
{
    *r = (col >> 16) & 0xff;
    *g = (col >> 8) & 0xff;
    *b = col & 0xff;
}

void ttk_unmakecol_ex (ttk_color col, int *r, int *g, int *b, ttk_surface srf)
{
    *r = (col >> 16) & 0xff;
    *g = (col >> 8) & 0xff;
    *b = col & 0xff;
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
void ttk_free_gc (ttk_gc gc)
{
    free (gc);
}

ttk_color ttk_gc_get_foreground (ttk_gc gc) { return gc->fg; }
ttk_color ttk_gc_get_background (ttk_gc gc) { return gc->bg; }
ttk_font ttk_gc_get_font (ttk_gc gc) { return gc->font; }
void ttk_gc_set_foreground (ttk_gc gc, ttk_color fgcol) { gc->fg = fgcol; }
void ttk_gc_set_background (ttk_gc gc, ttk_color bgcol) { gc->bg = bgcol; }
void ttk_gc_set_font (ttk_gc gc, ttk_font font) { gc->font = font; }
void ttk_gc_set_usebg (ttk_gc gc, int flag) { gc->usebg = flag; }
void ttk_gc_set_xormode (ttk_gc gc, int flag) { gc->xormode = flag; }

ttk_color ttk_getpixel (ttk_surface srf, int x, int y) 
{
    return HD_SRF_GETPIX (srf, x, y);
}

void ttk_pixel (ttk_surface srf, int x, int y, ttk_color col) 
{
    HD_SRF_SETPIX (srf, x, y, col);
}
void ttk_pixel_gc (ttk_surface srf, ttk_gc gc, int x, int y) 
{
    HD_SRF_SETPIX (srf, x, y, gc->fg);
}

void ttk_line (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col) 
{
    HD_Line (srf, x1, y1, x2, y2, col);
}
void ttk_line_gc (ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2) 
{
    HD_Line (srf, x1, y1, x2, y2, gc->fg);
}
void ttk_aaline (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col) 
{
    HD_AALine (srf, x1, y1, x2, y2, col);
}
void ttk_aaline_gc (ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2) 
{
    HD_AALine (srf, x1, y1, x2, y2, gc->fg);
}

void ttk_rect (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col) 
{
    HD_Rect (srf, x1, y1, x2, y2, col);
}
void ttk_rect_gc (ttk_surface srf, ttk_gc gc, int x, int y, int w, int h) 
{
    HD_Rect (srf, x, y, x + w, y + h, gc->fg);
}
void ttk_fillrect (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col) 
{
    HD_FillRect (srf, x1, y1, x2, y2, col);
}
void ttk_fillrect_gc (ttk_surface srf, ttk_gc gc, int x, int y, int w, int h) 
{
    HD_FillRect (srf, x, y, x + w, y + h, gc->fg);
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

static void poly_func (void (*fn)(hd_surface, hd_point*, int, uint32),
                       ttk_surface srf, int nv, short *vx, short *vy, ttk_color col) 
{
    hd_point *v = malloc (nv * sizeof(hd_point));
    int i;
    for (i = 0; i < nv; i++) {
        v[0].x = vx[0];
        v[0].y = vy[0];
    }
    fn (srf, v, nv, col);
    free (v);
}

void ttk_poly (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col) 
{
    poly_func (HD_Poly, srf, nv, vx, vy, col);
}
void ttk_poly_pt (ttk_surface srf, ttk_point *v, int n, ttk_color col) 
{
    HD_Poly (srf, v, n, col);
}
void ttk_poly_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v) 
{
    HD_Poly (srf, v, n, gc->fg);
}
void ttk_aapoly (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col) 
{
    poly_func (HD_AAPoly, srf, nv, vx, vy, col);
}
void ttk_aapoly_pt (ttk_surface srf, ttk_point *v, int n, ttk_color col) 
{
    HD_AAPoly (srf, v, n, col);
}
void ttk_aapoly_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v) 
{
    HD_AAPoly (srf, v, n, gc->fg);
}
void ttk_fillpoly (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col) 
{
    fprintf (stderr, "fillpoly not implemented\n"); //poly_func (HD_FillPoly, srf, nv, vx, vy, col);
}
void ttk_fillpoly_pt (ttk_surface srf, ttk_point *v, int n, ttk_color col) 
{
    fprintf (stderr, "fillpoly not implemented\n"); //HD_FillPoly (srf, v, n, col);
}
void ttk_fillpoly_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v) 
{
    fprintf (stderr, "fillpoly not implemented\n"); //HD_FillPoly (srf, v, n, gc->fg);
}
void ttk_polyline (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col) 
{
    poly_func (HD_Lines, srf, nv, vx, vy, col);
}
void ttk_polyline_pt (ttk_surface srf, ttk_point *v, int n, ttk_color col) 
{
    HD_Lines (srf, v, n, col);
}
void ttk_polyline_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v) 
{
    HD_Lines (srf, v, n, gc->fg);
}
void ttk_aapolyline (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col) 
{
    poly_func (HD_AALines, srf, nv, vx, vy, col);
}
void ttk_aapolyline_pt (ttk_surface srf, ttk_point *v, int n, ttk_color col) 
{
    HD_AALines (srf, v, n, col);
}
void ttk_aapolyline_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v) 
{
    HD_AALines (srf, v, n, gc->fg);
}

void ttk_ellipse (ttk_surface srf, int x, int y, int rx, int ry, ttk_color col) 
{
    if (rx == ry) HD_Circle (srf, x, y, rx, col);
    else HD_Ellipse (srf, x, y, rx, ry, col);
}
void ttk_ellipse_gc (ttk_surface srf, ttk_gc gc, int x, int y, int rx, int ry) 
{
    if (rx == ry) HD_Circle (srf, x, y, rx, gc->fg);
    else HD_Ellipse (srf, x, y, rx, ry, gc->fg);
}
void ttk_aaellipse (ttk_surface srf, int x, int y, int rx, int ry, ttk_color col) 
{
    if (rx == ry) HD_AACircle (srf, x, y, rx, col);
    else HD_AAEllipse (srf, x, y, rx, ry, col);
}
void ttk_aaellipse_gc (ttk_surface srf, ttk_gc gc, int x, int y, int rx, int ry) 
{
    if (rx == ry) HD_AACircle (srf, x, y, rx, gc->fg);
    else HD_AAEllipse (srf, x, y, rx, ry, gc->fg);
}
void ttk_fillellipse (ttk_surface srf, int x, int y, int rx, int ry, ttk_color col) 
{
    if (rx == ry) HD_FillCircle (srf, x, y, rx, col);
    else HD_FillEllipse (srf, x, y, rx, ry, col);
}
void ttk_fillellipse_gc (ttk_surface srf, ttk_gc gc, int x, int y, int rx, int ry) 
{
    if (rx == ry) HD_FillCircle (srf, x, y, rx, gc->fg);
    else HD_FillEllipse (srf, x, y, rx, ry, gc->fg);
}
void ttk_aafillellipse (ttk_surface srf, int x, int y, int rx, int ry, ttk_color col) 
{
    if (rx == ry) HD_AAFillCircle (srf, x, y, rx, col);
    else HD_AAFillEllipse (srf, x, y, rx, ry, col);
}
void ttk_aafillellipse_gc (ttk_surface srf, ttk_gc gc, int x, int y, int rx, int ry) 
{
    if (rx == ry) HD_AAFillCircle (srf, x, y, rx, gc->fg);
    else HD_AAFillEllipse (srf, x, y, rx, ry, gc->fg);
}

void ttk_bezier (ttk_surface srf, int x1, int y1, int x2, int y2,
                 int x3, int y3, int x4, int y4, int level, ttk_color col) 
{
    hd_point v[4];
    v[0].x = x1; v[0].y = y1;
    v[1].x = x2; v[1].y = y2;
    v[2].x = x3; v[2].y = y3;
    v[3].x = x4; v[3].y = y4;
    HD_Bezier (srf, 3, v, level, col);
}
void ttk_bezier_gc (ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2,
                    int x3, int y3, int x4, int y4, int level) 
{
    ttk_bezier (srf, x1, y1, x2, y2, x3, y3, x4, y4, level, gc->fg);
}
void ttk_aabezier (ttk_surface srf, int x1, int y1, int x2, int y2,
                   int x3, int y3, int x4, int y4, int level, ttk_color col) 
{
    hd_point v[4];
    v[0].x = x1; v[0].y = y1;
    v[1].x = x2; v[1].y = y2;
    v[2].x = x3; v[2].y = y3;
    v[3].x = x4; v[3].y = y4;
    HD_AABezier (srf, 3, v, level, col);
}
void ttk_aabezier_gc (ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2,
                      int x3, int y3, int x4, int y4, int level) 
{
    ttk_aabezier (srf, x1, y1, x2, y2, x3, y3, x4, y4, level, gc->fg);
}

void ttk_bitmap (ttk_surface srf, int x, int y, int w, int h, unsigned short *data, ttk_color col) 
{
    HD_Bitmap (srf, x, y, w, h, data, col);
}
void ttk_bitmap_gc (ttk_surface srf, ttk_gc gc, int x, int y, int w, int h, unsigned short *data) 
{
    if (gc->usebg)
        ttk_fillrect (srf, x, y, x + w, y + h, gc->bg);
    ttk_bitmap (srf, x, y, w, h, data, gc->fg);
}

void ttk_text (ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col, const char *str) 
{
    HD_Font_Draw (srf, fnt.f, x, y + fnt.ofs, col, str);
}
void ttk_text_lat1 (ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col, const char *str) 
{
    HD_Font_DrawLatin1 (srf, fnt.f, x, y + fnt.ofs, col, str);
}
void ttk_text_uc16 (ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col, const uc16 *str) 
{
    HD_Font_DrawUnicode (srf, fnt.f, x, y + fnt.ofs, col, str);
}
void ttk_text_gc (ttk_surface srf, ttk_gc gc, int x, int y, const char *str) 
{
    if (gc->usebg)
        ttk_fillrect (srf, x, y, x + ttk_text_width (gc->font, str), y + gc->font.f->h, gc->bg);
    HD_Font_Draw (srf, gc->font.f, x, y + gc->font.ofs, gc->fg, str);
}

void ttk_textf (ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col, const char *fmt, ...) 
{
    static char *buffer;
    va_list ap;

    if (!buffer) buffer = malloc(4096);

    va_start (ap, fmt);
    vsnprintf (buffer, 4096, fmt, ap);
    va_end (ap);
    HD_Font_Draw (srf, fnt.f, x, y + fnt.ofs, col, buffer);
}

int ttk_text_width (ttk_font fnt, const char *str) 
{
    return HD_Font_TextWidth (fnt.f, str);
}
int ttk_text_width_lat1 (ttk_font fnt, const char *str) 
{
    return HD_Font_TextWidthLatin1 (fnt.f, str);
}
int ttk_text_width_uc16 (ttk_font fnt, const uc16 *str) 
{
    return HD_Font_TextWidthUnicode (fnt.f, str);
}
int ttk_text_width_gc (ttk_gc gc, const char *str) 
{
    return HD_Font_TextWidth (gc->font.f, str);
}
int ttk_text_height (ttk_font fnt) 
{
    return fnt.f->h;
}
int ttk_text_height_gc (ttk_gc gc) 
{
    return gc->font.f->h;
}

ttk_surface ttk_load_image (const char *path) 
{
    int w, h;
    return HD_PNG_Load (path, &w, &h);
}

void ttk_free_image (ttk_surface img) 
{
    if (img)
        HD_FreeSurface (img);
}

void ttk_blit_image (ttk_surface src, ttk_surface dst, int dx, int dy) 
{
    HD_ScaleBlendClip (src, 0, 0, HD_SRF_WIDTH (src), HD_SRF_HEIGHT (src),
                       dst, dx, dy, HD_SRF_WIDTH (src), HD_SRF_HEIGHT (src), 0, 255);
}

void ttk_blit_image_ex (ttk_surface src, int sx, int sy, int sw, int sh,
                        ttk_surface dst, int dx, int dy) 
{
    HD_ScaleBlendClip (src, sx, sy, sw, sh,
                       dst, dx, dy, sw, sh, 0, 255);
}

ttk_surface ttk_new_surface (int w, int h, int bpp) 
{
    return HD_NewSurface (w, h);
}

ttk_surface ttk_scale_surface (ttk_surface srf, float factor) 
{
    ttk_surface ret;
    int sw = HD_SRF_WIDTH (srf), sh = HD_SRF_HEIGHT (srf);
    int dw = (int)(sw * factor), dh = (int)(sh * factor);
    
    ret = HD_NewSurface (dw, dh);
    HD_ScaleBlendClip (srf, 0, 0, sw, sh,
                       ret, 0, 0, dw, dh, 0, 255);
    return ret;
}

void ttk_surface_get_dimen (ttk_surface srf, int *w, int *h) 
{
    *w = HD_SRF_WIDTH (srf);
    *h = HD_SRF_HEIGHT (srf);
}

void ttk_free_surface (ttk_surface srf) 
{
    HD_FreeSurface (srf);
}

void ttk_load_font (ttk_fontinfo *fi, const char *fnbase, int size) 
{
    char *fname = alloca (strlen (fnbase) + 5); // space for an extension
    struct stat st;
    
    sprintf (fname, "%s.png", fnbase);
    if (stat (fname, &st) >= 0) {
        fi->f.f = HD_Font_LoadSFont (fname);
        if (fi->f.f) return;
    }

    sprintf (fname, "%s.ttf", fnbase);
    if (stat (fname, &st) >= 0) {
        /*
          fi->f.f = HD_Font_LoadTTF (fname, fi->size);
          if (fi->f.f) return;
        */
    }

    sprintf (fname, "%s.fnt", fnbase);
    if (stat (fname, &st) >= 0) {
        fi->f.f = HD_Font_LoadFNT (fname);
        if (fi->f.f) return;
    }

    sprintf (fname, "%s.pcf", fnbase);
    if (stat (fname, &st) >= 0) {
        fi->f.f = HD_Font_LoadPCF (fname);
        if (fi->f.f) return;
    }

    sprintf (fname, "%s.fff", fnbase);
    if (stat (fname, &st) >= 0) {
        fi->f.f = HD_Font_LoadFFF (fname);
        if (fi->f.f) return;
    }

    fi->good = 0;
    return;
}

void ttk_unload_font (ttk_fontinfo *fi) 
{
    HD_Font_Free (fi->f.f);
    fi->loaded = fi->good = 0;
}

#endif
