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
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Dependency */
typedef struct TiBuffer {
	char * text;
	int asize;
	int usize;
	int cpos;
	int idata[4];
	void * data;
	int (*callback)(TWidget *, char *);
} TiBuffer;

/* PodWrite State */
static int podwrite_mode = 0;
static int podwrite_linecount = 0;
static int podwrite_screenlines = 0;
static int podwrite_scroll = 0;
static int podwrite_cursor_out_of_bounds = 0;
static char * podwrite_filename = 0;

static PzWindow * podwrite_win;
static TWidget  * podwrite_wid;
static TiBuffer * podwrite_buf;
static TWidget * podwrite_menu;
static ttk_menu_item podwrite_fbx;

/* Dependencies */
extern TWidget * ti_new_standard_text_widget(int x, int y, int w, int h, int absheight, char * dt, int (*callback)(TWidget *, char *));
extern TWidget * ti_new_text_widget(int x, int y, int w, int h, int absheight, char * dt, int (*callback)(TWidget *, char *),
	void (*draw)(TWidget *, ttk_surface), int (*input)(TWidget *, int), int numeric);
extern int ti_widget_input_ml(TWidget * wid, int ch);
extern void ti_multiline_text(ttk_surface srf, ttk_font fnt, int x, int y, int w, int h, ttk_color col, const char *t,
	int cursorpos, int scroll, int * lc, int * sl, int * cb);
extern int ti_widget_start(TWidget * wid);
extern int ti_set_buffer(TiBuffer * buf, char * s);
extern void ti_buffer_cmove(TiBuffer * buf, int m);
extern ttk_color ti_ap_get(int i);

/* PodWrite Menu */

int podwrite_save_callback(TWidget * wid, char * fn)
{
	FILE * f;
	pz_close_window(wid->win);
	if (fn[0]) {
		f = fopen(fn, "wb");
		if (!f) {
			pz_error(_("Could not open file for saving."));
			return 0;
		}
		fwrite(podwrite_buf->text, 1, podwrite_buf->usize, f);
		fclose(f);
	}
	podwrite_mode = 0;
	pz_close_window(podwrite_menu->win);
	ti_widget_start(podwrite_wid);
	return 0;
}

PzWindow * podwrite_mh_save(ttk_menu_item * item)
{
	PzWindow * ret;
	TWidget * wid;
	ret = pz_new_window(_("Save to..."), PZ_WINDOW_NORMAL);
	wid = ti_new_standard_text_widget(10, 40, ret->w-20, 10+ttk_text_height(ttk_textfont), 0, (podwrite_filename?podwrite_filename:"/Notes/"), podwrite_save_callback);
	ttk_add_widget(ret, wid);
	ret = pz_finish_window(ret);
	ti_widget_start(wid);
	return ret;
}

TWindow * podwrite_mh_return(ttk_menu_item * item)
{
	podwrite_mode = 0;
	pz_close_window(podwrite_menu->win);
	ti_widget_start(podwrite_wid);
	return TTK_MENU_DONOTHING;
}

TWindow * podwrite_mh_cursor(ttk_menu_item * item)
{
	podwrite_mode = 1;
	return TTK_MENU_UPONE;
}

TWindow * podwrite_mh_scroll(ttk_menu_item * item)
{
	podwrite_mode = 2;
	return TTK_MENU_UPONE;
}

TWindow * podwrite_mh_quit(ttk_menu_item * item)
{
	if (podwrite_filename) {
		free(podwrite_filename);
		podwrite_filename = 0;
	}
	pz_close_window(podwrite_menu->win);
	pz_close_window(podwrite_win);
	return TTK_MENU_DONOTHING;
}

TWindow * podwrite_mh_input(ttk_menu_item * item)
{
	ttk_menu_item * timenu = pz_get_menu_item("/Settings/Text Input");
	if (timenu) {
		return pz_mh_sub(timenu);
	} else {
		return TTK_MENU_DONOTHING;
	}
}

TWindow * podwrite_mh_clear(ttk_menu_item * item)
{
	int cl = pz_dialog(_("Clear"), _("Clear all text?"), 2, 0, _("No"), _("Yes"));
	if (cl == 1) {
		ti_set_buffer(podwrite_buf, "");
		podwrite_wid->dirty = 1;
		return podwrite_mh_return(item);
	}
	return TTK_MENU_DONOTHING;
}

ttk_menu_item * podwrite_new_menu_item(const char * name, TWindow * (*func)(ttk_menu_item *), int flags)
{
	ttk_menu_item * mi = (ttk_menu_item *)calloc(1, sizeof(ttk_menu_item));
	if (mi) {
		mi->name = name;
		mi->makesub = func;
		mi->flags = flags;
	}
	return mi;
}

