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
#include <sys/time.h>
#include <unistd.h>
#include "pz.h"
#include "browser.h"
#include "ipod.h"

/* globals */
GR_SCREEN_INFO screen_info;
long hw_version;

static GR_WINDOW_ID root_wid;
static GR_GC_ID root_gc;

struct pz_window {
	GR_WINDOW_ID wid;
	void (*draw)(void);
	int (*keystroke)(GR_EVENT * event);
};

static struct pz_window windows[10];
static int n_opened = 0;
/* int BACKLIGHT_TIMER = 10; // in seconds */

#ifdef IPOD
static unsigned long int last_button_event = 0;
static unsigned long int wheel_evt_count = 0;
#endif
static unsigned long int last_keypress_event = 0;

#define WHEEL_EVT_MOD   3

static char key_pressed = '\0';

static int hold_is_on = 0;

#define BATT_POLY_POINTS 9

/*
  +-+
+-+ +-+
|     |
|     |
|     |
+-----+
*/
static GR_POINT hold_outline[] = {
	{8, 9},
	{9, 9},
	{9, 6},
	{10, 5},
	{13, 5},
	{14, 6},
	{14, 9},
	{15, 9},
	{15, 14},
	{8, 14},
	{8, 9},
};
#define HOLD_POLY_POINTS 11


extern void new_menu_window();
extern void beep(void);

void reboot_ipod(void)
{
	GrClose();
	execl("/bin/reboot", "reboot", NULL);
	exit(0);
}

void quit_podzilla(void)
{
	GrClose();
	exit(0);
}

void set_wheeldebounce(void)
{
	new_slider_widget(WHEEL_DEBOUNCE, "Wheel Sensitivity", 1, 20);
}

void set_buttondebounce(void)
{
	new_slider_widget(ACTION_DEBOUNCE, "Action Debounce", 100, 500);
}

static void draw_batt_status()
{
	GR_POINT batt_outline[] = {
		{screen_info.cols-22, 6},
		{screen_info.cols-4, 6},
		{screen_info.cols-4, 8},
		{screen_info.cols-3, 8},
		{screen_info.cols-3, 12},
		{screen_info.cols-4, 12},
		{screen_info.cols-4, 15},
		{screen_info.cols-22, 15},
		{screen_info.cols-22, 6}
	};
	GrPoly(root_wid, root_gc, BATT_POLY_POINTS, batt_outline);
	GrFillRect(root_wid, root_gc, screen_info.cols-20, 8, 15, 6);
}

static void draw_hold_status()
{
	if (hold_is_on) {
		GrSetGCForeground(root_gc, BLACK);
		GrPoly(root_wid, root_gc, HOLD_POLY_POINTS, hold_outline);
		GrFillRect(root_wid, root_gc, 8, 9, 7, 5);
	}
	else {
		GrSetGCForeground(root_gc, WHITE);
		GrPoly(root_wid, root_gc, HOLD_POLY_POINTS, hold_outline);
		GrFillRect(root_wid, root_gc, 8, 9, 7, 5);
	}
}

/* handles default functions for unused keystrokes */
void pz_default_handler(GR_EVENT *event)
{
	switch (event->type) {
	case GR_EVENT_TYPE_KEY_DOWN:
		switch (event->keystroke.ch) {
#if 0
		case 'f':
			printf("unused::fastforward\n");
			break;
		case 'w':
			printf("unused::rewind\n");
			break;
		case 'd':
			printf("unused::play\n");
			break;
#endif
		default:
			break;
		}
		break;
	}	
}

static inline struct pz_window *pz_find_window(GR_WINDOW_ID wid)
{
	int i;

	for (i = 0; i < n_opened; i++)
		if (windows[i].wid == wid && wid != GR_ROOT_WINDOW_ID)
			return &windows[i];
	return NULL;
}

