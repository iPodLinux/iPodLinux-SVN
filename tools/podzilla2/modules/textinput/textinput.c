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

TWidget * ti_create_tim_widget(int ht, int wd)
{
	int sh, sw;
	TWidget * wid;
	ttk_get_screensize(&sw, &sh, 0);
	wid = ttk_new_widget( ((wd>0)?(sw-wd):(-wd)), sh-ht );
	wid->h = ht;
	wid->w = ((wd>0)?(wd):(sw+wd));
	wid->focusable = 1;
	return wid;
}

/* the built-in Serial TIM */

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
	wid->down = ti_serial_down;
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
 *  8 - Dial Type
 *  9 - Telephone Keypad
 * 10 - Thumbscript
 * 11 - Four-Button Keypad
 * 12 - Dasher
 * 13 - Speech Recognition
 * 14 - Reserved
 * 15 - Reserved
 * 16 - Reserved
 * 17 - Reserved
 * 18 - Reserved
 * 19 - Reserved
 * 20 - Reserved
 * 21 - Reserved
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
}

TWindow * ti_ttkselect(ttk_menu_item * item)
{
	ti_select((int)item->data);
	return TTK_MENU_UPONE;
}

int ti_register(TWidget * (* create)(), TWidget * (* ncreate)(), char * name, int id)
{
	char menupath[128] = "/Settings/Text Input/";
	ti_tim_creators[id] = create;
	ti_tim_ncreators[id] = ncreate;
	strcat(menupath, name);
	pz_menu_add_ttkh(menupath, ti_ttkselect, (void *)id);
	if (ti_selected_tim == id) {
		ti_select(id);
	}
}

/* main text input module initialization */

void ti_free()
{
	pz_set_int_setting(ti_conf, TI_SETTING_SEL_TIM, ti_selected_tim);
	pz_save_config(ti_conf);
}

void ti_init()
{
	int i;
	ti_module = pz_register_module("textinput", ti_free);
	ti_conf = pz_load_config(pz_module_get_cfgpath(ti_module, "textinput.conf"));
	for (i=0; i<64; i++) {
		ti_tim_creators[i]=0;
		ti_tim_ncreators[i]=0;
	}
	ti_selected_tim = pz_get_int_setting(ti_conf, TI_SETTING_SEL_TIM);
	ti_register(ti_serial_create, ti_serial_create, "Serial", 0);
}

PZ_MOD_INIT(ti_init)
