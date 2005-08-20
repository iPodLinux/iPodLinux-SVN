/*
 * Todo:
 * 	- make static menus dynamicable
 * 	o invert colors rather than setting them..
 *	o end line with an elipsces if wider than screen?
 * 	o multiple item selection and handling
 * 	
 * Thoughts:
 * 	o window effects? fade, disolve, slide, zoom, rotate, etc.
 * 	o sort by popularity/usage?
 *
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "pz.h"
#include "ipod.h"
#include "mlist.h"

#define TRANSITION_STEPS 16
#define CFLASH 512

#ifdef DEBUG
#define Dprintf printf
#else
#define Dprintf(...)
#endif

void menu_retext_pixmap(menu_st *menulist, int pixmap, item_st *item);
void menu_clear_pixmap(menu_st *menulist, int pos);
extern GR_FONT_ID get_current_font(void);

void menu_draw_timer(menu_st *menulist)
{
	char *c = strdup(">");
	int colors[] = {CS_ARROW0, CS_ARROW1, CS_ARROW2, CS_ARROW3,
			CS_ARROW3, CS_ARROW2, CS_ARROW1, CS_ARROW0};

	if(menulist->items[menulist->sel].op & CFLASH) {
		menulist->items[menulist->sel].op ^= CFLASH;
		menu_clear_pixmap(menulist, menulist->sel - menulist->top_item);
		menu_retext_pixmap(menulist, menulist->sel - menulist->top_item,
				&menulist->items[menulist->sel]);
		menu_handle_timer(menulist, 0);
		free(c);
		return;
	}

	if(menulist->items[menulist->sel].text_width >
			menulist->w - (8 + (menulist->scrollbar ? 8 : 0))) {
		int item, diff, move;
		item = menulist->sel - menulist->top_item;
		diff = (menulist->items[menulist->sel].text_width + 8) -
			(menulist->w - (8 + (menulist->scrollbar ? 8 : 0)));
		menulist->timer_step++;
		move = (!((menulist->timer_step / diff) % 2) ?
			(menulist->timer_step % diff) :
			(diff - (menulist->timer_step % diff)));
		if (menulist->timer == INT_MAX - 1)
			menulist->timer = 0;
		/* xor the pixmap */
		GrSetGCMode(menulist->menu_gc, GR_MODE_XOR);
		if(hw_version == 0 || (hw_version >= 60000 &&
					hw_version < 70000))
		/* make sure that the xor works properly for devices with
		 * fbrev turned off (host, photo) */
			GrSetGCForeground(menulist->menu_gc, WHITE);
		GrFillRect(menulist->pixmaps[menulist->pixmap_pos[item]],
				menulist->menu_gc, move, 0,
				menulist->w, menulist->height);
		GrCopyArea(menulist->menu_wid, menulist->menu_gc, menulist->x,
				(item * menulist->height) + menulist->y,
				menulist->w - (menulist->scrollbar ? 8 : 0),
				menulist->height,
				menulist->pixmaps[menulist->pixmap_pos[item]],
				move, 0, 0);
		/* un umm xor the pixmap */
		GrFillRect(menulist->pixmaps[menulist->pixmap_pos[item]],
				menulist->menu_gc, move, 0,
				menulist->w, menulist->height);
		GrSetGCMode(menulist->menu_gc, GR_MODE_SET);
		if(hw_version == 0 || (hw_version >= 60000 &&
					hw_version < 70000))
		/* reset fix for xor on certain devices (host, photo) */
			GrSetGCForeground(menulist->menu_gc, BLACK);
		free(c);
		return;
	}
	GrSetGCUseBackground(menulist->menu_gc, GR_FALSE);

	/* cycle through colors */
	if (menulist->timer_step < 0 || menulist->timer_step >= 12)
		menulist->timer_step = 0;
	GrSetGCForeground(menulist->menu_gc,
		appearance_get_color(colors[menulist->timer_step < 8 ?
			menulist->timer_step : 7]));
	menulist->timer_step++;

	/* executable instead */
	if(EXECUTE_MENU & menulist->items[menulist->sel].op)
		*c = 'x';
	/* double; with a 1px offset for bold */
	GrText(menulist->menu_wid, menulist->menu_gc, menulist->x +
			(menulist->w - 8) - 6, menulist->height *
			(menulist->sel - menulist->top_item) + menulist->y + 2,
			c, -1, GR_TFASCII | GR_TFTOP);
	GrText(menulist->menu_wid, menulist->menu_gc, menulist->x +
			(menulist->w - 8) - 7, menulist->height *
			(menulist->sel - menulist->top_item) + menulist->y + 2,
			c, -1, GR_TFASCII | GR_TFTOP);
	GrSetGCForeground(menulist->menu_gc, BLACK);
	GrSetGCUseBackground(menulist->menu_gc, GR_TRUE);

	free(c);
}

