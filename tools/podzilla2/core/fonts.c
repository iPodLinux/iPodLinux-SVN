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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "pz.h"

extern ttk_gc pz_get_gc (int copy);

void pz_load_font (ttk_font *f, const char *def, int setting, PzConfig *conf)
{
    char *file;
    int size;

    file = strdup (pz_get_string_setting(conf, setting));
    if (!file || !file[0])
	file = strdup (def);

    if (!strchr (file, ',')) {
	size = 0;
    } else {
	size = atoi (strchr (file, ',') + 1);
	*strchr (file, ',') = 0;
    }

    *f = ttk_get_font (file, size);
    free (file);
}

static void save_font (ttk_fontinfo *fi, int setting, PzConfig *config)
{
    char fontline[128];

    snprintf(fontline, 127, "%s,%d", fi->name, fi->size);
    pz_set_string_setting(pz_global_config, setting, fontline);

    if (setting == TEXT_FONT)
	ttk_gc_set_font (pz_get_gc (0), fi->f);
}

struct font_data
{
    ttk_fontinfo *fi;
    ttk_font *target;
    int setting;
    PzConfig *config;
};

#define _MAKETHIS struct font_data *this = (struct font_data *)item->data

static TWindow *set_font (ttk_menu_item *item) 
{
    _MAKETHIS;

    ttk_done_font (*this->target);
    *this->target = ttk_get_font (this->fi->name, this->fi->size);
    ttk_epoch++;
    save_font (this->fi, this->setting, this->config);
    return TTK_MENU_UPONE;
}

TWindow *pz_select_font (ttk_menu_item *item)
{
    TWindow *ret = ttk_new_window();
    TWidget *this = ttk_new_menu_widget (0, ttk_menufont, ret->w, ret->h);
    ttk_fontinfo *cur = ttk_fonts;
    
    while (cur) {
	ttk_menu_item *i = calloc (1, sizeof(ttk_menu_item));
	struct font_data *fd = calloc (1, sizeof(struct font_data));

	i->name = cur->name;
	i->makesub = set_font;
	i->flags = 0;
	fd->fi = cur;
	fd->target = item->data;
	fd->setting = item->cdata;
        fd->config = item->data2;
        if (!fd->config) fd->config = pz_global_config;
	i->data = fd;
	ttk_menu_append (this, i);
	
	cur = cur->next;
    }

    ttk_add_widget (ret, this);
    ttk_window_title (ret, _("Select Font"));
    return ret;
}

