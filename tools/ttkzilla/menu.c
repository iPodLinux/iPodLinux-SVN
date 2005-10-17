/*
 * Copyright (C) 2005 Courtney Cavin and Joshua Oreman
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
#ifndef MPDC
extern void new_itunes_track(void);
extern void new_itunes_artist(void);
extern void new_itunes_album(void);
extern void new_itunes_plist(void);
#endif /* !MPDC */
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
extern void new_font_window(ttk_menu_item *);
extern void new_vortex_window(void);
extern void new_wumpus_window(void);
extern void about_podzilla(void);
extern void show_credits(void);
#ifdef MIKMOD
extern void new_mikmod_window(void);
#endif
#ifdef MPDC
extern void mpd_currently_playing(void);

extern ttk_menu_item mpdc_menu[];
#endif /* MPDC */
extern ttk_menu_item lights_menu[];

extern void quit_podzilla(void);
extern void poweroff_ipod(void);
extern void reboot_ipod(void);

#define ACTION_MENU      0
#define SUB_MENU_HEADER  TTK_MENU_ICON_SUB

static ttk_menu_item tuxchess_menu[] = {
	{N_("Last Game"), {pz_mh_legacy}, 0, last_tuxchess_window},
	{N_("New Game"), {pz_mh_legacy}, 0, new_tuxchess_window},
	{0}
};

static ttk_menu_item games_menu[] = {
	{N_("BlueCube"), {pz_mh_legacy}, 0, new_bluecube_window},
	{N_("Hunt The Wumpus"), {pz_mh_legacy}, 0, new_wumpus_window},
	{N_("Invaders"), {pz_mh_legacy}, 0, new_invaders_window},
	{N_("iPobble"), {pz_mh_legacy}, 0, new_ipobble_window},
	{N_("Lights"), {ttk_mh_sub}, TTK_MENU_ICON_SUB, lights_menu},
	{N_("Minesweeper"), {pz_mh_legacy}, 0, new_mines_window},
	{N_("Othello"), {pz_mh_legacy}, 0, new_oth_window},
	{N_("Pong"), {pz_mh_legacy}, 0, new_pong_window},
	{N_("Steroids"), {pz_mh_legacy}, 0, new_steroids_window},
	{N_("Tic-Tac-Toe"), {pz_mh_legacy}, 0, new_tictactoe_window},
	{N_("Tunnel"), {pz_mh_legacy}, 0, new_tunnel_window},
	{N_("TuxChess"), {ttk_mh_sub}, TTK_MENU_ICON_SUB, tuxchess_menu},
	{N_("Vortex Demo"), {pz_mh_legacy}, 0, new_vortex_window},
	{0}
};

static ttk_menu_item stuff_menu[] = {
	{N_("Cube"), {pz_mh_legacy}, 0, new_cube_window},
	{N_("Dialer"), {pz_mh_legacy}, 0, new_dialer_window},
	{N_("MandelPod"), {pz_mh_legacy}, 0, new_mandel_window},
	{N_("Matrix"), {pz_mh_legacy}, 0, new_matrix_window},
	{N_("PodDraw"), {pz_mh_legacy}, 0, new_poddraw_window},
	{0}
};

static const char *backlight_options[] = {
	N_("Off"), N_("1 sec"), N_("2 secs"), N_("5 secs"), N_("10 secs"),
	N_("30 secs"), N_("1 min"), N_("On"), 0
};

static const char *sample_rates[] = {
    "8 kHz", "32 kHz", "44.1 kHz", "88.2 kHz", "96 kHz", 0
};

static const char *shuffle_options[] = {
    N_("Off"), N_("Songs"), 0
};

static const char *repeat_options[] = {
    N_("Off"), N_("One"), N_("All"), 0
};

static const char *boolean_options[] = {
    N_("Off"), N_("On"), 0
};

#define MENU_SETTING(c,s) .choices=c, .cdata=s, .choicechanged=menu_set_setting, .choiceget=menu_get_setting
#define MENU_BOOL(s) .choices=boolean_options, .cdata=s, .choicechanged=menu_set_setting, .choiceget=menu_get_setting
static void menu_set_setting (ttk_menu_item *item, int cdata) 
{
    ipod_set_setting (cdata, item->choice);
}
static int menu_get_setting (ttk_menu_item *item, int cdata) 
{
    int ret = ipod_get_setting (cdata);
    if (ret > item->nchoices) {
	fprintf (stderr, "Warning! Setting %d set to %d on a %d scale - using 0\n", cdata, ret, item->nchoices);
	ret = 0;
    }
    return ret;
}