/* checks the selected item for timer worthy features */
void menu_handle_timer(menu_st *menulist, int force)
{
	item_st *item;
	item = &menulist->items[menulist->sel];
	/* is a submenu item or is wider than the screen */
	if((SUB_MENU_HEADER & item->op || item->text_width > menulist->w -
			(8 * 2) || ARROW_MENU & item->op || EXECUTE_MENU &
			item->op || CFLASH & item->op) && !force) {
		if(!menulist->timer)
			menulist->timer =
				GrCreateTimer(menulist->menu_wid, 150);
		menulist->timer_step = 0;
	}
	/* the timer needs destroying */
	else if(menulist->timer) {
		GrDestroyTimer(menulist->timer);
		menulist->timer = 0;
		menulist->timer_step = 0;
	}
}

/* draws the scrollbar; calculates it and all */
void menu_draw_scrollbar(menu_st *menulist)
{
	int per = menulist->screen_items * 100 / menulist->num_items;
	int height = (menulist->h - 2) * (per < 3 ? 3 : per) / 100;
	int y_top = ((((menulist->h - 3) - height) * 100) * menulist->sel /
			(menulist->num_items - 1)) / 100 + menulist->y + 1;
	int y_bottom = y_top + height;

	/* only draw if appropriate */
	if(menulist->screen_items >= menulist->num_items)
		return;

	/* draw the containing box */
	GrSetGCForeground(menulist->menu_gc,
				appearance_get_color( CS_SCRLBDR ));
	GrRect(menulist->menu_wid, menulist->menu_gc, menulist->w - 8,
			menulist->y, 8, menulist->h - 1);

	/* erase the scrollbar */
	GrSetGCForeground(menulist->menu_gc,
				appearance_get_color( CS_SCRLCTNR ));
	GrFillRect(menulist->menu_wid, menulist->menu_gc, menulist->w - (8 - 1),
			menulist->y + 1, (8 - 2), y_top - (menulist->y + 1));
	GrFillRect(menulist->menu_wid, menulist->menu_gc, menulist->w - (8 - 1),
			y_bottom, (8 - 2), (menulist->h - 3) - y_bottom);

	/* draw the bar */
	GrSetGCForeground(menulist->menu_gc,
				appearance_get_color( CS_SCRLKNOB ));
	GrFillRect(menulist->menu_wid, menulist->menu_gc, menulist->w -
			(8 - 1), y_top, (8 - 2), height);

	/* restore the fg */
	GrSetGCForeground(menulist->menu_gc, BLACK);
}

/* clears specified pixmap, (GrClearWindow doesnt work properly for a pixmap) */
void menu_clear_pixmap(menu_st *menulist, int pos)
{
	if(pos < 0 || pos > menulist->screen_items - 1) {
		Dprintf("menu_clear_pixmap::No Such Pixmap\n");
		return;
	}
	GrSetGCForeground(menulist->menu_gc, appearance_get_color( CS_BG ));
	GrFillRect(menulist->pixmaps[menulist->pixmap_pos[pos]],
			menulist->menu_gc, 0, 0, 440, menulist->height);
	GrSetGCForeground(menulist->menu_gc, appearance_get_color( CS_FG ));
}

