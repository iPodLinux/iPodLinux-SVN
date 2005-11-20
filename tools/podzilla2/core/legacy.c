/*
 * Copyright (c) 2005 Joshua Oreman
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

#define LEGACY_DOT_C
#define PZ_COMPAT
#include "pz.h"

extern ttk_gc pz_root_gc;
static TWindow *pz_last_window;
static char *pz_next_header;

void pz_old_event_handler (t_GR_EVENT *ev) 
{
    int sdir = 0;
    if (ttk_windows->w->focus) {
	if (ev->keystroke.ch == 'r')
	    sdir = 1;
	else if (ev->keystroke.ch == 'l')
	    sdir = -1;
	
	switch (ev->type) {
	case GR_EVENT_TYPE_KEY_DOWN:
	    if (sdir)
		ttk_windows->w->focus->scroll (ttk_windows->w->focus, sdir);
	    else
		ttk_windows->w->focus->down (ttk_windows->w->focus, ev->keystroke.ch);
	    break;
	case GR_EVENT_TYPE_KEY_UP:
	    if (!sdir)
		ttk_windows->w->focus->button (ttk_windows->w->focus, ev->keystroke.ch, 0);
	    break;
	}
    }
}

void pz_draw_header (char *str) 
{
    if (pz_last_window)
	ttk_window_set_title (pz_last_window, str);
    else
	pz_next_header = str;
}

typedef struct 
{
    void (*draw)();
    int (*key)(GR_EVENT *);
} legacy_data;

#define _MAKETHIS legacy_data *data = (legacy_data *)this->data

void pz_legacy_construct_GR_EVENT (GR_EVENT *ev, int type, int arg) 
{
    ev->type = type;
    if (ev->type == GR_EVENT_TYPE_KEY_UP || ev->type == GR_EVENT_TYPE_KEY_DOWN) {
	if (arg == TTK_BUTTON_ACTION)
	    arg = '\r';
	ev->keystroke.ch = arg;
	ev->keystroke.scancode = 0;
    }
}
void pz_legacy_draw (TWidget *this, ttk_surface srf) 
{
    _MAKETHIS;
    data->draw(); // it'll be on the window we returned from pz_new_window
}
int pz_legacy_button (TWidget *this, int button, int time)
{
    GR_EVENT ev;
    _MAKETHIS;
    pz_legacy_construct_GR_EVENT (&ev, GR_EVENT_TYPE_KEY_UP, button);
    this->dirty++;
    return data->key (&ev);
}
int pz_legacy_down (TWidget *this, int button)
{
    GR_EVENT ev;
    _MAKETHIS;
    pz_legacy_construct_GR_EVENT (&ev, GR_EVENT_TYPE_KEY_DOWN, button);
    this->dirty++;
    return data->key (&ev);
}
int pz_legacy_scroll (TWidget *this, int dir)
{
    GR_EVENT ev;
    int key = 'r';
    int ret = 0;
    _MAKETHIS;
    TTK_SCROLLMOD (dir, 4);

    if (dir < 0) {
	key = 'l';
	dir = -dir;
    }
    
    while (dir) {
	pz_legacy_construct_GR_EVENT (&ev, GR_EVENT_TYPE_KEY_DOWN, key);
	ret |= data->key (&ev);
	pz_legacy_construct_GR_EVENT (&ev, GR_EVENT_TYPE_KEY_UP, key);
	ret |= data->key (&ev);
	dir--;
    }
    this->dirty++;
    return ret;
}

int pz_legacy_timer (TWidget *this)
{
    GR_EVENT ev;
    _MAKETHIS;
    pz_legacy_construct_GR_EVENT (&ev, GR_EVENT_TYPE_TIMER, 0);
    this->dirty++;
    return data->key (&ev);
}
void pz_legacy_destroy (TWidget *this)
{
    free (this->data);
}


// Make widget 0 by 0 -- many old games draw to window only when they need to.
// ttk_run() blanks a WxH region before calling draw(), and draw() might
// only draw a few things.
TWidget *pz_new_legacy_widget (void (*do_draw)(), int (*do_keystroke)(GR_EVENT *))
{
    TWidget *ret = ttk_new_widget (0, 0);
    ret->w = 0;
    ret->h = 0;
    ret->data = calloc (1, sizeof(legacy_data));
    ret->focusable = 1;
    ret->dirty = 1;
    ret->draw = pz_legacy_draw;
    ret->button = pz_legacy_button;
    ret->down = pz_legacy_down;
    ret->scroll = pz_legacy_scroll;
    ret->timer = pz_legacy_timer;
    ret->destroy = pz_legacy_destroy;

    legacy_data *data = (legacy_data *)ret->data;
    data->draw = do_draw;
    data->key = do_keystroke;

    return ret;
}

TWindow *pz_mh_legacy (ttk_menu_item *item) 
{
    TWindow *old = ttk_windows->w;
    void (*newwin)() = (void (*)())item->data;
    (*newwin)();
    if (ttk_windows->w != old) {
	item->sub = ttk_windows->w;
	return TTK_MENU_ALREADYDONE;
    } else {
	item->flags &= ~TTK_MENU_MADESUB;
	return TTK_MENU_DONOTHING;
    }
}

t_GR_WINDOW_ID pz_old_window (int x, int y, int w, int h, void(*do_draw)(void), int(*do_keystroke)(t_GR_EVENT * event))
{
    TWindow *ret = ttk_new_window();
    fprintf (stderr, "Legacy code alert!\n");
    ttk_fillrect (ret->srf, 0, 0, ret->w, ret->h, ttk_makecol (255, 255, 255));
    ret->x = x;
    ret->y = y;
    if ((y == HEADER_TOPLINE) || (y == HEADER_TOPLINE + 1))
	ret->y = ttk_screen->wy, ret->h = ttk_screen->h - ttk_screen->wy;
    else if (!y)
	ret->show_header = 0;
    ret->w = w;
    ret->h = h;
    ttk_add_widget (ret, pz_new_legacy_widget (do_draw, do_keystroke));

    pz_last_window = ret;

    if (pz_next_header) {
	ttk_window_set_title (ret, pz_next_header);
	pz_next_header = 0;
    }

    return ret;
}

void pz_old_close_window(t_GR_WINDOW_ID wid)
{
    ttk_hide_window (wid);
    // pick new top legacy window:
    if (ttk_windows->w->focus && ttk_windows->w->focus->draw == pz_legacy_draw)
	pz_last_window = ttk_windows->w; // pick new top window
    else
	pz_last_window = 0;
    wid->data = 0x12345678; // hey menu: free it & recreate
}

t_GR_GC_ID pz_get_gc(int copy) 
{
    t_GR_GC_ID gc;
    
    if (!copy) return pz_root_gc;
    gc = ttk_new_gc();
    ttk_gc_set_foreground (gc, ttk_makecol (0, 0, 0));
    ttk_gc_set_background (gc, ttk_makecol (255, 255, 255));
    ttk_gc_set_usebg (gc, 0);
    ttk_gc_set_xormode (gc, 0);
    ttk_gc_set_font (gc, ttk_textfont);
    return gc;
}

void vector_render_char (t_GR_WINDOW_ID win, t_GR_GC_ID gc,
			 char c, int scale, int x, int y) 
{
    char s[2] = { 0, 0 };
    int cw = scale * 5;
    int ch = scale * 9;
    s[0] = c;
    
    pz_vector_string (win->srf, s, x, y, cw, ch, 0, ttk_gc_get_foreground (gc));
}
void vector_render_string (t_GR_WINDOW_ID win, t_GR_GC_ID gc,
			   const char *string, int kern, int scale, int x, int y) 
{
    int cw = scale * 5;
    int ch = scale * 9;
    
    pz_vector_string (win->srf, string, x, y, cw, ch, kern, ttk_gc_get_foreground (gc));
}
void vector_render_string_center (t_GR_WINDOW_ID win, t_GR_GC_ID gc,
				  const char *string, int kern, int scale, int x, int y) 
{
    int cw = scale * 5;
    int ch = scale * 9;
    
    pz_vector_string_center (win->srf, string, x, y, cw, ch, kern, ttk_gc_get_foreground (gc));    
}
void vector_render_string_right (t_GR_WINDOW_ID win, t_GR_GC_ID gc,
				 const char *string, int kern, int scale, int x, int y) 
{
    int cw = scale * 5;
    int ch = scale * 9;
    int w = pz_vector_width (string, cw, ch, kern);
    
    pz_vector_string (win->srf, string, x - w, y, cw, ch, kern, ttk_gc_get_foreground (gc));
}
int vector_string_pixel_width (const char *string, int kern, int scale) 
{
    int cw = scale * 5;
    int ch = scale * 9;

    return pz_vector_width (string, cw, ch, kern);
}

#define t_GR_RGB(r,g,b) (((r)<<16)|((g)<<8)|(b))
t_GR_COLOR appearance_get_color (const char *prop)
{
    int r, g, b;
    ttk_unmakecol (ttk_ap_getx (prop) -> color, &r, &g, &b);
    return t_GR_RGB (r, g, b);
}

void beep() 
{
	ttk_click();
}