PzWindow * new_podwrite_menu_window()
{
	PzWindow * w;
	podwrite_menu = ttk_new_menu_widget(0, ttk_menufont, ttk_screen->w, ttk_screen->h-ttk_screen->wy);
	if (!podwrite_menu) return 0;
	ttk_menu_append(podwrite_menu, podwrite_new_menu_item("Return to PodWrite", podwrite_mh_return, 0));
	ttk_menu_append(podwrite_menu, podwrite_new_menu_item("Move Cursor", podwrite_mh_cursor, 0));
	ttk_menu_append(podwrite_menu, podwrite_new_menu_item("Scroll", podwrite_mh_scroll, 0));
	ttk_menu_append(podwrite_menu, podwrite_new_menu_item("Clear All Text", podwrite_mh_clear, 0));
	ttk_menu_append(podwrite_menu, podwrite_new_menu_item("Save...", podwrite_mh_save, 0));
	ttk_menu_append(podwrite_menu, podwrite_new_menu_item("Change Input Method", podwrite_mh_input, TTK_MENU_ICON_SUB));
	ttk_menu_append(podwrite_menu, podwrite_new_menu_item("Quit PodWrite", podwrite_mh_quit, 0));
	w = pz_new_menu_window(podwrite_menu);
	if (w) w->title = _("PodWrite");
	return w;
}

/* PodWrite Event Handling */

int podwrite_callback(TWidget * wid, char * txt)
{
	PzWindow * w = new_podwrite_menu_window();
	if (w) pz_show_window(w);
	return 0;
}

int podwrite_check_bounds(void)
{
	int s = podwrite_scroll;
	if (podwrite_linecount < podwrite_screenlines) {
		if (podwrite_scroll != 0) { podwrite_scroll = 0; }
	} else {
		if (podwrite_scroll > (podwrite_linecount-podwrite_screenlines)) { podwrite_scroll = podwrite_linecount-podwrite_screenlines; }
		if (podwrite_cursor_out_of_bounds < 0) {
			podwrite_scroll -= 2;
			if (podwrite_scroll < 0) { podwrite_scroll = 0; }
		} else if (podwrite_cursor_out_of_bounds > 0) {
			podwrite_scroll += 2;
			if (podwrite_scroll > (podwrite_linecount-podwrite_screenlines)) { podwrite_scroll = podwrite_linecount-podwrite_screenlines; }
			if (podwrite_scroll < 0) { podwrite_scroll = 0; }
		}
	}
	return (podwrite_scroll == s)?0:1;
}

int podwrite_widget_input(TWidget * wid, int ch)
{
	int ret;
	if ( (ret = ti_widget_input_ml(wid,ch)) != TTK_EV_UNUSED ) {
		if (podwrite_check_bounds()) {
			wid->dirty = 1;
		}
	}
	return ret;
}

void podwrite_widget_draw(TWidget * wid, ttk_surface srf)
{
	int h = wid->h - (((TiBuffer *)wid->data)->idata[2]) - 1;
	
	ttk_fillrect(srf, wid->x, wid->y, wid->x+wid->w, wid->y+wid->h, ti_ap_get(0));
	ti_multiline_text(srf, ttk_textfont, wid->x+5, wid->y+5, wid->w-15, wid->h-10-(((TiBuffer *)wid->data)->idata[2]),
		ti_ap_get(1), ((TiBuffer *)wid->data)->text, ((TiBuffer *)wid->data)->cpos,
		((podwrite_linecount > podwrite_screenlines)?podwrite_scroll:0),
		&podwrite_linecount, &podwrite_screenlines, &podwrite_cursor_out_of_bounds);
	
	if (podwrite_linecount > podwrite_screenlines) {
	ttk_ap_fillrect (srf, ttk_ap_get ("scroll.bg"), wid->x + wid->w - 10,
			 wid->y + ttk_ap_getx ("header.line") -> spacing,
			 wid->x + wid->w, wid->y + h);
	ttk_ap_rect (srf, ttk_ap_get ("scroll.box"), wid->x + wid->w - 10,
		     wid->y + ttk_ap_getx ("header.line") -> spacing,
		     wid->x + wid->w, wid->y + h);
	ttk_ap_fillrect (srf, ttk_ap_get ("scroll.bar"), wid->x + wid->w - 10,
			 wid->y + ttk_ap_getx ("header.line") -> spacing + ((podwrite_scroll) * (h-2) / podwrite_linecount),
			 wid->x + wid->w,
			 wid->y - ttk_ap_getx ("header.line") -> spacing + ((podwrite_scroll + podwrite_screenlines) * (h-2) / podwrite_linecount) );	
    }
}

