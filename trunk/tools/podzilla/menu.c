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
#include <stdlib.h>

#include "pz.h"
#include "ipod.h"
#include "piezo.h"

extern void new_contrast_window(void);
extern void new_browser_window(void);
extern void toggle_backlight(void);
extern void set_wheeldebounce(void);
extern void set_buttondebounce(void);
#ifdef __linux__
extern void new_record_mic_window(void);
extern void new_record_line_in_window(void);
extern void new_playback_browse_window(void);
#endif
extern void new_calendar_window(void);
extern void new_oth_window(void);
extern void new_steroids_window(void);
extern void new_bluecube_window(void);
extern void new_itunes_track(void);
extern void new_itunes_artist(void);
extern void new_itunes_album(void);
extern void new_itunes_plist(void);
extern void new_pong_window(void);
extern void new_mines_window(void);
extern void new_tictactoe_window(void);
extern void new_calc_window(void);
extern void new_poddraw_window(void);
extern void new_cube_window(void);
extern void about_window(void);

static GR_WINDOW_ID menu_wid;
static GR_GC_ID menu_gc;

extern void quit_podzilla(void);
extern void reboot_ipod(void);

static int max_menu_items = 6;

struct menu_item {
	char *text;		/* Menu text to be displayed */
	int type;		/* Menu type */
	void *ptr;		/* For OPTION_MENU types a pointer to an */
				/* options string, all other menu types, */
				/* optional procedure ptr */
	int setting_id; /* If type is BOOLEAN_MENU or OPTION_MENU */
			/* this can be an index into the global settings */
	int item_count; /* For OPTION_MENU types, number of option items */
};

/* menu types */
#define DISPLAY_MENU	0
#define SUB_MENU_HEADER	1
#define ACTION_MENU	2
#define BOOLEAN_MENU	3
#define OPTION_MENU	4
#define SUB_MENU_PREV	5
#define ACTION_MENU_PREV_SUB	6

#define NOSETTING -1 /* For menu items not associated with global settings */

#define MENUSTR_TRUE "On"
#define MENUSTR_FALSE "Off"

#define MENU_PIXELOFFSET 8 /* pixels from screen border where menu text is drawn */

typedef void (*menu_action_t) (void);

static struct menu_item fun_menu[] = {
	{"BlueCube", ACTION_MENU, new_bluecube_window, NOSETTING, 0},
	{"Cube", ACTION_MENU, new_cube_window, NOSETTING, 0},
	{"Nimesweeper", ACTION_MENU, new_mines_window, NOSETTING, 0},
	{"Othello", ACTION_MENU, new_oth_window, NOSETTING, 0},
	{"PodDraw", ACTION_MENU, new_poddraw_window, NOSETTING, 0},
	{"Pong", ACTION_MENU, new_pong_window, NOSETTING, 0},
	{"Steroids", ACTION_MENU, new_steroids_window, NOSETTING, 0},
	{"Tic-Tac-Toe", ACTION_MENU, new_tictactoe_window, NOSETTING, 0},
	{0, 0, 0, NOSETTING, 0}
};

static char *sample_rate_options[] = {
	"8 kHz","32 kHz","44.1 kHz","88.2 kHz","96 kHz"
};
static char *shuffle_options[] = {
	"Off","Songs"
};
static char *repeat_options[] = {
	"Off","One","All"
};

static struct menu_item recording_menu[] = {
#ifdef __linux__	
	{"Mic Record", ACTION_MENU, new_record_mic_window, NOSETTING, 0},
	{"Line In Record", ACTION_MENU, new_record_line_in_window, NOSETTING, 0},	
	{"Playback", ACTION_MENU, new_playback_browse_window, NOSETTING, 0},
#endif
	{"Sample Rate", OPTION_MENU, sample_rate_options, DSPFREQUENCY, 5},
	{0, 0, 0, NOSETTING, 0}
};

static struct menu_item extras_menu[] = {
	{"Recordings", SUB_MENU_HEADER, recording_menu, NOSETTING, 0},
#if 0
	{"Clock", SUB_MENU_HEADER, 0, NOSETTING, 0},
	{"Contacts", SUB_MENU_HEADER, 0, NOSETTING, 0},
#endif
	{"Calendar", ACTION_MENU, new_calendar_window, NOSETTING, 0},
	{"Calculator", ACTION_MENU, new_calc_window, NOSETTING, 0},
#if 0
	{"Notes", SUB_MENU_HEADER, 0, NOSETTING, 0},
#endif
	{"Fun", SUB_MENU_HEADER, fun_menu, NOSETTING, 0},
	{0, 0, 0, NOSETTING, 0}
};

