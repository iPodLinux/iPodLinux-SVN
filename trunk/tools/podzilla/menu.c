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
#include <stdlib.h>

#include "pz.h"
#include "ipod.h"
#include "piezo.h"

extern void new_contrast_window(void);
extern void new_browser_window(void);
extern void toggle_backlight(void);
extern void set_wheeldebounce(void);
extern void set_buttondebounce(void);
extern void new_record_window(void);
extern void new_playback_browse_window(void);
extern void new_oth_window(void);
extern void new_bluecube_window(void);
extern void new_itunes_window(void);
extern void new_pong_window(void);
extern void new_mines_window(void);
extern void record_set_8(void);
extern void record_set_32(void);
extern void record_set_44(void);
extern void record_set_88(void);
extern void record_set_96(void);


static GR_WINDOW_ID menu_wid;
static GR_GC_ID menu_gc;
static GR_SCREEN_INFO screen_info;

extern void quit_podzilla(void);
extern void reboot_ipod(void);

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
#define SUB_MENU_PREV   4
#define ACTION_MENU_PREV_SUB	5

typedef void (*menu_action_t) (void);

static struct menu_item games_menu[] = {
	{"Othello", ACTION_MENU, new_oth_window},
	{"BlueCube", ACTION_MENU, new_bluecube_window},
	{"Pong", ACTION_MENU, new_pong_window},
	{"Nimesweeper", ACTION_MENU, new_mines_window},
	{0, 0, 0}
};


static struct menu_item sample_rate_menu[] = {
	{"8 kHz", ACTION_MENU_PREV_SUB, record_set_8},
	{"32 kHz", ACTION_MENU_PREV_SUB, record_set_32},
	{"44.1 kHz", ACTION_MENU_PREV_SUB, record_set_44},
	{"88.2 kHz", ACTION_MENU_PREV_SUB, record_set_88},
	{"96 kHz", ACTION_MENU_PREV_SUB, record_set_96},
	{0, 0, 0}
};

static struct menu_item recording_menu[] = {
	{"Mic Record", ACTION_MENU, new_record_window},
#if 0
	{"Line In Record", ACTION_MENU, new_record_window},
#endif
	{"Playback", ACTION_MENU, new_playback_browse_window},
	{"Sample Rate", SUB_MENU_HEADER, sample_rate_menu},
	{0, 0, 0}
};
static struct menu_item extras_menu[] = {
	{"Recordings", SUB_MENU_HEADER, recording_menu},
#if 0
	{"Clock", SUB_MENU_HEADER, 0},
	{"Contacts", SUB_MENU_HEADER, 0},
	{"Calendar", SUB_MENU_HEADER, 0},
	{"Notes", SUB_MENU_HEADER, 0},
#endif
	{"Games", SUB_MENU_HEADER, games_menu},
	{0, 0, 0}
};



static struct menu_item reset_menu[] = {
	{"Cancel", SUB_MENU_PREV, 0},
	{"Absolutely", ACTION_MENU, ipod_reset_settings},
	{0, 0, 0}
};

static struct menu_item settings_menu[] = {
#if 0
	{"About", SUB_MENU_HEADER, 0},
	{"Shuffle", VALUE_MENU, 0},
	{"Repeat", VALUE_MENU, 0},
	{"EQ -Off", VALUE_MENU, 0},
#endif
	{"Backlight", ACTION_MENU, toggle_backlight},
#if 0
	{"Backlight Timer", ACTION_MENU, set_backlight_timer},
#endif
	{"Contrast", ACTION_MENU, set_contrast},
	{"Wheel Sensitivity", ACTION_MENU, set_wheeldebounce},
	{"Button Debounce", ACTION_MENU, set_buttondebounce},
#if 0
	{"Alarms", SUB_MENU_HEADER, 0},
	{"Contacts", SUB_MENU_HEADER, 0},
#endif
	{"Clicker", ACTION_MENU, toggle_piezo},
#if 0
	{"Language", SUB_MENU_HEADER, 0},
	{"Legal", SUB_MENU_HEADER, 0},
#endif
	{"Reset All Settings", SUB_MENU_HEADER, reset_menu},
	{"Save Settings", ACTION_MENU, ipod_save_settings},
	{"Load Settings", ACTION_MENU, ipod_load_settings},
	{0, 0, 0}
};

static struct menu_item reboot_menu[] = {
	{"Cancel", SUB_MENU_PREV, 0},
	{"Absolutely", ACTION_MENU, reboot_ipod},
	{0, 0, 0}
};

static struct menu_item main_menu[] = {
#if 0
	{"Playlists", SUB_MENU_HEADER, 0},
#endif
	{"Music", ACTION_MENU, new_itunes_window},
	{"Extras", SUB_MENU_HEADER, extras_menu},
	{"Settings", SUB_MENU_HEADER, settings_menu},
	{"File Browser", ACTION_MENU, new_browser_window},
        {"Quit Podzilla", ACTION_MENU, quit_podzilla},
        {"Reboot iPod", SUB_MENU_HEADER, reboot_menu},
	{0, 0, 0}
};

static int current_menu_item = 0;
static int top_menu_item = 0;

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
	while (i <= 5) {
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

		if (m->text != 0) {
			GrText(menu_wid, menu_gc, 8, 1 + (i + 1) * height - 4, m->text, -1, GR_TFASCII);
			m++;
		}

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
#ifdef IPOD
	static int rcount = 0;
	static int lcount = 0;
#endif
	int ret = 0;

	switch (event->keystroke.ch) {
	case '\r':		/* action key */
	case '\n':
		ret = 1;
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
		case ACTION_MENU_PREV_SUB:
			if (menu[current_menu_item].ptr != 0) {
				((menu_action_t) menu[current_menu_item].
				 ptr) ();
			}
			event->keystroke.ch = 'm';
			menu_do_keystroke(event);
			break;
		case SUB_MENU_PREV:
			event->keystroke.ch = 'm';
			menu_do_keystroke(event);
			break;
			
		}
		break;

	case 'm':		/* menu key */
		ret = 1;
		if (menu_stack_pos > 0) {
			menu = menu_stack[--menu_stack_pos];
			current_menu_item = menu_item_stack[menu_stack_pos];
			top_menu_item = top_menu_item_stack[menu_stack_pos];

			pz_draw_header(menu[current_menu_item].text);
			draw_menu();
			ret = 1;
		}
		break;
	case 'l':
#ifdef IPOD
		lcount++;
		if (lcount < 1) {
			break;
		}
		lcount = 0;
#endif
		if (current_menu_item) {
			if (current_menu_item == top_menu_item) {
				top_menu_item--;
			}
			current_menu_item--;
			draw_menu();
			ret = 1;
		}
		break;

#ifndef IPOD
	case 'q':
		GrClose();
		exit(0);
		break;
#endif

	case 'r':
#ifdef IPOD
		rcount++;
		if (rcount < 1) {
			break;
		}
		rcount = 0;
#endif
		if (menu[current_menu_item + 1].text != 0) {
			current_menu_item++;
			if (current_menu_item - MAX_MENU_ITEMS == top_menu_item) {
				top_menu_item++;
			} 
			draw_menu();
			ret = 1;
		}
		break;
	}
	return ret;
}

void new_menu_window()
{
	GrGetScreenInfo(&screen_info);

	menu_gc = GrNewGC();
	GrSetGCUseBackground(menu_gc, GR_TRUE);
	GrSetGCForeground(menu_gc, WHITE);

	menu_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), menu_do_draw, menu_do_keystroke);

	GrSelectEvents(menu_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(menu_wid);
}
