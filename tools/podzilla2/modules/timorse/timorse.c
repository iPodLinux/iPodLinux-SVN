/*
 * Copyright (C) 2005 Jonathan Bettencourt (jonrelay)
 *
 * Morse Code concept by mattlivesey. Initially implemented by fre_ber and
 * adopted to text input system by jonrelay, but that version broke months
 * ago and therefore sucked gravy. Reimplemented by jonrelay for pz2.
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
 *  /!\ Warning! This is NOT the same as legacy morse.c, or any other morse.c!
 */

#include "pz.h"
#include <string.h>

static PzModule * module;

/* dependent on textinput module */
extern ttk_color ti_ap_get(int);
extern int ti_register(TWidget *(* cr)(), TWidget *(* ncr)(), char *, int);

#define NUM_MORSE_SYMBOLS 79
#define MORSE_MAX_SYMBOL_LENGTH 10

typedef struct ti_morse_timing_
{
	int longsep;   // Number of milliseconds that separates short from long
	int symbolsep; // Number of milliseconds that separates symbols
	int wordsep;   // Number of milliseconds that separates words
} ti_morse_timing;

typedef struct ti_morse_symbol_
{
	char character;
	char * symbol;
} ti_morse_symbol;

static ti_morse_timing ti_morse_settings = { 200, 800, 2000 };

const ti_morse_symbol ti_morse_table[NUM_MORSE_SYMBOLS] =
{
	{'A', ".-"},
	{'B', "-..."},
	{'C', "-.-."},
	{'D', "-.."},
	{'E', "."},
	{'F', "..-."},
	{'G', "--."},
	{'H', "...."},
	{'I', ".."},
	{'J', ".---"},
	{'K', "-.-"},
	{'L', ".-.."},
	{'M', "--"},
	{'N', "-."},
	{'O', "---"},
	{'P', ".--."},
	{'Q', "--.-"},
	{'R', ".-."},
	{'S', "..."},
	{'T', "-"},
	{'U', "..-"},
	{'V', "...-"},
	{'W', ".--"},
	{'X', "-..-"},
	{'Y', "-.--"},
	{'Z', "--.."},
	{'0', "-----"},
	{'1', ".----"},
	{'2', "..---"},
	{'3', "...--"},
	{'4', "....-"},
	{'5', "....."},
	{'6', "-...."},
	{'7', "--..."},
	{'8', "---.."},
	{'9', "----."},
	{'.', ".-.-.-"},
	{',', "--..--"},
	{'?', "..--.."},
	{'\'', ".----."},
	{'/', "-..-."},
	{'\\', "-..-.."}, /* not standard morse code */
	{'|', "-..-.-"}, /* not standard morse code */
	{'(', "-.--.-"},
	{')', "-.--.."}, /* not standard morse code */
	{'[', "----.-"}, /* not standard morse code */
	{']', "----.."}, /* not standard morse code */
	{'{', "------"}, /* not standard morse code */
	{'}', "-----."}, /* not standard morse code */
	{'<', "--...-"}, /* not standard morse code */
	{'>', "--...."}, /* not standard morse code */
	{':', "---..."},
	{';', "...---"}, /* not standard morse code */
	{'=', "-...-"},
	{'-', "-....-"},
	{'+', ".-.-."},
	{'_', ".-----"}, /* not standard morse code */
	{'\"', ".-..-."},
	{'@', ".--.-."}, /* AC */
	{'#', "-.---"}, /* NO not standard morse code */
	{'$', "..-..."}, /* US not standard morse code */
	{'&', ".-.-.."}, /* RD not standard morse code */
	{'*', ".-...-"}, /* AST not standard morse code */
	{'%', ".--..."}, /* PI not standard morse code */
	{'~', "-.-.-."}, /* not standard morse code */
	{'^', "---.--"}, /* not standard morse code */
	{'`', "--.---"}, /* not standard morse code */
	{'\xC4', ".-.-"},
	{'\xC5', ".--.-"},
	{'\xC7', "----"},
	{'\xC9', "..-.."},
	{'\xD1', "-.-.-"}, /* not standard morse code */
	{'\xD6', "---."},
	{'\xDC', "..--"},
	{'!', "..--."},
	{TTK_INPUT_LEFT, ".-..."}, /* not standard morse code */
	{TTK_INPUT_RIGHT, ".-..-"}, /* not standard morse code */
	{TTK_INPUT_ENTER, ".-.--"}, /* not standard morse code */
	{TTK_INPUT_BKSP, "......"} /* not standard morse code */
};

static char ti_morse_sym[MORSE_MAX_SYMBOL_LENGTH];
static int  ti_morse_symlen = 0;
static int  ti_morse_lowercase = 1;

extern TWidget * ti_create_tim_widget(int ht, int wd);