static struct menu_item reset_menu[] = {
	{"Cancel", SUB_MENU_PREV, 0, NOSETTING, 0},
	{"Absolutely", ACTION_MENU_PREV_SUB, ipod_reset_settings, NOSETTING, 0},
	{0, 0, 0, NOSETTING, 0}
};

static struct menu_item settings_menu[] = {
	{"About", ACTION_MENU, about_window, NOSETTING, 0},
#if 0
	{"EQ", BOOLEAN_MENU, 0, EQUILIZER, 0},
#endif
	{"Repeat", OPTION_MENU, repeat_options, REPEAT, 3},
	{"Shuffle", BOOLEAN_MENU, 0, SHUFFLE, 0},
	{"Backlight", BOOLEAN_MENU, 0, BACKLIGHT, 0},
#if 0
	{"Backlight Timer", ACTION_MENU, set_backlight_timer, NOSETTING, 0},
#endif
	{"Contrast", ACTION_MENU, set_contrast, NOSETTING, 0},
	{"Wheel Sensitivity", ACTION_MENU, set_wheeldebounce, NOSETTING, 0},
	{"Button Debounce", ACTION_MENU, set_buttondebounce, NOSETTING, 0},
#if 0
	{"Alarms", SUB_MENU_HEADER, 0, NOSETTING, 0},
	{"Contacts", SUB_MENU_HEADER, 0, NOSETTING, 0},
#endif
	{"Clicker", BOOLEAN_MENU, 0, CLICKER, 0},
#if 0
	{"Language", SUB_MENU_HEADER, 0, NOSETTING, 0},
	{"Legal", SUB_MENU_HEADER, 0, NOSETTING, 0},
#endif
	{"Reset All Settings", SUB_MENU_HEADER, reset_menu, NOSETTING, 0},
	{"Save Settings", ACTION_MENU, ipod_save_settings, NOSETTING, 0},
	{"Load Settings", ACTION_MENU, ipod_load_settings, NOSETTING, 0},
	{0, 0, 0, NOSETTING, 0}
};

static struct menu_item reboot_menu[] = {
	{"Cancel", SUB_MENU_PREV, 0, NOSETTING, 0},
	{"Absolutely", ACTION_MENU, reboot_ipod, NOSETTING, 0},
	{0, 0, 0, NOSETTING, 0}
};

static struct menu_item itunes_menu[] = {
 	{"Playlists", ACTION_MENU, new_itunes_plist, NOSETTING, 0},
	{"Artists", ACTION_MENU, new_itunes_artist, NOSETTING, 0},
	{"Albums", ACTION_MENU, new_itunes_album, NOSETTING, 0},
	{"Songs", ACTION_MENU, new_itunes_track, NOSETTING, 0},
	{0, 0, 0, NOSETTING, 0}
};

static struct menu_item main_menu[] = {
	{"Music", SUB_MENU_HEADER, itunes_menu, NOSETTING, 0},
	{"Extras", SUB_MENU_HEADER, extras_menu, NOSETTING, 0},
	{"Settings", SUB_MENU_HEADER, settings_menu, NOSETTING, 0},
	{"File Browser", ACTION_MENU, new_browser_window, NOSETTING, 0},
	{"Quit Podzilla", ACTION_MENU, quit_podzilla, NOSETTING, 0},
	{"Reboot iPod", SUB_MENU_HEADER, reboot_menu, NOSETTING, 0},
	{0, 0, 0, NOSETTING, 0}
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
	GR_SIZE rj_width, rj_height, rj_base;
	GR_COORD menu_draw_x, menu_draw_y;
	int option_menu_val;
	char **option_menu_strings;

	struct menu_item *m = &menu[top_menu_item];

	GrGetGCTextSize(menu_gc, "M", -1, GR_TFASCII, &width, &height, &base);
	height += 4;

