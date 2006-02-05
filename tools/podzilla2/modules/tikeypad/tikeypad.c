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

/*
 *  /!\ Warning! This is NOT the same as legacy keypad.c!
 */

#include "pz.h"
#include <string.h>

/* dependent on textinput module */
extern ttk_color ti_ap_get(int);
extern TApItem * ti_ap_getx(int);
extern int ti_register(TWidget *(* cr)(), TWidget *(* ncr)(), char *, int);
extern TWidget * ti_create_tim_widget(int ht, int wd);

static PzModule * module;

const char * ti_keypad_buttons[] = {
	" \x08.,10!?\x0A:;\"\'\xA1\xBF", "abcABC2@/\\_|\xA6", "defDEF3#\xBA\xAA\xB9\xB2\xB3",
	"ghiGHI4$\xA2\xA3\xA4\xA5", "jklJKL5%\xBC\xBD\xBE", "mnoMNO6^~`\xB4\xA8\xB8",
	"pqrsPQRS7&\xA7\xB6\xB0\xA9\xAE\xB5", "tuvTUV8*+-=<>", "wxyzWXYZ9()[]{}"
};
const char * ti_keypad_buttons_short[] = { "10.,!", "2abc@", "3def#", "4ghi$", "5jkl%", "6mno^", "7pqrs&", "8tuv*", "9wxyz(" };
const char * ti_keypad_buttons_n[] = { "10.-\x08""E+-*/^", "2", "3", "4", "5", "6", "7", "8", "9" };
const char * ti_keypad_buttons_short_n[] = { "10.-", "2", "3", "4", "5", "6", "7", "8", "9" };

static int ti_keypad_row = 0;
static int ti_keypad_curr_btn = 0;
static int ti_keypad_curr_char = 0;
static int ti_keypad_ptext = 0;
static int ti_keypad_numeric = 0;
static char ti_keypad_ptext_buffer[32] = {0};

/* predictive text calls; soft dependency */

static int (*ti_keypad_ptext_inited)(void) = 0;
static int (*ti_keypad_ptext_init)(void) = 0;
static void (*ti_keypad_ptext_free)(void) = 0;
static int (*ti_keypad_ptext_predict)(char * buf, int pos, int method) = 0;

#define TI_KEYPAD_PTEXT (ti_keypad_ptext_inited && ti_keypad_ptext_init && ti_keypad_ptext_free && ti_keypad_ptext_predict)

void ti_keypad_fbdraw(TWidget * wid, ttk_surface srf)
{
	int i;
	char s[2];
	ttk_ap_fillrect(srf, ti_ap_getx(0), wid->x, wid->y, wid->x+wid->w, wid->y+wid->h);
	if (ti_keypad_numeric) {
		for (i=0; i<3; i++) {
			ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/4)*i+1, wid->y, ti_ap_get(1), (char *)ti_keypad_buttons_short_n[ti_keypad_row*3 + i]);
		}
	} else {
		for (i=0; i<3; i++) {
			ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/4)*i+1, wid->y, ti_ap_get(1), (char *)ti_keypad_buttons_short[ti_keypad_row*3 + i]);
		}
		if (ti_keypad_ptext && TI_KEYPAD_PTEXT) {
			ttk_text_lat1(srf, ttk_menufont, wid->x+1, wid->y+ttk_text_height(ttk_menufont), ti_ap_get(1), ti_keypad_ptext_buffer);
			ttk_text_lat1(srf, ttk_menufont, wid->x+wid->w-10, wid->y+ttk_text_height(ttk_menufont)/2, ti_ap_get(1), "P");
		}
	}
	if (ti_keypad_curr_char != 0) {
		if (ti_keypad_numeric) {
			s[0] = ti_keypad_buttons_n[ti_keypad_curr_btn][ti_keypad_curr_char - 1];
		} else {
			s[0] = ti_keypad_buttons[ti_keypad_curr_btn][ti_keypad_curr_char - 1];
		}
		s[1]=0;
		switch (s[0]) {
		case  10: ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/4)*3+1, wid->y+ttk_text_height(ttk_menufont)/2, ti_ap_get(1), "nwln"); break;
		case  32: ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/4)*3+1, wid->y+ttk_text_height(ttk_menufont)/2, ti_ap_get(1), "sp"); break;
		case   8: ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/4)*3+1, wid->y+ttk_text_height(ttk_menufont)/2, ti_ap_get(1), "bs"); break;
		case 127: ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/4)*3+1, wid->y+ttk_text_height(ttk_menufont)/2, ti_ap_get(1), "del"); break;
		case   9: ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/4)*3+1, wid->y+ttk_text_height(ttk_menufont)/2, ti_ap_get(1), "tab"); break;
		case  28: ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/4)*3+1, wid->y+ttk_text_height(ttk_menufont)/2, ti_ap_get(1), "left"); break;
		case  29: ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/4)*3+1, wid->y+ttk_text_height(ttk_menufont)/2, ti_ap_get(1), "right"); break;
		default:
			ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/4)*3+1, wid->y+ttk_text_height(ttk_menufont)/2, ti_ap_get(1), s);
		}
	}
}

