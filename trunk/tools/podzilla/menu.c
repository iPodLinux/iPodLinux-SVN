/*
 * Copyright (C) 2004 Bernard Leach
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
#include <assert.h>

#include "pz.h"
#include "ipod.h"
#include "piezo.h"

extern void new_contrast_window(void);
extern void new_browser_window(void);
extern void toggle_backlight(void);

static GR_WINDOW_ID menu_wid;
static GR_GC_ID menu_gc;
static GR_SCREEN_INFO screen_info;

#define MAX_MENU_ITEMS 5

struct menu_item {
	char *text;
	int type;
	void *ptr;
};


#define DISPLAY_MENU	0
#define SUB_MENU_HEADER	1
#define ACTION_MENU	2
#define VALUE_MENU	3

typedef void (*menu_action_t) (void);


static struct menu_item browse_menu[] = {
	{"Artists", DISPLAY_MENU, 0},
	{"Albums", DISPLAY_MENU, 0},
	{"Songs", DISPLAY_MENU, 0},
	{"Genres", DISPLAY_MENU, 0},
	{"Composers", DISPLAY_MENU, 0},
	{0, 0, 0}
};

static struct menu_item extras_menu[] = {
	{"Clock", SUB_MENU_HEADER, 0},
	{"Contacts", SUB_MENU_HEADER, 0},
	{"Calendar", SUB_MENU_HEADER, 0},
	{"Notes", SUB_MENU_HEADER, 0},
	{"Games", SUB_MENU_HEADER, 0},
	{0, 0, 0}
};

static struct menu_item settings_menu[] = {
	{"About", SUB_MENU_HEADER, 0},
	{"Shuffle", VALUE_MENU, 0},
	{"Repeat", VALUE_MENU, 0},
	{"Sound Check", VALUE_MENU, 0},
	{"EQ -Off", VALUE_MENU, 0},
	{"Backlight Timer", SUB_MENU_HEADER, 0},
	{"Contrast", ACTION_MENU, new_contrast_window},
	{"Alarms", SUB_MENU_HEADER, 0},
	{"Contacts", SUB_MENU_HEADER, 0},
	{"Clicker", ACTION_MENU, toggle_piezo},
	{"Language", SUB_MENU_HEADER, 0},
	{"Legal", SUB_MENU_HEADER, 0},
	{"Reset All Settings", SUB_MENU_HEADER, 0},
	{0, 0, 0}
};

static struct menu_item main_menu[] = {
	{"Playlists", SUB_MENU_HEADER, 0},
	{"Browse", SUB_MENU_HEADER, browse_menu},
	{"Extras", SUB_MENU_HEADER, extras_menu},
	{"Settings", SUB_MENU_HEADER, settings_menu},
	{"Backlight", ACTION_MENU, toggle_backlight},
	{"File Browser", ACTION_MENU, new_browser_window},
	{0, 0, 0}
};

static int current_menu_item = 0;
static int top_menu_item = 0;
static int in_contrast = 0;

static struct menu_item *menu = main_menu;
static struct menu_item *menu_stack[5];
static int menu_item_stack[5];
static int top_menu_item_stack[5];
static int menu_stack_pos = 0;


static void draw_menu()
{
	int i;
	GR_SIZE width, height, base;
	struct menu_item *m = &menu[top_menu_item];

	GrGetGCTextSize(menu_gc, "M", -1, GR_TFASCII, &width, &height, &base);
	height += 5;

	i = 0;
	while (m->text != 0) {
		if (i + top_menu_item == current_menu_item) {
			GrSetGCForeground(menu_gc, WHITE);
			GrFillRect(menu_wid, menu_gc, 0,
				   1 + i * height,
				   screen_info.cols, height);
			GrSetGCForeground(menu_gc, BLACK);
			GrSetGCUseBackground(menu_gc, GR_FALSE);
		} else {
			GrSetGCUseBackground(menu_gc, GR_TRUE);
			GrSetGCMode(menu_gc, GR_MODE_SET);
			GrSetGCForeground(menu_gc, BLACK);
			GrFillRect(menu_wid, menu_gc, 0,
				   1 + i * height,
				   screen_info.cols, height);
			GrSetGCForeground(menu_gc, WHITE);
		}

		GrText(menu_wid, menu_gc, 8, 1 + (i + 1) * height - 4, m->text, -1, GR_TFASCII);

		m++;
		i++;

		if (i == MAX_MENU_ITEMS)
			break;
	}

	GrSetGCMode(menu_gc, GR_MODE_SET);
}

static void menu_do_draw()
{
	pz_draw_header("Podzilla");

	draw_menu();
}

static int menu_do_keystroke(GR_EVENT * event)
{
	static int rcount = 0;
	static int lcount = 0;
	int ret = -1;

	switch (event->keystroke.ch) {
	case '\r':		/* action key */
	case '\n':
		ret = 0;
		switch (menu[current_menu_item].type) {
		case SUB_MENU_HEADER:
			if (menu[current_menu_item].ptr != 0) {
				menu_stack[menu_stack_pos] = menu;
				menu_item_stack[menu_stack_pos] =
				    current_menu_item;
				top_menu_item_stack[menu_stack_pos++] =
				    top_menu_item;

				pz_draw_header(menu[current_menu_item].text);
				menu = (struct menu_item *)menu[current_menu_item].ptr;
				current_menu_item = 0;
				top_menu_item = 0;
				draw_menu();
			}
			break;
		case ACTION_MENU:
			if (menu[current_menu_item].ptr != 0) {
				((menu_action_t) menu[current_menu_item].
				 ptr) ();
			}
			break;

		}
		break;

	case 'm':		/* menu key */
		if (menu_stack_pos > 0) {
			menu = menu_stack[--menu_stack_pos];
			current_menu_item = menu_item_stack[menu_stack_pos];
			top_menu_item = top_menu_item_stack[menu_stack_pos];

			pz_draw_header(menu[current_menu_item].text);
			draw_menu();
			ret = 0;
		} else {
			GrClose();
			exit(1);
		}
		break;

	case 'q':
		GrClose();
		exit(1);

	case 'l':
		lcount++;
		if (lcount < 1) {
			break;
		}
		lcount = 0;
		if (current_menu_item) {
			if (current_menu_item == top_menu_item) {
				top_menu_item--;
			}
			current_menu_item--;
			draw_menu();
			ret = 0;
		}
		break;

	case 'r':
		rcount++;
		if (rcount < 1) {
			break;
		}
		rcount = 0;
		if (menu[current_menu_item + 1].text != 0) {
			current_menu_item++;
			if (current_menu_item - MAX_MENU_ITEMS == top_menu_item) {
				top_menu_item++;
			} 
			draw_menu();
			ret = 0;
		}
		break;
	}
	return ret;
}

static void menu_event_handler(GR_EVENT *event)
{
	int i;

	switch (event->type) {
	case GR_EVENT_TYPE_EXPOSURE:
		menu_do_draw();
		break;

	case GR_EVENT_TYPE_KEY_DOWN:
		if (menu_do_keystroke(event) == 0)
			beep();
		break;
	}
}

void new_menu_window()
{
	GrGetScreenInfo(&screen_info);

	menu_gc = GrNewGC();
	GrSetGCUseBackground(menu_gc, GR_TRUE);
	GrSetGCForeground(menu_gc, WHITE);

	menu_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), menu_event_handler);

	GrSelectEvents(menu_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(menu_wid);
}