	i = 0;
	while (i <= 5) {
		if (i + top_menu_item == current_menu_item) {
			GrSetGCForeground(menu_gc, BLACK);
			GrFillRect(menu_wid, menu_gc, 0,
				   1 + i * height,
				   screen_info.cols, height);
			GrSetGCForeground(menu_gc, WHITE);
			GrSetGCUseBackground(menu_gc, GR_TRUE);
		} else {
			GrSetGCUseBackground(menu_gc, GR_FALSE);
			GrSetGCMode(menu_gc, GR_MODE_SET);
			GrSetGCForeground(menu_gc, WHITE);
			GrFillRect(menu_wid, menu_gc, 0,
				   1 + i * height,
				   screen_info.cols, height);
			GrSetGCForeground(menu_gc, BLACK);
		}

		if (m->text != 0) {
			menu_draw_x = MENU_PIXELOFFSET;
			menu_draw_y = 1 + ((i + 1) * height - 4);
			GrText(menu_wid, menu_gc, menu_draw_x, menu_draw_y,
				m->text, -1, GR_TFASCII);

			if ((m->type == BOOLEAN_MENU) && (m->setting_id != NOSETTING)) {
				if (!ipod_get_setting(m->setting_id)) {
					GrGetGCTextSize(menu_gc, MENUSTR_FALSE,
						-1,	GR_TFASCII, &rj_width, &rj_height, &rj_base);
					menu_draw_x = (screen_info.cols - rj_width) - MENU_PIXELOFFSET;
					GrText(menu_wid, menu_gc, menu_draw_x,
						menu_draw_y, MENUSTR_FALSE, -1, GR_TFASCII);
				} else {
					GrGetGCTextSize(menu_gc, MENUSTR_TRUE, -1,
						GR_TFASCII, &rj_width, &rj_height, &rj_base);
					menu_draw_x = (screen_info.cols - rj_width) - MENU_PIXELOFFSET;
					GrText(menu_wid, menu_gc, menu_draw_x,
						menu_draw_y, MENUSTR_TRUE, -1, GR_TFASCII);
				}
			}
			else if ((m->type == OPTION_MENU) && (m->setting_id != NOSETTING) &&
					 (m->ptr != 0)) {
				option_menu_val = ipod_get_setting(m->setting_id);
				if(option_menu_val < m->item_count) {
					option_menu_strings = (char **)m->ptr;
					GrGetGCTextSize(menu_gc,option_menu_strings[option_menu_val],
						-1,	GR_TFASCII, &rj_width, &rj_height, &rj_base);
					menu_draw_x = (screen_info.cols - rj_width) - MENU_PIXELOFFSET;
					GrText(menu_wid, menu_gc, menu_draw_x, menu_draw_y,
						option_menu_strings[option_menu_val], -1, GR_TFASCII);
				}
			}

			m++;
		}

		i++;

		if (i == max_menu_items)
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
	int option_menu_val;

	switch(event->type) {
	case GR_EVENT_TYPE_KEY_DOWN:
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
					((menu_action_t) menu[current_menu_item].ptr)();
				}
				break;
			case ACTION_MENU_PREV_SUB:
				if (menu[current_menu_item].ptr != 0) {
					((menu_action_t) menu[current_menu_item].ptr)();
				}
				event->keystroke.ch = 'm';
				menu_do_keystroke(event);
				break;
			case SUB_MENU_PREV:
				event->keystroke.ch = 'm';
				menu_do_keystroke(event);
				break;
			case BOOLEAN_MENU:
				if (menu[current_menu_item].setting_id != NOSETTING) {
					if (ipod_get_setting(menu[current_menu_item].setting_id)) {
						ipod_set_setting(menu[current_menu_item].setting_id,0);
					} else {
						ipod_set_setting(menu[current_menu_item].setting_id,1);
					}
				}
				if (menu[current_menu_item].ptr != 0) {
					((menu_action_t) menu[current_menu_item].ptr)();
				}
				draw_menu();
				break;
			case OPTION_MENU:
				if ((menu[current_menu_item].setting_id != NOSETTING) &&
					(menu->ptr != 0)) {
					option_menu_val = ipod_get_setting(
						menu[current_menu_item].setting_id);
					option_menu_val++;
					if ((option_menu_val >= menu[current_menu_item].item_count) ||
						(option_menu_val < 0)) {
						option_menu_val = 0;
					}
					ipod_set_setting(menu[current_menu_item].setting_id,
						option_menu_val);
				}
				draw_menu();
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
#ifdef 	IPOD
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
				if (current_menu_item - max_menu_items == top_menu_item) {
					top_menu_item++;
				}
				draw_menu();
				ret = 1;
			}
			break;
		}
		break;
	}
	return ret;
}

void new_menu_window()
{
	if (screen_info.cols == 138) { /*mini*/
		max_menu_items = 5;
	}

	menu_gc = pz_get_gc(1);
	GrSetGCUseBackground(menu_gc, GR_FALSE);
	GrSetGCForeground(menu_gc, BLACK);

	menu_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), menu_do_draw, menu_do_keystroke);

	GrSelectEvents(menu_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN|GR_EVENT_MASK_KEY_UP);

	GrMapWindow(menu_wid);
}
