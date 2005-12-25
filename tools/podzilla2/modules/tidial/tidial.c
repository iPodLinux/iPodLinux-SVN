/*
 * Copyright (C) 2005 Jonathan Bettencourt (jonrelay)
 *
 * Scroll Pad concept by veteran, implemented by jonrelay
 * Dial Type concept by Peter Burk, implemented by jonrelay
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

/*
 *  /!\ Warning! This is NOT the same as legacy dial.c!
 */

#include "pz.h"
#include <string.h>

/* dependent on textinput module */
extern ttk_color ti_ap_get(int);
extern int ti_register(TWidget *(* cr)(), TWidget *(* ncr)(), char *, int);

static PzModule * module;

const char ti_dial_charlist[] = " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ=+?!@#$%^&*()[]{}_-<>~`\"\'1234567890\\/|:;.,M";
const char ti_dial_charlist_n[] = ".0123456789-E+-*/^M";
static int ti_dial_charlist_pos = 0;
static int ti_dial_cursormode = 0;
static int ti_dial_numeric = 0;
static int ti_dial_snapback = 0;

extern TWidget * ti_create_tim_widget(int ht, int wd);

char ti_dial_get_char(void)
{
	return ti_dial_numeric?ti_dial_charlist_n[ti_dial_charlist_pos]:ti_dial_charlist[ti_dial_charlist_pos];
}

void ti_dial_next_char(void)
{
	ti_dial_charlist_pos++;
	if (ti_dial_charlist_pos >= (ti_dial_numeric?19:96)) { ti_dial_charlist_pos = 0; }
}

void ti_dial_prev_char(void)
{
	ti_dial_charlist_pos--;
	if (ti_dial_charlist_pos < 0) { ti_dial_charlist_pos = (ti_dial_numeric?18:95); }
}

void ti_dial_reset(void)
{
	ti_dial_charlist_pos = 0;
	ti_dial_cursormode = 0;
}

#define CW 10 /* width of the space for each char */

void ti_dial_draw(TWidget * wid, ttk_surface srf)
{
	int sc, n, i, j, ty;
	int m = (ti_dial_numeric?19:96);
	char s[2];
	ttk_fillrect(srf, wid->x, wid->y, wid->x+wid->w, wid->y+wid->h, ti_ap_get(0));
	n = wid->w / CW;
	sc = ti_dial_charlist_pos - n/2;
	ty = 0;
	if (sc < 0) { sc = 0; }
	if (sc > (m-n) ) { sc = (m-n); }
	for (i = sc, j = 0; ((i<m) && (j<n)); i++, j++)
	{
		s[0] = ti_dial_numeric?ti_dial_charlist_n[i]:ti_dial_charlist[i];
		s[1] = 0;
		ttk_text_lat1(srf, ttk_menufont, wid->x+j*CW+(10-ttk_text_width_lat1(ttk_menufont, s))/2, wid->y+ty, ti_ap_get(1), s);
		if (i == ti_dial_charlist_pos) {
			ttk_rect(srf, wid->x+j*CW, wid->y, wid->x+j*CW+CW, wid->y+wid->h, ti_ap_get(1));
		}
	}
}

int ti_dial_scroll(TWidget * wid, int dir)
{
	TTK_SCROLLMOD (dir, 4);
	
	if (dir<0) {
		if (ti_dial_cursormode) {
			ttk_input_char(TTK_INPUT_LEFT);
		} else {
			ti_dial_prev_char();
			wid->dirty = 1;
		}
	} else {
		if (ti_dial_cursormode) {
			ttk_input_char(TTK_INPUT_RIGHT);
		} else {
			ti_dial_next_char();
			wid->dirty = 1;
		}
	}
	return TTK_EV_CLICK;
}

int ti_dial_down(TWidget * wid, int btn)
{
	switch (btn) {
	case TTK_BUTTON_ACTION:
		if (ti_dial_cursormode) {
			ti_dial_cursormode = 0;
		} else if (ti_dial_charlist_pos == (ti_dial_numeric?18:95)) {
			ti_dial_cursormode = 1;
		} else {
			ttk_input_char(ti_dial_get_char());
			if (ti_dial_snapback) {
				ti_dial_reset();
				wid->dirty = 1;
			}
		}
		break;
	case TTK_BUTTON_PREVIOUS:
		ttk_input_char(TTK_INPUT_BKSP);
		break;
	case TTK_BUTTON_NEXT:
		ttk_input_char(32);
		break;
	case TTK_BUTTON_PLAY:
		ttk_input_char(TTK_INPUT_ENTER);
		break;
	case TTK_BUTTON_MENU:
		ttk_input_end();
		break;
	default:
		return TTK_EV_UNUSED;
		break;
	}
	return TTK_EV_CLICK;
}

TWidget * ti_dial_create(int n, int s)
{
	TWidget * wid = ti_create_tim_widget(-1, 0);
	wid->down = ti_dial_down;
	wid->scroll = ti_dial_scroll;
	wid->draw = ti_dial_draw;
	ti_dial_reset();
	ti_dial_numeric = n;
	ti_dial_snapback = s;
	return wid;
}

TWidget * ti_dial_stcreate()
{
	return ti_dial_create(0, 0);
}

TWidget * ti_dial_stncreate()
{
	return ti_dial_create(1, 0);
}

TWidget * ti_dial_dtcreate()
{
	return ti_dial_create(0, 1);
}

TWidget * ti_dial_dtncreate()
{
	return ti_dial_create(1, 1);
}

void ti_dial_init()
{
	module = pz_register_module("tidial", 0);
	ti_register(ti_dial_stcreate, ti_dial_stncreate, N_("Scroll-Through"), 2);
	ti_register(ti_dial_dtcreate, ti_dial_dtncreate, N_("Dial Type"), 8);
}

PZ_MOD_INIT(ti_dial_init)

