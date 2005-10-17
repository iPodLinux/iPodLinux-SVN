/*
 * Copyright (C) 2005 Courtney Cavin
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

#define PZMOD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "pz.h"
#include "ipod.h"
#include "settings.h"

typedef struct 
{
    ttk_fontinfo *set;
    ttk_fontinfo *cur;
} font_data;

static ttk_fontinfo *thisfont;

void load_font()
{
    char *file;
    int size;

    file = (char *)get_string_setting(FONT_FILE);
    if (!file)
	file = "Espy Sans";

    if (!strchr (file, ',')) {
	size = 0;
    } else {
	size = atoi (strchr (file, ',') + 1);
	*strchr (file, ',') = 0;
    }

    // Note: TWO calls to ttk_get_font, because
    // we intend to free both menufont+textfont
    // but not thisfont.
    ttk_menufont = ttk_get_font (file, size);
    ttk_textfont = ttk_get_font (file, size);
    thisfont = ttk_menufont.fi;

    ttk_gc_set_font (pz_get_gc (0), ttk_textfont);
}

static void save_font (ttk_fontinfo *fi)
{
    char fontline[128];

    snprintf(fontline, 127, "%s,%d", fi->name, fi->size);
    set_string_setting(FONT_FILE, fontline);
    ttk_gc_set_font (pz_get_gc (0), ttk_textfont);
}

static void draw_centered (ttk_surface srf, ttk_font f, int w, int y, const char *str)
{
    int x = (w - ttk_text_width (f, str)) / 2;
    ttk_text (srf, f, x, y, ttk_makecol (BLACK), str);
}

#define _MAKETHIS font_data *data = (font_data *)this->data

static void font_draw (TWidget *this, ttk_surface srf) 
{
    _MAKETHIS;
    
    draw_centered (srf, data->cur->f, this->w, 2, data->cur->name);
    draw_centered (srf, data->cur->f, this->w, 17, "AaBbCcFfGgJjQqTt");
    draw_centered (srf, data->cur->f, this->w, 32, "0123456789 <([*])>");
    draw_centered (srf, data->set->f, this->w, this->h - 50, _("Scroll to preview. Be patient."));
    draw_centered (srf, data->set->f, this->w, this->h - 35, _("Might take a few sec. to load."));
    draw_centered (srf, data->set->f, this->w, this->h - 15, _("Use center button to select."));
}

static int font_down (TWidget *this, int button) 
{
    int ret = 0;
    _MAKETHIS;
    
    switch (button) {
    case TTK_BUTTON_ACTION:
    case TTK_BUTTON_PLAY:
	ttk_done_font (ttk_menufont);
	ttk_done_font (ttk_textfont);
	
	thisfont = data->set = data->cur;

	ttk_menufont = ttk_textfont = data->set->f;
	data->set->refs += 2;
	
	save_font (data->set);
	this->dirty++;
	ttk_epoch++;
	ret |= TTK_EV_CLICK;
	break;
    case TTK_BUTTON_MENU:
	ttk_hide_window (this->win);
	break;
    default:
	ret |= TTK_EV_UNUSED;
	break;
    }
    return ret;
}

static int font_scroll (TWidget *this, int dir) 
{
#ifdef IPOD
    static int sofar = 0;
    sofar += dir;
    if (sofar > -7 && sofar < 7) return 0;
    dir = sofar / 7;
    sofar = 0;
#endif
    _MAKETHIS;
    
    ttk_fontinfo *old = data->cur;

    if (dir > 0) {
	while (dir) {
	    data->cur = data->cur->next;
	    if (!data->cur) {
		data->cur = ttk_fonts;
	    }
	    dir--;
	}
	this->dirty++;
    } else if (dir < 0) {
	// Ugh. Iterating backwards in a singly-linked list.
	dir = -dir;
	while (dir) {
	    ttk_fontinfo *c = ttk_fonts;
	    if (data->cur == c) {
		while (c->next) c = c->next;
		data->cur = c; // set to end of list
	    } else {
		while (c && (c->next != data->cur)) c = c->next;
		if (c) data->cur = c;
	    }
	    dir--;
	}
	this->dirty++;
    }

    if (this->dirty && (old != data->cur)) {
	// Unload the old
	ttk_done_fontinfo (old);
	// load the new
	(void) ttk_get_font (data->cur->name, data->cur->size);

	return TTK_EV_CLICK;
    }
    return 0;
}

static void font_destroy (TWidget *this) 
{
    free (this->data);
}

void new_font_window (ttk_menu_item *item)
{
    TWindow *ret = ttk_new_window();
    TWidget *this = ttk_new_widget (0, 0);
    this->w = ret->w;
    this->h = ret->h;
    this->focusable = 1;
    this->draw = font_draw;
    this->down = font_down;
    this->scroll = font_scroll;
    this->destroy = font_destroy;
    this->data = calloc (1, sizeof(font_data));
    _MAKETHIS;
    
    data->set = data->cur = thisfont;
    thisfont->refs++;
    
    ttk_add_widget (ret, this);
    ttk_window_title (ret, _("Select Font"));
    ttk_show_window (ret);
}

