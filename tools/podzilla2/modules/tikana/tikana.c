/*
 * Copyright (C) 2006 Jonathan Bettencourt (jonrelay)
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
#include <string.h>
#include <unistd.h>

/* dependent on textinput module */
extern ttk_color ti_ap_get(int);
extern int ti_register(TWidget *(* cr)(), TWidget *(* ncr)(), char *, int);

static PzModule * module;

#define CW 10
#define CH 10

const uc16 ti_kana_hiragana_glyph[] = { 0x304B, 0x306A, 0 };
const uc16 ti_kana_katakana_glyph[] = { 0x30AB, 0x30CA, 0 };
const uc16 ti_kana_romaji_glyph[] = { 0x82F1, 0x6570, 0 };
const uc16 ti_kana_space_glyph[] = { 0x7A7A, 0x767D, 0 };
const uc16 ti_kana_bksp_glyph[] = { 0x2190, 0x6D88, 0x3059, 0 };
const uc16 ti_kana_newline_glyph[] = { 0x6539, 0x884C, 0 };
const uc16 ti_kana_prev_glyph[] = { 0x2190, 0 };
const uc16 ti_kana_next_glyph[] = { 0x2192, 0 };

const uc16 ti_kana_hiragana[] = {
	0x30FC, 0x3063, 0x3083, 0x3041, 0x3071, 0x3070, 0x3060, 0x3056, 0x304C, 0x3093,
	0x308F, 0x3089, 0x3084, 0x307E, 0x306F, 0x306A, 0x305F, 0x3055, 0x304B, 0x3042,
	0x3001, 0xFF08, 0x3000, 0x3043, 0x3074, 0x3073, 0x3062, 0x3058, 0x304E, 0x3000,
	0x3090, 0x308A, 0x3000, 0x307F, 0x3072, 0x306B, 0x3061, 0x3057, 0x304D, 0x3044,
	0x3002, 0xFF09, 0x3085, 0x3045, 0x3077, 0x3076, 0x3065, 0x305A, 0x3050, 0x3000,
	0x3000, 0x308B, 0x3086, 0x3080, 0x3075, 0x306C, 0x3064, 0x3059, 0x304F, 0x3046,
	0xFF01, 0x300C, 0x3000, 0x3047, 0x307A, 0x3079, 0x3067, 0x305C, 0x3052, 0x3000,
	0x3091, 0x308C, 0x3000, 0x3081, 0x3078, 0x306D, 0x3066, 0x305B, 0x3051, 0x3048,
	0xFF1F, 0x300D, 0x3087, 0x3049, 0x307D, 0x307C, 0x3069, 0x305E, 0x3054, 0x3000,
	0x3092, 0x308D, 0x3088, 0x3082, 0x307B, 0x306E, 0x3068, 0x305D, 0x3053, 0x304A,
	0
};

const uc16 ti_kana_katakana[] = {
	0x30FC, 0x30C3, 0x30E3, 0x30A1, 0x30D1, 0x30D0, 0x30C0, 0x30B6, 0x30AC, 0x30F3,
	0x30EF, 0x30E9, 0x30E4, 0x30DE, 0x30CF, 0x30CA, 0x30BF, 0x30B5, 0x30AB, 0x30A2,
	0x3001, 0xFF08, 0x3000, 0x30A3, 0x30D4, 0x30D3, 0x30C2, 0x30B8, 0x30AE, 0x3000,
	0x30F0, 0x30EA, 0x3000, 0x30DF, 0x30D2, 0x30CB, 0x30C1, 0x30B7, 0x30AD, 0x30A4,
	0x3002, 0xFF09, 0x30E5, 0x30A5, 0x30D7, 0x30D6, 0x30C5, 0x30BA, 0x30B0, 0x30F4,
	0x3000, 0x30EB, 0x30E6, 0x30E0, 0x30D5, 0x30CC, 0x30C4, 0x30B9, 0x30AF, 0x30A6,
	0xFF01, 0x300C, 0x3000, 0x30A7, 0x30DA, 0x30D9, 0x30C7, 0x30BC, 0x30B2, 0x3000,
	0x30F1, 0x30EC, 0x3000, 0x30E1, 0x30D8, 0x30CD, 0x30C6, 0x30BB, 0x30B1, 0x30A8,
	0xFF1F, 0x300D, 0x30E7, 0x30A9, 0x30DD, 0x30DC, 0x30C9, 0x30BE, 0x30B4, 0x3000,
	0x30F2, 0x30ED, 0x30E8, 0x30E2, 0x30DB, 0x30CE, 0x30C8, 0x30BD, 0x30B3, 0x30AA,
	0
};

const uc16 ti_kana_romaji[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', ' ', '@', ':', '_', '~', '/', '\\',
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', ' ', '$', 165, '(', ')', '\'', '\"',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', ' ', '-', ',', '.', '!', '?', '&',
	'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', ' ', ';', '[', ']', '+', '=', '*',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ' ', '`', '#', '%', '^', '{', '}', '|', '<', '>',
	0
};

int ti_kana_numeric = 0;
int ti_kana_mode = 0;
int ti_kana_sel = 0;

extern TWidget * ti_create_tim_widget(int ht, int wd);