static void create_pixmaps(menu_st *menulist, int num)
{
	int i;
	
	menulist->pixmaps = (GR_WINDOW_ID *)malloc(sizeof(GR_WINDOW_ID) * num);
	menulist->pixmap_pos = (int *)malloc(sizeof(int) * num);

	/* initialize pixmaps */
	for (i = 0; i < num; i++) {
		menulist->pixmaps[i] = GrNewPixmap(440, menulist->height, NULL);
		menulist->pixmap_pos[i] = i;
		menu_clear_pixmap(menulist, i);
	}
}

/* draws the pixmap on the screen; should probably be menu_draw_pixmap() */
/* this would be much cleaner if raster ops worked properly; perhaps I just
 * misunderstand how they are supposed to be used */
void menu_draw_item(menu_st *menulist, int item)
{
	if(item < 0 || item > menulist->screen_items - 1) {
		Dprintf("menu_draw_item::No Such Pixmap\n");
		return;
	}
	/* xor the pixmap */
	if(item == (menulist->sel - menulist->top_item)) {
		GrSetGCMode(menulist->menu_gc, GR_MODE_XOR);
		if(hw_version == 0 || (hw_version >= 60000 &&
					hw_version < 70000))
		/* make sure that the xor works properly for devices with
		 * fbrev turned off (host, photo) */
			GrSetGCForeground(menulist->menu_gc, WHITE );
			/* just xor the menu option for now */
			/* eventually, draw the correct colors... */
		GrFillRect(menulist->pixmaps[menulist->pixmap_pos[item]],
				menulist->menu_gc, 0, 0, menulist->w,
				menulist->height);
	}
	GrCopyArea(menulist->menu_wid, menulist->menu_gc, menulist->x,
			(item * menulist->height) + menulist->y,
			menulist->w - (menulist->scrollbar ? 8 : 0),
			menulist->height,
			menulist->pixmaps[menulist->pixmap_pos[item]],
			0, 0, 0);
	/* un umm xor the pixmap */
	if(item == (menulist->sel - menulist->top_item)) {
		GrFillRect(menulist->pixmaps[menulist->pixmap_pos[item]],
				menulist->menu_gc, 0, 0, menulist->w,
				menulist->height);
		GrSetGCMode(menulist->menu_gc, GR_MODE_SET);
		if(hw_version == 0 || (hw_version >= 60000 &&
					hw_version < 70000))
		/* reset fix for xor on certain devices (host, photo) */
			GrSetGCForeground(menulist->menu_gc, BLACK);
	}
}

