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

#include "pz.h"
#include <stdarg.h>

TWindow *pz_create_stringview(const char *buf, const char *title)
{
    TWindow *ret = ttk_new_window();
    ttk_add_widget (ret, ttk_new_textarea_widget (ret->w, ret->h, buf, ttk_textfont, ttk_text_height (ttk_textfont) + 2));
    ttk_window_title (ret, title);
    return ret;
}

PzWindow *pz_do_window (const char *name, int geometry,
			void (*draw)(PzWidget *this, ttk_surface srf),
			int (*event)(PzEvent *ev), int timer) 
{
    PzWindow *win = pz_new_window (name, geometry);
    PzWidget *wid = pz_add_widget (win, draw, event);
    pz_widget_set_timer (wid, timer);
    return pz_finish_window (win);
}

PzWindow *pz_new_window (const char *name, int geometry, ...) 
{
    PzWindow *ret = ttk_new_window();
    if (geometry == PZ_WINDOW_XYWH) {
	va_list ap;
	va_start (ap, geometry);
	ret->x = va_arg (ap, int);
	ret->y = va_arg (ap, int);
	ret->w = va_arg (ap, int);
	ret->h = va_arg (ap, int);
	va_end (ap);
	ret->data = 0;
    } else if (geometry == PZ_WINDOW_FULLSCREEN) {
	ret->show_header = 0;
	ret->x = 0;
	ret->y = 0;
	ret->w = ttk_screen->w;
	ret->h = ttk_screen->h;
	ret->data = 0;
    } else if (geometry == PZ_WINDOW_POPUP) {
	ret->show_header = 1;
	// Provide a sensible default for apps that fill the window.
	ret->x = ttk_screen->wx;
	ret->y = ttk_screen->wy;
	ret->w = (ttk_screen->w - ttk_screen->wx) * 2 / 3;
	ret->h = (ttk_screen->h - ttk_screen->wy) * 3 / 5;
	ret->data = 1;
    } else {
	ret->show_header = 1;
	ret->x = ttk_screen->wx;
	ret->y = ttk_screen->wy;
	ret->w = ttk_screen->w - ttk_screen->wx;
	ret->h = ttk_screen->h - ttk_screen->wy;
	ret->data = 0;
    }

    if (name)
	ttk_window_set_title (ret, name);

    return ret;
}

PzWindow *pz_finish_window (PzWindow *win) 
{
    if (win->data)
	ttk_set_popup (win);
    return win;
}

PzWidget *pz_add_widget (PzWindow *win, void (*draw)(PzWidget *this, ttk_surface srf),
			 int (*event)(PzEvent *ev)) 
{
    PzWidget *ret = pz_new_widget (draw, event);
    ret->w = win->w;
    ret->h = win->h;
    ttk_add_widget (win, ret);
    return ret;
}

static int dispatch_event (TWidget *wid, int ev, int earg, int time) 
{
    PzEvent pev;
    int (*evh)(PzEvent*) = wid->data2;
    pev.wid = wid;
    pev.type = ev;
    pev.arg = earg;
    pev.time = time;
    return (*evh)(&pev);
}

static int e_scroll (TWidget *wid, int a) { return dispatch_event (wid, PZ_EVENT_SCROLL, a, 0); }
static int e_stap (TWidget *wid, int a) { return dispatch_event (wid, PZ_EVENT_STAP, a, 0); }
static int e_button (TWidget *wid, int a, int t) { return dispatch_event (wid, PZ_EVENT_BUTTON_UP, a, t); }
static int e_down (TWidget *wid, int a) { return dispatch_event (wid, PZ_EVENT_BUTTON_DOWN, a, 0); }
static int e_held (TWidget *wid, int a) { return dispatch_event (wid, PZ_EVENT_BUTTON_HELD, a, 0); }
static int e_input (TWidget *wid, int a) { return dispatch_event (wid, PZ_EVENT_INPUT, a, 0); }
static int e_frame (TWidget *wid) { return dispatch_event (wid, PZ_EVENT_FRAME, 0, 0); }
static int e_timer (TWidget *wid) { return dispatch_event (wid, PZ_EVENT_TIMER, 0, 0); }
static void e_destroy (TWidget *wid) { dispatch_event (wid, PZ_EVENT_DESTROY, 0, 0); }

PzWidget *pz_new_widget (void (*draw)(PzWidget *this, ttk_surface srf), int (*event)(PzEvent *ev)) 
{
    TWidget *ret = ttk_new_widget (0, 0);
    ret->draw = draw;
    ret->data2 = event;
    ret->scroll = e_scroll;
    ret->stap = e_stap;
    ret->button = e_button;
    ret->down = e_down;
    ret->held = e_held;
    ret->input = e_input;
    ret->frame = e_frame;
    ret->timer = e_timer;
    ret->destroy = e_destroy;
    ret->focusable = 1;
    return ret;
}

void pz_resize_widget (PzWidget *wid, int w, int h) 
{
    wid->w = w;
    wid->h = h;
}

void pz_widget_set_timer (PzWidget *wid, int ms) 
{
    ttk_widget_set_timer (wid, ms);
}

void pz_hide_window (PzWindow *win) 
{
    ttk_hide_window (win);
}
void pz_close_window (PzWindow *win) 
{
    ttk_hide_window (win);
    win->data = 0x12345678; // tell menu to free it
}
void pz_show_window (PzWindow *win) 
{
    ttk_show_window (win);
}