unsigned char ti_morse_decode(char * symbol)
{
	unsigned char c = 0;
	int i = 0; /* I actually think this single line, which was uninitialized in the old version, was the whole reason it didn't work. :D */
	while ((i < NUM_MORSE_SYMBOLS) && (!c)) {
		if (!strcmp(symbol, ti_morse_table[i].symbol)) {
			c = ti_morse_table[i].character;
		} else {
			i++;
		}
	}
	if (c && ti_morse_lowercase && ( ((c >= 'A') && (c <= 'Z')) || ((c >= 192) && (c < 224)) )) {
		c += 32;
	}
	return c;
}

void ti_morse_draw(TWidget * wid, ttk_surface srf)
{
	int x = wid->x + 2;
	int y = wid->y + 2;
	int i;
	ttk_fillrect(srf, wid->x, wid->y, wid->x+wid->w, wid->y+wid->h, ti_ap_get(0));
	for (i=0; i<ti_morse_symlen; i++) {
		if (ti_morse_sym[i] == '.') {
			ttk_fillrect(srf, x, y, x+2, y+2, ti_ap_get(1));
			x += 4;
		} else {
			ttk_fillrect(srf, x, y, x+8, y+2, ti_ap_get(1));
			x += 10;
		}
	}
	if (!ti_morse_lowercase) {
		x = wid->x + wid->w - 10;
		ttk_fillrect(srf, x, y, x+8, y+2, ti_ap_get(2));
	}
}

int ti_morse_down(TWidget * wid, int btn)
{
	ttk_widget_set_timer(wid, 0);
	switch (btn) {
	case TTK_BUTTON_PLAY:
		ti_morse_lowercase = !ti_morse_lowercase;
		ti_morse_sym[0] = 0;
		ti_morse_symlen = 0;
		wid->dirty = 1;
		break;
	case TTK_BUTTON_MENU:
		ttk_input_end();
		break;
	case TTK_BUTTON_PREVIOUS:
		ttk_input_char(8);
		ti_morse_sym[0] = 0;
		ti_morse_symlen = 0;
		wid->dirty = 1;
		break;
	case TTK_BUTTON_NEXT:
		ttk_input_char(32);
		ti_morse_sym[0] = 0;
		ti_morse_symlen = 0;
		wid->dirty = 1;
		break;
	case TTK_BUTTON_ACTION:
		break;
	default:
		return TTK_EV_UNUSED;
	}
	return TTK_EV_CLICK;
}

int ti_morse_up(TWidget * wid, int btn, int t)
{
	if (btn == TTK_BUTTON_ACTION) {
		if (t < ti_morse_settings.longsep) {
			ti_morse_sym[ti_morse_symlen++] = '.';
			ti_morse_sym[ti_morse_symlen] = 0;
		} else {
			ti_morse_sym[ti_morse_symlen++] = '-';
			ti_morse_sym[ti_morse_symlen] = 0;
		}
		wid->dirty = 1;
		ttk_widget_set_timer(wid, ti_morse_settings.symbolsep);
		return TTK_EV_CLICK;
	}
	return TTK_EV_UNUSED;
}

int ti_morse_scroll(TWidget * wid, int dir)
{
	TTK_SCROLLMOD (dir,4);
	
	ttk_widget_set_timer(wid, 0);
	ti_morse_sym[0] = 0;
	ti_morse_symlen = 0;
	wid->dirty = 1;
	if (dir<0) {
		ttk_input_char(TTK_INPUT_LEFT);
	} else {
		ttk_input_char(TTK_INPUT_RIGHT);
	}
	return TTK_EV_CLICK;
}

int ti_morse_timer(TWidget * wid)
{
	unsigned char ch;
	if (ti_morse_symlen) {
		ch = ti_morse_decode(ti_morse_sym);
		ti_morse_sym[0] = 0;
		ti_morse_symlen = 0;
		wid->dirty = 1;
		if (ch) {
			ttk_input_char(ch);
			ttk_widget_set_timer(wid, ti_morse_settings.wordsep);
		} else {
			ttk_input_char(32);
			ttk_widget_set_timer(wid, 0);
		}
	} else {
		ttk_input_char(32);
		ttk_widget_set_timer(wid, 0);
	}
	return TTK_EV_CLICK;
}

TWidget * ti_morse_create()
{
	TWidget * wid = ti_create_tim_widget(6, 0);
	wid->draw = ti_morse_draw;
	wid->down = ti_morse_down;
	wid->button = ti_morse_up;
	wid->scroll = ti_morse_scroll;
	wid->timer = ti_morse_timer;
	ti_morse_sym[0] = 0;
	ti_morse_symlen = 0;
	ti_morse_lowercase = 1;
	return wid;
}

void ti_morse_init()
{
	module = pz_register_module("timorse", 0);
	ti_register(ti_morse_create, ti_morse_create, N_("Morse Code"), 4);
}

PZ_MOD_INIT(ti_morse_init)