/* simply erases and redoes the text on the pixmap */
void menu_retext_pixmap(menu_st *menulist, int pixmap, item_st *item)
{
	int op;
	char *text;

	if(pixmap < 0 || pixmap > menulist->screen_items - 1) {
		Dprintf("menu_retext_pixmap::No Such Pixmap\n");
		return;
	}
	if(item < 0) {
		Dprintf("menu_retext_pixmap::Item non-existent\n");
		return;
	}

	text = (TRANSLATE & menulist->op) ? gettext(item->text) : item->text;

	/* set the bit for a checked long item */
	if(!(LONG_ITEM & item->op)) {
		GR_SIZE width, height, base;
		GrGetGCTextSize(menulist->menu_gc, text, -1, GR_TFASCII,
				&width, &height, &base);
		item->text_width = width;

		item->op |= LONG_ITEM; 
	}

	menu_clear_pixmap(menulist, pixmap);

	if (UTF8 & menulist->op)
		op = GR_TFUTF8;
	else if (UC16 & menulist->op)
		op = GR_TFUC16;
	else if (ASCII & menulist->op)
		op = GR_TFASCII;
	/* this makes the text draw without outlines */
	GrSetGCUseBackground(menulist->menu_gc, GR_FALSE);
	GrText(menulist->pixmaps[menulist->pixmap_pos[pixmap]],
			menulist->menu_gc, 8, 1, text, strlen(text),
			op | GR_TFTOP);

	
	if(BOOLEAN_MENU & item->op) {
		GR_SIZE width, height, base;
		char option[4];
		/* get setting info */
		if(item->setting != 0)
			item->sel_option = ipod_get_setting(item->setting);
		/* draw boolean text */	
		strcpy(option, (item->sel_option ? _("On") : _("Off")));
		GrGetGCTextSize(menulist->menu_gc, option, -1, GR_TFASCII,
				&width,	&height, &base);
		GrText(menulist->pixmaps[menulist->pixmap_pos[pixmap]],
				menulist->menu_gc, (menulist->w - width) -
				(8 + 2), 1, option, -1, GR_TFASCII| GR_TFTOP);
	}
	else if(OPTION_MENU & item->op) {
		GR_SIZE width, height, base;
		char **option;
		/* get setting info */
		if(item->setting != 0)
			item->sel_option = ipod_get_setting(item->setting);
		/* draw option text */
		option = (char **)item->action;
		text = gettext(option[item->sel_option]);
		GrGetGCTextSize(menulist->menu_gc, text, -1, GR_TFASCII,
				&width,	&height, &base);
		GrText(menulist->pixmaps[menulist->pixmap_pos[pixmap]],
				menulist->menu_gc, (menulist->w - width) -
				(8 + 2), 1, text, -1, GR_TFASCII | GR_TFTOP);
	}
	else if((SUB_MENU_HEADER & item->op || ARROW_MENU & item->op) &&
			(item->text_width < (menulist->w - 8) - 8)) {
		GrSetGCUseBackground(menulist->menu_gc, GR_FALSE);
		GrText(menulist->pixmaps[menulist->pixmap_pos[pixmap]],
				menulist->menu_gc, (menulist->w - 8) - 7,
				2, ">", -1, GR_TFASCII | GR_TFTOP);
		GrText(menulist->pixmaps[menulist->pixmap_pos[pixmap]],
				menulist->menu_gc, (menulist->w - 8) - 6,
				2, ">", -1, GR_TFASCII | GR_TFTOP);
		GrSetGCUseBackground(menulist->menu_gc, GR_TRUE);
	}
	
	menu_draw_item(menulist, pixmap);
}

void menu_update_menu(menu_st *menulist)
{
	int i;

	/* wipe all of the pixmaps */
	for (i = 0; i < menulist->screen_items; i++) {
		menu_clear_pixmap(menulist, i);
	}

	if (get_current_font() != menulist->font) {
		int items;
		int h = menulist->height;
		GrDestroyGC(menulist->menu_gc);
		menulist->menu_gc = pz_get_gc(1);
		GrGetGCTextSize(menulist->menu_gc, "abcdefghijhlmnopqrstuvwxyz"
				"ABCDEFGHIJKLMNOPQRSTUVWXYZ", -1, GR_TFASCII,
				&menulist->width, &menulist->height,
				&menulist->base);
		/* add a 2px padding to the text */
		menulist->height += 4;
		items = (int)menulist->h/menulist->height;
		if (menulist->height != h) {
			for (i = 0; i < menulist->screen_items; i++)
				GrDestroyWindow(menulist->pixmaps[i]);
			free(menulist->pixmaps);
			free(menulist->pixmap_pos);
			create_pixmaps(menulist, items);
		}
		menulist->screen_items = items;
		menulist->font = get_current_font();
		if (menulist->sel > menulist->top_item + menulist->screen_items)
			menu_select_item(menulist, menulist->sel);

	}
	
	/* force the rest of the redraw */
	menulist->scheme_no = appearance_get_color_scheme();
	/* NOTE: if "color scheme" is anywhere other than on the first
	   screen's worth of menu items, this doesn't work correctly,
	   but that's not an issue right now */
	menulist->init = 0;
}

