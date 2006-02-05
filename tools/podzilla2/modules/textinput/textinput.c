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
 *  /!\ Warning! This is NOT the same as legacy textinput.c!
 */

#include "pz.h"
#include <string.h>

/* defines for text input */

#define TI_MAX_TIMS 64
#define TI_SETTING_SEL_TIM 1

/* variables for text input */

static PzModule * ti_module;
static PzConfig * ti_conf;
static int ti_selected_tim;
static TWidget * (* ti_tim_creators[TI_MAX_TIMS])();
static TWidget * (* ti_tim_ncreators[TI_MAX_TIMS])();

/* API calls for TIM widgets */

TApItem * ti_ap_getx(int i)
{
	TApItem * tai;
	switch (i) {
	case 0:
		tai = ttk_ap_get("input.bg");
		if (!tai) { tai = ttk_ap_getx("window.bg"); }
		return tai;
		break;
	case 1:
		tai = ttk_ap_get("input.fg");
		if (!tai) { tai = ttk_ap_getx("window.fg"); }
		return tai;
		break;
	case 2:
		tai = ttk_ap_get("input.selbg");
		if (!tai) { tai = ttk_ap_getx("menu.selbg"); }
		return tai;
		break;
	case 3:
		tai = ttk_ap_get("input.selfg");
		if (!tai) { tai = ttk_ap_getx("menu.selfg"); }
		return tai;
		break;
	case 4:
		tai = ttk_ap_get("input.border");
		if (!tai) { tai = ttk_ap_getx("window.border"); }
		return tai;
		break;
	case 5:
		tai = ttk_ap_get("input.cursor");
		if (!tai) { tai = ttk_ap_getx("window.border"); }
		return tai;
		break;
	}
	return 0;
}

ttk_color ti_ap_get(int i)
{
	TApItem * tai = ti_ap_getx(i);
	if (!tai) {
		switch (i) {
		case 0:
		case 3:
			return ttk_makecol(WHITE);
			break;
		case 4:
		case 5:
			return ttk_makecol(GREY);
			break;
		case 2:
			return ttk_makecol(DKGREY);
			break;
		default:
			return ttk_makecol(BLACK);
			break;
		}
	} else {
		return tai->color;
	}
}

TWidget * ti_create_tim_widget(int ht, int wd)
{
	int sh, sw;
	TWidget * wid;
	ttk_get_screensize(&sw, &sh, 0);
	wid = ttk_new_widget( ((wd>0)?(sw-wd):(-wd)), sh-((ht<0)?(ttk_text_height(ttk_menufont)*(-ht)+1):ht) );
	wid->h = ((ht<0)?(ttk_text_height(ttk_menufont)*(-ht)+1):ht);
	wid->w = ((wd>0)?(wd):(sw+wd));
	wid->focusable = 1;
	return wid;
}

/* the built-in Serial TIM */

#ifdef IPOD
int ti_serial_abort(TWidget * this, int ch) 
{
	if (ch == TTK_BUTTON_ACTION) {
		pz_warning (_("I'd suggest picking another text input method before you try that again."));
		ttk_input_end();
		return TTK_EV_CLICK;
	}
	return TTK_EV_UNUSED;
}
#endif

int ti_serial_down(TWidget * this, int ch)
{
	if (ch == 27) {
		ttk_input_end();
		return TTK_EV_CLICK;
	} else if (ch == 10 || ch == 13) {
		ttk_input_char(TTK_INPUT_ENTER);
		return TTK_EV_CLICK;
	} else if (ch < 256) {
		ttk_input_char(ch);
		return TTK_EV_CLICK;
	}
	return TTK_EV_UNUSED;
}

TWidget * ti_serial_create()
{
	TWidget * wid = ti_create_tim_widget(0, 0);
	wid->rawkeys = 1;
	wid->keyrepeat = 1;
	wid->down = ti_serial_down;
#ifdef IPOD
	wid->held = ti_serial_abort;
	wid->holdtime = 3000;
#endif
	return wid;
}

/*
 * Known TIM ID numbers:
 *  0 - Serial (was Off)
 *  1 - Reserved (was Serial)
 *  2 - Scroll Through
 *  3 - On-Screen Keyboard
 *  4 - Morse Code
 *  5 - Cursive
 *  6 - Wheelboard
 *  7 - Four-Button Keyboard
 *  8 - Scroll Through with Return
 *  9 - Telephone Keypad
 * 10 - Thumbscript
 * 11 - Four-Button Keypad
 * 12 - Dasher
 * 13 - Speech Recognition
 * 14 - FreqMod's TIM (http://freqmod.dyndns.org/upload/ipnno.png)
 * 15 - Reserved
 * 16 - Unicode Hex Input
 * 17 - Kana Palette
 * 18 - Multilingual Wheelboard
 * 19 - Reserved
 * 20 - Scroll Through with Fixed Layout
 * 21 - Scroll Through with Prediction
 * 22 - Reserved
 * 23 - Reserved
 *
 * All other ID numbers (24-63) are free for other devs to use.
 */

/* API calls for TIM modules */

int ti_select(int id)
{
	pz_register_input_method(ti_tim_creators[id]);
	pz_register_input_method_n(ti_tim_ncreators[id]);
	ti_selected_tim = id;
	pz_set_int_setting(ti_conf, TI_SETTING_SEL_TIM, ti_selected_tim);
	pz_save_config(ti_conf);
	return 0;
}

TWindow * ti_ttkselect(ttk_menu_item * item)
{
	ti_select((long)item->data);
	return TTK_MENU_UPONE;
}

int ti_register(TWidget * (* create)(), TWidget * (* ncreate)(), char * name, int id)
{
	char menupath[128] = "/Settings/Text Input/";
	ti_tim_creators[id] = create;
	ti_tim_ncreators[id] = ncreate;
	strcat(menupath, name);
	pz_menu_add_ttkh(menupath, ti_ttkselect, (void *)(long)id);
	if (ti_selected_tim == id) {
		ti_select(id);
	}
	return 0;
}

/* main text input module initialization */

void ti_init()
{
	int i;
	ti_module = pz_register_module("textinput", 0);
	ti_conf = pz_load_config(pz_module_get_cfgpath(ti_module, "textinput.conf"));
	for (i=0; i<64; i++) {
		ti_tim_creators[i]=0;
		ti_tim_ncreators[i]=0;
	}
	ti_selected_tim = pz_get_int_setting(ti_conf, TI_SETTING_SEL_TIM);
#ifdef IPOD
	if (!ti_selected_tim) {
		pz_warning (_("Serial text input is selected. Make sure a keyboard is attached or select a different input method."));
	}
#endif
	ti_register(ti_serial_create, ti_serial_create, N_("Serial"), 0);
}

PZ_MOD_INIT(ti_init)