int podwrite_widget_scroll(TWidget * wid, int dir)
{
	if (podwrite_mode == 0) { return ti_widget_start(wid); }
	
	TTK_SCROLLMOD (dir,4);
	switch (podwrite_mode) {
	case 1:
		ti_buffer_cmove((TiBuffer *)wid->data, dir);
		podwrite_check_bounds();
		wid->dirty = 1;
		break;
	case 2:
		podwrite_scroll += dir;
		if (podwrite_scroll < 0) podwrite_scroll = 0;
		if (podwrite_scroll > (podwrite_linecount - podwrite_screenlines)) podwrite_scroll = (podwrite_linecount - podwrite_screenlines);
		wid->dirty = 1;
		break;
	default:
		return TTK_EV_UNUSED;
		break;
	}
	return TTK_EV_CLICK;
}

int podwrite_widget_button(TWidget * wid, int btn, int t)
{
	if (btn == TTK_BUTTON_MENU ||
		btn == TTK_BUTTON_PLAY ||
		btn == TTK_BUTTON_ACTION ||
		podwrite_mode == 0) { return ti_widget_start(wid); }
	
	switch (podwrite_mode) {
	case 1:
		if (btn == TTK_BUTTON_PREVIOUS) {
			((TiBuffer *)wid->data)->cpos = 0;
			podwrite_scroll = 0;
			wid->dirty = 1;
		} else if (btn == TTK_BUTTON_NEXT) {
			((TiBuffer *)wid->data)->cpos = ((TiBuffer *)wid->data)->usize;
			podwrite_scroll = (podwrite_linecount - podwrite_screenlines);
			wid->dirty = 1;
		} else {
			return TTK_EV_UNUSED;
		}
		break;
	case 2:
		if (btn == TTK_BUTTON_PREVIOUS) {
			podwrite_scroll = 0;
			wid->dirty = 1;
		} else if (btn == TTK_BUTTON_NEXT) {
			podwrite_scroll = (podwrite_linecount - podwrite_screenlines);
			wid->dirty = 1;
		} else {
			return TTK_EV_UNUSED;
		}
		break;
	default:
		return TTK_EV_UNUSED;
		break;
	}
	return TTK_EV_CLICK;
}

/* PodWrite Initialization */

PzWindow * new_podwrite_window_with_text(char * dt)
{
	PzWindow * ret;
	TWidget * wid;
	
	podwrite_mode = 0;
	podwrite_linecount = 0;
	podwrite_screenlines = 0;
	podwrite_scroll = 0;
	podwrite_cursor_out_of_bounds = 0;
	podwrite_filename = 0;
	
	podwrite_win = ret = pz_new_window(_("PodWrite"), PZ_WINDOW_NORMAL);
	podwrite_wid = wid = ti_new_text_widget(0, 0, ret->w, ret->h, 1, dt, podwrite_callback, podwrite_widget_draw, podwrite_widget_input, 0);
	podwrite_buf = (TiBuffer *)wid->data;
	wid->scroll = podwrite_widget_scroll;
	wid->button = podwrite_widget_button;
	ttk_add_widget(ret, wid);
	ret = pz_finish_window(ret);
	ti_widget_start(wid);
	return ret;
}

PzWindow * new_podwrite_window_with_file(char * fn)
{
	PzWindow * w;
	FILE * f;
	char * buf;
	int fl;
	
	f = fopen(fn, "rb");
	if (!f) return 0;
	fseek(f, 0, SEEK_END);
	fl = ftell(f);
	if (fl<0) {
		fclose(f);
		return 0;
	}
	fseek(f, 0, SEEK_SET);
	buf = (char *)malloc(fl+1);
	if (!buf) {
		fclose(f);
		return 0;
	}
	fl = fread(buf, 1, fl, f);
	buf[fl]=0;
	fclose(f);
	
	w = new_podwrite_window_with_text(buf);
	podwrite_filename = strdup(fn);
	free(buf);
	return w;
}

PzWindow * new_podwrite_window()
{
	return new_podwrite_window_with_text("");
}

/* Module & File Browser Extension Initialization */

int podwrite_openable(const char * filename)
{
	struct stat st;
	stat(filename, &st);
	return !S_ISDIR(st.st_mode);
}

PzWindow * podwrite_open_handler(ttk_menu_item * item)
{
	return new_podwrite_window_with_file(item->data);
}

void podwrite_mod_init(void)
{
	pz_menu_add_action("/Extras/Applications/PodWrite", new_podwrite_window);
	
	podwrite_fbx.name = "Open with PodWrite";
	podwrite_fbx.makesub = podwrite_open_handler;
	pz_browser_add_action (podwrite_openable, &podwrite_fbx);
}

PZ_MOD_INIT(podwrite_mod_init)