/* does the drawing, safe(and recommended) to use {in, as} an exposure event */
void menu_draw(menu_st *menulist)
{
	int i;

	/* appearance changed, force a redraw */
	if(menulist->scheme_no != appearance_get_color_scheme() ||
			get_current_font() != menulist->font) {
		menu_update_menu(menulist);
	}

	/* first draw; init onscreen text items */
	if(menulist->init == 0) {
		for(i = (menulist->num_items > menulist->screen_items) ?
				menulist->screen_items : menulist->num_items;
				i; i--)
			menu_retext_pixmap(menulist, i - 1,
				&menulist->items[menulist->top_item + (i - 1)]);
		menulist->init = 1;
	}
#if 0
	else if(menulist->lastsel == menulist->sel) {
		Dprintf("Aborted draw because %d == %d\n", menulist->sel,
				menulist->lastsel);
		return;
	}
	Dprintf("Continuing, %d != %d\n", menulist->sel, menulist->lastsel);
#endif
	/* draw each pixmap */
	for(i = 0; i < menulist->screen_items; i++)
		menu_draw_item(menulist, i);

	/* erase the bottom unused part of the allocated screen */
	GrSetGCForeground(menulist->menu_gc, appearance_get_color( CS_BG ));
	GrFillRect(menulist->menu_wid, menulist->menu_gc, menulist->x,
			menulist->height * menulist->screen_items, menulist->w,
			menulist->h - (menulist->height *
			menulist->screen_items));

	GrSetGCForeground(menulist->menu_gc, BLACK);

	/* draw scrollbar if needed */
	if(menulist->num_items > menulist->screen_items) {
		menulist->scrollbar = 1;
		menu_draw_scrollbar(menulist);
	}
	else
		menulist->scrollbar = 0;

	/* deal with the timer */
	menu_handle_timer(menulist, 0);

	menulist->lastsel = menulist->sel;
}
/* draws pixmaps starting at pos a, going to pos b; sofar unused/untested */
void menu_draw_range(menu_st *menulist, int a, int b)
{
	int i;
	if((a > menulist->screen_items - 1 || a < 0)
			|| (b > menulist->screen_items - 1 || b < 0)) {
		Dprintf("menu_draw_range::Invalid range\n");
		return;
	}
	for(i = a; (a > b ? (i >= b) : (i <= b)); (a > b ? i-- : i++))
		menu_draw_item(menulist, i);
}

/* selects specified item */
void menu_select_item(menu_st *menulist, int sel)
{
	int diff, i, j, tmp;
	
	/* if it goes off the end either way, do nothing */
	if(sel > menulist->num_items - 1 || sel < 0)
		return;

	/* the difference in select */
	diff = sel - menulist->top_item;

	/* set selected */
	menulist->sel = sel;
	
	/* if sel is below the visible screen */
	if(diff > (menulist->screen_items - 1)) {
		/* set top item */
		menulist->top_item = sel - (menulist->screen_items - 1);
		/* for each time the screen shifts down, rotate the pixmaps */
		for(j = diff - (menulist->screen_items - 1); j > 0; j--) {
			tmp = menulist->pixmap_pos[0];
			for(i = 0; i < menulist->screen_items - 1; i++)
				menulist->pixmap_pos[i] =
					menulist->pixmap_pos[i + 1];
			menulist->pixmap_pos[menulist->screen_items - 1] = tmp;
			/* draw appropriate text to the bottom pixmap */
			menu_retext_pixmap(menulist, menulist->screen_items -
					1, &menulist->items[sel - (j-1)]);
		}
	}
	/* if sel is above the visible screen */
	else if(diff < 0) {
		/* set top item */
		menulist->top_item = sel;
		/* for each time the screen shifts up, rotate the pixmaps */
		for(j = diff; j < 0; j++) {
			tmp = menulist->pixmap_pos[menulist->screen_items - 1];
			for(i = menulist->screen_items - 1; i > 0; i--)
				menulist->pixmap_pos[i] =
					menulist->pixmap_pos[i - 1];
			menulist->pixmap_pos[0] = tmp;
			/* draw appropriate text to the top pixmap */
			menu_retext_pixmap(menulist, 0,
					&menulist->items[sel - (j+1)]);
		}
	}
}
/* selection changer, for the lazy */
int menu_shift_selected(menu_st *menulist, int num)
{
	int sel;
	sel = menulist->sel + num;
	/* overboard */
	if(sel < 0 || sel > menulist->num_items - 1)
		return 0;
	menu_select_item(menulist, sel);
	return 1;
}