static ttk_menu_item recording_menu[] = {
#ifdef __linux__
	{N_("Mic Record"), {pz_mh_legacy}, 0, new_record_mic_window},
	{N_("Line In Record"), {pz_mh_legacy}, 0, new_record_line_in_window},
	{N_("Playback"), {pz_mh_legacy}, 0, new_playback_browse_window},
#endif /* __linux__ */
	{N_("Sample Rate"), MENU_SETTING (sample_rates, DSPFREQUENCY)},
	{0}
};

extern const char * clocks_timezones[]; /* for the timezones.  in clocks.c */
extern const char * clocks_dsts[];	  /* for dst display.  in clocks.c */

static ttk_menu_item world_clock_menu[] = {
	{N_("Local Clock"), {pz_mh_legacy}, 0, new_clock_window},
	{N_("World Clock"), {pz_mh_legacy}, 0, new_world_clock_window},
	{N_("TZ"), MENU_SETTING (clocks_timezones, TIME_WORLDTZ)},
	{N_("DST"), MENU_SETTING (clocks_dsts, TIME_WORLDDST)},
	{0}
};

static ttk_menu_item extras_menu[] = {
	{N_("Recordings"), {ttk_mh_sub}, TTK_MENU_ICON_SUB, recording_menu},
	{N_("Calendar"), {pz_mh_legacy}, 0, new_calendar_window},
	{N_("Calculator"), {pz_mh_legacy}, 0, new_calc_window},
	{N_("Clock"), {ttk_mh_sub}, TTK_MENU_ICON_SUB, world_clock_menu},
	{N_("Games"), {ttk_mh_sub}, TTK_MENU_ICON_SUB, games_menu},
	{N_("Stuff"), {ttk_mh_sub}, TTK_MENU_ICON_SUB, stuff_menu},
	{0}
};

static TWindow *menu_reset_settings (ttk_menu_item *item) { ipod_reset_settings(); return TTK_MENU_UPONE; }
static TWindow *menu_save_settings (ttk_menu_item *item) { ipod_save_settings(); return TTK_MENU_UPONE; }
static TWindow *menu_load_settings (ttk_menu_item *item) { ipod_load_settings(); return TTK_MENU_DONOTHING; }

static ttk_menu_item reset_menu[] = {
	{N_("Cancel"), { .sub = (TWindow *)TTK_MENU_UPONE }, TTK_MENU_MADESUB},
	{N_("Absolutely"), {menu_reset_settings}, TTK_MENU_ICON_EXE, 0},
	{0}
};

static const char * time1224_options[] = { N_("12-hour"), N_("24-hour"), 0 };

static ttk_menu_item clocks_menu[] = {
        { N_("Clock"), {pz_mh_legacy}, 0, new_clock_window },
        { N_("Set Time"), {pz_mh_legacy}, 0, new_Set_Time_window },
        { N_("Set Time & Date"), {pz_mh_legacy}, 0, new_Set_DateTime_window },
/* -- future expansion --
	{ N_("Set Alarm"), NULL, SUB_MENU_PREV },
	{ N_("Set Sleep Timer"), NULL, SUB_MENU_PREV },
	{ N_("Time In Title"), NULL, BOOLEAN_MENU, TIME_IN_TITLE },
*/
	{ N_("TZ"), MENU_SETTING (clocks_timezones, TIME_ZONE) },
	{ N_("DST"), MENU_SETTING (clocks_dsts, TIME_DST) },
	{ N_("Time"), MENU_SETTING (time1224_options, TIME_1224) },
	{ N_("Time Tick Noise"), MENU_BOOL (TIME_TICKER) },
        { 0 }
};

static const char * transit_options[] = { N_("Off"), N_("Slow"), N_("Fast"), 0 };

static ttk_menu_item appearance_menu[] = {
	{N_("Color Scheme"), {appearance_select_color_scheme} },
     	{N_("Decorations"), MENU_SETTING (appearance_decorations, DECORATIONS) },
	{N_("Battery Digits"), MENU_BOOL (BATTERY_DIGITS) },
	{N_("Display Load Average"), MENU_BOOL (DISPLAY_LOAD) },
	{N_("Menu Transition"), MENU_SETTING (transit_options, SLIDE_TRANSIT) },
	{N_("Font"), {pz_mh_legacy}, TTK_MENU_ICON_SUB, new_font_window},
	{ 0 }
};

