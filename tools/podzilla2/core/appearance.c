/*
 * appearance - color scheming and such for all of podzilla
 * Copyright (C) 2005 Scott Lawrence
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include <stdio.h>	/* NULL */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include "pz.h"

#ifdef IPOD
#define SCHEMESDIR "/usr/share/schemes/"
#else
#define SCHEMESDIR "schemes/"
#endif

/* this sets a new scheme.  It constrains the input value to something valid */
static void set_color_scheme (const char *file, int save)
{
    if (save) {
    	unlink (SCHEMESDIR "default.cs");
    	symlink (file, SCHEMESDIR "default.cs");
    }
    
    ttk_epoch++;
    
    int odfd = open (".", O_RDONLY);
    chdir (SCHEMESDIR);
    ttk_ap_load (file);
    fchdir (odfd);
    close (odfd);

    pz_header_colors_dirty();
}

/* color scheme selector */
static int apsc_scroll (TWidget *this, int dir) 
{
	int ret = ttk_menu_scroll (this, dir);
	set_color_scheme (ttk_menu_get_selected_item (this) -> data, 0);
	ttk_epoch++;
	return ret;
}
static int apsc_button (TWidget *this, int button, int time)
{
	if (button == TTK_BUTTON_ACTION) {
		set_color_scheme (ttk_menu_get_selected_item (this) -> data, 1);
		return ttk_menu_button (this, TTK_BUTTON_MENU, 0);
	} else if (button == TTK_BUTTON_MENU) {
            ttk_ap_load (SCHEMESDIR "default.cs");
        }
    return ttk_menu_button (this, button, time);
}

TWindow *pz_select_color_scheme() 
{
    char linktarget[256];
    TWindow *ret = ttk_new_window();
    TWidget *menu = ttk_new_menu_widget (0, ttk_menufont,
                                         ttk_screen->w - ttk_screen->wx,
                                         ttk_screen->h - ttk_screen->wy);
    menu->scroll = apsc_scroll;
    menu->button = apsc_button;

    if (readlink (SCHEMESDIR "default.cs", linktarget, 256) < 0) {
        pz_perror ("reading default.cs symlink");
        linktarget[0] = 0;
    }

    int odfd = open (".", O_RDONLY);
    chdir (SCHEMESDIR);
    DIR *dp = opendir (".");
    if (!dp) {
    	pz_perror (SCHEMESDIR);
    	return TTK_MENU_DONOTHING;
    }
    struct dirent *d;
    int iidx = 0;
    while ((d = readdir (dp)) != 0) {
    	if (strchr (d->d_name, '.') && !strcmp (strrchr (d->d_name, '.'), ".cs") && !!strcmp (d->d_name, "default.cs")) {
    		ttk_menu_item *item = calloc (1, sizeof(struct ttk_menu_item));
    		FILE *f;
    		char buf[100];

    		f = fopen (d->d_name, "r");
    		if (!f) continue;
    		
    		item->data = malloc (strlen (d->d_name) + 1);
    		strcpy (item->data, d->d_name);
    		
    		fgets (buf, 100, f);
    		if (strchr (buf, ' ')) {
    			item->name = strdup (strchr (buf, ' ') + 1);
			*(strchr(item->name, '\n')) = '\0'; 
		}
    		else
    			item->name = strdup (d->d_name);
    		fclose (f);
    		
    		ttk_menu_append (menu, item);
                iidx++;
                if (!strcmp (d->d_name, linktarget)) {
                    ttk_menu_scroll (menu, iidx * 5);
                }
    	}
    }
    closedir (dp);
    fchdir (odfd);
    close (odfd);
    
    ttk_add_widget (ret, menu);
    ttk_window_set_title (ret, _("Color Scheme"));
    return ret;
}
