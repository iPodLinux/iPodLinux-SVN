/*
 * TTK Mouse Emulation Widget
 * Copyright (C) 2005 Jonathan Bettencourt (jonrelay)
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
#include <stdio.h>

static PzModule * module;

typedef struct ttk_mousedata {
	int x;
	int y;
	int scrollaxis;
	ttk_surface cursor;
	int hidecursor;
	void (*draw)(TWidget * wid, ttk_surface srf);
	void (*mouseDown)(TWidget * wid, int x, int y);
	void (*mouseStillDown)(TWidget * wid, int x, int y);
	void (*mouseUp)(TWidget * wid, int x, int y);
	void (*mouseMove)(TWidget * wid, int x, int y);
	int mouseDownHappened;
} ttk_mousedata;

ttk_mousedata * ttk_new_mousedata_with(
	void (*d)(TWidget * wid, ttk_surface srf),
	void (*md)(TWidget * wid, int x, int y),
	void (*msd)(TWidget * wid, int x, int y),
	void (*mu)(TWidget * wid, int x, int y),
	void (*mm)(TWidget * wid, int x, int y)
)
{
	ttk_mousedata * m = (ttk_mousedata *)calloc(1, sizeof(ttk_mousedata));
	m->draw = d;
	m->mouseDown = md;
	m->mouseStillDown = msd;
	m->mouseUp = mu;
	m->mouseMove = mm;
	return m;
}

ttk_mousedata * ttk_new_mousedata()
{
	ttk_mousedata * m = (ttk_mousedata *)calloc(1, sizeof(ttk_mousedata));
	return m;
}

void ttk_free_mousedata(ttk_mousedata * m)
{
	if (m) free(m);
}

void ttk_mouse_draw(TWidget * wid, ttk_surface srf)
{
	ttk_mousedata * m = (ttk_mousedata *)wid->data;
	int cx = wid->x + m->x - 16;
	int cy = wid->y + m->y - 16;
	if (m->draw) {
		(m->draw)(wid, srf);
	}
	if (m->cursor && !m->hidecursor) {
		ttk_blit_image(m->cursor, srf, cx, cy);
	}
}

int ttk_mouse_scroll(TWidget * wid, int dir)
{
	ttk_mousedata * m = (ttk_mousedata *)wid->data;
	
	TTK_SCROLLMOD (dir,4);
	
	if (!m->scrollaxis) {
		m->x += dir;
		if (m->x < 0) m->x = 0;
		if (m->x >= wid->w) m->x = wid->w-1;
	} else {
		m->y += dir;
		if (m->y < 0) m->y = 0;
		if (m->y >= wid->h) m->y = wid->h-1;
	}
	if (m->mouseMove) {
		(m->mouseMove)(wid, m->x, m->y);
	}
	wid->dirty = 1;
	return TTK_EV_CLICK;
}

int ttk_mouse_up(TWidget * wid, int btn, int t)
{
	ttk_mousedata * m = (ttk_mousedata *)wid->data;
	if (ttk_get_podversion() & TTK_POD_3G) {
		if (btn != TTK_BUTTON_PLAY && btn != TTK_BUTTON_MENU) return TTK_EV_UNUSED;
	} else {
		if (btn != TTK_BUTTON_ACTION) return TTK_EV_UNUSED;
	}
	m->mouseDownHappened = 0;
	if (m->mouseUp) {
		(m->mouseUp)(wid, m->x, m->y);
	}
	return TTK_EV_CLICK;
}

int ttk_mouse_down(TWidget * wid, int btn)
{
	ttk_mousedata * m = (ttk_mousedata *)wid->data;
	if (ttk_get_podversion() & TTK_POD_3G) {
		switch (btn) {
		case TTK_BUTTON_ACTION:
			m->scrollaxis = !m->scrollaxis;
			break;
		case TTK_BUTTON_PREVIOUS:
			m->scrollaxis = 0;
			break;
		case TTK_BUTTON_NEXT:
			m->scrollaxis = 1;
			break;
		case TTK_BUTTON_PLAY:
		case TTK_BUTTON_MENU:
			if (m->mouseDownHappened) {
				if (m->mouseStillDown) {
					(m->mouseStillDown)(wid, m->x, m->y);
				}
			} else {
				m->mouseDownHappened = 1;
				if (m->mouseDown) {
					(m->mouseDown)(wid, m->x, m->y);
				}
			}
			break;
		default:
			return TTK_EV_UNUSED;
			break;
		}
		return TTK_EV_CLICK;
	} else {
		switch (btn) {
		case TTK_BUTTON_PREVIOUS:
			m->x--;
			if (m->x < 0) m->x = 0;
			m->scrollaxis = 0;
			break;
		case TTK_BUTTON_NEXT:
			m->x++;
			if (m->x >= wid->w) m->x = wid->w-1;
			m->scrollaxis = 0;
			break;
		case TTK_BUTTON_MENU:
			m->y--;
			if (m->y < 0) m->y = 0;
			m->scrollaxis = 1;
			break;
		case TTK_BUTTON_PLAY:
			m->y++;
			if (m->y >= wid->h) m->y = wid->h-1;
			m->scrollaxis = 1;
			break;
		case TTK_BUTTON_ACTION:
			if (m->mouseDownHappened) {
				if (m->mouseStillDown) {
					(m->mouseStillDown)(wid, m->x, m->y);
				}
			} else {
				m->mouseDownHappened = 1;
				if (m->mouseDown) {
					(m->mouseDown)(wid, m->x, m->y);
				}
			}
			return TTK_EV_CLICK;
			break;
		default:
			return TTK_EV_UNUSED;
			break;
		}
		if (m->mouseMove) {
			(m->mouseMove)(wid, m->x, m->y);
		}
		wid->dirty = 1;
		return TTK_EV_CLICK;
	}
}

void ttk_mouse_destroy(TWidget * wid)
{
	ttk_mousedata * m = (ttk_mousedata *)wid->data;
	ttk_free_mousedata(m);
}

TWidget * ttk_new_mouse_widget_with(int x, int y, int w, int h, ttk_surface cursor,
	void (*d)(TWidget * wid, ttk_surface srf),
	void (*md)(TWidget * wid, int x, int y),
	void (*msd)(TWidget * wid, int x, int y),
	void (*mu)(TWidget * wid, int x, int y),
	void (*mm)(TWidget * wid, int x, int y)
)
{
	ttk_mousedata * m = ttk_new_mousedata_with(d, md, msd, mu, mm);
	TWidget * wid = ttk_new_widget(x, y);
	wid->w = w;
	wid->h = h;
	m->cursor = cursor;
	wid->data = (void *)m;
	wid->draw = ttk_mouse_draw;
	wid->down = ttk_mouse_down;
	wid->button = ttk_mouse_up;
	wid->scroll = ttk_mouse_scroll;
	wid->destroy = ttk_mouse_destroy;
	wid->keyrepeat = 1;
	wid->focusable = 1;
	return wid;
}

TWidget * ttk_new_mouse_widget(int x, int y, int w, int h, ttk_surface cursor)
{
	ttk_mousedata * m = ttk_new_mousedata();
	TWidget * wid = ttk_new_widget(x, y);
	wid->w = w;
	wid->h = h;
	m->cursor = cursor;
	wid->data = (void *)m;
	wid->draw = ttk_mouse_draw;
	wid->down = ttk_mouse_down;
	wid->button = ttk_mouse_up;
	wid->scroll = ttk_mouse_scroll;
	wid->destroy = ttk_mouse_destroy;
	wid->keyrepeat = 1;
	wid->focusable = 1;
	return wid;
}

ttk_surface ttk_get_cursor(const char * filename)
{
	return ttk_load_image(pz_module_get_datapath(module, filename));
}

static void mouse_demo_mup(TWidget * wid, int x, int y)
{
	char msg[50];
	snprintf(msg, 50, "You clicked at %d,%d.", x, y);
	pz_message(msg);
	pz_close_window(wid->win);
}

static void mouse_demo_draw(TWidget * wid, ttk_surface srf)
{
	ttk_fillrect(srf, 40, 40, wid->w-40, wid->h-40, ttk_makecol(255, 100, 200));
}

PzWindow * new_mouse_demo_window()
{
	PzWindow * ret = pz_new_window(_("Mouse Demo"), PZ_WINDOW_NORMAL);
	TWidget * wid = ttk_new_mouse_widget(0, 0, ret->w, ret->h, ttk_get_cursor("arrow.png"));
	((ttk_mousedata *)wid->data)->draw = mouse_demo_draw;
	((ttk_mousedata *)wid->data)->mouseUp = mouse_demo_mup;
	ttk_add_widget(ret, wid);
	return pz_finish_window(ret);
}

void mouse_module_init()
{
	module = pz_register_module("mouse", 0);
	pz_menu_add_action("/Extras/Demos/Mouse Demo", new_mouse_demo_window);
}

PZ_MOD_INIT(mouse_module_init)
