/*
 * Copyright (C) 2005 Rebecca G. Bettencourt
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
 *  /!\ Warning! This is NOT the same as legacy osk.c!
 */

#include "pz.h"
#include <string.h>
#include <unistd.h>

/* dependent on textinput module */
extern ttk_color ti_ap_get(int);
extern TApItem * ti_ap_getx(int);
extern int ti_register(TWidget *(* cr)(), TWidget *(* ncr)(), char *, int);
extern TWidget * ti_create_tim_widget(int ht, int wd);

static PzModule * module;
static ttk_font ti_osk_font;

const int ti_osk_key_x[] = {
	0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, /* number row */
	0, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, /* qwerty row */
	0, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, /* home row */
	0, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, /* zxcv row */
	0, 4, 22, 26, 28 /* space bar row */
};
const int ti_osk_key_y[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* number row */
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* qwerty row */
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, /* home row */
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, /* zxcv row */
	8, 8, 8, 8, 8 /* space bar row */
};
const int ti_osk_key_width[] = {
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* number row */
	3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, /* qwerty row */
	4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, /* home row */
	5, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 5, /* zxcv row */
	4, 18, 4, 2, 2 /* space bar row */
};
const int ti_osk_key_normal[] = {
	'`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 8, 127,
	9, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\\',
	2, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', 10,
	1, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 1,
	3, ' ', 3, 28, 29
};
const int ti_osk_key_shift[] = {
	'~', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 8, 127,
	9, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '|',
	1, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', 10,
	0, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
	4, ' ', 4, 28, 29
};
const int ti_osk_key_caps[] = {
	'`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 8, 127,
	9, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\\',
	0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', 10,
	1, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', 1,
	4, ' ', 4, 28, 29
};
const int ti_osk_key_option[] = {
	7, 161, 8482, 163, 162, 166, 167, 182, 8226, 170, 186, 8211, 8800, 8, 127,
	9, 339, 8721, 7, 174, 8224, 165, 7, 7, 248, 254, 8220, 8216, 171,
	4, 229, 223, 240, 402, 169, 295, 8710, 8734, 172, 8230, 230, 10,
	4, 937, 8776, 231, 8730, 8747, 7, 181, 8804, 8805, 247, 4,
	0, 160, 0, 28, 29
};
const int ti_osk_key_opt_shift[] = {
	'`', 185, 178, 179, 188, 189, 190, 8225, 176, 183, 186, 8212, 177, 8, 127,
	9, 338, 8719, 180, 8240, 8225, 8364, 168, 710, 216, 222, 8221, 8217, 187,
	3, 197, 658, 208, 64257, 8480, 294, 8471, 63743, 64258, 8253, 198, 10,
	3, 184, 215, 199, 9674, 305, 732, 331, 175, 728, 191, 3,
	1, 160, 1, 28, 29
};
const int ti_osk_dead_keys[] = {
	'`', 224, 'b', 'c', 'd', 232, 'f', 'g', 'h', 236, 'j', 'k', 'l', 'm', 505, 242, 'p', 'q', 'r', 's', 't', 249, 'v', 'w', 'x', 'y', 'z',
	     192, 'B', 'C', 'D', 200, 'F', 'G', 'H', 204, 'J', 'K', 'L', 'M', 504, 210, 'P', 'Q', 'R', 'S', 'T', 217, 'V', 'W', 'X', 'Y', 'Z',
	180, 225, 'b', 263, 240, 233, 'f', 501, 'h', 237, 'j', 'k', 322, 'm', 324, 243, 'p', 'q', 341, 347, 254, 250, 'v', 'w', 'x', 253, 378,
	     193, 'B', 262, 208, 201, 'F', 500, 'H', 205, 'J', 'K', 321, 'M', 323, 211, 'P', 'Q', 340, 346, 222, 218, 'V', 'W', 'X', 221, 377,
	710, 226, 'b', 265, 'd', 234, 'f', 285, 293, 238, 309, 'k', 'l', 'm', 'n', 244, 'p', 'q', 'r', 353, 't', 251, 'v', 373, 'x', 375, 382,
	     194, 'B', 264, 'D', 202, 'F', 284, 292, 206, 308, 'K', 'L', 'M', 'N', 212, 'P', 'Q', 'R', 352, 'T', 219, 'V', 372, 'X', 374, 381,
	732, 227, 'b', 'c', 'd', 'e', 'f', 'g', 'h', 297, 'j', 'k', 'l', 'm', 241, 245, 'p', 'q', 'r', 's', 't', 361, 'v', 'w', 'x', 'y', 'z',
	     195, 'B', 'C', 'D', 'E', 'F', 'G', 'H', 296, 'J', 'K', 'L', 'M', 209, 213, 'P', 'Q', 'R', 'S', 'T', 360, 'V', 'W', 'X', 'Y', 'Z',
	168, 228, 'b', 'c', 'd', 235, 'f', 'g', 'h', 239, 'j', 'k', 'l', 'm', 'n', 246, 'p', 'q', 'r', 's', 't', 252, 'v', 'w', 'x', 255, 'z',
	     196, 'B', 'C', 'D', 203, 'F', 'G', 'H', 207, 'J', 'K', 'L', 'M', 'N', 214, 'P', 'Q', 'R', 'S', 'T', 220, 'V', 'W', 'X', 376, 'Z',
};
const char * ti_osk_special_key_names[] = {
	"norm", "shft", "clk", "opt", "opsh", "5", "6", "dead", "bs", "tab", "nl", "vtab", "feed", "rtn", "14", "15",
	"16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "esc", "<-", "->", "/\\", "\\/", "space"
};
const int ti_osk_cell_count = 59;
const int ti_osk_key_x_n[] = {
	0, 2, 4, 6,  0, 2, 4, 6,  0, 2, 4, 6,  0, 2, 4, 6,  0, 4, 6
};
const int ti_osk_key_y_n[] = {
	0, 0, 0, 0,  2, 2, 2, 2,  4, 4, 4, 4,  6, 6, 6, 6,  8, 8, 8
};
const int ti_osk_key_width_n[] = {
	2, 2, 2, 2,  2, 2, 2, 2,  2, 2, 2, 2,  2, 2, 2, 2,  4, 2, 2
};
const int ti_osk_key_numeric[] = {
	8, 'E', '^', '/',  '7', '8', '9', '*',  '4', '5', '6', '-',  '1', '2', '3', '+',  '0', '.', 10
};
const int ti_osk_cell_count_n = 19;

