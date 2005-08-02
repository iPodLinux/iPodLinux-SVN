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
extern void new_clock_window(void);
extern void new_world_clock_window(void);
extern void new_Set_Time_window(void);
extern void new_Set_DateTime_window(void);
extern void new_oth_window(void);
extern void new_steroids_window(void);
extern void new_bluecube_window(void);
extern void new_mandel_window(void);
extern void new_itunes_track(void);
extern void new_itunes_artist(void);
extern void new_itunes_album(void);
extern void new_itunes_plist(void);
extern void new_pong_window(void);
extern void new_mines_window(void);
extern void new_tictactoe_window(void);
extern void new_tunnel_window(void);
extern void new_tuxchess_window(void);
extern void last_tuxchess_window(void);
extern void new_calc_window(void);
extern void new_dialer_window(void);
extern void new_poddraw_window(void);
extern void new_cube_window(void);
extern void new_matrix_window(void);
extern void new_ipobble_window(void);
extern void new_invaders_window(void);
extern void new_font_window(menu_st *);
extern void new_vortex_window(void);
extern void new_wumpus_window(void);
extern void about_window(void);
#ifdef MIKMOD
extern void new_mikmod_window(void);
#endif

extern item_st lights_menu[];

extern void quit_podzilla(void);
extern void poweroff_ipod(void);
extern void reboot_ipod(void);
static void change_font(void);

static GR_WINDOW_ID menu_wid;
static GR_GC_ID menu_gc;
static menu_st *menuz;

static item_st tuxchess_menu[] = {
	{N_("Last Game"), last_tuxchess_window, ACTION_MENU},
	{N_("New Game"), new_tuxchess_window, ACTION_MENU},
	{0}
};

static item_st games_menu[] = {
	{N_("BlueCube"), new_bluecube_window, ACTION_MENU},
	{N_("Hunt The Wumpus"), new_wumpus_window, ACTION_MENU},
	{N_("Invaders"), new_invaders_window, ACTION_MENU},
	{N_("iPobble"), new_ipobble_window, ACTION_MENU},
	{N_("Lights"), lights_menu, SUB_MENU_HEADER},
	{N_("Minesweeper"), new_mines_window, ACTION_MENU},
	{N_("Othello"), new_oth_window, ACTION_MENU},
	{N_("Pong"), new_pong_window, ACTION_MENU},
	{N_("Steroids"), new_steroids_window, ACTION_MENU},
	{N_("Tic-Tac-Toe"), new_tictactoe_window, ACTION_MENU},
	{N_("Tunnel"), new_tunnel_window, ACTION_MENU},
	{N_("TuxChess"), tuxchess_menu, SUB_MENU_HEADER},
	{N_("Vortex Demo"), new_vortex_window, ACTION_MENU},
	{0}
};

static item_st stuff_menu[] = {
	{N_("Cube"), new_cube_window, ACTION_MENU},
	{N_("Dialer"), new_dialer_window, ACTION_MENU},
	{N_("MandelPod"), new_mandel_window, ACTION_MENU},
	{N_("Matrix"), new_matrix_window, ACTION_MENU},
	{N_("PodDraw"), new_poddraw_window, ACTION_MENU},
	{0}
};

static char *backlight_options[] = {
	N_("Off"), N_("1 sec"), N_("2 secs"), N_("5 secs"), N_("10 secs"),
	N_("30 secs"), N_("1 min"), N_("On")
};

static char *sample_rates[] = {
	"8 kHz", "32 kHz", "44.1 kHz", "88.2 kHz", "96 kHz"
};

static char *shuffle_options[] = {
	N_("Off"), N_("Songs")
};

static char *repeat_options[] = {
	N_("Off"), N_("One"), N_("All")
};

static item_st recording_menu[] = {
#ifdef __linux__
	{N_("Mic Record"), new_record_mic_window, ACTION_MENU},
	{N_("Line In Record"), new_record_line_in_window, ACTION_MENU},
	{N_("Playback"), new_playback_browse_window, ACTION_MENU},
#endif /* __linux__ */
	{N_("Sample Rate"), sample_rates, OPTION_MENU, DSPFREQUENCY, 5},
	{0}
};

extern char * clocks_timezones[]; /* for the timezones.  in clocks.c */
extern char * clocks_dsts[];	  /* for dst display.  in clocks.c */

static item_st world_clock_menu[] = {
	{N_("Local Clock"), new_clock_window, ACTION_MENU},
	{N_("World Clock"), new_world_clock_window, ACTION_MENU},
	{N_("TZ"), clocks_timezones, OPTION_MENU, TIME_WORLDTZ, 39},
	{N_("DST"), clocks_dsts, OPTION_MENU, TIME_WORLDDST, 3},
	{0}
};

static item_st extras_menu[] = {
	{N_("Recordings"), recording_menu, SUB_MENU_HEADER},
	{N_("Calendar"), new_calendar_window, ACTION_MENU},
	{N_("Calculator"), new_calc_window, ACTION_MENU},
	{N_("Clock"), world_clock_menu, SUB_MENU_HEADER},
	{N_("Games"), games_menu, SUB_MENU_HEADER},
	{N_("Stuff"), stuff_menu, SUB_MENU_HEADER},
	{0}
};

static item_st reset_menu[] = {
	{N_("Cancel"), NULL, SUB_MENU_PREV},
	{N_("Absolutely"), ipod_reset_settings, ACTION_MENU | SUB_MENU_PREV},
	{0}
};

static char * time1224_options[] = { N_("12-hour"), N_("24-hour") };

