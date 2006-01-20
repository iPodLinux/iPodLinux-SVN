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

#include "pz.h"
#include <ctype.h>
#include <string.h>
#include <unistd.h>

/* dependent on textinput module */
extern ttk_color ti_ap_get(int);

static PzModule * module;

/* Text Input Buffer Management */

typedef struct TiBuffer {
	char * text;
	int asize; /* allocated size */
	int usize; /* used size */
	int cpos; /* cursor position */
	int idata[4];
	void * data;
	int (*callback)(TWidget *, char *); /* callback function */
} TiBuffer;

TiBuffer * ti_create_buffer(int size)
{
	TiBuffer * buf = (TiBuffer *)malloc(sizeof(TiBuffer));
	int s = (size>1024)?size:1024;
	if (buf) {
		if ( (buf->text = (char *)malloc(s)) ) {
			buf->asize = s;
			buf->text[0] = 0;
		} else {
			buf->asize = 0;
		}
		buf->usize = 0;
		buf->cpos = 0;
		buf->idata[0] = 0;
		buf->idata[1] = 0;
		buf->idata[2] = 0;
		buf->idata[3] = 0;
		buf->data = 0;
		buf->callback = 0;
	}
	return buf;
}

void ti_destroy_buffer(TiBuffer * buf)
{
	if (buf->text) free(buf->text);
	free(buf);
}

int ti_expand_buffer(TiBuffer * buf, int exp)
{
	char * t;
	if ((buf->usize + exp) >= buf->asize) {
		if ( (t = (char *)realloc(buf->text, buf->asize + 1024)) ) {
			buf->text = t;
			buf->asize += 1024;
			buf->usize += exp;
			return 1;
		}
		return 0; /* realloc failed */
	} else {
		buf->usize += exp;
		return 1;
	}
}

int ti_set_buffer(TiBuffer * buf, char * s)
{
	char * t;
	int l = strlen(s);
	if ( (t = (char *)malloc(l+1024)) ) {
		if (buf->text) free(buf->text);
		buf->text = t;
		buf->asize = l+1024;
		buf->usize = l;
		buf->cpos = l;
		strcpy(buf->text, s);
		return 1;
	}
	return 0;
}

TiBuffer * ti_create_buffer_with(char * s)
{
	TiBuffer * b = ti_create_buffer(0);
	if (b) {
		ti_set_buffer(b, s);
	}
	return b;
}

char * ti_get_buffer(TiBuffer * buf)
{
	return buf->text;
}

void ti_buffer_insert_byte(TiBuffer * buf, int pos, char ch)
{
	int i;
	if (pos < 0) return;
	if (ti_expand_buffer(buf, 1)) {
		for (i=(buf->usize); i>pos; i--) {
			buf->text[i] = buf->text[i-1];
		}
		buf->text[pos] = ch;
	}
}

void ti_buffer_remove_byte(TiBuffer * buf, int pos)
{
	int i;
	if (pos < 0) return;
	for (i=pos; i<(buf->usize); i++) {
		buf->text[i] = buf->text[i+1];
	}
	buf->usize--;
}

int ti_utf8_encode(int ch, char c[4])
{
	if (ch < 0) {
		c[0]=('?');
		return 0;
	} else if (ch < 0x80) {
		c[0]=(ch);
		return 1;
	} else if (ch < 0x0800) {
		c[0]=((ch >> 6        ) | 0xC0);
		c[1]=((ch       & 0x3F) | 0x80);
		return 2;
	} else if (ch < 0x10000) {
		c[0]=((ch >> 12       ) | 0xE0);
		c[1]=((ch >> 6  & 0x3F) | 0x80);
		c[2]=((ch       & 0x3F) | 0x80);
		return 3;
	} else if (ch < 0x110000) {
		c[0]=((ch >> 18       ) | 0xF0);
		c[1]=((ch >> 12 & 0x3F) | 0x80);
		c[2]=((ch >> 6  & 0x3F) | 0x80);
		c[3]=((ch       & 0x3F) | 0x80);
		return 4;
	} else {
		c[0]=('?');
		return 0;
	}
}

