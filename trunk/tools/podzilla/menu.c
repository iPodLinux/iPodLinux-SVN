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

#include "pz.h"
#include "ipod.h"
#include "mlist.h"


extern void new_contrast_window(void);
extern void new_browser_window(void);
extern void toggle_backlight(void);
extern void set_wheeldebounce(void);
extern void set_buttondebounce(void);
#ifdef __linux__
extern void new_record_mic_window(void);
extern void new_record_line_in_window(void);
extern void new_playback_browse_window(void);
#endif /* __linux__ */
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
extern void new_tuxchess_window(void);
extern void last_tuxchess_window(void);
extern void new_calc_window(void);
extern void new_poddraw_window(void);
extern void new_cube_window(void);
extern void new_matrix_window(void);
extern void about_window(void);

extern void quit_podzilla(void);
extern void reboot_ipod(void);

static GR_WINDOW_ID menu_wid;
static GR_GC_ID menu_gc;
static menu_st *menuz;

static item_st tuxchess_menu[] = {
	{"Last Game", last_tuxchess_window, ACTION_MENU},
	{"New Game", new_tuxchess_window, ACTION_MENU},
	{0}
};

static item_st games_menu[] = {
	{"BlueCube", new_bluecube_window, ACTION_MENU},
	{"Nimesweeper", new_mines_window, ACTION_MENU},
	{"Othello", new_oth_window, ACTION_MENU},
	{"Pong", new_pong_window, ACTION_MENU},
	{"Steroids", new_steroids_window, ACTION_MENU},
	{"Tic-Tac-Toe", new_tictactoe_window, ACTION_MENU},
	{"TuxChess", tuxchess_menu, SUB_MENU_HEADER},
	{0}
};

static item_st stuff_menu[] = {
	{"Cube", new_cube_window, ACTION_MENU},
	{"Matrix", new_matrix_window, ACTION_MENU},
	{"PodDraw", new_poddraw_window, ACTION_MENU},
	{0}
};

static char *backlight_options[] = {
	"Off", "1 sec", "2 secs", "5 secs", "10 secs", "30 secs", "1 min"
};

static char *sample_rates[] = {
	"8 kHz", "32 kHz", "44.1 kHz", "88.2 kHz", "96 kHz"
};

static char *shuffle_options[] = {
	"Off", "Songs"
};

static char *repeat_options[] = {
	"Off", "One", "All"
};

static item_st recording_menu[] = {
#ifdef __linux__
	{"Mic Record", new_record_mic_window, ACTION_MENU},
	{"Line In Record", new_record_line_in_window, ACTION_MENU},
	{"Playback", new_playback_browse_window, ACTION_MENU},
#endif /* __linux__ */
	{"Sample Rate", sample_rates, OPTION_MENU, DSPFREQUENCY, 5},
	{0}
};

static item_st extras_menu[] = {
	{"Recordings", recording_menu, SUB_MENU_HEADER},
	{"Calendar", new_calendar_window, ACTION_MENU},
	{"Calculator", new_calc_window, ACTION_MENU},
	{"Games", games_menu, SUB_MENU_HEADER},
	{"Stuff", stuff_menu, SUB_MENU_HEADER},
	{0}
};

static item_st reset_menu[] = {
	{"Cancel", NULL, SUB_MENU_PREV},
	{"Absolutely", ipod_reset_settings, ACTION_MENU},
	{0}
};

static item_st settings_menu[] = {
	{"About", about_window, ACTION_MENU},
	{"Repeat", repeat_options, OPTION_MENU, REPEAT, 3},
	{"Shuffle", shuffle_options, OPTION_MENU, SHUFFLE, 2},
	{"Backlight Timer", backlight_options, OPTION_MENU, BACKLIGHT_TIMER, 7},
	{"Contrast", set_contrast, ACTION_MENU},
	{"Wheel Sensitivity", set_wheeldebounce, ACTION_MENU},
	{"Button Debounce", set_buttondebounce, ACTION_MENU},
	{"Clicker", NULL, BOOLEAN_MENU, CLICKER},
	{"Reset All Settings", reset_menu, SUB_MENU_HEADER},
	{"Save Settings", ipod_save_settings, ACTION_MENU},
	{"Load Settings", ipod_load_settings, ACTION_MENU},
	{0}
};

static item_st reboot_menu[] = {
	{"Cancel", NULL, SUB_MENU_PREV},
	{"Absolutely", reboot_ipod, ACTION_MENU},
	{0}
};

static item_st itunes_menu[] = {
	{"Playlists", new_itunes_plist, ACTION_MENU | ARROW_MENU},
	{"Artists", new_itunes_artist, ACTION_MENU | ARROW_MENU},
	{"Albums", new_itunes_album, ACTION_MENU | ARROW_MENU},
	{"Songs", new_itunes_track, ACTION_MENU | ARROW_MENU},
	{0}
};

static item_st main_menu[] = {
	{"Music", itunes_menu, SUB_MENU_HEADER},
	{"Extras", extras_menu, SUB_MENU_HEADER},
	{"Settings", settings_menu, SUB_MENU_HEADER},
	{"File Browser", new_browser_window, ACTION_MENU | ARROW_MENU},
	{"Quit Podzilla", quit_podzilla, ACTION_MENU},
	{"Reboot iPod", reboot_menu, SUB_MENU_HEADER},
	{0}
};

static void menu_do_draw()
{
	/* window is focused */
	if(menu_wid == GrGetFocus()) {
		pz_draw_header(menuz->title);
		menu_draw(menuz);
	}
}

static int menu_do_keystroke(GR_EVENT * event)
{
	int ret = 0;

	switch (event->type) {
	case GR_EVENT_TYPE_TIMER:
		menu_draw_timer(menuz);
		break;
	case GR_EVENT_TYPE_KEY_DOWN:
		switch (event->keystroke.ch) {
		case '\r':
		case '\n':
			menuz = menu_handle_item(menuz, menuz->sel);
			menu_do_draw();
			ret |= KEY_CLICK;
			break;
		case 'l':
			menu_shift_selected(menuz, -1);
			menu_draw(menuz);
			ret |= KEY_CLICK;
			break;
		case 'r':
			menu_shift_selected(menuz, 1);
			menu_draw(menuz);
			ret |= KEY_CLICK;
			break;
		case 'm':
			if(menuz->parent != NULL) {
				menuz = menu_destroy(menuz);
				menu_do_draw();
				ret |= KEY_CLICK;
			}
			break;
		case 'q':
			menu_destroy_all(menuz);
			pz_close_window(menu_wid);
			GrDestroyGC(menu_gc);
			exit(0);
			break;
		default:
			ret |= KEY_UNUSED;
			break;
		}
		break;
	default:
		ret |= EVENT_UNUSED;
		break;
	}
	return ret;
}

void new_menu_window()
{
	GrGetScreenInfo(&screen_info);

	menu_gc = pz_get_gc(1);
	GrSetGCUseBackground(menu_gc, GR_TRUE);
	GrSetGCForeground(menu_gc, BLACK);
	GrSetGCBackground(menu_gc, WHITE);

	menu_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols,
			screen_info.rows - (HEADER_TOPLINE + 1), menu_do_draw,
			menu_do_keystroke);

	GrSelectEvents(menu_wid, GR_EVENT_MASK_EXPOSURE| GR_EVENT_MASK_KEY_UP|
			GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_TIMER);

	menuz = menu_init(menu_wid, menu_gc, "podzilla", 0, 1,
			screen_info.cols, screen_info.rows -
			(HEADER_TOPLINE + 1), NULL, main_menu);

	GrMapWindow(menu_wid);
}
