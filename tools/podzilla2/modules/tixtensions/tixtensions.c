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
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

PzModule * module;

static char * tix_rename_oldname;
static char * tix_file_clipboard = 0;
static char * tix_file_clipboard_name = 0;
static int    tix_file_clipboard_copy = 0;

static ttk_menu_item tix_rename_menuitem;
static ttk_menu_item tix_mkdir_menuitem;
static ttk_menu_item tix_cut_menuitem;
static ttk_menu_item tix_copy_menuitem;
static ttk_menu_item tix_paste_menuitem;

extern TWidget * ti_new_standard_text_widget(int x, int y, int w, int h, int absheight, char * dt, int (*callback)(TWidget *, char *));
extern void ti_multiline_text(ttk_surface srf, ttk_font fnt, int x, int y, int w, int h, ttk_color col, const char *t, int cursorpos, int scroll, int * lc, int * sl, int * cb);
extern int ti_widget_start(TWidget * wid);

/* ===== COMMON ===== */

int tix_true(const char * fn)
{
	return 1;
}

/* ===== CUT COPY PASTE ===== */

int tix_paste_visible(ttk_menu_item * item)
{
	return !!tix_file_clipboard;
}

#define TIX_FILE_BUFFER_SIZE 10240

PzWindow * tix_paste_handler(ttk_menu_item * item)
{
	char fn[1024];
	FILE * fin;
	FILE * fout;
	char * buf;
	int buflen;
	if (tix_file_clipboard) {
		getcwd(fn, 1024);
		if (fn[strlen(fn)-1] != '/') strcat(fn, "/");
		strcat(fn, tix_file_clipboard_name);
		if (tix_file_clipboard_copy) {
			
			if (! (buf = (char *)malloc(TIX_FILE_BUFFER_SIZE)) ) {
				pz_error("malloc for file copy failed\n");
				return TTK_MENU_UPONE;
			}
			fin = fopen(tix_file_clipboard, "rb");
			fout = fopen(fn, "wb");
			while ((buflen = fread(buf, 1, TIX_FILE_BUFFER_SIZE, fin))) {
				fwrite(buf, 1, buflen, fout);
			}
			if (!feof(fin)) {
				pz_perror("Fwrite call returned error in file copy");
			}
			if (fin) fclose(fin);
			if (fout) fclose(fout);
			free(buf);
			
		} else {
			if (rename(tix_file_clipboard, fn)) {
				pz_perror("Rename call returned error");
			}
			free(tix_file_clipboard);
			free(tix_file_clipboard_name);
			tix_file_clipboard = 0;
			tix_file_clipboard_name = 0;
		}
	}
	return TTK_MENU_UPONE;
}

PzWindow * tix_copy_handler(ttk_menu_item * item)
{
	if (tix_file_clipboard) free(tix_file_clipboard);
	if (tix_file_clipboard_name) free(tix_file_clipboard_name);
	tix_file_clipboard = (char *)malloc(1024);
	getcwd(tix_file_clipboard, 1024);
	if (tix_file_clipboard[strlen(tix_file_clipboard)-1] != '/') strcat(tix_file_clipboard, "/");
	strcat(tix_file_clipboard, item->data);
	tix_file_clipboard_name = strdup(item->data);
	tix_file_clipboard_copy = 1;
	return TTK_MENU_UPONE;
}

PzWindow * tix_cut_handler(ttk_menu_item * item)
{
	if (tix_file_clipboard) free(tix_file_clipboard);
	if (tix_file_clipboard_name) free(tix_file_clipboard_name);
	tix_file_clipboard = (char *)malloc(1024);
	getcwd(tix_file_clipboard, 1024);
	if (tix_file_clipboard[strlen(tix_file_clipboard)-1] != '/') strcat(tix_file_clipboard, "/");
	strcat(tix_file_clipboard, item->data);
	tix_file_clipboard_name = strdup(item->data);
	tix_file_clipboard_copy = 0;
	return TTK_MENU_UPONE;
}

/* ===== MAKE DIR ===== */

void tix_mkdir_draw(TWidget * wid, ttk_surface srf)
{
	ttk_fillrect(srf, wid->x, wid->y, wid->w, wid->h, ttk_ap_getx("window.bg")->color);
	ttk_text(srf, ttk_menufont, wid->x, wid->y, ttk_ap_getx("window.fg")->color, _("Make directory named:"));
}

int tix_mkdir_callback(TWidget * wid, char * fn)
{
	pz_close_window(wid->win);
	if (fn[0]) {
		if (mkdir(fn, 0755)) {
			perror("Mkdir call returned error");
		}
	}
	return 0;
}

PzWindow * new_mkdir_window(ttk_menu_item * item)
{
	PzWindow * ret;
	TWidget * wid;
	TWidget * wid2;
	ret = pz_new_window(_("Make Directory"), PZ_WINDOW_NORMAL);
	wid = ti_new_standard_text_widget(10, 10+ttk_text_height(ttk_textfont)*2, ret->w-20, 10+ttk_text_height(ttk_textfont), 0, "", tix_mkdir_callback);
	ttk_add_widget(ret, wid);
	wid2 = ttk_new_widget(10, 10);
	wid2->w = ret->w-20;
	wid2->h = ttk_text_height(ttk_menufont);
	wid2->draw = tix_mkdir_draw;
	ttk_add_widget(ret, wid2);
	ret = pz_finish_window(ret);
	ti_widget_start(wid);
	return ret;
}