void ti_buffer_insert_char(TiBuffer * buf, int pos, int ch)
{
	int i;
	char c[4];
	int l = ti_utf8_encode(ch, c);
	if (pos<0 || l<=0) return;
	if (ti_expand_buffer(buf, l)) {
		for (i=(buf->usize)-l; i>=pos; i--) {
			buf->text[i+l] = buf->text[i];
		}
		for (i=0; i<l; i++) {
			buf->text[pos+i] = c[i];
		}
	}
}

void ti_buffer_remove_char(TiBuffer * buf, int pos)
{
	ti_buffer_remove_byte(buf, pos);
	while ((buf->text[pos] & 0xC0) == 0x80) {
		ti_buffer_remove_byte(buf, pos);
	}
}

void ti_buffer_cmove(TiBuffer * buf, int m)
{
	if (m<0) {
		/* move left */
		while (m<0) {
			do {
				buf->cpos--;
				if (buf->cpos < 0) {
					buf->cpos = 0;
					return;
				}
			} while ((buf->text[buf->cpos] & 0xC0) == 0x80);
			m++;
		}
	} else {
		/* move right */
		while (m>0) {
			do {
				buf->cpos++;
				if (buf->cpos > buf->usize) {
					buf->cpos = buf->usize;
					return;
				}
			} while ((buf->text[buf->cpos] & 0xC0) == 0x80);
			m--;
		}
	}
}

void ti_buffer_cset(TiBuffer * buf, int c)
{
	if (c < 0) {
		buf->cpos = 0;
	} else if (c > buf->usize) {
		buf->cpos = buf->usize;
	} else {
		buf->cpos = c;
	}
}

int ti_buffer_cget(TiBuffer * buf)
{
	return buf->cpos;
}

void ti_buffer_input(TiBuffer * buf, int ch)
{
	switch (ch) {
	/* case 0: */
	case TTK_INPUT_END:
		break;
	case 28:
	case TTK_INPUT_LEFT:
		ti_buffer_cmove(buf,-1);
		break;
	case 29:
	case TTK_INPUT_RIGHT:
		ti_buffer_cmove(buf,1);
		break;
	/* case 8: */
	case TTK_INPUT_BKSP:
		if (!(buf->cpos)) break;
		ti_buffer_cmove(buf,-1);
	case 127:
		ti_buffer_remove_char(buf, buf->cpos);
		break;
	case 10:
	/* case 13: */
	case TTK_INPUT_ENTER:
		ch = 10;
	default:
		ti_buffer_insert_char(buf, buf->cpos, ch);
		ti_buffer_cmove(buf,1);
		break;
	}
}

/* Multiline Text */

