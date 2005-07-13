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

/* globals */
GR_SCREEN_INFO screen_info;
long hw_version;

static GR_WINDOW_ID root_wid;
static GR_GC_ID root_gc;
static GR_TIMER_ID backlight_tid;
static GR_TIMER_ID startupcontrast_tid;
#ifdef IPOD
static GR_TIMER_ID battery_tid;
#endif


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

#define WHEEL_EVT_MOD   3

static int hold_is_on = 0;

#define BATT_POLY_POINTS 9
#define BATTERY_LEVEL_LOW 50 
#define BATTERY_LEVEL_FULL 512
/*
  +-+
+-+ +-+
|     |
|     |
|     |
+-----+
*/

#define HOLD_X (9)	/* horizontal position of the padlock icon */
			/* (this is a #define now, so it's easy to move if
			   we put in song playing status later) */
static GR_POINT hold_outline[] = {
	{HOLD_X+0, 9},
	{HOLD_X+1, 9},
	{HOLD_X+1, 6},
	{HOLD_X+2, 5},
	{HOLD_X+5, 5},
	{HOLD_X+6, 6},
	{HOLD_X+6, 9},
	{HOLD_X+7, 9},
	{HOLD_X+7, 14},
	{HOLD_X+0, 14},
	{HOLD_X+0, 9},
};
#define HOLD_POLY_POINTS 11


extern void new_menu_window();
extern void load_font();
extern void beep(void);

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
	new_slider_widget(WHEEL_DEBOUNCE, "Wheel Sensitivity", 1, 20);
}

void set_buttondebounce(void)
{
	new_slider_widget(ACTION_DEBOUNCE, "Action Debounce", 100, 500);
}



#define BATT_UPDATE_FULL (-1)
/* which can be -1 to update all things, 0/1 to update just the blinky bits */
static void draw_batt_status( int which )
{
	static int battery_is_charging = 0;
	static int battery_fill = 512;
	static int battery_fill_16 = 0;
	static int last_level = 0;

	GR_POINT batt_outline[] = {
		{screen_info.cols-22, 5},
		{screen_info.cols-4, 5},
		{screen_info.cols-4, 8},
		{screen_info.cols-3, 8},
		{screen_info.cols-3, 12},
		{screen_info.cols-4, 12},
		{screen_info.cols-4, 15},
		{screen_info.cols-22, 15},
		{screen_info.cols-22, 6}
	};

	GR_POINT charging_bolt_outline[] = { /* 12 */
		{screen_info.cols-8, 2},
		{screen_info.cols-15, 2},
		{screen_info.cols-17, 9},
		{screen_info.cols-12, 9},

		{screen_info.cols-13, 12},
		{screen_info.cols-14, 12},
		{screen_info.cols-17, 9},
		{screen_info.cols-18, 18},

		{screen_info.cols-15, 14},
		{screen_info.cols-13, 16},
		{screen_info.cols-7, 7},
		{screen_info.cols-11, 7},

		{screen_info.cols-8, 2},
	};

	GR_POINT charging_bolt[] = {
		{screen_info.cols-8, 3},
		{screen_info.cols-14, 3},
		{screen_info.cols-16, 8},
		{screen_info.cols-11, 8},

		{screen_info.cols-13, 13},
		{screen_info.cols-16, 10},
		{screen_info.cols-17, 18},
		{screen_info.cols-14, 13},

		{screen_info.cols-12, 16},
		{screen_info.cols-7, 8},
		{screen_info.cols-11, 8},
		{screen_info.cols-9, 3},
	};

	if( which == BATT_UPDATE_FULL ) {
		/* set battery level to be 1..15 */
		battery_fill = ipod_get_battery_level();
		battery_fill_16 = (battery_fill>>5)-1;
		if( battery_fill_16 < 1 ) battery_fill_16=1;
		if( battery_fill_16 > 15 ) battery_fill_16=15;

		/* set battery charging indicator */
		battery_is_charging = 0; // ipod_get_battery_charging();
	}

	/* eliminiate unnecessary redraw/flicker */
	/* this could be eliminated by doublebuffering... */
	if( !battery_is_charging
	    && (which >= 0)
	    && (last_level == battery_fill_16)
	    && (battery_fill > BATTERY_LEVEL_LOW))
		return;
	last_level = battery_fill_16;

	/* draw it */

	/* fill the battery outline */
	GrSetGCForeground(root_gc, appearance_get_color(CS_BATTCTNR) );
	if( which == BATT_UPDATE_FULL ) /* try to eliminate some blink */
		GrFillPoly(root_wid, root_gc, BATT_POLY_POINTS, batt_outline);
	else
		GrFillRect(root_wid, root_gc, screen_info.cols-21, 6, 17, 9 );

	/* draw the outline */
	GrSetGCForeground(root_gc, appearance_get_color(CS_BATTBDR) );
	GrPoly(root_wid, root_gc, BATT_POLY_POINTS, batt_outline);

	if( !battery_is_charging ) {
	    /* draw fullness level */
	    GrSetGCForeground(root_gc, appearance_get_color(CS_BATTFILL));

	    /* change color to low color if appropriate */
	    if( battery_fill <= BATTERY_LEVEL_LOW )
		GrSetGCForeground(root_gc, appearance_get_color(CS_BATTLOW));

	    /* draw amount of battery fullness */
	    if(    (battery_fill > BATTERY_LEVEL_LOW)
		|| (which < 1 ) ) {
		    GrFillRect(root_wid, root_gc, screen_info.cols-20, 7, 
			battery_fill_16, 7);
	    }
	} else {
	    /* draw charging bolt, if applicable */
	    if( which < 1 ) {
		    /* erase charging bolt overlap */
		    GrSetGCForeground(root_gc, appearance_get_color(CS_TITLEBG));
		    GrFillRect(root_wid, root_gc, screen_info.cols-16,2, 9,3);
		    GrFillRect(root_wid, root_gc, screen_info.cols-18,16, 6,3);
		    return;
	    }

	    /* draw charging bolt */
	    GrSetGCForeground(root_gc, appearance_get_color(CS_BATTCHRG) );
	    GrFillPoly(root_wid, root_gc, 11, charging_bolt);
	    /* and a few cleanups... */
	    GrLine(root_wid, root_gc, screen_info.cols-17, 8,
				      screen_info.cols-8, 8);
	    GrLine(root_wid, root_gc, screen_info.cols-18, 16,
				      screen_info.cols-18, 17 );

	    GrSetGCForeground(root_gc, appearance_get_color(CS_BATTCTNR) );
	    GrPoly(root_wid, root_gc, 13, charging_bolt_outline);
	}
}

