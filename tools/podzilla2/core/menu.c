/*
 * Copyright (c) 2005 Joshua Oreman, David Carne, and Courtney Cavin
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

static TWidget *root_menu;
static int inited = 0;

// Structure:
// [menu]
// items:
//   item    (menu -> [menu], data -> whatever)
//   item
//   submenu (menu -> [menu], data -> [sub], makesub -> <mh_sub>)
//     [sub]
//     items:
//       item
//       item
// etc.

static void check_init() 
{
    if (!inited) {
	root_menu = ttk_new_menu_widget (0, ttk_menufont, ttk_screen->w - ttk_screen->wx,
					 ttk_screen->h - ttk_screen->wy);
	inited = 1;
    }
}

#define NO_ADD    -2
#define LOC_START  0
#define LOC_END   -1
static void add_at_loc (TWidget *menu, ttk_menu_item *item, int loc) 
{
    if (loc == LOC_END)
	ttk_menu_append (menu, item);
    else if (loc == LOC_START)
	ttk_menu_insert (menu, item, 0);
    else
	ttk_menu_insert (menu, item, loc);
}

static TWindow *mh_sub (ttk_menu_item *item)
{
    TWindow *ret = ttk_new_window();
    ttk_add_widget (ret, item->data);
    ttk_window_title (ret, item->name);
    ret->widgets->v->draw (ret->widgets->v, ret->srf);
    return ret;
}

static ttk_menu_item *resolve_menupath (const char *path, int loc)
{
    const char *p = path;
    TWidget *menu;
    ttk_menu_item *item;
    int i, len;

    check_init();
    menu = root_menu;

    while (p && *p) {
	while (*p == '/') p++;
	if (strchr (p, '/'))
	    len = strchr (p, '/') - p;
	else
	    len = strlen (p);
	
	for (i = 0; (item = ttk_menu_get_item (menu, i)) != 0; i++) {
	    if (!strncmp (item->name, p, len))
		break;
	}
	
	p = strchr (p, '/');
	if (!p || !p[1]) break;

	if (!item) {
	    if (loc == NO_ADD) return 0;
	    
	    item = calloc (1, sizeof(ttk_menu_item));
	    item->name = malloc (len + 1);
	    strncpy ((char *)item->name, p, len);
	    ((char *)item->name)[len] = 0;
	    item->makesub = mh_sub;
	    item->flags = TTK_MENU_ICON_SUB;
	    item->data = ttk_new_menu_widget (0, ttk_menufont, menu->w, menu->h);
	    add_at_loc (menu, item, loc);
	}

	menu = item->data;
    }

    if (!item) {
	if (loc == NO_ADD) return 0;
	
	// allocate new item
	item = calloc (1, sizeof(ttk_menu_item));
	if (p && !p[1]) { // asking for a submenu
	    item->makesub = mh_sub;
	    item->flags = TTK_MENU_ICON_SUB;
	    item->data = ttk_new_menu_widget (0, ttk_menufont, menu->w, menu->h);
	}
	add_at_loc (menu, item, loc);
    }
    if (strrchr (path, '/')) {
	if (p && !p[1]) {
	    item->name = path + strlen (path);
	    while (item->name != path && item->name[0] != '/')
		item->name--;
	    if (item->name != path)
		item->name++;
	} else {
	    item->name = strrchr (path, '/') + 1;
	}
    } else {
	item->name = path;
    }
    item->visible = 0; // always visible
    return item;
}

void pz_menu_add_after (const char *menupath, PzWindow *(*handler)(), const char *after) 
{
    int i;
    ttk_menu_item *item;
    TWidget *menu;
    
    if (strchr (menupath, '/')) {
	char *parentpath = strdup (menupath);
	*(strrchr (parentpath, '/') + 1) = 0;
	item = resolve_menupath (parentpath, LOC_END);
	menu = item->data;
    } else {
	menu = root_menu;
    }

    for (i = 0; (item = ttk_menu_get_item (menu, i)) != 0; i++) {
	if (!strcmp (item->name, after))
	    break;
    }
    if (!item)
	i = LOC_END;
    
    item = resolve_menupath (menupath, i);
    item->makesub = handler;
    item->flags = 0;
}

void pz_menu_add_top (const char *menupath, PzWindow *(*handler)()) 
{
    ttk_menu_item *item = resolve_menupath (menupath, LOC_START);
    item->makesub = handler;
    item->flags = 0;
}

#if 0
void pz_menu_add_legacy (const char *menupath, void (*handler)()) 
{
    ttk_menu_item *item = resolve_menupath (menupath, LOC_END);
    item->makesub = pz_mh_legacy;
    item->data = handler;
    item->flags = 0;
}
#endif

void pz_menu_add_action (const char *menupath, PzWindow *(*handler)()) 
{
    ttk_menu_item *item = resolve_menupath (menupath, LOC_END);
    item->makesub = handler;
    item->flags = 0;
}

void pz_menu_add_option (const char *menupath, const char **choices) 
{
    ttk_menu_item *item = resolve_menupath (menupath, LOC_END);
    item->choices = choices;
    ttk_menu_item_updated (item->menu, item);
}

int pz_menu_get_option (const char *menupath) 
{
    ttk_menu_item *item = resolve_menupath (menupath, NO_ADD);
    if (!item) return;
    return item->choice;
}

void pz_menu_set_option (const char *menupath, int choice) 
{
    ttk_menu_item *item = resolve_menupath (menupath, NO_ADD);
    if (!item) return;
    item->choice = choice;
}

static void ch_set_setting (ttk_menu_item *item, int sid) 
{ pz_set_int_setting (item->data, sid, item->choice); }
static int ch_get_setting (ttk_menu_item *item, int sid) 
{ return pz_get_int_setting (item->data, sid); }

void pz_menu_add_setting (const char *menupath, unsigned int sid, PzConfig *conf, const char **choices) 
{
    ttk_menu_item *item = resolve_menupath (menupath, LOC_END);
    item->choices = choices;
    item->choicechanged = ch_set_setting;
    item->choiceget = ch_get_setting;
    item->cdata = sid;
    item->data = conf;
    ttk_menu_item_updated (item->menu, item);
}

void pz_menu_sort (const char *menupath)
{
    ttk_menu_item *item = resolve_menupath (menupath, NO_ADD);
    if (!item) return;
    if (item->makesub != mh_sub) {
	pz_error ("Can't sort a non-menu");
	return;
    }
    ttk_menu_sort (item->data);
}

void pz_menu_remove (const char *menupath)
{
    ttk_menu_item *item = resolve_menupath (menupath, NO_ADD);
    if (!item) return;
    ttk_menu_remove_by_ptr (item->menu, item);
}