void ti_multiline_text(ttk_surface srf, ttk_font fnt, int x, int y, int w, int h, ttk_color col, const char *t, int cursorpos, int scroll, int * lc, int * sl, int * cb)
{
	char * sot; /* start of text */
	char * curtextptr;
	char curline[1024];
	int currentLine = 0;
	int screenlines = 0;
	int coob = 0; /* Cursor Out Of Bounds, or iPodLinux dev; you decide */
	int width, height;
	ttk_color curcol = ti_ap_get(5);
	height = ttk_text_height(fnt);
	screenlines = h / height;

	sot = (char *)t;
	curtextptr = (char *)t;
	if ((cursorpos >= 0) && (*curtextptr == '\0')) {
		/* text is empty, draw the cursor at initial position */
		ttk_line(srf, x, y, x, y+height, curcol);
	}
	while (*curtextptr != '\0') {
		char * sol; /* start of line */
		char ctp;
		sol = curtextptr;
		curline[0] = 0;
		/* find the line break */
		while (1) {
			/* bring in the next character */
			do {
				ctp=curline[curtextptr-sol]=(*curtextptr);
				curline[curtextptr-sol+1]=0;
				curtextptr++;
			} while ((*curtextptr & 0xC0) == 0x80);
			/* if that was a newline, that's where the break is */
			if ((ctp == '\n')||(ctp == '\r')) {
				curline[curtextptr-sol-1]=0;
				break;
			/* if that was a null, that's where the break is */
			} else if (ctp == '\0') {
				curtextptr--; /* need to decrease so that the next iteration sees that *curtextptr == 0 and exits */
				break;
			}
			/* determine if we've gotten a few characters too many */
			width = ttk_text_width(fnt, curline);
			if (width > w) {
				/* backtrack to the last word */
				char *optr;
				int fndWrd = 0;
				for (optr=curtextptr-1; optr>sol; optr--) {
					if (isspace(*optr)||(*optr=='\t')||(*optr=='-')) {
						curtextptr=optr+1;
						curline[curtextptr-sol]=0;
						fndWrd = 1;
						break;
					}
				}
				if (!fndWrd) {
					/* it's one long string of letters, chop off the last one */
					curtextptr--;
					while ((*curtextptr & 0xC0) == 0x80) {
						curtextptr--;
					}
					curline[curtextptr-sol]=0;
				}
				break;
			}
		}
		/* if the line is in the viewable area, draw it */
		if ( ((currentLine - scroll) >= 0) && ((currentLine - scroll) < screenlines) ) {
			ttk_text(srf, fnt, x, y+((currentLine-scroll)*height), col, curline);
			if ((cursorpos >= sol-sot) && (cursorpos < curtextptr-sot)) {
				/* cursor is on the line, but not at the very end */
				if (cursorpos == (sol-sot)) {
					width = 0;
				} else {
					curline[cursorpos-(sol-sot)] = 0;
					width = ttk_text_width(fnt, curline);
				}
				ttk_line(srf, x+width, y+((currentLine-scroll)*height), x+width, y+((currentLine-scroll+1)*height), curcol);
			} else if ((cursorpos == curtextptr-sot) && (*curtextptr == '\0')) {
				/* cursor is not only at the very end of the line, but the very end of the text */
				if ((*(curtextptr-1) == '\n')||(*(curtextptr-1) == '\r')) {
					/* last character of the text is a newline, so display the cursor on the next line */
					ttk_line(srf, x, y+((currentLine-scroll+1)*height), x, y+((currentLine-scroll+2)*height), curcol);
					if ((currentLine - scroll + 1) == screenlines) { coob = 1; }
				} else {
					/* display the cursor normally */
					/*GrGetGCTextSize(gc, sol, cursorpos-(sol-sot), GR_TFUTF8, &width, &height, &base);*/
					curline[cursorpos-(sol-sot)] = 0;
					width = ttk_text_width(fnt, curline);
					ttk_line(srf, x+width, y+((currentLine-scroll)*height), x+width, y+((currentLine-scroll+1)*height), curcol);
				}
			}
			
			if ( ((currentLine - scroll) == 0) && (cursorpos < sol-sot) ) {
				coob = -1; /* cursor is before the viewable area */
			} else if ( ((currentLine - scroll + 1) == screenlines) && (cursorpos >= curtextptr-sot) ) {
				coob = 1; /* cursor is after the viewable area */
			}
		}
		currentLine++;
	}
	if ( (*curtextptr == '\0') && ((*(curtextptr-1) == '\n') || (*(curtextptr-1) == '\r')) && (t[0] != 0) ) {
		currentLine++; /* while loop doesn't count trailing newline as another line */
	}
	
	if (lc) *lc = currentLine;
	if (sl) *sl = screenlines;
	if (cb) *cb = coob;
}

/* Text Input Widget Event Handlers */