static int ti_osk_modifier_state = 0;
static int ti_osk_dead_key_state = 0;
static int ti_osk_current_key = 0;
static int ti_osk_numeric = 0;


void ti_osk_reset(void)
{
	ti_osk_modifier_state = 0;
	ti_osk_dead_key_state = 0;
	ti_osk_current_key = (ti_osk_numeric?0:55);
}

int ti_osk_get_key(int k)
{
	if (ti_osk_numeric) {
		return ti_osk_key_numeric[k];
	} else {
		switch (ti_osk_modifier_state) {
		case 0:
			return ti_osk_key_normal[k];
		case 1:
			return ti_osk_key_shift[k];
		case 2:
			return ti_osk_key_caps[k];
		case 3:
			return ti_osk_key_option[k];
		case 4:
			return ti_osk_key_opt_shift[k];
		}
	}
	return 0;
}

void ti_osk_draw_keyboard(ttk_surface srf, int x, int y, int gridsize, int sel)
{
	int i;
	int c;
	int kx, ky;
	char * keylabel;
	uc16 keytemp[2] = {0, 0};
	ttk_color kc;
	int width, height;
	height = ttk_text_height(ti_osk_font);
	if (ti_osk_numeric) {
		for (i=0; i<ti_osk_cell_count_n; i++) {
			if (i == sel) {
				ttk_ap_fillrect(srf, ti_ap_getx(2), x+gridsize*ti_osk_key_x_n[i], y+gridsize*ti_osk_key_y_n[i],
					x+gridsize*ti_osk_key_x_n[i]+gridsize*ti_osk_key_width_n[i]+1, y+gridsize*ti_osk_key_y_n[i]+gridsize*2+1);
				kc = ti_ap_get(3);
			} else {
				kc = ti_ap_get(1);
			}
			ttk_rect(srf, x+gridsize*ti_osk_key_x_n[i], y+gridsize*ti_osk_key_y_n[i], x+gridsize*ti_osk_key_x_n[i]+gridsize*ti_osk_key_width_n[i]+1, y+gridsize*ti_osk_key_y_n[i]+gridsize*2+1, ti_ap_get(1));
			kx = x+gridsize*ti_osk_key_x_n[i]+gridsize*ti_osk_key_width_n[i]/2+1;
			ky = y+gridsize*ti_osk_key_y_n[i]+gridsize-height/2+1;
			c = ti_osk_get_key(i);
			if (c == 127) {
				keylabel = "dl";
				width = ttk_text_width_lat1(ti_osk_font, keylabel);
				ttk_text_lat1(srf, ti_osk_font, kx-width/2, ky, kc, keylabel);
			} else if (c <= 32) {
				keylabel = (char *)ti_osk_special_key_names[c];
				width = ttk_text_width_lat1(ti_osk_font, keylabel);
				ttk_text_lat1(srf, ti_osk_font, kx-width/2, ky, kc, keylabel);
			} else {
				keytemp[0] = c;
				width = ttk_text_width_uc16(ti_osk_font, keytemp);
				ttk_text_uc16(srf, ti_osk_font, kx-width/2, ky, kc, keytemp);
			}
		}
	} else {
		for (i=0; i<ti_osk_cell_count; i++) {
			if (i == sel) {
				ttk_ap_fillrect(srf, ti_ap_getx(2), x+gridsize*ti_osk_key_x[i], y+gridsize*ti_osk_key_y[i],
					x+gridsize*ti_osk_key_x[i]+gridsize*ti_osk_key_width[i]+1, y+gridsize*ti_osk_key_y[i]+gridsize*2+1);
				kc = ti_ap_get(3);
			} else {
				kc = ti_ap_get(1);
			}
			ttk_rect(srf, x+gridsize*ti_osk_key_x[i], y+gridsize*ti_osk_key_y[i], x+gridsize*ti_osk_key_x[i]+gridsize*ti_osk_key_width[i]+1, y+gridsize*ti_osk_key_y[i]+gridsize*2+1, ti_ap_get(1));
			kx = x+gridsize*ti_osk_key_x[i]+gridsize*ti_osk_key_width[i]/2+1;
			ky = y+gridsize*ti_osk_key_y[i]+gridsize-height/2+1;
			c = ti_osk_get_key(i);
			if (c == 127) {
				keylabel = "dl";
				width = ttk_text_width_lat1(ti_osk_font, keylabel);
				ttk_text_lat1(srf, ti_osk_font, kx-width/2, ky, kc, keylabel);
			} else if (c == 7) {
				keytemp[0] = ti_osk_key_opt_shift[i];
				width = ttk_text_width_uc16(ti_osk_font, keytemp);
				ttk_text_uc16(srf, ti_osk_font, kx-width/2, ky, kc, keytemp);
			} else if (c <= 32) {
				keylabel = (char *)ti_osk_special_key_names[c];
				width = ttk_text_width_lat1(ti_osk_font, keylabel);
				ttk_text_lat1(srf, ti_osk_font, kx-width/2, ky, kc, keylabel);
			} else {
				keytemp[0] = c;
				width = ttk_text_width_uc16(ti_osk_font, keytemp);
				ttk_text_uc16(srf, ti_osk_font, kx-width/2, ky, kc, keytemp);
			}
		}
	}
}

