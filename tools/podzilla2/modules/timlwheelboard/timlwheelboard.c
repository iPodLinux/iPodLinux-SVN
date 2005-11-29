/*
 * Copyright (C) 2005 Stefan Lange-Hegermann (BlackMac) & Jonathan Bettencourt (jonrelay)
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
#include <stdio.h>

typedef struct ti_mlwlb_cset_ {
	unsigned short chars[32];
	unsigned short nwln;
} ti_mlwlb_cset;

typedef struct ti_mlwlb_lset_ {
	ti_mlwlb_cset charsets[16];
	int csetcount;
	unsigned short endian;
} ti_mlwlb_lset;

static PzModule * module;

/* dependent on textinput module */
extern ttk_color ti_ap_get(int);
extern int ti_register(TWidget *(* cr)(), TWidget *(* ncr)(), char *, int);

const ti_mlwlb_cset ti_mlwlb_numericmode = {
	{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '-', '+', '-', '*', 'E',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '-', '+', '/', '^', 'E' },
	10
};
const ti_mlwlb_cset ti_mlwlb_cursormode = {
	{ 'M', 'o', 'v', 'e', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
	'C', 'u', 'r', 's', 'o', 'r', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' },
	10
};
static int ti_mlwlb_numeric = 0;
static ti_mlwlb_lset * ti_mlwlb_langs = 0;
static int ti_mlwlb_langcount;
static int ti_mlwlb_curlset;
static int ti_mlwlb_curcset;
static ti_mlwlb_lset ti_mlwlb_langset;
static ti_mlwlb_cset ti_mlwlb_charset;
static short ti_mlwlb_endian=0x0A00;
static int ti_mlwlb_startpos=0;
static int ti_mlwlb_limit=16;
static int ti_mlwlb_position=3;

extern TWidget * ti_create_tim_widget(int ht, int wd);

void ti_mlwlb_load_language(char * fn)
{
	ti_mlwlb_lset * l;
	FILE * fp = fopen(pz_module_get_datapath(module, fn), "rb");
	if (fp) {
		if ((
			(ti_mlwlb_langs)?
			(l = (ti_mlwlb_lset *)realloc(ti_mlwlb_langs, (ti_mlwlb_langcount+1)*(sizeof(ti_mlwlb_lset)))):
			(l = (ti_mlwlb_lset *)malloc(sizeof(ti_mlwlb_lset)))
		)) {
			ti_mlwlb_langs = l;
			ti_mlwlb_langs[ti_mlwlb_langcount].csetcount = 0;
			while (!feof(fp)) {
				fread((char *)(&ti_mlwlb_langs[ti_mlwlb_langcount].charsets[ti_mlwlb_langs[ti_mlwlb_langcount].csetcount]), sizeof(ti_mlwlb_cset), 1, fp);
				if (!ti_mlwlb_langs[ti_mlwlb_langcount].charsets[ti_mlwlb_langs[ti_mlwlb_langcount].csetcount].chars[0]) { break; }
				ti_mlwlb_langs[ti_mlwlb_langcount].csetcount++;
			}
			if (ti_mlwlb_langs[ti_mlwlb_langcount].csetcount) {
				ti_mlwlb_langs[ti_mlwlb_langcount].endian = ti_mlwlb_langs[ti_mlwlb_langcount].charsets[0].nwln;
			} else {
				ti_mlwlb_langs[ti_mlwlb_langcount].endian = 0x0A00;
			}
			ti_mlwlb_langcount++;
		}
		fclose(fp);
	}
}

void ti_mlwlb_load_all(void)
{
	char ln[64];
	FILE * fp = fopen(pz_module_get_datapath(module, "languages.lst"), "r");
	if (ti_mlwlb_langs) {
		free(ti_mlwlb_langs);
	}
	ti_mlwlb_langs = 0;
	ti_mlwlb_langcount = 0;
	if (fp) {
		while (!feof(fp)) {
			fgets(ln, 64, fp);
			if (ln[0] && (ln[0] != '\n')) {
				if (ln[strlen(ln)-1] == '\n') {
					ln[strlen(ln)-1] = 0;
				}
				ti_mlwlb_load_language(ln);
			}
		}
		fclose(fp);
	}
}

void ti_mlwlb_free_all(void)
{
	if (ti_mlwlb_langs) {
		free(ti_mlwlb_langs);
	}
	ti_mlwlb_langs = 0;
	ti_mlwlb_langcount = 0;
}

void ti_mlwlb_reset(void)
{
	ti_mlwlb_curcset = 0;
	ti_mlwlb_curlset = 0;
	if (ti_mlwlb_numeric) {
		ti_mlwlb_charset = ti_mlwlb_numericmode;
		ti_mlwlb_endian = 0x0A;
	} else {
		ti_mlwlb_langset = ti_mlwlb_langs[0];
		ti_mlwlb_charset = ti_mlwlb_langset.charsets[0];
		ti_mlwlb_endian = ti_mlwlb_langset.endian;
	}
	ti_mlwlb_startpos=0;
	ti_mlwlb_limit=16;
	ti_mlwlb_position=3;
}

void ti_mlwlb_switch_cset(void)
{
	ti_mlwlb_limit=16;
	ti_mlwlb_position=3;
	ti_mlwlb_startpos=0;
	if (ti_mlwlb_numeric) {
		if (ti_mlwlb_curcset) {
			ti_mlwlb_charset = ti_mlwlb_numericmode;
			ti_mlwlb_curcset = 0;
		} else {
			ti_mlwlb_charset = ti_mlwlb_cursormode;
			ti_mlwlb_curcset = -1;
		}
	} else {
		ti_mlwlb_curcset++;
		if (ti_mlwlb_curcset >= ti_mlwlb_langset.csetcount) { ti_mlwlb_curcset = -1; }
		if (ti_mlwlb_curcset < 0) {
			ti_mlwlb_charset = ti_mlwlb_cursormode;
		} else {
			ti_mlwlb_charset = ti_mlwlb_langset.charsets[ti_mlwlb_curcset];
		}
	}
}

void ti_mlwlb_switch_lset(void)
{
	ti_mlwlb_limit=16;
	ti_mlwlb_position=3;
	ti_mlwlb_startpos=0;
	if (ti_mlwlb_numeric) {
		if (ti_mlwlb_curcset) {
			ti_mlwlb_charset = ti_mlwlb_numericmode;
			ti_mlwlb_curcset = 0;
		} else {
			ti_mlwlb_charset = ti_mlwlb_cursormode;
			ti_mlwlb_curcset = -1;
		}
	} else {
		ti_mlwlb_curlset++;
		if (ti_mlwlb_curlset >= ti_mlwlb_langcount) { ti_mlwlb_curlset = 0; }
		ti_mlwlb_curcset = 0;
		ti_mlwlb_langset = ti_mlwlb_langs[ti_mlwlb_curlset];
		ti_mlwlb_charset = ti_mlwlb_langset.charsets[ti_mlwlb_curcset];
		ti_mlwlb_endian = ti_mlwlb_langset.endian;
	}
}

void ti_mlwlb_input(unsigned short i)
{
	if (ti_mlwlb_endian == 0x000A || ti_mlwlb_curcset < 0 || ti_mlwlb_numeric) {
		ttk_input_char(i);
	} else {
		ttk_input_char(((i & 0xFF00) >> 8) | ((i & 0x00FF) << 8));
	}
}

void ti_mlwlb_advance_level(int i)
{
	int add;
	ti_mlwlb_position=3;
	if (ti_mlwlb_limit%2!=0) 
		add=1;
	else
		add=0;
		
	if (ti_mlwlb_limit>1) {
		if (i==1) {
			ti_mlwlb_startpos=ti_mlwlb_startpos+(ti_mlwlb_limit)-add;
		}
		ti_mlwlb_limit=(ti_mlwlb_limit)/2+add;
	} else {
		if (i==1) {
			ti_mlwlb_input(ti_mlwlb_charset.chars[ti_mlwlb_startpos+1]);
		} else {
			ti_mlwlb_input(ti_mlwlb_charset.chars[ti_mlwlb_startpos]);
		}
		ti_mlwlb_limit=16;
		ti_mlwlb_startpos=0;
	}
}

void ti_mlwlb_draw(TWidget * wid, ttk_surface srf)
{
	ttk_color tcol;
	unsigned short disp[33];
	ttk_fillrect(srf, wid->x, wid->y, wid->x+wid->w, wid->y+wid->h, ti_ap_get(0));
	
	if (ti_mlwlb_position<1) {
		ttk_fillrect(srf, wid->x, wid->y, wid->x+wid->w/2, wid->y+wid->h, ti_ap_get(2));
		tcol = ti_ap_get(3);
	} else {
		tcol = ti_ap_get(1);
	}
	
	memcpy(disp, &ti_mlwlb_charset.chars[ti_mlwlb_startpos], ti_mlwlb_limit*2);
	disp[ti_mlwlb_limit]=0;
	ttk_text_uc16(srf, ttk_menufont, wid->x+1, wid->y, tcol, disp);
	
	if (ti_mlwlb_position>5) {
		ttk_fillrect(srf, wid->x+wid->w/2, wid->y, wid->x+wid->w, wid->y+wid->h, ti_ap_get(2));
		tcol = ti_ap_get(3);
	} else {
		tcol = ti_ap_get(1);
	}
	memcpy(disp, &ti_mlwlb_charset.chars[ti_mlwlb_limit+ti_mlwlb_startpos], ti_mlwlb_limit*2);
	disp[ti_mlwlb_limit]=0;
	ttk_text_uc16(srf, ttk_menufont, wid->x+wid->w/2+1, wid->y, tcol, disp);
	
	/* Position */
	ttk_line(srf, wid->x+wid->w/2, wid->y, wid->x+((ti_mlwlb_position*wid->w)/6), wid->y, ttk_makecol(GREY));
	ttk_line(srf, wid->x+wid->w/2, wid->y+1, wid->x+((ti_mlwlb_position*wid->w)/6), wid->y+1, ttk_makecol(GREY));
	ttk_line(srf, wid->x+wid->w/2, wid->y+wid->h-2, wid->x+((ti_mlwlb_position*wid->w)/6), wid->y+wid->h-2, ttk_makecol(GREY));
	ttk_line(srf, wid->x+wid->w/2, wid->y+wid->h-1, wid->x+((ti_mlwlb_position*wid->w)/6), wid->y+wid->h-1, ttk_makecol(GREY));
}

int ti_mlwlb_held(TWidget * wid, int btn)
{
	if (btn == TTK_BUTTON_ACTION) {
		ti_mlwlb_switch_lset();
		wid->dirty=1;
		return TTK_EV_CLICK;
	}
	return TTK_EV_UNUSED;
}

int ti_mlwlb_button(TWidget * wid, int btn, int t)
{
	if (btn == TTK_BUTTON_ACTION && t < 500) {
	   	ti_mlwlb_switch_cset();
		wid->dirty=1;
		return TTK_EV_CLICK;
	}
	return TTK_EV_UNUSED;
}

int ti_mlwlb_down(TWidget * wid, int btn)
{
	switch (btn)
	{
	case TTK_BUTTON_PLAY:
		ttk_input_char(TTK_INPUT_ENTER);
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
	default:
		return TTK_EV_UNUSED;
		break;
	}
	return TTK_EV_CLICK;
}

int ti_mlwlb_scroll(TWidget * wid, int dir)
{
	TTK_SCROLLMOD (dir,4)
	
	if (dir<0) {
		if (ti_mlwlb_curcset<0) {
			ttk_input_char(TTK_INPUT_LEFT);
		} else {
			if ((ti_mlwlb_position--)==0) {
				ti_mlwlb_advance_level(0);
			}
			wid->dirty=1;
		}
	} else {
		if (ti_mlwlb_curcset<0) {
			ttk_input_char(TTK_INPUT_RIGHT);
		} else {
			if ((ti_mlwlb_position++)==6) {
				ti_mlwlb_advance_level(1);
			}
			wid->dirty=1;
		}
	}
	return TTK_EV_CLICK;
}

TWidget * ti_mlwlb_create(int n)
{
	TWidget * wid = ti_create_tim_widget(-1, 0);
	wid->draw = ti_mlwlb_draw;
	wid->down = ti_mlwlb_down;
	wid->button = ti_mlwlb_button;
	wid->held = ti_mlwlb_held;
	wid->scroll = ti_mlwlb_scroll;
	wid->holdtime = 500;
	ti_mlwlb_numeric = n;
	ti_mlwlb_reset();
	return wid;
}

TWidget * ti_mlwlb_screate()
{
	return ti_mlwlb_create(0);
}

TWidget * ti_mlwlb_ncreate()
{
	return ti_mlwlb_create(1);
}

void ti_mlwlb_init()
{
	module = pz_register_module("timlwheelboard", ti_mlwlb_free_all);
	ti_register(ti_mlwlb_screate, ti_mlwlb_ncreate, _("Multilingual Wheelboard"), 18);
	ti_mlwlb_load_all();
}

PZ_MOD_INIT(ti_mlwlb_init)