int ti_widget_start(TWidget * wid)
{
	/* Start text input if it hasn't been started already. */
	int ht;
	if (((TiBuffer *)wid->data)->idata[0]) {
		ht = pz_start_input_n_for (wid->win);
	} else {
		ht = pz_start_input_for (wid->win);
	}
	if (((TiBuffer *)wid->data)->idata[1]) {
		ttk_input_move_for(wid->win, wid->win->x+wid->x+1, wid->win->y+wid->y+wid->h-ht-1);
		((TiBuffer *)wid->data)->idata[2] = ht;
	} else {
		ttk_input_move_for(wid->win, wid->win->x+wid->x+1, wid->win->y+wid->y+wid->h);
		((TiBuffer *)wid->data)->idata[2] = 0;
	}
	wid->win->input->w = wid->w-2;
	return TTK_EV_CLICK;
}

int ti_widget_start1(TWidget * wid, int x)
{
	return ti_widget_start(wid);
}

int ti_widget_start2(TWidget * wid, int x, int y)
{
	return ti_widget_start(wid);
}

int ti_widget_input(TWidget * wid, int ch)
{
	if (ch == TTK_INPUT_END || ch == TTK_INPUT_ENTER) {
		if (((TiBuffer *)wid->data)->callback) {
			if (((TiBuffer *)wid->data)->callback(wid, ((TiBuffer *)wid->data)->text)) {
				ti_widget_start(wid);
			}
		}
	} else {
		ti_buffer_input(((TiBuffer *)wid->data), ch);
		wid->dirty = 1;
	}
	return 0;
}

int ti_widget_input_n(TWidget * wid, int ch)
{
	if (ch == TTK_INPUT_END || ch == TTK_INPUT_ENTER) {
		if (((TiBuffer *)wid->data)->callback) {
			if (((TiBuffer *)wid->data)->callback(wid, ((TiBuffer *)wid->data)->text)) {
				ti_widget_start(wid);
			}
		}
	} else {
		if (ch >= 32) {
			if ((ch >= '0') && (ch <= '9')) {}
			else if ((ch == '+') || (ch == '*') || (ch == '/') || (ch == '^')) {}
			else if ((ch == '-') || (ch == '=')) { ch='-'; }
			else if ((ch >= ' ') && (ch <= '?')) { ch='.'; }
			else if ((ch == 'E') || (ch == '_')) { ch='e'; }
			else { ch = ( '0' + ((ch-7)%10) ); }
		}
		ti_buffer_input(((TiBuffer *)wid->data), ch);
		wid->dirty = 1;
	}
	return 0;
}

int ti_widget_input_ml(TWidget * wid, int ch)
{
	if (ch == TTK_INPUT_END) {
		if (((TiBuffer *)wid->data)->callback) {
			if (((TiBuffer *)wid->data)->callback(wid, ((TiBuffer *)wid->data)->text)) {
				ti_widget_start(wid);
			}
		}
	} else {
		ti_buffer_input(((TiBuffer *)wid->data), ch);
		wid->dirty = 1;
	}
	return 0;
}

void ti_widget_destroy(TWidget * wid)
{
	ttk_input_end();
	ti_destroy_buffer(((TiBuffer *)wid->data));
}

void ti_widget_draw(TWidget * wid, ttk_surface srf)
{
	int sw, cp, i;
	char * tmp;
	ttk_fillrect(srf, wid->x, wid->y, wid->x+wid->w, wid->y+wid->h, ti_ap_get(0));
	
	/* draw border */
	ttk_rect(srf, wid->x, wid->y, wid->x+wid->w, wid->y+wid->h, ti_ap_get(4));
	
	/* scroll horizontally if it doesn't fit */
	sw = ((TiBuffer *)wid->data)->usize;
	cp = ((TiBuffer *)wid->data)->cpos;
	tmp = (char *)malloc(sw+1);
	strcpy(tmp, ((TiBuffer *)wid->data)->text);
	while (ttk_text_width(ttk_textfont, tmp) > (wid->w-10)) {
		do {
			for (i=0; i<sw; i++) {
				tmp[i] = tmp[i+1];
			}
			sw--;
			cp--;
		} while ((tmp[0] & 0xC0) == 0x80);
	}
	
	/* draw text */
	ttk_text(srf, ttk_textfont, wid->x+5, wid->y+5, ti_ap_get(1), tmp);
	
	/* draw cursor */
	tmp[cp]=0;
	sw = ttk_text_width(ttk_textfont, tmp);
	ttk_line(srf, wid->x+5+sw, wid->y+5, wid->x+5+sw, wid->y+5+ttk_text_height(ttk_textfont), ti_ap_get(5));
	
	free(tmp);
}