void ti_osk_push_key(int k)
{
	int c;
	int i;
	c = ti_osk_get_key(k);
	if (c < 7) {
		ti_osk_modifier_state = c;
	} else if (c == 7) {
		ti_osk_dead_key_state = ti_osk_key_opt_shift[k];
	} else {
		if ((ti_osk_dead_key_state != 0) && (c >= 32)) {
			for (i=0; i<5; i++) {
				if (ti_osk_dead_keys[i*53] == ti_osk_dead_key_state) {
					if (c == 32) {
						ttk_input_char(ti_osk_dead_key_state);
					} else if ((c >= 'A') && (c <= 'Z')) {
						ttk_input_char(ti_osk_dead_keys[i*53 + 27 + (c-'A')]);
					} else if ((c >= 'a') && (c <= 'z')) {
						ttk_input_char(ti_osk_dead_keys[i*53 + 1 + (c-'a')]);
					} else {
						ttk_input_char(ti_osk_dead_key_state);
						ttk_input_char(c);
					}
				}
			}
		} else {
			switch (c) {
			case 8:
				ttk_input_char(TTK_INPUT_BKSP);
				break;
			case 10:
				ttk_input_char(TTK_INPUT_ENTER);
				break;
			case 28:
				ttk_input_char(TTK_INPUT_LEFT);
				break;
			case 29:
				ttk_input_char(TTK_INPUT_RIGHT);
				break;
			default:
				ttk_input_char(c);
				break;
			}
		}
		ti_osk_dead_key_state = 0;
	}
}

