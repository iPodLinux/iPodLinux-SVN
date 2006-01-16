/*
 * Copyright (C) 2005-2006 Jonathan Bettencourt (jonrelay)
 *
 * Scroll Pad concept by veteran.
 * Dial Type concept by Peter Burk.
 * Fixed Layout and Scroll with Prediction concepts by mp.
 * All implemented by jonrelay.
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
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* dependent on textinput module */
extern ttk_color ti_ap_get(int);
extern int ti_register(TWidget *(* cr)(), TWidget *(* ncr)(), char *, int);

static PzModule * module;

const char ti_dial_charlist[] = " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ=+?!@#$%^&*()[]{}_-<>~`\"\'1234567890\\/|:;.,M";
const char ti_dial_charlist_n[] = ".0123456789-E+-*/^M";
const char ti_dial_cmstring[] = "Move Cursor";
const int ti_dial_flchars[][32] = {
	{' ','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z','!','?',',','.','\''},
	{' ','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',161,191,';',':','\"'},
	{'.','0','1','2','3','4','5','6','7','8','9','.','-','E','0','1','2','3','4','5','6','7','8','9','.','-','E','+','-','*','/','^'},
	{' ','@','#','$','%','&','(',')','[',']','{','}','<','>','=','_','|','\\','~','`',180,168,184,163,165,162,171,187,215,247,177,176},
	{' ',224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,248,249,250,251,252,253,254,255},
	{' ',192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,216,217,218,219,220,221,222,376},
	{160,338,339,223,402,169,174,8482,170,186,185,178,179,188,189,190,167,182,181,166,164,175,172,173,183,8364,8211,8212,8216,8217,8220,8221}
};
const char ti_dial_eta_lc[] = "etaoinshrdlucmfwypvbgkqjxz"; //26
const char ti_dial_eta_uc[] = "ETAOINSHRDLUCMFWYPVBGKQJXZ"; //26
const char ti_dial_number[] = "0123456789"; //10
const char ti_dial_stuff1[] = "=+?!@#$%^&*()[]{}_-<>~`"; //23
const char ti_dial_stuff2[] = "\"\'\\/|:;.,M"; //10
const char ti_dial_numberstarters[] = "+*=^#$%<>0123456789";
static int ti_dial_mode = 0;
/*
	 -2 - old numeric cursormode
	 -1 - old cursormode
	  0 - old charlist
	  1 - old numeric
	2-8 - fixed layout
	  9 - fixed layout cursormode
	 10 - prediction
	 11 - prediction cursormode
*/
static int ti_dial_charlist_pos = 0;
static int ti_dial_snapback = 0;
static int ti_dial_ppchar = 0;
static int ti_dial_pchar = 0;
static char ti_dial_predict[64][64][5];

extern TWidget * ti_create_tim_widget(int ht, int wd);


#define C2N(c) (((c)<32)?(0):(((c)<96)?((c)-32):(((c)<127)?((c)-64):(0))))

int ti_dial_max(void)
{
	switch (ti_dial_mode) {
	case -2:	case -1:
	case  9:	case 11:
		return 10;
		break;
	case 0:
		return 95;
		break;
	case 1:
		return 18;
		break;
	case 2:	case 3:	case 4:	case 5:
	case 6:	case 7:	case 8:
		return 39;
		break;
	case 10:
		return 100;
		break;
	}
	return 0;
}

int ti_dial_cursormode(void)
{
	return (ti_dial_mode == -2 || ti_dial_mode == -1 || ti_dial_mode == 9 || ti_dial_mode == 11);
}

void ti_dial_togglecursor(void)
{
	switch (ti_dial_mode) {
	case -2:
		ti_dial_mode = 1;
		break;
	case -1:
		ti_dial_mode = 0;
		break;
	case 0:
		ti_dial_mode = -1;
		break;
	case 1:
		ti_dial_mode = -2;
		break;
	case 2:	case 3:	case 4:	case 5:
	case 6:	case 7:	case 8:
		ti_dial_mode = 9;
		break;
	case 9:
		ti_dial_mode = 2;
		break;
	case 10:
		ti_dial_mode = 11;
		break;
	case 11:
		ti_dial_mode = 10;
		break;
	}
}