static item_st clocks_menu[] = {
        { N_("Clock"), new_clock_window, ACTION_MENU },
        { N_("Set Time"), new_Set_Time_window, ACTION_MENU },
        { N_("Set Time & Date"), new_Set_DateTime_window, ACTION_MENU },
/* -- future expansion --
	{ N_("Set Alarm"), NULL, SUB_MENU_PREV },
	{ N_("Set Sleep Timer"), NULL, SUB_MENU_PREV },
	{ N_("Time In Title"), NULL, BOOLEAN_MENU, TIME_IN_TITLE },
*/
	{ N_("TZ"), clocks_timezones, OPTION_MENU, TIME_ZONE, 39 },
	{ N_("DST"), clocks_dsts, OPTION_MENU, TIME_DST, 3},
	{ N_("Time"), time1224_options, OPTION_MENU, TIME_1224, 2 },
	{ N_("Time Tick Noise"), NULL, BOOLEAN_MENU, TIME_TICKER },
        { 0 }
};

static item_st appearance_menu[] = {
	{N_("Color Scheme"), colorscheme_names, OPTION_MENU, COLORSCHEME, CS_NSCHEMES },
	{N_("Decorations"), appearance_decorations, OPTION_MENU, DECORATIONS, NDECORATIONS },
	{N_("Battery Digits"), NULL, BOOLEAN_MENU, BATTERY_DIGITS },
	{N_("Display Load Average"), NULL, BOOLEAN_MENU, DISPLAY_LOAD },
	{N_("Menu Transition"), NULL, BOOLEAN_MENU, SLIDE_TRANSIT},
	{N_("Font"), change_font, ACTION_MENU},
	{ 0 }
};

static void change_font()
{
	new_font_window(menuz);
}

void menu_adjust_nschemes( int val )
{
	// '0' is the index above for "Color Scheme"
	appearance_menu[0].item_count = val;
}

static item_st settings_menu[] = {
	{N_("About"), about_window, ACTION_MENU},
	{N_("Date & Time"), clocks_menu, SUB_MENU_HEADER},
	{N_("Repeat"), repeat_options, OPTION_MENU, REPEAT, 3},
	{N_("Shuffle"), shuffle_options, OPTION_MENU, SHUFFLE, 2},
	{N_("Contrast"), set_contrast, ACTION_MENU},
	{N_("Wheel Sensitivity"), set_wheeldebounce, ACTION_MENU},
	{N_("Button Debounce"), set_buttondebounce, ACTION_MENU},
	{N_("Backlight Timer"), backlight_options, OPTION_MENU, BACKLIGHT_TIMER, 8},
	{N_("Clicker"), NULL, BOOLEAN_MENU, CLICKER},
	{N_("Appearance"), appearance_menu, SUB_MENU_HEADER},
	{N_("Browser Path Display"), NULL, BOOLEAN_MENU, BROWSER_PATH},
	{N_("Show Hidden Files"), NULL, BOOLEAN_MENU, BROWSER_HIDDEN},
	{N_("Reset All Settings"), reset_menu, SUB_MENU_HEADER},
	{N_("Save Settings"), ipod_save_settings, ACTION_MENU},
	{N_("Load Settings"), ipod_load_settings, ACTION_MENU},
	{0}
};

static item_st reboot_menu[] = {
	{N_("Cancel"), NULL, SUB_MENU_PREV},
	{N_("Absolutely"), reboot_ipod, ACTION_MENU},
	{0}
};

static item_st turnoff_menu[] = {
	{N_("Cancel"), NULL, SUB_MENU_PREV},
	{N_("Absolutely"), poweroff_ipod, ACTION_MENU},
	{0}
};

static item_st itunes_menu[] = {
	{N_("Playlists"), new_itunes_plist, ACTION_MENU | ARROW_MENU},
	{N_("Artists"), new_itunes_artist, ACTION_MENU | ARROW_MENU},
	{N_("Albums"), new_itunes_album, ACTION_MENU | ARROW_MENU},
	{N_("Songs"), new_itunes_track, ACTION_MENU | ARROW_MENU},
#ifdef MIKMOD
	{N_("MikMod"), new_mikmod_window, ACTION_MENU},
#endif
	{0}
};

static item_st power_menu[] = {
	{N_("Quit Podzilla"), quit_podzilla, ACTION_MENU},
	{N_("Reboot iPod"), reboot_menu, SUB_MENU_HEADER},
#ifdef NEVER /* just to show where this should go */
	{N_("Sleep iPod"), sleep, ACTION_MENU};
#endif
	{N_("Turn off iPod"), turnoff_menu, SUB_MENU_HEADER},	
	{0}
};

static item_st main_menu[] = {
	{N_("Music"), itunes_menu, SUB_MENU_HEADER},
	{N_("Extras"), extras_menu, SUB_MENU_HEADER},
	{N_("Settings"), settings_menu, SUB_MENU_HEADER},
	{N_("File Browser"), new_browser_window, ACTION_MENU | ARROW_MENU},
	{N_("Power"), power_menu, SUB_MENU_HEADER},
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
			if (menu_shift_selected(menuz, -1)) {
				menu_draw(menuz);
				ret |= KEY_CLICK;
			}
			break;
		case 'r':
			if (menu_shift_selected(menuz, 1)) {
				menu_draw(menuz);
				ret |= KEY_CLICK;
			}
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

	menu_wid = pz_new_window(0, HEADER_TOPLINE + 2, screen_info.cols,
			screen_info.rows - (HEADER_TOPLINE + 2), menu_do_draw,
			menu_do_keystroke);

	GrSelectEvents(menu_wid, GR_EVENT_MASK_EXPOSURE| GR_EVENT_MASK_KEY_UP|
			GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_TIMER);

	menuz = menu_init(menu_wid, "podzilla", 0, 0, screen_info.cols,
			screen_info.rows - (HEADER_TOPLINE + 1), NULL,
			main_menu, ASCII | TRANSLATE);

	GrMapWindow(menu_wid);
}
