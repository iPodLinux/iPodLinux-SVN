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
//#include <assert.h>
#include <sys/time.h>
#include "pz.h"
#include "browser.h"
#include "ipod.h"

static GR_WINDOW_ID root_wid;
static GR_GC_ID root_gc;
static GR_SCREEN_INFO screen_info;

struct pz_window {
	GR_WINDOW_ID wid;
	void (*draw)(void);
	int (*keystroke)(GR_EVENT * event);
};

static struct pz_window windows[10];
static int n_opened = 0;
/* int BACKLIGHT_TIMER = 10; // in seconds */

static unsigned long int last_wheel_event = 0;
static unsigned long int last_button_event = 0;
static unsigned long int last_keypress_event = 0;
static unsigned long int wheel_evt_count = 0;

#define WHEEL_EVT_MOD   3

char key_pressed = '\0';

/*
+----+
|    ++
| ||  |
| ||  |
|    ++
+----+
*/

static GR_POINT batt_outline[] = {
	{138, 6},
	{156, 6},
	{156, 8},
	{157, 8},
	{157, 12},
	{156, 12},
	{156, 15},
	{138, 15},
	{138, 6}
};

#define BATT_POLY_POINTS 9

void reboot_ipod(void)
{
	GrClose();
	execl("/bin/reboot", "reboot", NULL);
	exit(1);
}

void quit_podzilla(void)
{
	GrClose();
	exit(1);
}                

void set_wheeldebounce(void)
{
	new_slider_widget(WHEEL_DEBOUNCE, "Set Wheel Debounce", 100, 500);
}
                                                                                
void set_buttondebounce(void)
{
	new_slider_widget(ACTION_DEBOUNCE, "Set Action Debounce", 100, 500);
}

static void event_handler(GR_EVENT *event)
{
	int i;
	unsigned long int curtime;
	struct timeval cur_st;
	GR_WINDOW_ID wid = GR_ROOT_WINDOW_ID;

	gettimeofday( &cur_st, NULL);
	curtime = (cur_st.tv_sec % 1000) * 1000 + cur_st.tv_usec / 1000;

	switch (event->type) {
	case GR_EVENT_TYPE_TIMEOUT:
		/* Test to see if backlight timer has expired, if so, turn it off. */
		if (ipod_get_setting(BACKLIGHT_TIMER) > 0) {
			if ((curtime - last_keypress_event) > ((ipod_get_setting(BACKLIGHT_TIMER) + 1) * 1000)) {
				ipod_set_setting(BACKLIGHT, 0);
			}
		}
		break;

	case GR_EVENT_TYPE_EXPOSURE:
		wid = ((GR_EVENT_EXPOSURE *)event)->wid;
		for (i = 0; i < n_opened; i++) {
			if (windows[i].wid == wid && wid != GR_ROOT_WINDOW_ID) {
				windows[i].draw();
			}
		}
		break;

	case GR_EVENT_TYPE_KEY_DOWN:
		wid = ((GR_EVENT_KEYSTROKE *)event)->wid;
	
		/* If backlight timer isn't off and backlight isn't on turn it on. */
	
		last_keypress_event = curtime;

		if (ipod_get_setting(BACKLIGHT_TIMER) > 0) {
			ipod_set_setting(BACKLIGHT, 1);	
		}

		switch (event->keystroke.ch) {
		case 'm':
		case 'q':
		case 'w':
		case 'f':
		case '\r':
#ifdef IPOD
			if (curtime - last_button_event > ipod_get_setting(ACTION_DEBOUNCE)) {
				last_button_event = curtime;
				break;
			}

			wid = root_wid;
#endif
			break;

		case 'l':
		case 'r':
#ifdef IPOD
			wheel_evt_count ++;
			if (wheel_evt_count % WHEEL_EVT_MOD != 0) {
				wid = GR_ROOT_WINDOW_ID;
				break;
			}

			if (curtime - last_wheel_event > ipod_get_setting(WHEEL_DEBOUNCE)) {
				last_wheel_event = curtime;
				break;
			}

			wid = root_wid;
#endif
			break;
		}

		key_pressed = event->keystroke.ch;
		for (i = 0; i < n_opened; i++) {
			if (windows[i].wid == wid && wid != GR_ROOT_WINDOW_ID) {
				if(windows[i].keystroke(event) == 1)
					beep();
			}
		}
		break;

	case GR_EVENT_TYPE_KEY_UP:
		wid = ((GR_EVENT_KEYSTROKE *)event)->wid;
		event->keystroke.ch = key_pressed;
		break;

	default:
		printf("AN UNKNOWN EVENT OCCURED!!\n");
	}
}

static void draw_batt_status()
{
	GrPoly(root_wid, root_gc, BATT_POLY_POINTS, batt_outline);
	GrFillRect(root_wid, root_gc, 140, 8, 15, 6);
}

void pz_draw_header(char *header)
{
	GR_SIZE width, height, base;

	GrSetGCForeground(root_gc, WHITE);
	GrSetGCBackground(root_gc, BLACK);
	GrClearWindow(root_wid, 0);

	GrGetGCTextSize(root_gc, header, -1, GR_TFASCII, &width, &height,
			&base);

	GrText(root_wid, root_gc, (screen_info.cols - width) / 2, HEADER_BASELINE,
		header, -1, GR_TFASCII);
	GrLine(root_wid, root_gc, 0, HEADER_TOPLINE, screen_info.cols, HEADER_TOPLINE);

	draw_batt_status();
}

GR_WINDOW_ID pz_new_window(int x, int y, int w, int h, void(*do_draw), int(*do_keystroke)(GR_EVENT * event))
{
	GR_WINDOW_ID new_wid = GrNewWindowEx(GR_WM_PROPS_APPFRAME |
			    GR_WM_PROPS_CAPTION |
			    GR_WM_PROPS_CLOSEBOX,
			    "Podzilla",
			    root_wid,
			    x, y, w, h, BLACK);

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
	//assert(windows[n_opened-1].wid == wid);

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
	GrSetGCUseBackground(root_gc, GR_TRUE);
	GrSetGCForeground(root_gc, WHITE);
	GrGetScreenInfo(&screen_info);

	root_wid = GrNewWindowEx(GR_WM_PROPS_APPFRAME |
			    GR_WM_PROPS_CAPTION |
			    GR_WM_PROPS_CLOSEBOX,
			    "Podzilla",
			    GR_ROOT_WINDOW_ID,
			    0, 0, screen_info.cols, screen_info.rows, BLACK);

	GrSelectEvents(root_wid, GR_EVENT_MASK_EXPOSURE |
		       GR_EVENT_MASK_CLOSE_REQ | GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(root_wid);

	ipod_load_settings();

	new_menu_window();

	while (1) {
		GR_EVENT event;

		GrGetNextEventTimeout(&event, 1000);
		event_handler(&event);
	}

	return 0;
}