void ti_widget_draw_p(TWidget * wid, ttk_surface srf)
{
	int sw, cp, i;
	char * tmp;
	ttk_fillrect(srf, wid->x, wid->y, wid->x+wid->w, wid->y+wid->h, ti_ap_get(0));
	
	/* draw border */
	ttk_rect(srf, wid->x, wid->y, wid->x+wid->w, wid->y+wid->h, ti_ap_get(4));
	
	/* replace with stars */
	sw = ((TiBuffer *)wid->data)->usize;
	cp = ((TiBuffer *)wid->data)->cpos;
	tmp = (char *)malloc(sw+1);
	strcpy(tmp, ((TiBuffer *)wid->data)->text);
	for (i=0; i<sw; i++) tmp[i]='*';
	/* scroll horizontally if it doesn't fit */
	while (ttk_text_width(ttk_textfont, tmp) > (wid->w-10)) {
		for (i=0; i<sw; i++) {
			tmp[i] = tmp[i+1];
		}
		sw--;
		cp--;
	}
	
	/* draw text */
	ttk_text(srf, ttk_textfont, wid->x+5, wid->y+5, ti_ap_get(1), tmp);
	
	/* draw cursor */
	tmp[cp]=0;
	sw = ttk_text_width(ttk_textfont, tmp);
	ttk_line(srf, wid->x+5+sw, wid->y+5, wid->x+5+sw, wid->y+5+ttk_text_height(ttk_textfont), ti_ap_get(5));
	
	free(tmp);
}

void ti_widget_draw_ml(TWidget * wid, ttk_surface srf)
{
	ttk_fillrect(srf, wid->x, wid->y, wid->x+wid->w, wid->y+wid->h, ti_ap_get(0));
	
	/* draw border */
	ttk_rect(srf, wid->x, wid->y, wid->x+wid->w, wid->y+wid->h, ti_ap_get(4));
	
	ti_multiline_text(srf, ttk_textfont, wid->x+5, wid->y+5, wid->w-10, wid->h-10-(((TiBuffer *)wid->data)->idata[2]),
		ti_ap_get(1), ((TiBuffer *)wid->data)->text, ((TiBuffer *)wid->data)->cpos, 0, 0, 0, 0);
}

/* Text Input Widgets */

/* What does this code (particularly the headers) remind you of? ^.^ */

TWidget * ti_new_text_widget(int x, int y, int w, int h, int absheight, char * dt, int (*callback)(TWidget *, char *),
	void (*draw)(TWidget *, ttk_surface), int (*input)(TWidget *, int), int numeric)
{
	TWidget * wid = ttk_new_widget(x,y);
	wid->h = h;
	wid->w = w;
	if ( (wid->data = ti_create_buffer_with(dt)) ) {
		((TiBuffer *)wid->data)->callback = callback;
	}
	wid->focusable = 1;
	wid->button = ti_widget_start2;
	wid->scroll = ti_widget_start1;
	wid->input = input;
	wid->draw = draw;
	wid->destroy = ti_widget_destroy;
	((TiBuffer *)wid->data)->idata[0] = numeric;
	((TiBuffer *)wid->data)->idata[1] = absheight;
	return wid;
}