static void menu_left_transition(menu_st *menulist)
{
	menu_st *m;
	int i, jump;

	m = menulist->parent;
	GrCopyArea(m->transition, menulist->menu_gc, m->w, 0,
			menulist->w, menulist->h, menulist->menu_wid,
			menulist->x, menulist->y, 0);

	if (m->scheme_no != appearance_get_color_scheme()) {
		m->menu_wid = m->transition;
		m->x = 0;
		m->y = 0;

		menu_draw(m);

		m->menu_wid = menulist->menu_wid;
		m->x = menulist->x;
		m->y = menulist->y;
	}
	
	jump = (ipod_get_setting(SLIDE_TRANSIT)) ?
		m->w / TRANSITION_STEPS : 0;
	for (i = TRANSITION_STEPS; i; i--) {
		GrCopyArea(menulist->menu_wid, menulist->menu_gc,
				menulist->x, menulist->y, menulist->w,
				menulist->h, m->transition,
				i * jump, 0, 0);
	}
}

static menu_st *menu_right_transition(menu_st *menulist, item_st *item)
{
	menu_st *m;
	int i, jump;

	GrCopyArea(menulist->transition, menulist->menu_gc, 0, 0,
			menulist->w, menulist->h, menulist->menu_wid,
			menulist->x, menulist->y, 0);
	m = menu_init(menulist->transition, item->text,
			menulist->w, 0, menulist->w, menulist->h,
			menulist, (item_st *)item->action, menulist->op);
	menu_draw(m);

	m->menu_wid = menulist->menu_wid;
	m->x = menulist->x;
	m->y = menulist->y;

	jump = (ipod_get_setting(SLIDE_TRANSIT)) ?
		menulist->w / TRANSITION_STEPS : 0;
	for (i = 0; i < TRANSITION_STEPS; i++) {
		GrCopyArea(menulist->menu_wid, menulist->menu_gc,
				menulist->x, menulist->y, menulist->w,
				menulist->h, menulist->transition,
				(i + 1) * jump, 0, 0);
	}
	return m;
}

menu_st *menu_handle_item(menu_st *menulist, int num)
{
	item_st *item;
	/* out of bounds */
	if(num < 0 || num >= menulist->num_items) {
		Dprintf("menu_handle_item::Invalid item");
		return menulist;
	}
	item = &menulist->items[num];
	menulist->lastsel = -1;

	if(SUB_MENU_HEADER & item->op) {
		/* Destroy timer */
		menu_handle_timer(menulist, 1);
		/* create another sub-menu  */
		menulist = menu_right_transition(menulist, item);
	}
	else if(ACTION_MENU & item->op) {
		/* execute the function */
		void (* fp)(void);
		fp = item->action;
		(* fp)();
		menulist->lastsel = 0;
	}
	else if(BOOLEAN_MENU & item->op) {
		/* toggle boolean; draw */
		item->sel_option = !(item->sel_option);
		if(item->setting != 0)
			ipod_set_setting(item->setting, item->sel_option);
		menu_retext_pixmap(menulist, num - menulist->top_item, item);
	}
	else if(OPTION_MENU & item->op) {
		/* rotate option; draw */
		item->sel_option = ((item->sel_option + 1) >
				(item->item_count - 1) ? 0 :
				item->sel_option + 1);
		if(item->setting != 0)
			ipod_set_setting(item->setting, item->sel_option);
		menu_retext_pixmap(menulist, num - menulist->top_item, item);
	}
	
	/* this isnt an else, so you can do (ACTION_MENU|SUB_MENU_PREV) */
	if(SUB_MENU_PREV & item->op) {
		menulist = menu_destroy(menulist);
	}
	return menulist;
}

