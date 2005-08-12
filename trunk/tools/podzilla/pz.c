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
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "pz.h"
#include "browser.h"
#include "ipod.h"
#include "vectorfont.h"

/* globals */
GR_SCREEN_INFO screen_info;
long hw_version;

GR_WINDOW_ID root_wid;
GR_GC_ID root_gc;
static GR_TIMER_ID backlight_tid;
static GR_TIMER_ID startupcontrast_tid;
static GR_TIMER_ID battery_tid;

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
static int startup_contrast = -1;

#endif
static unsigned long int last_keypress_event = 0;

int battery_is_charging = 0;
static int low_battery_count = 0;
int usb_connected = 0;
int old_usb_connected = 0;
static int usb_dialog_open = 0;
int fw_connected = 0;
int old_fw_connected = 0;
static int fw_dialog_open = 0;
#define WHEEL_EVT_MOD   3

static int hold_is_on = 0;

extern void new_menu_window();
extern void load_font();
extern void beep(void);

extern void header_update_hold_status( int curr );
void header_timer_update( void );

void poweroff_ipod(void)
{
	ipod_touch_settings();
	GrClose();
	printf("\nPowering down.\nPress action to power on.\n");
	execl("/bin/poweroff", "poweroff", NULL);

	printf("No poweroff binary available.  Rebooting.\n");
	execl("/bin/reboot", "reboot", NULL);
	exit(0);
}

void reboot_ipod(void)
{
	ipod_touch_settings();
	GrClose();
	execl("/bin/reboot", "reboot", NULL);
	exit(0);
}

void quit_podzilla(void)
{
	ipod_touch_settings();
	GrClose();
	exit(0);
}

void set_wheeldebounce(void)
{
	new_slider_widget(WHEEL_DEBOUNCE, _("Wheel Sensitivity"), 1, 20);
}

void set_buttondebounce(void)
{
	new_slider_widget(ACTION_DEBOUNCE, _("Action Debounce"), 100, 500);
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
			if (!backlight_tid)
                               backlight_tid = GrCreateTimer(root_wid, 500);
			if (!startupcontrast_tid)
			       startupcontrast_tid = GrCreateTimer(
							    root_wid, 5000);
			window = NULL;
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
				header_update_hold_status( hold_is_on );
			}
			break;
		}

		if (window != NULL) {
			ret = window->keystroke(event);
			if(KEY_CLICK & ret)
				beep();
			if(KEY_UNUSED & ret || EVENT_UNUSED & ret)
				pz_default_handler(event);
		}
		break;

	case GR_EVENT_TYPE_KEY_UP:
		window = pz_find_window(((GR_EVENT_KEYSTROKE *)event)->wid);

		switch (event->keystroke.ch) {
		case 'h':
			if (hold_is_on) {
				hold_is_on = 0;
				header_update_hold_status( hold_is_on );
			}
			break;
		case 'm':
			if (backlight_tid) {
				GrDestroyTimer(backlight_tid);
				backlight_tid = 0;
				event->type = GR_EVENT_TYPE_KEY_DOWN;
				if (window) window->keystroke(event);
				event->type = GR_EVENT_TYPE_KEY_UP;
			}
			if (startupcontrast_tid) {
				GrDestroyTimer(startupcontrast_tid);
				startupcontrast_tid = 0;
			}
			break;
		default:
			break;
		}
		if (window != NULL) {
			ret = window->keystroke(event);
			if(KEY_UNUSED & ret || EVENT_UNUSED & ret)
				pz_default_handler(event);
		}
		break;

	case GR_EVENT_TYPE_TIMER:
		window = pz_find_window(((GR_EVENT_TIMER *)event)->wid);

		if (((GR_EVENT_TIMER *)event)->tid == backlight_tid) {
			GrDestroyTimer(backlight_tid);
			backlight_tid = 0;
			ipod_set_backlight(!ipod_get_backlight());
		}
#ifdef IPOD
		else if (((GR_EVENT_TIMER *)event)->tid == startupcontrast_tid) 		{
			GrDestroyTimer(startupcontrast_tid);
			startupcontrast_tid = 0;
			ipod_set_contrast(startup_contrast);
		} 