int ti_dial_get_char(int w)
{
	if (w<0) {
		w = ti_dial_charlist_pos;
	}
	switch (ti_dial_mode) {
	case -2:	case -1:
	case  9:	case 11:
		return ti_dial_cmstring[w];
		break;
	case 0:
		return ti_dial_charlist[w];
		break;
	case 1:
		return ti_dial_charlist_n[w];
		break;
	case 2:	case 3:	case 4:	case 5:
	case 6:	case 7:	case 8:
		switch (w) {
		case 32: return 'a'; break;
		case 33: return 'A'; break;
		case 34: return '0'; break;
		case 35: return '@'; break;
		case 36: return 228; break;
		case 37: return 196; break;
		case 38: return 169; break;
		case 39: return 'M'; break;
		default:
			return ti_dial_flchars[ti_dial_mode-2][w];
			break;
		}
		break;
	case 10:
		if (w == 0) {
			return ' ';
		} else if (w<6) {
			int c = ti_dial_predict[C2N(ti_dial_ppchar)][C2N(ti_dial_pchar)][w-1];
			if (!c) c = ti_dial_predict[10][C2N(ti_dial_pchar)][w-1];
			if (!c) c = ti_dial_predict[C2N(ti_dial_ppchar)][10][w-1];
			if (!c) c = ti_dial_predict[10][10][w-1];
			return c;
		} else {
			int c = ti_dial_predict[C2N(ti_dial_ppchar)][C2N(ti_dial_pchar)][0];
			if (!c) c = ti_dial_predict[10][C2N(ti_dial_pchar)][0];
			if (!c) c = ti_dial_predict[C2N(ti_dial_ppchar)][10][0];
			if (!c) c = ti_dial_predict[10][10][0];
			w -= 6;
			if (strchr(ti_dial_numberstarters,c) || isdigit(c)) {
				if (w<10) { return ti_dial_number[w]; } else { w -= 10; }
				if (w<23) { return ti_dial_stuff1[w]; } else { w -= 23; }
				if (w<26) { return ti_dial_eta_lc[w]; } else { w -= 26; }
				if (w<26) { return ti_dial_eta_uc[w]; } else { w -= 26; }
				if (w<10) { return ti_dial_stuff2[w]; } else { w -= 10; }
				return ' ';
			} else if (isupper(c)) {
				if (w<26) { return ti_dial_eta_uc[w]; } else { w -= 26; }
				if (w<26) { return ti_dial_eta_lc[w]; } else { w -= 26; }
				if (w<23) { return ti_dial_stuff1[w]; } else { w -= 23; }
				if (w<10) { return ti_dial_number[w]; } else { w -= 10; }
				if (w<10) { return ti_dial_stuff2[w]; } else { w -= 10; }
				return ' ';
			} else {
				if (w<26) { return ti_dial_eta_lc[w]; } else { w -= 26; }
				if (w<26) { return ti_dial_eta_uc[w]; } else { w -= 26; }
				if (w<23) { return ti_dial_stuff1[w]; } else { w -= 23; }
				if (w<10) { return ti_dial_number[w]; } else { w -= 10; }
				if (w<10) { return ti_dial_stuff2[w]; } else { w -= 10; }
				return ' ';
			}
		}
		break;
	}
	return 0;
}

void ti_dial_next_char(void)
{
	ti_dial_charlist_pos++;
	if (ti_dial_charlist_pos > ti_dial_max()) { ti_dial_charlist_pos = 0; }
}

void ti_dial_prev_char(void)
{
	ti_dial_charlist_pos--;
	if (ti_dial_charlist_pos < 0) { ti_dial_charlist_pos = ti_dial_max(); }
}

void ti_dial_reset(void)
{
	ti_dial_charlist_pos = 0;
}

int ti_dial_rotatechar(int a)
{
	ti_dial_ppchar = ti_dial_pchar;
	ti_dial_pchar = a;
	return a;
}

void ti_dial_predict_clear(void)
{
	int i;
	for (i=0; i<(64*64*5); i++) {
		((char *)ti_dial_predict)[i] = 0;
	}
}

void ti_dial_predict_load(const char * fn)
{
	FILE * fp;
	char buf[120];
	ti_dial_predict_clear();
	fp = fopen(fn, "r");
	if (fp) {
		while (fgets(buf, 120, fp)) {
			if (strlen(buf)>=7 && !(buf[0]=='#' && buf[1]=='#')) {
				strncpy(ti_dial_predict[C2N(buf[0])][C2N(buf[1])], &buf[2], 5);
			}
		}
		fclose(fp);
	}
}

#define CW 10 /* width of the space for each char */