/* removes the referenced item from the menu */
void menu_delete_item(menu_st *menulist, int num)
{
	int i, tmp;
	/* if num is off the end either way, do nothing */
	if(num > menulist->num_items - 1 || num < 0) {
		Dprintf("menu_delete_item::Invalid item\n");
		return;
	}
	if(menulist->alloc_items == 0) {
		Dprintf("menu_delete_item::Deleting from a static menu causes "
				"undefined behavior\n");
		return;
	}

	/* assume removed item is onscreen */
	tmp = menulist->pixmap_pos[num-menulist->top_item];
	for(i = 0; i < (menulist->screen_items-(num-menulist->top_item))-1;
			i++) {
		menulist->pixmap_pos[(num-menulist->top_item) + i] =
			menulist->pixmap_pos[(num-menulist->top_item) + (i+1)];
		menu_draw_item(menulist, num-menulist->top_item + i);
	}
	menulist->pixmap_pos[menulist->screen_items - 1] = tmp;
	
	/* actually remove the item */
	for(i = num; i < menulist->num_items - 1; i++)
		menulist->items[i] = menulist->items[i + 1];
	menulist->num_items--;
	/* if last item was selected, shift selected */
	if(menulist->sel == menulist->num_items)
		menu_shift_selected(menulist, -1);

	/* erase pixmaps that are off the bottom of the list */
	if(menulist->top_item + (menulist->screen_items - 1) >
			menulist->num_items - 1)
		menu_clear_pixmap(menulist, menulist->screen_items - 1);

	/* draw appropriate text to the bottom pixmap */
	else
		menu_retext_pixmap(menulist, menulist->screen_items -
				1, &menulist->items[menulist->top_item +
				(menulist->screen_items - 1)]);

	/* specify if scrollbar should draw */
	if(menulist->num_items <= menulist->screen_items)
		menulist->scrollbar = 0;

}

void menu_add_item(menu_st *menulist, char *text, void *action, int count,
		unsigned int op)
{
	if(menulist->alloc_items == 0) {
		Dprintf("Can't add items to a static menu\n");
		return;
	}
	/* allocated more space to the item list if necessary */
	if(menulist->num_items + 1 > menulist->alloc_items) {
		menulist->items = (item_st *)realloc(menulist->items, (20 +
				menulist->alloc_items) * sizeof(item_st));
		menulist->alloc_items+=20;
	}
	
	menulist->items[menulist->num_items].action = action;
	menulist->items[menulist->num_items].item_count = count;
	menulist->items[menulist->num_items].op = op;
	menulist->items[menulist->num_items].text = text;
	menulist->items[menulist->num_items].orig_pos = menulist->num_items++;

	/* specify if scrollbar should draw */
	if(menulist->num_items > menulist->screen_items)
		menulist->scrollbar = 1;
}

inline void swap(item_st a[], int i, int j)
{
	item_st t = a[i];
	a[i] = a[j];
	a[j] = t;
}
/* sorting and indexing; this one doesnt require a sentiant */
void quicksort(item_st a[], int lower, int upper)
{
	int i, m;
	item_st pivot;

	if(lower < upper) {
		swap(a, lower, (upper+lower)/2);
		pivot = a[lower];
		m = lower;
		for(i = lower + 1; i <= upper; i++)
			if(strcasecmp(a[i].text, pivot.text) < 0)
				swap(a, ++m, i);
		swap(a, lower, m);
		quicksort(a, lower, m - 1);
		quicksort(a, m + 1, upper);
	}
}

/* sorts items alphabetically and indexes them */
void menu_sort_menu(menu_st *menulist)
{
	quicksort(menulist->items, 0, menulist->num_items-1);
}