TWidget * ti_new_standard_text_widget(int x, int y, int w, int h, int absheight, char * dt, int (*callback)(TWidget *, char *))
{
	return ti_new_text_widget(x, y, w, h, absheight, dt, callback, ti_widget_draw, ti_widget_input, 0);
}

TWidget * ti_new_numeric_text_widget(int x, int y, int w, int h, int absheight, char * dt, int (*callback)(TWidget *, char *))
{
	return ti_new_text_widget(x, y, w, h, absheight, dt, callback, ti_widget_draw, ti_widget_input_n, 1);
}

TWidget * ti_new_password_text_widget(int x, int y, int w, int h, int absheight, char * dt, int (*callback)(TWidget *, char *))
{
	return ti_new_text_widget(x, y, w, h, absheight, dt, callback, ti_widget_draw_p, ti_widget_input, 0);
}

TWidget * ti_new_multiline_text_widget(int x, int y, int w, int h, int absheight, char * dt, int (*callback)(TWidget *, char *))
{
	return ti_new_text_widget(x, y, w, h, absheight, dt, callback, ti_widget_draw_ml, ti_widget_input_ml, 0);
}

/* Text Input Demo */

int ti_destructive_callback(TWidget * wid, char * txt)
{
	pz_close_window(wid->win);
	return 0;
}

PzWindow * new_standard_text_demo_window()
{
	PzWindow * ret;
	TWidget * wid;
	ret = pz_new_window(_("Text Input Demo"), PZ_WINDOW_NORMAL);
	wid = ti_new_standard_text_widget(10, 40, ret->w-20, 10+ttk_text_height(ttk_textfont), 0, "", ti_destructive_callback);
	ttk_add_widget(ret, wid);
	ret = pz_finish_window(ret);
	ti_widget_start(wid);
	return ret;
}

PzWindow * new_numeric_text_demo_window()
{
	PzWindow * ret;
	TWidget * wid;
	ret = pz_new_window(_("Text Input Demo"), PZ_WINDOW_NORMAL);
	wid = ti_new_numeric_text_widget(10, 40, ret->w-20, 10+ttk_text_height(ttk_textfont), 0, "", ti_destructive_callback);
	ttk_add_widget(ret, wid);
	ret = pz_finish_window(ret);
	ti_widget_start(wid);
	return ret;
}

PzWindow * new_password_text_demo_window()
{
	PzWindow * ret;
	TWidget * wid;
	ret = pz_new_window(_("Text Input Demo"), PZ_WINDOW_NORMAL);
	wid = ti_new_password_text_widget(10, 40, ret->w-20, 10+ttk_text_height(ttk_textfont), 0, "", ti_destructive_callback);
	ttk_add_widget(ret, wid);
	ret = pz_finish_window(ret);
	ti_widget_start(wid);
	return ret;
}

PzWindow * new_multiline_text_demo_window()
{
	PzWindow * ret;
	TWidget * wid;
	ret = pz_new_window(_("Text Input Demo"), PZ_WINDOW_NORMAL);
	wid = ti_new_multiline_text_widget(10, 10, ret->w-20, ret->h-20, 1, "", ti_destructive_callback);
	ttk_add_widget(ret, wid);
	ret = pz_finish_window(ret);
	ti_widget_start(wid);
	return ret;
}

void init_textinput_demos()
{
	module = pz_register_module ("tiwidgets", 0);
	pz_menu_add_action ("/Extras/Stuff/Text Input Demo/Standard", new_standard_text_demo_window);
	pz_menu_add_action ("/Extras/Stuff/Text Input Demo/Numeric", new_numeric_text_demo_window);
	pz_menu_add_action ("/Extras/Stuff/Text Input Demo/Password", new_password_text_demo_window);
	pz_menu_add_action ("/Extras/Stuff/Text Input Demo/Multiline", new_multiline_text_demo_window);
}

PZ_MOD_INIT(init_textinput_demos)
