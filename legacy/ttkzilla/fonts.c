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
    // we intend to free both menufont and textfont.
    ttk_menufont = ttk_get_font (file, size);
    ttk_textfont = ttk_get_font (file, size);

    ttk_gc_set_font (pz_get_gc (0), ttk_textfont);
}

static void save_font (ttk_fontinfo *fi)
{
    char fontline[128];

    snprintf(fontline, 127, "%s,%d", fi->name, fi->size);
    set_string_setting(FONT_FILE, fontline);
    ttk_gc_set_font (pz_get_gc (0), ttk_textfont);
}

static TWindow *select_font (ttk_menu_item *item) 
{
    ttk_fontinfo *fi = (ttk_fontinfo *)item->data;

    ttk_done_font (ttk_textfont);
    ttk_done_font (ttk_menufont);

    ttk_textfont = ttk_get_font (fi->name, fi->size);
    ttk_menufont = ttk_get_font (fi->name, fi->size);
    ttk_epoch++;
    save_font (fi);
    return TTK_MENU_UPONE;
}

TWindow *new_font_window (ttk_menu_item *item)
{
    TWindow *ret = ttk_new_window();
    TWidget *this = ttk_new_menu_widget (0, ttk_menufont, ret->w, ret->h);
    ttk_fontinfo *cur = ttk_fonts;
    
    while (cur) {
	ttk_menu_item *item = calloc (1, sizeof(ttk_menu_item));

	item->name = cur->name;
	item->makesub = select_font;
	item->flags = 0;
	item->data = cur;
	ttk_menu_append (this, item);
	
	cur = cur->next;
    }

    ttk_add_widget (ret, this);
    ttk_window_title (ret, _("Select Font"));
    return ret;
}

