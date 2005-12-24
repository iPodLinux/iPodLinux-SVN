/*
 * Copyright (C) 2005 Jonathan Bettencourt (jonrelay)
 * Four-Button Keyboard concept by Jonathan Bettencourt (jonrelay)
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
 *  /!\ Warning! This is NOT the same as legacy fourbtnkbd.c!
 */

#include "pz.h"
#include <string.h>

static PzModule * module;

/* dependent on textinput module */
extern ttk_color ti_ap_get(int);
extern int ti_register(TWidget *(* cr)(), TWidget *(* ncr)(), char *, int);

const unsigned long int ti_fbk_MSlongs[] = {
	0x01020304, 0x05060708, 0x090A0B0C, 0x0D0E0F10, 0x11000012, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x13141500, 0x16171819, 0x1A1B1C1D,
	0x1E1F2021, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x22232425, 0x00000000,
	0x00000000, 0x00000000, 0x26272829, 0x2A2B2C2D, 0x2E2F302D, 0x31323334, 0x35363738,
	0x00000000, 0x00000000, 0x00003F40, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x393A3B2D, 0x3C3D3E2D, 0x00000000, 0x00004143, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00004200, 0x00000000, 0x00000000, 0x00004445,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	
	0x4748494A, 0x46464646, 0x46464646, 0x46464646, 0x4B4C4646, 0x46464646, 0x46464646 };

const unsigned long int ti_fbk_COlongs[] = {
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00200800, 0x41614262, 0x43634464,
	0x45654666, 0x47674868, 0x49694A6A, 0x4B6B4C6C, 0x4D6D4E6E, 0x4F6F5070, 0x51715272,
	0x53735474, 0x55755676, 0x57775878, 0x59795A7A, 0x0000000A, 0x00000000, 0x00000000,
	0x00000000, 0x30313233, 0x34353637, 0x38394142, 0x43444546, 0x2E2C213F, 0x3A3B2722,
	0x2D2F2829, 0x5B5D7B7D, 0x2B2D2A5E, 0x40232526, 0x24A2A3A5, 0x00000000, 0x3C3E3DB1,
	0x2F5C7C5F, 0x7E60B4A8, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0xC0E0C8E8, 0xCCECD2F2, 0xD9F90000, 0xD0F0DEFE, 0xC1E1C9E9, 0xCDEDD3F3, 0xDAFADDFD,
	0x00000000, 0x00000000, 0xC3E3D5F5, 0xC6E60000, 0xC5E5C7E7, 0xD1F1D8F8, 0xDFA7AEA9,
	0xA1BFABBB, 0xC4E4CBEB, 0xCFEFD6F6, 0xDCFC00FF, 0xC2E2CAEA, 0xCEEED4F4, 0xDBFB0000,
	0xB0B9B2B3, 0xBCBDBEB7, 0xB7B6AFAD, 0xD7F7A6AC, 0xA4B5BAAA, 0xB8AFB0B7, 0x090B0D7F,
	
	0x00000000, 0x30313233, 0x34353637, 0x38392E2D, 0x0000080A, 0x452E2B2D, 0x2B2A2F5E };

const char * ti_fbk_ModeNames[] = {
	"Home",		"A-H",		"I-P",		"Q-X",		"YZEtc",	"AB",		"CD",
	"EF",		"GH",		"IJ",		"KL",		"MN",		"OP",		"QR",
	"ST",		"UV",		"WX",		"YZ",		"Etc",		"Num",		"Punct",
	"Symb",		"0123",		"4567",		"89AB",		"CDEF",		".,!?",		":;\'\"",
	"-/()",		"[]{}",		"+-*^",		"@#%&",		"$\xA2\xA3\xA5", "Etc2", "<>=\xB1",
	"/\\|_",	"~`\xB4\xA8", "Accen",	"`",		"\xB4",		"\xA8^~&",	"\xE5\xF1\xDF\xBF\xA9",
	"\xE0\xE8", "\xEC\xF2", "\xF9Supr", "\xF0\xFE", "\xE1\xE9", "\xED\xF3", "\xFA\xFD",
	"\xA8",		"^",		"\xE3\xF5", "LgPtX",	"\xE5\xE7", "\xF1\xF8", "\xDF\xA7\xAE\xA9",
	"IPnct",	"\xE4\xEB", "\xEF\xF6", "\xFC\xFFMth", "\xE2\xEA", "\xEE\xF4", "\xFB""AcXC",
	"Super",	"Fract",	"PnctX",	"MathX",	"PctX2",	"AccX",		"Ctrl",
	
	"#Home",	"0123",		"4567",		"89.-",		"Etc",		"Symb",		"Math" };

const char * ti_fbk_CtrlCharNames[] = {
	"Null", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "Bell",
	"Bksp", "Tab", "Newln", "VTab", "FrmFd", "CRtn", "SOut", "SIn",
	"DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
	"CAN", "EM", "SUB", "Esc", "FS", "GS", "RS", "US", "Space" };

static unsigned long int ti_fbk_CurrModeSwitch = 0x01020304;
static unsigned long int ti_fbk_CurrCharOutput = 0;
static int ti_fbk_numeric = 0;

extern TWidget * ti_create_tim_widget(int ht, int wd);