static ttk_menu_item settings_menu[] = {
	{N_("About"), {about_podzilla}},
	{N_("Credits"), {show_credits}},
	{N_("Date & Time"), {ttk_mh_sub}, TTK_MENU_ICON_SUB, clocks_menu},
	{N_("Repeat"), MENU_SETTING (repeat_options, REPEAT)},
	{N_("Shuffle"), MENU_SETTING (shuffle_options, SHUFFLE)},
	{N_("Contrast"), {pz_mh_legacy}, 0, set_contrast},
	{N_("Wheel Sensitivity"), {pz_mh_legacy}, 0, set_wheeldebounce},
	// TTK does button debouncing for you.
	//	{N_("Button Debounce"), {pz_mh_legacy}, 0, set_buttondebounce},
	{N_("Backlight Timer"), MENU_SETTING (backlight_options, BACKLIGHT_TIMER)},
	{N_("Clicker"), MENU_BOOL (CLICKER)},
	{N_("Appearance"), {ttk_mh_sub}, TTK_MENU_ICON_SUB, appearance_menu},
	{N_("Browser Path Display"), MENU_BOOL (BROWSER_PATH)},
	{N_("Show Hidden Files"), MENU_BOOL (BROWSER_HIDDEN)},
	{N_("Reset All Settings"), {ttk_mh_sub}, TTK_MENU_ICON_SUB, reset_menu},
	{N_("Save Settings"), {menu_save_settings}},
	{N_("Load Settings"), {menu_load_settings}},
	{0}
};

static ttk_menu_item reboot_menu[] = {
	{N_("Cancel"), { .sub = (TWindow *)TTK_MENU_UPONE }},
	{N_("Absolutely"), {pz_mh_legacy}, 0, reboot_ipod},
	{0}
};

static ttk_menu_item turnoff_menu[] = {
	{N_("Cancel"), { .sub = (TWindow *)TTK_MENU_UPONE }},
	{N_("Absolutely"), {pz_mh_legacy}, 0, poweroff_ipod},
	{0}
};

#ifndef MPDC
static ttk_menu_item itunes_menu[] = {
	{N_("Playlists"), {pz_mh_legacy}, TTK_MENU_ICON_SUB, new_itunes_plist},
	{N_("Artists"), {pz_mh_legacy}, TTK_MENU_ICON_SUB, new_itunes_artist},
	{N_("Albums"), {pz_mh_legacy}, TTK_MENU_ICON_SUB, new_itunes_album},
	{N_("Songs"), {pz_mh_legacy}, TTK_MENU_ICON_SUB, new_itunes_track},
#ifdef MIKMOD
	{N_("MikMod"), {pz_mh_legacy}, 0, new_mikmod_window},
#endif
	{0}
};
#endif /* !MPDC */

static ttk_menu_item power_menu[] = {
	{N_("Quit Podzilla"), {pz_mh_legacy}, 0, quit_podzilla},
	{N_("Reboot iPod"), {ttk_mh_sub}, 0, reboot_menu},
#ifdef NEVER /* just to show where this should go */
	{N_("Sleep iPod"), {pz_mh_legacy}, 0, sleep};
#endif
	{N_("Turn off iPod"), {ttk_mh_sub}, 0, turnoff_menu},
	{0}
};

int settings_down (TWidget *this, int button) 
{
    if (button == TTK_BUTTON_MENU)
	ipod_save_settings();
    return ttk_menu_down (this, button);
}

TWindow *pz_settings_sub (ttk_menu_item *item) 
{
    TWindow *ret = ttk_new_window();
    TWidget *menu = ttk_new_menu_widget (item->data, ttk_menufont, item->menuwidth, item->menuheight);
    ttk_window_title (ret, item->name);
    menu->draw (menu, ret->srf);
    menu->down = settings_down;
    ttk_add_widget (ret, menu);
    return ret;
}

static ttk_menu_item main_menu[] = {
#ifdef MPDC
	{N_("Music"), {ttk_mh_sub}, TTK_MENU_ICON_SUB, mpdc_menu},
#else
	{N_("Music"), {ttk_mh_sub}, TTK_MENU_ICON_SUB, itunes_menu},
#endif /* MPDC */
	{N_("Extras"), {ttk_mh_sub}, TTK_MENU_ICON_SUB, extras_menu},
	{N_("Settings"), {pz_settings_sub}, TTK_MENU_ICON_SUB, settings_menu},
#ifdef MPDC
	{N_("Now Playing"), {pz_mh_legacy}, 0, mpd_currently_playing},
#endif /* MPDC */
	{N_("File Browser"), {pz_mh_legacy}, TTK_MENU_ICON_SUB, new_browser_window},
	{N_("Power"), {ttk_mh_sub}, TTK_MENU_ICON_SUB, power_menu},
	{0}
};

void new_menu_window()
{
    TWindow *ret = ttk_new_window();
    TWidget *menu = ttk_new_menu_widget (main_menu, ttk_menufont, ret->w, ret->h);
    ttk_menu_set_closeable (menu, 0);
    ttk_add_widget (ret, menu);
    ttk_window_set_title (ret, "ttk-zilla");
    ttk_show_window (ret);
}