static void draw_hold_status()
{
	if (hold_is_on) {
		/* draw the body of the padlock */
		GrSetGCForeground(root_gc, appearance_get_color(CS_HOLDFILL));
		GrFillRect(root_wid, root_gc, HOLD_X+1, 9, 7, 5);

		/* draw the outline of the padlock */
		GrSetGCForeground(root_gc, appearance_get_color(CS_HOLDBDR));
		GrPoly(root_wid, root_gc, HOLD_POLY_POINTS, hold_outline);
	}
	else {
		/* erase the padlock */
		GrSetGCForeground(root_gc, appearance_get_color(CS_TITLEBG));
		GrFillPoly(root_wid, root_gc, HOLD_POLY_POINTS, hold_outline);
		GrPoly(root_wid, root_gc, HOLD_POLY_POINTS, hold_outline);

		/* erase out the body bit */
		GrFillRect(root_wid, root_gc, HOLD_X+1, 9, 7, 5);
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
#ifdef IPOD
	static int battery_count = 0;
#endif
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
#if 1
			if (!backlight_tid)
                               backlight_tid = GrCreateTimer(root_wid, 500);
			if (!startupcontrast_tid)
			       startupcontrast_tid = GrCreateTimer(
							    root_wid, 5000);
			window = NULL;
#endif
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
				draw_hold_status();
			}
			break;
#if 1
		case 'm':
			if (backlight_tid || startupcontrast_tid) {
				if( backlight_tid ) {
					GrDestroyTimer(backlight_tid);
					backlight_tid = 0;
				}
				if( startupcontrast_tid ) {
					GrDestroyTimer(startupcontrast_tid);
					startupcontrast_tid = 0;
				}

				event->type = GR_EVENT_TYPE_KEY_DOWN;
				if( window ) window->keystroke(event);
				event->type = GR_EVENT_TYPE_KEY_UP;
			}
			break;
#endif
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
		else if (((GR_EVENT_TIMER *)event)->tid == battery_tid)
		{
			battery_count++;
			if( battery_count > 59 ) {
				battery_count = 0;
				draw_batt_status( BATT_UPDATE_FULL );
			} else {
				draw_batt_status( battery_count & 1 );
			}
		}
#endif
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



#define DEC_WIDG_SZ 	(27)	/* size of the top widgets */

void pz_draw_header(char *header)
{
	GR_SIZE width, height, base, elwidth;
	int decorations;
	int centered_text = 1;
	int len, i;
	
	//GrClearWindow(root_wid, 0);
	GrSetGCForeground(root_gc, appearance_get_color(CS_TITLEBG) );
	GrFillRect( root_wid, root_gc, 0, 0, screen_info.cols, HEADER_TOPLINE);

	GrSetGCBackground(root_gc, appearance_get_color(CS_TITLEBG) );
	GrSetGCForeground(root_gc, appearance_get_color(CS_TITLEACC) );

	GrGetGCTextSize(root_gc, header, -1, GR_TFASCII, &width, &height,
			&base);

	decorations = appearance_get_decorations();
	/* draw amiga dragbars */
	if( decorations == DEC_AMIGA13 || decorations == DEC_AMIGA11 ) {
		/* drag bars */
		if( decorations == DEC_AMIGA13 ) {
			GrFillRect( root_wid, root_gc,
				DEC_WIDG_SZ+6, 4,
				screen_info.cols - (DEC_WIDG_SZ*2)-12, 4 );
			GrFillRect( root_wid, root_gc,
				DEC_WIDG_SZ+6, HEADER_TOPLINE-4-4, 
				screen_info.cols - (DEC_WIDG_SZ*2)-12, 4 );
		} else {
			for( i=2 ; i<(HEADER_TOPLINE) ; i+=2 ){
				GrLine( root_wid, root_gc, DEC_WIDG_SZ+2, i,
					screen_info.cols - DEC_WIDG_SZ-3, i );
			}
		}

		/* vertical widget separators */
		GrFillRect( root_wid, root_gc, DEC_WIDG_SZ,
						0, 2, HEADER_TOPLINE );
		GrFillRect( root_wid, root_gc, screen_info.cols-DEC_WIDG_SZ-2,
						0, 2, HEADER_TOPLINE );
		centered_text = 0;
	}
	if( decorations == DEC_MROBE ) {
		/* to make this symmetrical, we draw the left half, left to 
		    right, then we draw the right half, right to left.
		    This should make it such that it will fill properly,
		    and fill with only complete dots. no more slivers.
		- this isn't an OCD thing, it's a 'making it look good' thing.
		*/
		/* draw left side */
		for( 	i = DEC_WIDG_SZ+2 ;
			i < ((screen_info.cols - width)>>1)-5 ;
			i+=11 ) {
		    GrFillEllipse( root_wid, root_gc, i, HEADER_TOPLINE>>1,
				2, 2);
		}

		/* draw right side */
		for( 	i = screen_info.cols-DEC_WIDG_SZ-4 ;
			i > ((screen_info.cols + width)>>1)+5 ;
			i-=11 ) {
		    GrFillEllipse( root_wid, root_gc, i, HEADER_TOPLINE>>1,
				2, 2);
		}
	}

	GrSetGCForeground(root_gc, appearance_get_color(CS_TITLEFG) );
	if (width > screen_info.cols - 46) {
		GrGetGCTextSize(root_gc, "...", -1, GR_TFASCII, &elwidth,
				&height, &base);
		len = strlen(header);
		for (; elwidth + width > screen_info.cols - 46; len--) {
			GrGetGCTextSize(root_gc, header, len, GR_TFASCII,
					&width, &height, &base);
		}
		GrText(root_wid, root_gc, (screen_info.cols - (width + elwidth))
				/ 2, HEADER_BASELINE, header, len, GR_TFASCII);
		GrText(root_wid, root_gc, ((screen_info.cols -
				(width + elwidth)) / 2) + width,
				HEADER_BASELINE, "...", -1, GR_TFASCII);
	}
	else {
		if( decorations != DEC_PLAIN ) {
			GrSetGCForeground(root_gc, 
				appearance_get_color(CS_TITLEBG) );
			GrFillRect( root_wid, root_gc, 
			(centered_text) ? ((screen_info.cols - width)>>1) - 4
					: DEC_WIDG_SZ+2,
				0, width + 8, HEADER_TOPLINE );
		}

		GrSetGCForeground(root_gc, appearance_get_color(CS_TITLEFG) );
		GrText(root_wid, root_gc,
			(centered_text) ? (screen_info.cols - width) / 2
					: DEC_WIDG_SZ+6,
			HEADER_BASELINE, header, -1, GR_TFASCII);
	}

	GrSetGCForeground(root_gc, appearance_get_color(CS_TITLELINE) );
	GrLine(root_wid, root_gc, 0, HEADER_TOPLINE, screen_info.cols,
			HEADER_TOPLINE);

	draw_batt_status( BATT_UPDATE_FULL );
	draw_hold_status();
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
	// assert(windows[n_opened-1].wid == wid);

	GrUnmapWindow(wid);
	GrDestroyWindow(wid);

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
		fprintf(stderr, "GrOpen failed");
		exit(1);
	}

	root_gc = GrNewGC();
	GrSetGCUseBackground(root_gc, GR_FALSE);
	GrSetGCForeground(root_gc, BLACK);
	GrGetScreenInfo(&screen_info);
	load_font();

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
	appearance_init();
	backlight_tid = 0;
	startupcontrast_tid = 0;
#ifdef IPOD
	battery_tid = GrCreateTimer(root_wid, 1000);
#endif
	new_menu_window();

	while (1) {
		GR_EVENT event;

		GrGetNextEventTimeout(&event, 1000);
		pz_event_handler(&event);
	}

	return 0;
}