void ti_kana_drawcell(ttk_surface srf, int bx, int by, int x, int y, int w, int i, const uc16 * c)
{
	int cx = bx + CW*x;
	int cy = by + CH*y;
	int tx = cx + (CW*w - ttk_text_width_uc16(ttk_menufont, c) + 1)/2;
	int ty = cy + (CH*w - ttk_text_height(ttk_menufont) + 1)/2;
	if (ti_kana_sel == i) {
		ttk_fillrect(srf, cx, cy, cx+CW*w+1, cy+CH+1, ti_ap_get(2));
		ttk_text_uc16(srf, ttk_menufont, tx, ty, ti_ap_get(3), c);
	} else {
		ttk_fillrect(srf, cx, cy, cx+CW*w+1, cy+CH+1, ti_ap_get(0));
		ttk_text_uc16(srf, ttk_menufont, tx, ty, ti_ap_get(1), c);
	}
	ttk_rect(srf, cx, cy, cx+CW*w+1, cy+CH+1, ti_ap_get(1));
}

void ti_kana_draw(TWidget * wid, ttk_surface srf)
{
	int bx = wid->x + (wid->w - (CW*27+1))/2;
	int by = wid->y + 1;
	int i;
	uc16 c[2] = {0, 0};
	for (i=0; i<100; i++) {
		switch (ti_kana_mode) {
		case 0:
			c[0] = (ti_kana_hiragana[i]);
			break;
		case 1:
			c[0] = (ti_kana_katakana[i]);
			break;
		case 2:
			c[0] = (ti_kana_romaji[i]);
			break;
		}
		ti_kana_drawcell(srf, bx, by, i%20, i/20, 1, i, c);
	}
	ti_kana_drawcell(srf, bx, by, 21, 0, 3, 100, ti_kana_hiragana_glyph);
	ti_kana_drawcell(srf, bx, by, 21, 1, 3, 101, ti_kana_katakana_glyph);
	ti_kana_drawcell(srf, bx, by, 21, 2, 3, 102, ti_kana_romaji_glyph);
	ti_kana_drawcell(srf, bx, by, 24, 0, 3, 103, ti_kana_space_glyph);
	ti_kana_drawcell(srf, bx, by, 24, 1, 3, 104, ti_kana_bksp_glyph);
	ti_kana_drawcell(srf, bx, by, 24, 2, 3, 105, ti_kana_newline_glyph);
	ti_kana_drawcell(srf, bx, by, 21, 4, 3, 106, ti_kana_prev_glyph);
	ti_kana_drawcell(srf, bx, by, 24, 4, 3, 107, ti_kana_next_glyph);
}

int ti_kana_down(TWidget * wid, int btn)
{
	switch (btn) {
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
	case TTK_BUTTON_ACTION:
		if (ti_kana_sel < 100) {
			switch (ti_kana_mode) {
			case 0:
				ttk_input_char(ti_kana_hiragana[ti_kana_sel]);
				break;
			case 1:
				ttk_input_char(ti_kana_katakana[ti_kana_sel]);
				break;
			case 2:
				ttk_input_char(ti_kana_romaji[ti_kana_sel]);
				break;
			}
		} else {
			switch (ti_kana_sel) {
			case 100: if (!ti_kana_numeric) { ti_kana_mode = 0; wid->dirty = 1; } break;
			case 101: if (!ti_kana_numeric) { ti_kana_mode = 1; wid->dirty = 1; } break;
			case 102: if (!ti_kana_numeric) { ti_kana_mode = 2; wid->dirty = 1; } break;
			case 103: ttk_input_char(32); break;
			case 104: ttk_input_char(TTK_INPUT_BKSP); break;
			case 105: ttk_input_char(TTK_INPUT_ENTER); break;
			case 106: ttk_input_char(TTK_INPUT_LEFT); break;
			case 107: ttk_input_char(TTK_INPUT_RIGHT); break;
			}
		}
		break;
	default:
		return TTK_EV_UNUSED;
		break;
	}
	return TTK_EV_CLICK;
}

int ti_kana_scroll(TWidget * wid, int n)
{
	TTK_SCROLLMOD(n,4);
	
	if (n<0) {
		ti_kana_sel--;
		if (ti_kana_sel < 0) ti_kana_sel = 107;
	} else {
		ti_kana_sel++;
		if (ti_kana_sel > 107) ti_kana_sel = 0;
	}
	wid->dirty = 1;
	return TTK_EV_CLICK;
}

TWidget * ti_kana_create(int n)
{
	TWidget * wid = ti_create_tim_widget(CH*5+3, 0);
	wid->draw = ti_kana_draw;
	wid->down = ti_kana_down;
	wid->scroll = ti_kana_scroll;
	ti_kana_numeric = n;
	ti_kana_mode = (n?2:0);
	ti_kana_sel = 0;
	return wid;
}

TWidget * ti_kana_screate()
{
	return ti_kana_create(0);
}

TWidget * ti_kana_ncreate()
{
	return ti_kana_create(1);
}

void ti_kana_init()
{
	module = pz_register_module("tikana", 0);
	ti_register(ti_kana_screate, ti_kana_ncreate, N_("Kana Palette"), 17);
}

PZ_MOD_INIT(ti_kana_init)