void ti_dial_draw(TWidget * wid, ttk_surface srf)
{
	int sc, n, i, j, ty;
	int m = ti_dial_max()+1;
	uc16 s[2] = {0,0};
	ttk_fillrect(srf, wid->x, wid->y, wid->x+wid->w, wid->y+wid->h, ti_ap_get(0));
	n = wid->w / CW;
	sc = ti_dial_charlist_pos - n/2;
	ty = 0;
	if (sc > (m-n) ) { sc = (m-n); }
	if (sc < 0) { sc = 0; }
	if (ti_dial_cursormode()) {
		ttk_text(srf, ttk_menufont, wid->x+1, wid->y+ty, ti_ap_get(1), ti_dial_cmstring);
	} else {
		for (i = sc, j = 0; ((i<m) && (j<n)); i++, j++)
		{
			s[0] = ti_dial_get_char(i);
			ttk_text_uc16(srf, ttk_menufont, wid->x+j*CW+(10-ttk_text_width_uc16(ttk_menufont, s))/2, wid->y+ty, ti_ap_get(1), s);
			if (i == ti_dial_charlist_pos) {
				ttk_rect(srf, wid->x+j*CW, wid->y, wid->x+j*CW+CW, wid->y+wid->h, ti_ap_get(1));
			}
		}
	}
}

int ti_dial_scroll(TWidget * wid, int dir)
{
	TTK_SCROLLMOD (dir, 4);
	
	if (dir<0) {
		if (ti_dial_cursormode()) {
			ttk_input_char(TTK_INPUT_LEFT);
		} else {
			ti_dial_prev_char();
			wid->dirty = 1;
		}
	} else {
		if (ti_dial_cursormode()) {
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
		if (ti_dial_cursormode() || ti_dial_charlist_pos == ti_dial_max()) {
			ti_dial_togglecursor();
			wid->dirty = 1;
		} else if (ti_dial_mode >= 2 && ti_dial_mode <= 8 && ti_dial_charlist_pos >= 32) {
			ti_dial_mode = (ti_dial_charlist_pos - 30);
			ti_dial_reset();
			wid->dirty = 1;
		} else {
			ttk_input_char(ti_dial_rotatechar(ti_dial_get_char(-1)));
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
		ttk_input_char(ti_dial_rotatechar(32));
		break;
	case TTK_BUTTON_PLAY:
		ttk_input_char(ti_dial_rotatechar(TTK_INPUT_ENTER));
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

int ti_dial_stap(TWidget * wid, int p)
{
	int m;
	if (ti_dial_cursormode()) { return TTK_EV_UNUSED; }
	m = ti_dial_max()+1;
	ti_dial_charlist_pos = (p*m)/96;
	wid->dirty = 1;
	return TTK_EV_CLICK;
}

/*
	When calling ti_dial_create:
	The first parameter is the starting ti_dial_mode
	The second parameter is a boolean indicating if the thing should
		snap back to the beginning after input.
	The third parameter is a boolean indicating if staps should be
		caught and the position set to the position on the wheel.
*/

TWidget * ti_dial_create(int n, int s, int st)
{
	TWidget * wid = ti_create_tim_widget(-1, 0);
	wid->down = ti_dial_down;
	wid->scroll = ti_dial_scroll;
	wid->draw = ti_dial_draw;
	if (st) {
		wid->stap = ti_dial_stap;
	}
	ti_dial_mode = n;
	ti_dial_snapback = s;
	ti_dial_reset();
	ti_dial_ppchar = 0;
	ti_dial_pchar = 0;
	return wid;
}

TWidget * ti_dial_stcreate()
{
	return ti_dial_create(0, 0, 0);
}

TWidget * ti_dial_stncreate()
{
	return ti_dial_create(1, 0, 0);
}

TWidget * ti_dial_dtcreate()
{
	return ti_dial_create(0, 1, 0);
}

TWidget * ti_dial_dtncreate()
{
	return ti_dial_create(1, 1, 0);
}

TWidget * ti_dial_ftcreate()
{
	return ti_dial_create(2, 0, 1);
}

TWidget * ti_dial_ftncreate()
{
	return ti_dial_create(1, 0, 1);
}

TWidget * ti_dial_ptcreate()
{
	return ti_dial_create(10, 1, 0);
}

TWidget * ti_dial_ptncreate()
{
	return ti_dial_create(1, 1, 0);
}

void ti_dial_init()
{
	module = pz_register_module("tidial", 0);
	ti_register(ti_dial_stcreate, ti_dial_stncreate, N_("Scroll-Through"), 2);
	ti_register(ti_dial_dtcreate, ti_dial_dtncreate, N_("Scroll with Return"), 8);
	ti_register(ti_dial_ftcreate, ti_dial_ftncreate, N_("Scroll with Fixed Layout"), 20);
	ti_register(ti_dial_ptcreate, ti_dial_ptncreate, N_("Scroll with Prediction"), 21);
	ti_dial_predict_load(pz_module_get_datapath(module, "predict.txt"));
}

PZ_MOD_INIT(ti_dial_init)