/* ===== RENAME ===== */

void tix_rename_draw(TWidget * wid, ttk_surface srf)
{
	ttk_fillrect(srf, wid->x, wid->y, wid->w, wid->h, ttk_ap_getx("window.bg")->color);
	ttk_text(srf, ttk_menufont, wid->x, wid->y, ttk_ap_getx("window.fg")->color, _("Rename file to:"));
}

int tix_rename_callback(TWidget * wid, char * fn)
{
	pz_close_window(wid->win);
	if (fn[0]) {
		if (rename(tix_rename_oldname, fn)) {
			perror("Rename call returned error");
		}
	}
	free(tix_rename_oldname);
	return 0;
}

PzWindow * new_rename_window(ttk_menu_item * item)
{
	PzWindow * ret;
	TWidget * wid;
	TWidget * wid2;
	tix_rename_oldname = strdup(item->data);
	ret = pz_new_window(_("Rename"), PZ_WINDOW_NORMAL);
	wid = ti_new_standard_text_widget(10, 10+ttk_text_height(ttk_textfont)*2, ret->w-20, 10+ttk_text_height(ttk_textfont), 0, tix_rename_oldname, tix_rename_callback);
	ttk_add_widget(ret, wid);
	wid2 = ttk_new_widget(10, 10);
	wid2->w = ret->w-20;
	wid2->h = ttk_text_height(ttk_menufont);
	wid2->draw = tix_rename_draw;
	ttk_add_widget(ret, wid2);
	ret = pz_finish_window(ret);
	ti_widget_start(wid);
	return ret;
}

/* ===== RUN ===== */

void tix_run_draw(TWidget * wid, ttk_surface srf)
{
	ttk_fillrect(srf, wid->x, wid->y, wid->w, wid->h, ttk_ap_getx("window.bg")->color);
	if (ttk_screen->w < 160) {
		ti_multiline_text(srf, ttk_textfont, wid->x, wid->y, wid->w, wid->h, ttk_ap_getx("window.fg")->color,
			_("Type the name of a file to open."), -1, 0, 0, 0, 0);
	} else if (ttk_screen->w < 200) {
		ti_multiline_text(srf, ttk_textfont, wid->x, wid->y, wid->w, wid->h, ttk_ap_getx("window.fg")->color,
			_("Type the name of a program, folder, or document to open."), -1, 0, 0, 0, 0);
	} else {
		ti_multiline_text(srf, ttk_textfont, wid->x, wid->y, wid->w, wid->h, ttk_ap_getx("window.fg")->color,
			_("Type the name of a program, folder, or document, and podzilla will open it for you."), -1, 0, 0, 0, 0);
	}
}

int tix_run_callback(TWidget * wid, char * fn)
{
	TWindow * w;
	pz_close_window(wid->win);
	if (fn[0]) {
		w = pz_browser_open(fn);
		if (w) {
			ttk_show_window(w);
		}
	}
	return 0;
}

PzWindow * new_run_window()
{
	PzWindow * ret;
	TWidget * wid;
	TWidget * wid2;
	ret = pz_new_window(_("Run..."), PZ_WINDOW_NORMAL);
	wid = ti_new_standard_text_widget(10, 10+ttk_text_height(ttk_textfont)*((ttk_screen->w < 160 || ttk_screen->w >= 320)?2:3), ret->w-20, 10+ttk_text_height(ttk_textfont), 0, "", tix_run_callback);
	ttk_add_widget(ret, wid);
	wid2 = ttk_new_widget(10, 5);
	wid2->w = ret->w-20;
	wid2->h = ttk_text_height(ttk_menufont)*((ttk_screen->w < 160 || ttk_screen->w >= 320)?2:3);
	wid2->draw = tix_run_draw;
	ttk_add_widget(ret, wid2);
	ret = pz_finish_window(ret);
	ti_widget_start(wid);
	return ret;
}

/* ===== MODULE INIT ===== */

void init_ti_extensions()
{
	module = pz_register_module ("tixtensions", 0);
	tix_file_clipboard = 0;
	tix_file_clipboard_name = 0;
	tix_file_clipboard_copy = 0;
	
	pz_menu_add_action ("/Run...", new_run_window);
	
	tix_rename_menuitem.name = _("Rename");
	tix_rename_menuitem.makesub = new_rename_window;
	pz_browser_add_action (tix_true, &tix_rename_menuitem);
	
	tix_mkdir_menuitem.name = _("Make Directory");
	tix_mkdir_menuitem.makesub = new_mkdir_window;
	pz_browser_add_action (tix_true, &tix_mkdir_menuitem);
	
	tix_cut_menuitem.name = _("Cut");
	tix_cut_menuitem.makesub = tix_cut_handler;
	pz_browser_add_action (tix_true, &tix_cut_menuitem);
	
	tix_copy_menuitem.name = _("Copy");
	tix_copy_menuitem.makesub = tix_copy_handler;
	pz_browser_add_action (tix_true, &tix_copy_menuitem);
	
	tix_paste_menuitem.name = _("Paste");
	tix_paste_menuitem.makesub = tix_paste_handler;
	tix_paste_menuitem.visible = tix_paste_visible;
	pz_browser_add_action (tix_true, &tix_paste_menuitem);
}

PZ_MOD_INIT(init_ti_extensions)