void ti_keypad_4gdraw(TWidget * wid, ttk_surface srf)
{
	char s[2];
	ttk_ap_fillrect(srf, ti_ap_getx(0), wid->x, wid->y, wid->x+wid->w, wid->y+wid->h);
	if (ti_keypad_ptext && TI_KEYPAD_PTEXT) {
		ttk_text_lat1(srf, ttk_menufont, wid->x+1, wid->y, ti_ap_get(1), ti_keypad_ptext_buffer);
		ttk_text_lat1(srf, ttk_menufont, wid->x+wid->w-10, wid->y, ti_ap_get(1), "P");
	}
	if (ti_keypad_curr_char != 0) {
		if (ti_keypad_numeric) {
			s[0] = ti_keypad_buttons_n[ti_keypad_curr_btn][ti_keypad_curr_char - 1];
		} else {
			s[0] = ti_keypad_buttons[ti_keypad_curr_btn][ti_keypad_curr_char - 1];
		}
		s[1]=0;
		switch (s[0]) {
		case  10: ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/2), wid->y, ti_ap_get(1), "nwln"); break;
		case  32: ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/2), wid->y, ti_ap_get(1), "sp"); break;
		case   8: ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/2), wid->y, ti_ap_get(1), "bs"); break;
		case 127: ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/2), wid->y, ti_ap_get(1), "del"); break;
		case   9: ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/2), wid->y, ti_ap_get(1), "tab"); break;
		case  28: ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/2), wid->y, ti_ap_get(1), "left"); break;
		case  29: ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/2), wid->y, ti_ap_get(1), "right"); break;
		default:
			ttk_text_lat1(srf, ttk_menufont, wid->x+(wid->w/2), wid->y, ti_ap_get(1), s);
		}
	}
}

void ti_keypad_output(void)
{
	int ch;
	int l, i;
	if (ti_keypad_curr_char != 0)
	{
		if (ti_keypad_numeric) {
			ttk_input_char(ti_keypad_buttons_n[ti_keypad_curr_btn][ti_keypad_curr_char - 1]);
		} else if (ti_keypad_ptext && TI_KEYPAD_PTEXT) {
			if (ti_keypad_curr_btn == 0) {
				ch = ti_keypad_buttons[ti_keypad_curr_btn][ti_keypad_curr_char - 1];
			} else {
				ch = ti_keypad_buttons_n[ti_keypad_curr_btn][ti_keypad_curr_char - 1];
			}
			if ((ch>='0')&&(ch<='9')) {
				l = strlen(ti_keypad_ptext_buffer);
				ti_keypad_ptext_buffer[l++] = ch;
				ti_keypad_ptext_buffer[l] = 0;
				ti_keypad_ptext_predict(ti_keypad_ptext_buffer, l, 1);
			} else {
				l = strlen(ti_keypad_ptext_buffer);
				ti_keypad_ptext_predict(ti_keypad_ptext_buffer, l, 1);
				for (i=0; i<l; i++) ttk_input_char(ti_keypad_ptext_buffer[i]);
				ttk_input_char(ch);
				ti_keypad_ptext_buffer[0] = 0;
			}
		} else {
			ttk_input_char(ti_keypad_buttons[ti_keypad_curr_btn][ti_keypad_curr_char - 1]);
		}
	}
	ti_keypad_curr_char = 0;
}