/* flash selected item. ala apple fw */
void menu_flash_selected(menu_st * menulist)
{
	int item;
	item = menulist->sel - menulist->top_item;
	GrSetGCForeground(menulist->menu_gc, WHITE);
	GrFillRect(menulist->menu_wid, menulist->menu_gc, menulist->x,
			menulist->y + item * menulist->height,
			menulist->w - (menulist->scrollbar ? 8 : 0),
			menulist->height);
	GrSetGCForeground(menulist->menu_gc, BLACK);
	menulist->items[menulist->sel].op |= CFLASH;
	menu_handle_timer(menulist, 0);
}

#if 0
void menu_move_item(menu_st *menulist, int sel, int shift)
{
	int i;
	for(i = shift; (shift > 0) ? (i > 0; i--) : (i < 0; i++)) {
		swap(menulist->items, sel + ,) 
	}
}
#endif

/* menu initialization, make sure to do a menu_destroy when you are finished */
menu_st *menu_init(GR_WINDOW_ID menu_wid, char *title, int x, int y, int w,
		int h, menu_st *parent, item_st *items, int op)
{
	menu_st *menulist;
	int i;

	menulist = (menu_st *)malloc(sizeof(menu_st));

	/* starting with an empty slate */
	if(items == NULL) {
		menulist->items = (item_st *)malloc(20 * sizeof(item_st));
		menulist->num_items = 0;
		menulist->alloc_items = 20;
	}
	/* starting with a static menu */
	else {
		menulist->items = items;
		for(i = 0; items[i].text != 0; i++)
			items[i].orig_pos = i;
		menulist->num_items = i;
		menulist->alloc_items = 0;
	}
	
	menulist->title = title;
	menulist->op = op;
	menulist->sel = 0;
	menulist->init = 0;
	menulist->timer = 0;
	menulist->top_item = 0;
	menulist->scheme_no = -1; 
	menulist->scrollbar = 0;
	menulist->timer_step = 0;
	menulist->parent = parent;
	menulist->lastsel = -1;
	menulist->menu_gc = pz_get_gc(1); 
	GrGetGCTextSize(menulist->menu_gc, "abcdefghijhlmnopqrstuvwxyz"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ", -1, GR_TFASCII,
			&menulist->width, &menulist->height, &menulist->base);
	/* add a 2px padding to the text */
	menulist->height += 4;

	/* determine how many items can fit on a screen */
	menulist->screen_items = (int)h/menulist->height;
	menulist->x = x;
	menulist->y = y;
	menulist->w = w;
	menulist->h = h;

	menulist->menu_wid = menu_wid; 
	menulist->font = get_current_font(); 

	create_pixmaps(menulist, menulist->screen_items);

	menulist->transition = GrNewPixmap(menulist->w*2, menulist->h, NULL);
#if 0
	Dprintf("Init::%d items per screen at %dpx\n\tbecause %d/%d == %d\n",
			menulist->screen_items, menulist->height, menulist->h,
			menulist->height, menulist->screen_items);
#endif
	return menulist;
}

/* if you are apt enough to use this to clean-up, everyone profits */
menu_st *menu_destroy(menu_st *menulist)
{
	int i;
	menu_st *parent = menulist->parent;

	if(parent != NULL)
		menu_left_transition(menulist);

	/* make sure its not a static menu we are trying to free */
	if(menulist->alloc_items != 0)
		free(menulist->items);
	if(menulist->timer)
		GrDestroyTimer(menulist->timer);
	GrDestroyGC(menulist->menu_gc);
	for(i=0; i<menulist->screen_items; i++)
		GrDestroyWindow(menulist->pixmaps[i]);
	GrDestroyWindow(menulist->transition);
	free(menulist->pixmaps);
	free(menulist->pixmap_pos);
	free(menulist);

	return parent;
}

/* I AM BENDER.  PLEASE INSERT GIRDER */
/* destroy all humans. err.. menus */
void menu_destroy_all(menu_st *menulist)
{
	menu_st *parent = menulist;
	while((parent = menu_destroy(parent)) != NULL);
}