void ti_osk_draw(TWidget * wid, ttk_surface srf)
{
	int x, y;
	ttk_ap_fillrect(srf, ti_ap_getx(0), wid->x, wid->y, wid->x+wid->w, wid->y+wid->h);
	if (ti_osk_numeric) {
		x = wid->x + wid->w/2 - 17;
		y = wid->y + 1;
	} else {
		x = wid->x + wid->w/2 - 61;
		y = wid->y + 1;
	}
	ti_osk_draw_keyboard(srf, x, y, 4, ti_osk_current_key);
}

int ti_osk_down(TWidget * wid, int ch)
{
	switch (ch) {
	case TTK_BUTTON_ACTION:
		ti_osk_push_key(ti_osk_current_key);
		wid->dirty=1;
		break;
	case TTK_BUTTON_PREVIOUS:
		ttk_input_char(TTK_INPUT_BKSP);
		break;
	case TTK_BUTTON_NEXT:
		ttk_input_char(32);
		break;
	case TTK_BUTTON_MENU:
		ttk_input_end();
		break;
	case TTK_BUTTON_PLAY:
		ttk_input_char(TTK_INPUT_ENTER);
		break;
	default:
		return TTK_EV_UNUSED;
		break;
	}
	return TTK_EV_CLICK;
}

int ti_osk_scroll(TWidget * wid, int dir)
{
	TTK_SCROLLMOD (dir,4);
	
	if (dir<0) {
		ti_osk_current_key--;
		if (ti_osk_current_key < 0) { ti_osk_current_key = ((ti_osk_numeric)?(ti_osk_cell_count_n-1):(ti_osk_cell_count-1)); }
	} else {
		ti_osk_current_key++;
		if (ti_osk_current_key >= ((ti_osk_numeric)?ti_osk_cell_count_n:ti_osk_cell_count)) { ti_osk_current_key = 0; }
	}
	wid->dirty=1;
	return TTK_EV_CLICK;
}

TWidget * ti_osk_create(int n)
{
	TWidget * wid = ti_create_tim_widget(43, 0);
	wid->draw = ti_osk_draw;
	wid->down = ti_osk_down;
	wid->scroll = ti_osk_scroll;
	ti_osk_numeric = n;
	ti_osk_reset();
	return wid;
}

TWidget * ti_osk_screate()
{
	return ti_osk_create(0);
}

TWidget * ti_osk_ncreate()
{
	return ti_osk_create(1);
}

void ti_osk_init()
{
	module = pz_register_module("tiosk", 0);
	ti_register(ti_osk_screate, ti_osk_ncreate, N_("On-Screen Keyboard"), 3);
	ti_osk_font = ttk_get_font("SeaChel", 9);
}

PZ_MOD_INIT(ti_osk_init)

