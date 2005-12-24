/*
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

/* dependent on textinput module */
extern ttk_color ti_ap_get(int);
extern int ti_register(TWidget *(* cr)(), TWidget *(* ncr)(), char *, int);

static PzModule * module;

const char * ti_unihex_numbers[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "+", "-", "*", "/", "^", "E"};
const char * ti_unihex_string[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F"};

static int ti_unihex_numeric = 0;
static int ti_unihex_digit[4] = {0, 0, 0, 0};
static int ti_unihex_pos = 0;

extern TWidget * ti_create_tim_widget(int ht, int wd);

void ti_unihex_draw(TWidget * wid, ttk_surface srf)
{
	int x = wid->x+wid->w/2;
	int y = wid->y;
	int h = wid->h;
	int cw = ttk_text_width_lat1(ttk_menufont, "0123456789ABCDEF")/16+1;
	int i;
	ttk_fillrect(srf, wid->x, wid->y, wid->x+wid->w, wid->y+wid->h, ti_ap_get(0));
	if (ti_unihex_numeric) {
		ttk_text_lat1(srf, ttk_menufont, x-ttk_text_width_lat1(ttk_menufont, ti_unihex_numbers[ti_unihex_pos])/2, y, ti_ap_get(1), ti_unihex_numbers[ti_unihex_pos]);
	} else {
		x -= (cw*2);
		for (i=0; i<4; i++) {
			ttk_text_lat1(srf, ttk_menufont, x+(cw*i)+(cw-ttk_text_width_lat1(ttk_menufont, ti_unihex_string[ti_unihex_digit[i]]))/2, y, ti_ap_get(1), ti_unihex_string[ti_unihex_digit[i]]);
			if (i==ti_unihex_pos) {
				ttk_line(srf, x+(cw*i), y, x+(cw*i)+cw, y, ttk_makecol(GREY));
				ttk_line(srf, x+(cw*i), y+1, x+(cw*i)+cw, y+1, ttk_makecol(GREY));
				ttk_line(srf, x+(cw*i), y+h-2, x+(cw*i)+cw, y+h-2, ttk_makecol(GREY));
				ttk_line(srf, x+(cw*i), y+h-1, x+(cw*i)+cw, y+h-1, ttk_makecol(GREY));
			}
		}
	}
}

int ti_unihex_scroll(TWidget * wid, int dir)
{
	TTK_SCROLLMOD (dir, 4);
	
	if (ti_unihex_numeric) {
		if (dir<0) {
			ti_unihex_pos = ((ti_unihex_pos + 16) % 17);
		} else {
			ti_unihex_pos = ((ti_unihex_pos + 1) % 17);
		}
	} else {
		if (dir<0) {
			ti_unihex_digit[ti_unihex_pos] = ((ti_unihex_digit[ti_unihex_pos]-1) & 0xF);
		} else {
			ti_unihex_digit[ti_unihex_pos] = ((ti_unihex_digit[ti_unihex_pos]+1) & 0xF);
		}
	}
	wid->dirty = 1;
	return TTK_EV_CLICK;
}

int ti_unihex_down(TWidget * wid, int btn)
{
	if (ti_unihex_numeric) {
		switch (btn) {
		case TTK_BUTTON_PLAY:
			ttk_input_char(ti_unihex_numbers[ti_unihex_pos][0]);
			break;
		case TTK_BUTTON_ACTION:
			ttk_input_char(TTK_INPUT_BKSP);
			break;
		case TTK_BUTTON_PREVIOUS:
			ttk_input_char(TTK_INPUT_LEFT);
			break;
		case TTK_BUTTON_NEXT:
			ttk_input_char(TTK_INPUT_RIGHT);
			break;
		case TTK_BUTTON_MENU:
			ttk_input_end();
			break;
		default:
			return TTK_EV_UNUSED;
			break;
		}
	} else {
		switch (btn) {
		case TTK_BUTTON_PLAY:
			{
				int ch = ((ti_unihex_digit[0]<<12) + (ti_unihex_digit[1]<<8) + (ti_unihex_digit[2]<<4) + (ti_unihex_digit[3]));
				if (ch) ttk_input_char(ch);
			}
			break;
		case TTK_BUTTON_ACTION:
			ti_unihex_pos = ((ti_unihex_pos + 1) % 4);
			wid->dirty=1;
			break;
		case TTK_BUTTON_PREVIOUS:
			ttk_input_char(TTK_INPUT_LEFT);
			break;
		case TTK_BUTTON_NEXT:
			ttk_input_char(TTK_INPUT_RIGHT);
			break;
		case TTK_BUTTON_MENU:
			ttk_input_end();
			break;
		default:
			return TTK_EV_UNUSED;
			break;
		}
	}
	return TTK_EV_CLICK;
}

TWidget * ti_unihex_create(int n)
{
	TWidget * wid = ti_create_tim_widget(-1, 0);
	wid->draw = ti_unihex_draw;
	wid->down = ti_unihex_down;
	wid->scroll = ti_unihex_scroll;
	ti_unihex_numeric = n;
	ti_unihex_digit[0] = 0;
	ti_unihex_digit[1] = 0;
	ti_unihex_digit[2] = 0;
	ti_unihex_digit[3] = 0;
	ti_unihex_pos = 0;
	return wid;
}

TWidget * ti_unihex_screate()
{
	return ti_unihex_create(0);
}

TWidget * ti_unihex_ncreate()
{
	return ti_unihex_create(1);
}

void ti_unihex_init()
{
	module = pz_register_module("tiunihex", 0);
	ti_register(ti_unihex_screate, ti_unihex_ncreate, "Unicode Hex Input", 16);
}

PZ_MOD_INIT(ti_unihex_init)

