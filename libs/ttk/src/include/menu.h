/*
 * Copyright (c) 2005 Joshua Oreman
 *
 * This file is a part of TTK.
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

#ifndef _TTK_MENU_H_
#define _TTK_MENU_H_

#include <ttk.h>

// returns from menu handler:
#define TTK_MENU_DONOTHING  (TWindow *)0
#define TTK_MENU_UPONE      (TWindow *)1
#define TTK_MENU_UPALL      (TWindow *)2
#define TTK_MENU_ALREADYDONE (TWindow *)3
#define TTK_MENU_QUIT       (TWindow *)4
#define TTK_MENU_REPLACE    (TWindow *)5
#define TTK_MENU_DESC_MAX   (TWindow *)8

#define TTK_MENU_ICON_SUB 01
#define TTK_MENU_ICON_EXE 02
#define TTK_MENU_ICON_SND 04
#define TTK_MENU_ICON     07
#define TTK_MENU_MADESUB  010  // if sub is set, set flag. if makesub is set, don't.

/* private: */
#define TTK_MENU_ICON_FLASHOFF  0100
#define TTK_MENU_TEXT_SLEFT     0200
#define TTK_MENU_TEXT_SRIGHT    0400
#define TTK_MENU_TEXT_SCROLLING 0600

/* flags for the groups */
#define TTK_MENU_GROUP_HIDENAME	(0x01)	/* should the group name be hidden? */
#define TTK_MENU_GROUP_HEADER	(0x02)	/* the header item itself */
#define TTK_MENU_GROUP_ONLY	(0x10)	/* item is the only one */
#define TTK_MENU_GROUP_FIRST	(0x20)	/* item is the first menu item in a group */
#define TTK_MENU_GROUP_MIDDLE	(0x40)	/* item is in a group */
#define TTK_MENU_GROUP_LAST	(0x80)	/* item is the last menu item in a group */
#define TTK_MENU_GROUP_SET	(TTK_MENU_GROUP_FIRST|TTK_MENU_GROUP_MIDDLE|TTK_MENU_GROUP_LAST|TTK_MENU_GROUP_ONLY)	/* has F/M/L been set yet? */

typedef struct ttk_menu_item 
{
    const char *name; // required
    struct {
	TWindow *(*makesub) (struct ttk_menu_item *item); // called once, return value put in sub, flags |= madesub
	TWindow *sub;
    };
    int flags;
    void *data; // whatever you want
    const char **choices; // this, or sub; end with a NULL ptr
    int cdata;
    void (*choicechanged)(struct ttk_menu_item *item, int cdata);
    int (*choiceget)(struct ttk_menu_item *item, int cdata);
    int choice; // optional
    int (*visible)(struct ttk_menu_item *item);
    int free_name;
    int free_data; // if set, free(item->data) on destruction

    /* for groups */
    char * group_name;
    int group_flags;

    /* readonly */ int menuwidth, menuheight;
    /* private */ int nchoices;
    /* private */ int textofs, scrolldelay;
    /* private */ int textwidth, linewidth;
    /* private */ void *menudata;
    /* private */ int iconflash, iftime, textflash;
    /* readonly */ TWidget *menu;
    void *data2;
    void (*predraw)(struct ttk_menu_item *item);
} ttk_menu_item;

TWidget *ttk_new_menu_widget (ttk_menu_item *items, ttk_font font, int w, int h);
ttk_menu_item *ttk_menu_get_item (TWidget *_this, int i);
ttk_menu_item *ttk_menu_get_item_called (TWidget *_this, const char *s);
ttk_menu_item *ttk_menu_get_selected_item (TWidget *_this);
void ttk_menu_set_closeable (TWidget *_this, int closeable);
void ttk_menu_set_i18nable (TWidget *_this, int i18nable);
void ttk_menu_sort (TWidget *_this);
void ttk_menu_sort_groups (TWidget *_this);

void ttk_menu_sort_my_way (TWidget *_this, int (*cmp)(const void *, const void *));
void ttk_menu_sort_my_way_groups (TWidget *_this, int (*cmp)(const void *, const void *));
void ttk_menu_flash (ttk_menu_item *item, int nflashes);
void ttk_menu_append (TWidget *_this, ttk_menu_item *item);
void ttk_menu_insert (TWidget *_this, ttk_menu_item *item, int pos);
void ttk_menu_remove (TWidget *_this, int pos);
void ttk_menu_remove_by_ptr (TWidget *_this, ttk_menu_item *item);
void ttk_menu_remove_by_name (TWidget *_this, const char *name);
void ttk_menu_clear (TWidget *_this);
void ttk_menu_item_updated (TWidget *, ttk_menu_item *); // call whenever you edit that item
void ttk_menu_updated (TWidget *_this); // call whenever you edit the item list somehow;
                                       // FORGETS ABOUT ALL ADDED ITEMS!

void ttk_menu_draw (TWidget *_this, ttk_surface srf);
int ttk_menu_scroll (TWidget *_this, int dir);
int ttk_menu_down (TWidget *_this, int button);
int ttk_menu_button (TWidget *_this, int button, int time);
int ttk_menu_frame (TWidget *_this);
void ttk_menu_free (TWidget *_this); // You don't need to call this, just call ttk_free_widget()

TWindow *ttk_mh_sub (struct ttk_menu_item *item);
void *ttk_md_sub (struct ttk_menu_item *submenu);

#endif