void ti_keypad_switch_row(void)
{
	ti_keypad_output();
	ti_keypad_row++;
	if (ti_keypad_row >= 3) { ti_keypad_row = 0; }
}

void ti_keypad_reset(void)
{
	ti_keypad_row = 0;
	ti_keypad_curr_btn = 0;
	ti_keypad_curr_char = 0;
	ti_keypad_ptext_buffer[0] = 0;
}

void ti_keypad_push(int n)
{
	/* this takes 0-8, NOT 1-9 */
	if ((n != 0) && (ti_keypad_numeric || (ti_keypad_ptext && TI_KEYPAD_PTEXT))) {
		ti_keypad_output();
		ti_keypad_curr_btn = n;
		ti_keypad_curr_char = 1;
		ti_keypad_output();
	} else if (n == ti_keypad_curr_btn) {
		ti_keypad_curr_char++;
		if (ti_keypad_curr_char > (int)strlen(ti_keypad_numeric?ti_keypad_buttons_n[ti_keypad_curr_btn]:ti_keypad_buttons[ti_keypad_curr_btn])) { ti_keypad_curr_char = 1; }
	} else {
		ti_keypad_output();
		ti_keypad_curr_btn = n;
		ti_keypad_curr_char = 1;
	}
}

void ti_keypad_ptext_toggle(void)
{
	if (!TI_KEYPAD_PTEXT) {
		ti_keypad_ptext = 0;
		return;
	}
	ti_keypad_ptext = (!ti_keypad_ptext);
	if (ti_keypad_ptext) {
		if (!ti_keypad_ptext_inited()) {
			/* ti_draw_message(_("Loading ptext...")); */
			ti_keypad_ptext_buffer[0] = 0;
			ti_keypad_ptext_init();
		}
	} else {
		ti_keypad_ptext_free();
	}
}

int ti_keypad_button_map(int c)
{
	/*
		0 - switch rows
		1-3 - keypad buttons
		4 - exit
	*/
	if (ttk_get_podversion() & TTK_POD_3G) {
		switch (c) {
		case TTK_BUTTON_PREVIOUS:
			return 1;
			break;
		case TTK_BUTTON_MENU:
			return 2;
			break;
		case TTK_BUTTON_PLAY:
			return 3;
			break;
		case TTK_BUTTON_NEXT:
			return 0;
			break;
		case TTK_BUTTON_ACTION:
			return 4;
			break;
		}
	} else {
		switch (c) {
		case TTK_BUTTON_PREVIOUS:
			return 1;
			break;
		case TTK_BUTTON_ACTION:
			return 2;
			break;
		case TTK_BUTTON_NEXT:
			return 3;
			break;
		case TTK_BUTTON_PLAY:
			return 0;
			break;
		case TTK_BUTTON_MENU:
			return 4;
			break;
		}
	}
	return -1;
}

void ti_keypad_fbpush(int n)
{
	if (n == 0) {
		ti_keypad_switch_row();
		ti_keypad_curr_char = 0;
	} else if (n == 4) {
		ttk_input_end();
	} else if (n > 0) {
		ti_keypad_push(ti_keypad_row*3 + n - 1);
	}
}

/* implementation */

int ti_keypad_timer(TWidget * wid)
{
	ti_keypad_output();
	wid->dirty = 1;
	ttk_widget_set_timer(wid, 0);
	return TTK_EV_CLICK;
}

int ti_keypad_stap(TWidget * wid, int n)
{
	n += 6;
	n /= 12;
	if (n>7) n=0;
	
	switch (n) {
	case 0: ti_keypad_push(1); break;
	case 1: ti_keypad_push(2); break;
	case 2: ti_keypad_push(5); break;
	case 3: ti_keypad_push(8); break;
	case 4: ti_keypad_push(7); break;
	case 5: ti_keypad_push(6); break;
	case 6: ti_keypad_push(3); break;
	case 7: ti_keypad_push(0); break;
	}
	wid->dirty = 1;
	ttk_widget_set_timer(wid, 1000);
	return TTK_EV_CLICK;
}