void pz_event_handler(GR_EVENT *event)
{
	int ret = 0;
	unsigned long int curtime;
	struct timeval cur_st;
	struct pz_window *window;

	gettimeofday( &cur_st, NULL);
	curtime = (cur_st.tv_sec % 1000) * 1000 + cur_st.tv_usec / 1000;

	switch (event->type) {
	case GR_EVENT_TYPE_EXPOSURE:
		window = pz_find_window(((GR_EVENT_EXPOSURE *)event)->wid);

		if (window != NULL)
			window->draw();
		break;

	case GR_EVENT_TYPE_KEY_DOWN:
		window = pz_find_window(((GR_EVENT_KEYSTROKE *)event)->wid);

		last_keypress_event = curtime;

		switch (event->keystroke.ch) {
#ifdef IPOD
		case 'm':
		case 'q':
		case 'w':
		case 'f':
		case '\r':
			if (curtime - last_button_event >
					ipod_get_setting(ACTION_DEBOUNCE)) {
				last_button_event = curtime;
			}
			else {
				window = NULL;
			}
			break;

		case 'l':
		case 'r':
			wheel_evt_count++;
			if (wheel_evt_count % ipod_get_setting(WHEEL_DEBOUNCE)
					!= 0) {
				window = NULL;
			}
			break;
#endif /* IPOD */
		case 'h':
			if (!hold_is_on) {
				hold_is_on = 1;
				draw_hold_status();
			}
			break;
		}

		key_pressed = event->keystroke.ch;

		if (window != NULL) {
			ret = window->keystroke(event);
			if(KEY_CLICK & ret)
				beep();
			if(KEY_UNUSED & ret || EVENT_UNUSED & ret)
				pz_default_handler(event);
		}
		break;

	case GR_EVENT_TYPE_KEY_UP:
		if (event->keystroke.ch == 'h') {
			if (hold_is_on) {
				hold_is_on = 0;
				draw_hold_status();
			}
		}
		window = pz_find_window(((GR_EVENT_KEYSTROKE *)event)->wid);

		event->keystroke.ch = key_pressed;
		if (window != NULL) {
			ret = window->keystroke(event);
			if(KEY_UNUSED & ret || EVENT_UNUSED & ret)
				pz_default_handler(event);
		}
		break;

	case GR_EVENT_TYPE_TIMER:
		window = pz_find_window(((GR_EVENT_TIMER *)event)->wid);

		if (window != NULL) 
			window->keystroke(event);
		break;
	case GR_EVENT_TYPE_TIMEOUT:
		break;

	case GR_EVENT_TYPE_SCREENSAVER:
		if(((GR_EVENT_SCREENSAVER *)event)->activate)
			ipod_set_setting(BACKLIGHT, 0);
		else
			ipod_set_setting(BACKLIGHT, 1);
		break;
		
	default:
		window = pz_find_window(((GR_EVENT_GENERAL *)event)->wid);

		if (window != NULL) 
			ret = window->keystroke(event);

		if(EVENT_UNUSED & ret)
			printf("AN UNKNOWN EVENT OCCURED!!\n");
	}
}

void pz_draw_header(char *header)
{
	GR_SIZE width, height, base;

	GrSetGCForeground(root_gc, BLACK);
	GrSetGCBackground(root_gc, WHITE);
	GrClearWindow(root_wid, 0);

	GrGetGCTextSize(root_gc, header, -1, GR_TFASCII, &width, &height,
			&base);

	GrText(root_wid, root_gc, (screen_info.cols - width) / 2,
			HEADER_BASELINE, header, -1, GR_TFASCII);
	GrLine(root_wid, root_gc, 0, HEADER_TOPLINE, screen_info.cols,
			HEADER_TOPLINE);

	draw_batt_status();
	draw_hold_status();
}

GR_GC_ID pz_get_gc(int copy)
{
	return (copy ? GrCopyGC(root_gc) : root_gc);
}

GR_WINDOW_ID pz_new_window(int x, int y, int w, int h, void(*do_draw), int(*do_keystroke)(GR_EVENT * event))
{
	GR_WINDOW_ID new_wid = GrNewWindowEx(GR_WM_PROPS_APPFRAME |
			    GR_WM_PROPS_CAPTION |
			    GR_WM_PROPS_CLOSEBOX,
			    "Podzilla",
			    root_wid,
			    x, y, w, h, WHITE);

	/* FIXME: assumes always ok */

	windows[n_opened].wid = new_wid;
	windows[n_opened].draw = do_draw;
	windows[n_opened].keystroke = do_keystroke;
	n_opened++;

	return new_wid;
}

void
pz_close_window(GR_WINDOW_ID wid)
{
	// assert(windows[n_opened-1].wid == wid);

	GrUnmapWindow(wid);
	GrDestroyWindow(wid);

	n_opened--;
}

int
main(int argc, char **argv)
{

	if (GrOpen() < 0) {
		fprintf(stderr, "GrOpen failed");
		exit(1);
	}

	root_gc = GrNewGC();
	GrSetGCUseBackground(root_gc, GR_FALSE);
	GrSetGCForeground(root_gc, BLACK);
	GrGetScreenInfo(&screen_info);

	root_wid = GrNewWindowEx(GR_WM_PROPS_APPFRAME |
			    GR_WM_PROPS_CAPTION |
			    GR_WM_PROPS_CLOSEBOX,
			    "Podzilla",
			    GR_ROOT_WINDOW_ID,
			    0, 0, screen_info.cols, screen_info.rows, WHITE);

	GrSelectEvents(root_wid, GR_EVENT_MASK_EXPOSURE |
			GR_EVENT_MASK_CLOSE_REQ | GR_EVENT_MASK_KEY_UP |
			GR_EVENT_MASK_KEY_DOWN);
	GrSelectEvents(GR_ROOT_WINDOW_ID, GR_EVENT_MASK_SCREENSAVER);

	GrMapWindow(root_wid);

	ipod_load_settings();
	hw_version = ipod_get_hw_version();

	new_menu_window();

	while (1) {
		GR_EVENT event;

		GrGetNextEventTimeout(&event, 1000);
		pz_event_handler(&event);
	}

	return 0;
}
