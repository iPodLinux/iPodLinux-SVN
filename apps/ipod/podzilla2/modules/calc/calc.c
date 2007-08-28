/*
 * Copyright (C) 2004, 2006 Courtney Cavin
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pz.h"

#define DIV  16
#define MULT 17
#define SUBT 18
#define ADD  19
#define EQL  20
#define DP   21

#define N_BUTTONS (16 - 1)
struct _buttons {
	char *ch;
	unsigned char math;
} buttons[] = {
	{"7", 7}, {"8", 8}, {"9", 9}, {"/", DIV},
	{"4", 4}, {"5", 5}, {"6", 6}, {"*", MULT},
	{"1", 1}, {"2", 2}, {"3", 3}, {"-", SUBT},
	{"0", 0}, {".", DP},{"=", EQL},{"+",ADD}
};

static PzModule *module;
static double number, lastnumber;
static signed char e;
static unsigned char op;
static int cur_button;

static void draw_button(PzWidget *wid, ttk_surface srf, int row, int col)
{
#define XOFF (wid->w / 12)
#define YOFF (wid->h / 4)
#define SECW ((wid->w - XOFF*2)/4) 
#define SECH ((wid->h - YOFF - wid->h/12)/4) 
#define BPAD ((SECW - ttk_text_width(ttk_textfont, "8"))/4)
#define BW (SECW - BPAD)
#define BH (SECH - BPAD)
	char *text[] = {"button.selected.fg", "button.default.fg",
		"button.selected.bg", "button.default.bg",
		"button.selected.border", "button.default.border"};
	int pos = (row * 4) + col;
	unsigned char ap = !(pos == cur_button);
	int x = (XOFF + BPAD/2) + col*(BW + BPAD);
	int y = (YOFF + BPAD/2) + row*(BH + BPAD);

	ttk_ap_fillrect(srf, ttk_ap_get(text[ap+2]), x+1, y+1, x+BW-1, y+BH-1);
	ttk_ap_rect(srf, ttk_ap_get(text[ap+4]), x, y, x + BW, y + BH);
	ttk_text(srf, ttk_textfont, x+(BW-ttk_text_width(ttk_textfont, "8"))/2,
			y+(BH-ttk_text_height(ttk_textfont))/2,
			ttk_ap_getx(text[ap])->color, buttons[pos].ch);
}

static void calculator_draw(PzWidget *wid, ttk_surface srf)
{
	int row, col;
	int x = XOFF + BPAD/2;
	int y = wid->h/12;
	int xe = x + (wid->w - XOFF*2) - BPAD;
	char s[24];

	snprintf(s, 24, "%g", number);
	ttk_rect(srf, x, y, xe, y+BH, ttk_ap_getx("window.fg")->color);
	ttk_text(srf, ttk_textfont, xe - ttk_text_width(ttk_textfont, s) - 1,
			y + (BH - ttk_text_height(ttk_textfont))/2,
			ttk_ap_getx("window.fg")->color, s);
	
	for (row = 0; row < 4; row++)
		for (col = 0; col < 4; col++)
			draw_button(wid, srf, row, col);
}

static double lpow(double x, signed char p)
{
	double i = x;
	if (p > 0) while (--p > 0) x *= i;
	else if (p-- < 0) while (p++ < 0) x /= i;
	else return 1;
	return x;
}

static double operation(double a, unsigned char op, double b)
{
	switch (op) {
	case DIV: return a / b;
	case MULT:return a * b;
	case SUBT:return a - b;
	case ADD: return a + b;
	}
	return 0;
}

static void calculate(int pos)
{
	struct _buttons *p = &buttons[pos];
	static int stump = 0;
	if (p->math & 16) {
		switch (p->math) {
		case DIV:
		case MULT:
		case SUBT:
		case ADD:
			if (op) number = operation(lastnumber, op, number);
			op = p->math;
			lastnumber = number;
			stump = 1;
			e = 0;
			break;
		case EQL:
			number = operation(lastnumber, op, number);
			op = 0;
			stump = 1;
			e = 0;
			break;
		case DP:
			if (!e) --e;
			break;
		}
	}
	else if (!stump) {
		if (!e) number *= 10;
		number += p->math * lpow(10, e);
		if (e < 0) --e;
	}
	else {
		number = p->math;
		stump = 0;
	}
}

#define BWRAP(x,y,z) if (x < y) x = z
#define TWRAP(x,y,z) if (x > z) x = y
#define WRAP(x,y,z) do { BWRAP(x,y,z); TWRAP(x,y,z); } while(0)

static int calculator_events(PzEvent *e)
{
	int ret = TTK_EV_CLICK;

	switch(e->type) {
	case PZ_EVENT_BUTTON_DOWN:
		switch (e->arg) {
		case PZ_BUTTON_ACTION:
			calculate(cur_button);
			e->wid->dirty = 1;
			break;

		case PZ_BUTTON_MENU:
			pz_close_window(e->wid->win);
			break;

		default:
			ret = TTK_EV_UNUSED;
		}
		break;

	case PZ_EVENT_SCROLL:
		TTK_SCROLLMOD(e->arg, 5);
		cur_button += e->arg;
		WRAP(cur_button, 0, N_BUTTONS);
		e->wid->dirty = 1;
		break;

	default:
		ret = TTK_EV_UNUSED;
		break;
	}
	return ret;
}

static PzWindow *new_calculator()
{
	PzWindow *ret;
	number = lastnumber = 0;
	e = 0;
	cur_button = 0;
	op = 0;

	ret = pz_new_window(_("Calculator"), PZ_WINDOW_NORMAL);
	pz_add_widget(ret, calculator_draw, calculator_events);

	return pz_finish_window(ret);
}

static void init_calculator()
{
	module = pz_register_module("calc", NULL);
	pz_menu_add_action("/Extras/Utilities/Calculator", new_calculator);
}

PZ_MOD_INIT(init_calculator)