int ti_keypad_held(TWidget * wid, int btn)
{
	if (btn != TTK_BUTTON_ACTION) return TTK_EV_UNUSED;
	ti_keypad_ptext_toggle();
	wid->dirty = 1;
	return TTK_EV_CLICK;
}

int ti_keypad_scroll(TWidget * wid, int dir)
{
	TTK_SCROLLMOD (dir,4);
	
	if (dir<0) {
		ttk_input_char(TTK_INPUT_LEFT);
	} else {
		ttk_input_char(TTK_INPUT_RIGHT);
	}
	return TTK_EV_CLICK;
}

int ti_keypad_fbup(TWidget * wid, int btn, int t)
{
	if (btn != TTK_BUTTON_ACTION || t >= 1000) return TTK_EV_UNUSED;
	ti_keypad_fbpush(ti_keypad_button_map(btn));
	wid->dirty = 1;
	ttk_widget_set_timer(wid, 1000);
	return TTK_EV_CLICK;
}

int ti_keypad_fbdown(TWidget * wid, int btn)
{
	switch (btn)
	{
	case TTK_BUTTON_PREVIOUS:
	case TTK_BUTTON_NEXT:
	case TTK_BUTTON_PLAY:
	case TTK_BUTTON_MENU:
		ti_keypad_fbpush(ti_keypad_button_map(btn));
		wid->dirty = 1;
		ttk_widget_set_timer(wid, 1000);
		return TTK_EV_CLICK;
		break;
	}
	return TTK_EV_UNUSED;
}

int ti_keypad_4gup(TWidget * wid, int btn, int t)
{
	if (btn != TTK_BUTTON_ACTION || t >= 1000) return TTK_EV_UNUSED;
	ti_keypad_push(4);
	wid->dirty = 1;
	ttk_widget_set_timer(wid, 1000);
	return TTK_EV_CLICK;
}

int ti_keypad_4gdown(TWidget * wid, int btn)
{
	switch (btn) {
	case TTK_BUTTON_PREVIOUS:
		ttk_input_char(TTK_INPUT_LEFT);
		break;
	case TTK_BUTTON_NEXT:
		ttk_input_char(TTK_INPUT_RIGHT);
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

TWidget * ti_keypad_create(int fb, int n)
{
	TWidget * wid = ti_create_tim_widget(((fb)?(-2):(-1)), 0);
	wid->draw = (fb?ti_keypad_fbdraw:ti_keypad_4gdraw);
	wid->down = (fb?ti_keypad_fbdown:ti_keypad_4gdown);
	wid->button = (fb?ti_keypad_fbup:ti_keypad_4gup);
	wid->timer = ti_keypad_timer;
	wid->scroll = ti_keypad_scroll;
	if (!fb) wid->stap = ti_keypad_stap;
	wid->holdtime = 1000;
	ti_keypad_numeric = n;
	ti_keypad_reset();
	return wid;
}

TWidget * ti_keypad_4gcreate()
{
	return ti_keypad_create(0, 0);
}

TWidget * ti_keypad_4gncreate()
{
	return ti_keypad_create(0, 1);
}

TWidget * ti_keypad_fbcreate()
{
	return ti_keypad_create(1, 0);
}

TWidget * ti_keypad_fbncreate()
{
	return ti_keypad_create(1, 1);
}

void ti_keypad_init()
{
	module = pz_register_module("tikeypad", 0);
	ti_register(ti_keypad_4gcreate, ti_keypad_4gncreate, N_("Telephone Keypad"), 9);
	ti_register(ti_keypad_fbcreate, ti_keypad_fbncreate, N_("Four-Button Telephone Keypad"), 11);
	
	ti_keypad_ptext_inited = pz_module_softdep("tiptext", "ti_ptext_inited");
	ti_keypad_ptext_init = pz_module_softdep("tiptext", "ti_ptext_init");
	ti_keypad_ptext_free = pz_module_softdep("tiptext", "ti_ptext_free");
	ti_keypad_ptext_predict = pz_module_softdep("tiptext", "ti_ptext_predict");
}

PZ_MOD_INIT(ti_keypad_init)