void ti_fbk_draw(TWidget * wid, ttk_surface srf)
{
	unsigned long int c, ch, m, mh;
	int i, x, y, cw;
	char cho[2];
	m = ti_fbk_CurrModeSwitch;
	c = ti_fbk_CurrCharOutput;
	ttk_fillrect(srf, wid->x, wid->y, wid->x+wid->w, wid->y+wid->h, ti_ap_get(0));
	
	if (ttk_get_podversion() & TTK_POD_3G) {
		cw = (wid->w/4);
	} else {
		cw = (wid->w/3);
	}
	
	for (i=0; i<4; i++) {
		if (ttk_get_podversion() & TTK_POD_3G) {
			x = cw*i + 1;
			y = 0;
		} else {
			switch (i) {
			case 0: x =      1; y = (ttk_text_height(ttk_menufont)/2); break;
			case 1: x = cw  +1; y = 0; break;
			case 2: x = cw  +1; y = ttk_text_height(ttk_menufont); break;
			case 3: x = cw*2+1; y = (ttk_text_height(ttk_menufont)/2); break;
			}
		}
		x+=wid->x; y+=wid->y;
		x+=(cw/2);
		ch = (c / 16777216);
		mh = (m / 16777216);
		if (ch != 0) {
			if (ch < 33) {
				x -= (ttk_text_width_lat1(ttk_menufont, ti_fbk_CtrlCharNames[ch])/2);
				ttk_text_lat1(srf, ttk_menufont, x, y, ti_ap_get(1), ti_fbk_CtrlCharNames[ch]);
			} else {
				cho[0]=ch;
				cho[1]=0;
				x -= (ttk_text_width_lat1(ttk_menufont, cho)/2);
				ttk_text_lat1(srf, ttk_menufont, x, y, ti_ap_get(1), cho);
			}
		} else {
			x -= (ttk_text_width_lat1(ttk_menufont, ti_fbk_ModeNames[mh])/2);
			ttk_text_lat1(srf, ttk_menufont, x, y, ti_ap_get(1), ti_fbk_ModeNames[mh]);
		}
		m = (m % 16777216)*256;
		c = (c % 16777216)*256;
	}
}

void ti_fbk_SetMode(int m)
{
	if ((m < 77) && (m >= 0)) {
		ti_fbk_CurrModeSwitch = ti_fbk_MSlongs[m];
		ti_fbk_CurrCharOutput = ti_fbk_COlongs[m];
	} else {
		ti_fbk_CurrModeSwitch = ti_fbk_numeric?0x4748494A:0x01020304;
		ti_fbk_CurrCharOutput = 0;
	}
}

void ti_fbk_PushButton(int b)
{
	unsigned long int c, ch, m;
	int i;
	m = ti_fbk_CurrModeSwitch;
	c = ti_fbk_CurrCharOutput;
	for (i=b; i<3; i++) {
		m /= 256;
		c /= 256;
	}
	ti_fbk_SetMode(m % 256);
	ch = (c % 256);
	if (ch != 0) {
		ttk_input_char(ch);
	}
}

int ti_fbk_scroll(TWidget * wid, int dir)
{
	TTK_SCROLLMOD (dir, 4);
	
	if (dir<0) {
		ttk_input_char(TTK_INPUT_LEFT);
	} else {
		ttk_input_char(TTK_INPUT_RIGHT);
	}
	return TTK_EV_CLICK;
}

int ti_fbk_down(TWidget * wid, int btn)
{
	switch (btn) {
	case TTK_BUTTON_PREVIOUS:
		ti_fbk_PushButton(0);
		wid->dirty = 1;
		break;
	case TTK_BUTTON_MENU:
		ti_fbk_PushButton(1);
		wid->dirty = 1;
		break;
	case TTK_BUTTON_PLAY:
		ti_fbk_PushButton(2);
		wid->dirty = 1;
		break;
	case TTK_BUTTON_NEXT:
		ti_fbk_PushButton(3);
		wid->dirty = 1;
		break;
	default:
		return TTK_EV_UNUSED;
		break;
	}
	return TTK_EV_CLICK;
}

int ti_fbk_button(TWidget * wid, int btn, int t)
{
	if (btn != TTK_BUTTON_ACTION) return TTK_EV_UNUSED;
	if (t>1000) return TTK_EV_UNUSED;
	ti_fbk_SetMode(-1);
	wid->dirty = 1;
	return TTK_EV_CLICK;
}

int ti_fbk_held(TWidget * wid, int btn)
{
	if (btn != TTK_BUTTON_ACTION) return TTK_EV_UNUSED;
	ttk_input_end();
	return TTK_EV_CLICK;
}

TWidget * ti_fbk_create(int n)
{
	TWidget * wid;
	if (ttk_get_podversion() & TTK_POD_3G) {
		wid = ti_create_tim_widget(-1, 0);
	} else {
		wid = ti_create_tim_widget(-2, 0);
	}
	wid->down = ti_fbk_down;
	wid->button = ti_fbk_button;
	wid->held = ti_fbk_held;
	wid->scroll = ti_fbk_scroll;
	wid->draw = ti_fbk_draw;
	wid->holdtime = 500;
	ti_fbk_numeric = n;
	ti_fbk_SetMode(-1);
	return wid;
}

TWidget * ti_fbk_screate()
{
	return ti_fbk_create(0);
}

TWidget * ti_fbk_ncreate()
{
	return ti_fbk_create(1);
}

void ti_fbk_init()
{
	module = pz_register_module("tifbk", 0);
	ti_register(ti_fbk_screate, ti_fbk_ncreate, "Four-Button Keyboard", 7);
}

PZ_MOD_INIT(ti_fbk_init)