#endif
		else if (((GR_EVENT_TIMER *)event)->tid == battery_tid)
		{
			if ((usb_connected=usb_is_connected())) {
				if (!old_usb_connected)
				{
					old_usb_connected = usb_connected;
					if (!usb_dialog_open) {
						usb_dialog_open = 1;
						usb_check_goto_diskmode();
						usb_dialog_open = 0;	
					} 	
				} 

			} 
			old_usb_connected = usb_connected;	

			
			if ((fw_connected=fw_is_connected())) {
				if (!old_fw_connected)
				{
					old_fw_connected = fw_connected;
					if (!fw_dialog_open) {
						fw_dialog_open = 1;
						fw_check_goto_diskmode();
						fw_dialog_open = 0;	
					} 	
				} 

			} 
			old_fw_connected = fw_connected;
			
			if (!ipod_is_charging() && ipod_get_battery_level() < BATTERY_LEVEL_LOW) {
				low_battery_count++;
			} else {
				low_battery_count = 0;
			}
			
			if (low_battery_count > 4) {
				ipod_beep();
				// This is here to turn off the iPod before disk corruption occurs.
				poweroff_ipod();
			}
				
			header_timer_update();
		}
		else if (window != NULL)
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


GR_GC_ID pz_get_gc(int copy)
{
	return (copy ? GrCopyGC(root_gc) : root_gc);
}

GR_WINDOW_ID pz_new_window(int x, int y, int w, int h, void(*do_draw)(void), int(*do_keystroke)(GR_EVENT * event))
{
	GR_WINDOW_ID new_wid = GrNewWindowEx(GR_WM_PROPS_APPFRAME |
			    GR_WM_PROPS_CAPTION |
			    GR_WM_PROPS_CLOSEBOX,
			    "podzilla",
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
	int i;
	int window_to_remove = -1;	
	// assert(windows[n_opened-1].wid == wid);
	
	GrUnmapWindow(wid);
	GrDestroyWindow(wid);


	/*  If we are closing a window which wasn't the last one opened
	    we must shift the contents of the window array  */  
	if (windows[n_opened-1].wid != wid)  
	{
		for (i = 0; i < n_opened; i++)
			if (windows[i].wid == wid && wid != GR_ROOT_WINDOW_ID)
				window_to_remove = i;
	
		if (window_to_remove != -1)
		{
			for (i = window_to_remove; i < n_opened-1; i++)
			{
				windows[i].wid = windows[i+1].wid;
				windows[i].draw = windows[i+1].draw;
				windows[i].keystroke = windows[i+1].keystroke;	
			}
		}	
	}

	n_opened--;
}

void pz_set_time_from_file(void)
{
#ifdef IPOD
	struct timeval tv_s;
	struct stat statbuf;

	/* find the last modified time of the settings file */
	stat( IPOD_SETTINGS_FILE, &statbuf );

	/* convert timespec to timeval */
	tv_s.tv_sec  = statbuf.st_mtime;
	tv_s.tv_usec = 0;

	settimeofday( &tv_s, NULL );
#endif
}


int
main(int argc, char **argv)
{
#ifdef IPOD
	startup_contrast = ipod_get_contrast();
#endif

	if (GrOpen() < 0) {
		fprintf(stderr, _("GrOpen failed"));
		exit(1);
	}

#ifdef LOCALE
	setlocale(LC_ALL, "");
	bindtextdomain("podzilla", LOCALEDIR);
	textdomain("podzilla");
#endif
	root_gc = GrNewGC();
	GrSetGCUseBackground(root_gc, GR_FALSE);
	GrSetGCForeground(root_gc, BLACK);
	GrGetScreenInfo(&screen_info);

	root_wid = GrNewWindowEx(GR_WM_PROPS_APPFRAME |
			    GR_WM_PROPS_CAPTION |
			    GR_WM_PROPS_CLOSEBOX,
			    "podzilla",
			    GR_ROOT_WINDOW_ID,
			    0, 0, screen_info.cols, screen_info.rows, WHITE);

	GrSelectEvents(root_wid, GR_EVENT_MASK_EXPOSURE |
			GR_EVENT_MASK_CLOSE_REQ | GR_EVENT_MASK_KEY_UP |
			GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_TIMER);
	GrSelectEvents(GR_ROOT_WINDOW_ID, GR_EVENT_MASK_SCREENSAVER);

	GrMapWindow(root_wid);

	hw_version = ipod_get_hw_version();
	
	if( hw_version && hw_version < 30000 ) { /* 1g/2g only */
		pz_set_time_from_file();
	}
	
	ipod_load_settings();
	load_font();
	appearance_init();
	backlight_tid = 0;
	startupcontrast_tid = 0;
	battery_tid = GrCreateTimer(root_wid, 1000);
	usb_connected = usb_is_connected();
	old_usb_connected = usb_connected;	
	fw_connected = fw_is_connected();
	old_fw_connected = fw_connected;	
	
	new_menu_window();

	while (1) {
		GR_EVENT event;

		GrGetNextEventTimeout(&event, 1000);
		pz_event_handler(&event);
	}

	return 0;
}
