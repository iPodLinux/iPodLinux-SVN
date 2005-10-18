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
#include "slider.h"
#include "ipod.h"
#include "vectorfont.h"
#ifdef MPDC
#include "mpdc/mpdc.h"
#endif

int pz_setting_debounce = 0;

/* globals */
GR_SCREEN_INFO screen_info;
long hw_version;

GR_WINDOW_ID root_wid;
GR_GC_ID root_gc;

struct pz_window {
	GR_WINDOW_ID wid;
	void (*draw)(void);
	int (*keystroke)(GR_EVENT * event);
};

/* int BACKLIGHT_TIMER = 10; // in seconds */

#ifdef IPOD
int pz_startup_contrast = -1;

#endif
//static unsigned long int last_keypress_event = 0;

int usb_connected = 0;
int fw_connected = 0;
ttk_timer connection_timer = 0;

void check_connection() 
{
    int temp;
    
    if ((temp = usb_is_connected()) != usb_connected) {
	usb_connected = temp;
	usb_check_goto_diskmode();
    }
    if ((temp = fw_is_connected()) != fw_connected) {
	fw_connected = temp;
	fw_check_goto_diskmode();
    }
    connection_timer = ttk_create_timer (1000, check_connection);
}

int hold_is_on = 0;

extern void new_menu_window();
extern void load_font();
extern void beep(void);

extern void header_update_hold_status( int curr );
void header_timer_update( void );

void poweroff_ipod(void)
{
	ipod_touch_settings();
#ifdef MPDC
	mpdc_destroy();
#endif
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
#ifdef MPDC
	mpdc_destroy();
#endif
	GrClose();
	execl("/bin/reboot", "reboot", NULL);
	exit(0);
}

void quit_podzilla(void)
{
	ipod_touch_settings();
#ifdef MPDC
	mpdc_destroy();
#endif
	GrClose();
	exit(0);
}

void set_wheeldebounce(void)
{
	pz_setting_debounce = 1;
	ttk_set_scroll_multiplier (1, 1); // for selecting the speed
	new_settings_slider_window(_("Wheel Sensitivity"),
				   WHEEL_DEBOUNCE, 0, 19);
	ttk_windows->w->data = 0x12345678;
}

void pz_event_handler (t_GR_EVENT *ev) 
{
    int sdir = 0;
    if (ttk_windows->w->focus) {
	if (ev->keystroke.ch == 'r')
	    sdir = 1;
	else if (ev->keystroke.ch == 'l')
	    sdir = -1;
	
	switch (ev->type) {
	case GR_EVENT_TYPE_KEY_DOWN:
	    if (sdir)
		ttk_windows->w->focus->scroll (ttk_windows->w->focus, sdir);
	    else
		ttk_windows->w->focus->down (ttk_windows->w->focus, ev->keystroke.ch);
	    break;
	case GR_EVENT_TYPE_KEY_UP:
	    if (!sdir)
		ttk_windows->w->focus->button (ttk_windows->w->focus, ev->keystroke.ch, 0);
	    break;
	}
    }
}

extern void header_fix_hold (void);


static ttk_timer bloff_timer = 0;
static int bl_forced_on = 0;

static void backlight_off() { if (!bl_forced_on) ipod_set_setting (BACKLIGHT, 0); bloff_timer = 0; }
static void backlight_on()  { ipod_set_setting (BACKLIGHT, 1); }

void pz_set_backlight_timer (int sec) 
{
    static int last = BL_OFF;
    if (sec != BL_RESET) last = sec;

    if (last == BL_OFF) {
	if (bloff_timer) ttk_destroy_timer (bloff_timer);
	bloff_timer = 0;
	backlight_off();
    } else if (last == BL_ON) {
	if (bloff_timer) ttk_destroy_timer (bloff_timer);
	bloff_timer = 0;
	backlight_on();
    } else {
	if (bloff_timer) ttk_destroy_timer (bloff_timer);
	bloff_timer = ttk_create_timer (1000*last, backlight_off);
	backlight_on();
    }
}

int held_times[128] = { ['m'] = 500, ['d'] = 1000 }; // key => ms
int held_ignores[128]; // set a char to 1 = ignore its UP event once.
ttk_timer held_timers[128]; // the timers

void backlight_toggle() 
{
    bl_forced_on = !bl_forced_on;
    held_timers['m'] = 0;
    held_ignores['m']++;
}

void (*held_handlers[128])() = { ['m'] = backlight_toggle, ['d'] = /* sleep_ipod */ 0 }; // key => fn

int pz_new_event_handler (int ev, int earg, int time)
{
    static int vtswitched = 0;

    pz_set_backlight_timer (BL_RESET);

    if (pz_setting_debounce && (ttk_windows->w->focus->draw != ttk_slider_draw)) {
	pz_setting_debounce = 0;
	ipod_set_setting (WHEEL_DEBOUNCE, ipod_get_setting (WHEEL_DEBOUNCE));
    }

    switch (ev) {
    case TTK_BUTTON_DOWN:
	switch (earg) {
	case TTK_BUTTON_HOLD:
	    hold_is_on = 1;
	    header_fix_hold();
	    break;
	case TTK_BUTTON_MENU:
	    vtswitched = 0;
	    break;
	}
	if (held_times[earg] && held_handlers[earg]) {
	    if (held_timers[earg]) ttk_destroy_timer (held_timers[earg]);
	    held_timers[earg] = ttk_create_timer (held_times[earg], held_handlers[earg]);
	}
	break;
    case TTK_BUTTON_UP:
	if (held_timers[earg]) {
	    ttk_destroy_timer (held_timers[earg]);
	    held_timers[earg] = 0;
	}
	if (held_ignores[earg]) {
	    held_ignores[earg] = 0;
	    return 1;
	}
	switch (earg) {
	case TTK_BUTTON_HOLD:
	    hold_is_on = 0;
	    header_fix_hold();
	    break;
	case TTK_BUTTON_MENU:
	    if (time > 500 && time < 1000) {
		bl_forced_on = !bl_forced_on;
		if (bl_forced_on)
		    backlight_on();
		return 1;
	    }
	    break;
	case TTK_BUTTON_PREVIOUS:
	    if (ttk_button_pressed (TTK_BUTTON_MENU) && ttk_button_pressed (TTK_BUTTON_PLAY)) {
		// vt switch code <<
		printf ("VT SWITCH <<\n");
		vtswitched = 1;
		return 1;
	    } else if (ttk_button_pressed (TTK_BUTTON_MENU) && ttk_button_pressed (TTK_BUTTON_NEXT)) {
		// vt switch code [0]
		printf ("VT SWITCH 0 (N-P)\n");
		vtswitched = 1;
		return 1;
	    } else if (ttk_button_pressed (TTK_BUTTON_MENU) && !vtswitched) {
		TWindowStack *lastwin = ttk_windows;
		while (lastwin->next) lastwin = lastwin->next;
		if (lastwin->w != ttk_windows->w) {
		    ttk_move_window (lastwin->w, 0, TTK_MOVE_ABS);
		    printf ("WINDOW CYCLE >>\n");
		} else
		    printf ("WINDOW CYCLE >> DIDN'T\n");
		return 1;
	    }
	    break;
	case TTK_BUTTON_NEXT:
	    if (ttk_button_pressed (TTK_BUTTON_MENU) && ttk_button_pressed (TTK_BUTTON_PLAY)) {
		// vt switch code >>
		printf ("VT SWITCH >>\n");
		vtswitched = 1;
		return 1;
	    } else if (ttk_button_pressed (TTK_BUTTON_MENU) && ttk_button_pressed (TTK_BUTTON_PREVIOUS)) {
		// vt switch code [0]
		printf ("VT SWITCH 0 (P-N)\n");
		vtswitched = 1;
		return 1;
	    } else if (ttk_button_pressed (TTK_BUTTON_MENU) && !vtswitched) {
		printf ("WINDOW CYCLE <<\n");
		if (ttk_windows->next) {
		    ttk_move_window (ttk_windows->w, 0, TTK_MOVE_END);
		    return 1;
		}
	    }
	    break;
	case TTK_BUTTON_PLAY:
	    if (ttk_button_pressed (TTK_BUTTON_MENU) && !vtswitched) {
		printf ("WINDOW MINIMIZE\n");
		if (ttk_windows->next) {
		    ttk_windows->minimized = 1;
		    return 1;
		}
	    }
	}
	break;
    }
    return 0; // keep event
}


int pz_unused_handler (int ev, int earg, int time) 
{
    switch (ev) {
    case TTK_BUTTON_UP:
	switch (earg) {
#ifdef MPDC
	case 'f':
	    mpdc_next (mpdz);
	    break;
	case 'w':
	    mpdc_prev (mpdz);
	    break;
	case 'd':
	    mpdc_playpause (mpdz);
	    break;
#endif
	default:
	    break;
	}
	break;
    }
    return 0; // no clickiness
}


typedef struct 
{
    void (*draw)();
    int (*key)(GR_EVENT *);
} legacy_data;

#define _MAKETHIS legacy_data *data = (legacy_data *)this->data

void pz_legacy_construct_GR_EVENT (GR_EVENT *ev, int type, int arg) 
{
    ev->type = type;
    if (ev->type == GR_EVENT_TYPE_KEY_UP || ev->type == GR_EVENT_TYPE_KEY_DOWN) {
	if (arg == TTK_BUTTON_ACTION)
	    arg = '\r';
	ev->keystroke.ch = arg;
	ev->keystroke.scancode = 0;
    }
}
void pz_legacy_draw (TWidget *this, ttk_surface srf) 
{
    _MAKETHIS;
    data->draw(); // it'll be on the window we returned from pz_new_window
}
int pz_legacy_button (TWidget *this, int button, int time)
{
    GR_EVENT ev;
    _MAKETHIS;
    pz_legacy_construct_GR_EVENT (&ev, GR_EVENT_TYPE_KEY_UP, button);
    this->dirty++;
    return data->key (&ev);
}
int pz_legacy_down (TWidget *this, int button)
{
    GR_EVENT ev;
    _MAKETHIS;
    pz_legacy_construct_GR_EVENT (&ev, GR_EVENT_TYPE_KEY_DOWN, button);
    this->dirty++;
    return data->key (&ev);
}
int pz_legacy_scroll (TWidget *this, int dir)
{
#ifdef IPOD
#define SPER 4
    static int sofar = 0;
    sofar += dir;
    if (sofar > -SPER && sofar < SPER) return 0;
    dir = sofar / SPER;
    sofar -= SPER*dir;
#endif

    GR_EVENT ev;
    int key = 'r';
    int ret = 0;
    _MAKETHIS;

    if (dir < 0) {
	key = 'l';
	dir = -dir;
    }
    
    while (dir) {
	pz_legacy_construct_GR_EVENT (&ev, GR_EVENT_TYPE_KEY_DOWN, key);
	ret |= data->key (&ev);
	pz_legacy_construct_GR_EVENT (&ev, GR_EVENT_TYPE_KEY_UP, key);
	ret |= data->key (&ev);
	dir--;
    }
    this->dirty++;
    return ret;
}
int pz_legacy_timer (TWidget *this)
{
    GR_EVENT ev;
    _MAKETHIS;
    pz_legacy_construct_GR_EVENT (&ev, GR_EVENT_TYPE_TIMER, 0);
    this->dirty++;
    return data->key (&ev);
}
void pz_legacy_destroy (TWidget *this)
{
    free (this->data);
}


// Make widget 0 by 0 -- many old games draw to window only when they need to.
// ttk_run() blanks a WxH region before calling draw(), and draw() might
// only draw a few things.
TWidget *pz_new_legacy_widget (void (*do_draw)(), int (*do_keystroke)(GR_EVENT *))
{
    TWidget *ret = ttk_new_widget (0, 0);
    ret->w = 0;
    ret->h = 0;
    ret->data = calloc (1, sizeof(legacy_data));
    ret->focusable = 1;
    ret->dirty = 1;
    ret->draw = pz_legacy_draw;
    ret->button = pz_legacy_button;
    ret->down = pz_legacy_down;
    ret->scroll = pz_legacy_scroll;
    ret->timer = pz_legacy_timer;
    ret->destroy = pz_legacy_destroy;

    legacy_data *data = (legacy_data *)ret->data;
    data->draw = do_draw;
    data->key = do_keystroke;

    return ret;
}

TWindow *pz_mh_legacy (ttk_menu_item *item) 
{
    TWindow *old = ttk_windows->w;
    void (*newwin)() = (void (*)())item->data;
    (*newwin)();
    if (ttk_windows->w != old) {
	item->sub = ttk_windows->w;
	return TTK_MENU_ALREADYDONE;
    } else {
	item->flags &= ~TTK_MENU_MADESUB;
	return TTK_MENU_DONOTHING;
    }
}

GR_GC_ID pz_get_gc(int copy)
{
    return (copy ? GrCopyGC(root_gc) : root_gc);
}

// for knowing what to change the header of
TWindow *pz_last_window = 0;
extern char *pz_next_header;

GR_WINDOW_ID pz_new_window (int x, int y, int w, int h, void(*do_draw)(void), int(*do_keystroke)(GR_EVENT * event))
{
    fprintf (stderr, "Legacy code alert!\n");

    TWindow *ret = ttk_new_window();
    ttk_fillrect (ret->srf, 0, 0, ret->w, ret->h, ttk_makecol (255, 255, 255));
    ret->x = x;
    ret->y = y;
    if ((y == HEADER_TOPLINE) || (y == HEADER_TOPLINE + 1))
	ret->y = ttk_screen->wy, ret->h = ttk_screen->h - ttk_screen->wy;
    else if (!y)
	ret->show_header = 0;
    ret->w = w;
    ret->h = h;
    ttk_add_widget (ret, pz_new_legacy_widget (do_draw, do_keystroke));

    pz_last_window = ret;

    if (pz_next_header) {
	ttk_window_set_title (ret, pz_next_header);
	pz_next_header = 0;
    }

    return ret;
}

void
pz_close_window(GR_WINDOW_ID wid)
{
    ttk_hide_window (wid);
    // pick new top legacy window:
    if (ttk_windows->w->focus && ttk_windows->w->focus->draw == pz_legacy_draw)
	pz_last_window = ttk_windows->w; // pick new top window
    else
	pz_last_window = 0;
    wid->data = 0x12345678; // hey menu: free it & recreate
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
	pz_startup_contrast = ipod_get_contrast();
	usb_connected = usb_is_connected();
	fw_connected = fw_is_connected();
#endif

	if ((root_wid = ttk_init()) == 0) {
	    fprintf(stderr, _("ttk_init failed"));
	    exit(1);
	}
	ttk_hide_window (root_wid);

	ttk_set_global_event_handler (pz_new_event_handler);
	ttk_set_global_unused_handler (pz_unused_handler);
	
#ifdef LOCALE
	setlocale(LC_ALL, "");
	bindtextdomain("podzilla", LOCALEDIR);
	textdomain("podzilla");
#endif
	
	root_gc = GrNewGC();
	GrSetGCUseBackground(root_gc, GR_FALSE);
	GrSetGCForeground(root_gc, BLACK);
	GrGetScreenInfo(&screen_info);

	hw_version = ipod_get_hw_version();
	
	if( hw_version && hw_version < 30000 ) { /* 1g/2g only */
		pz_set_time_from_file();
	}

	ipod_load_settings();
	load_font();
	appearance_init();
#ifdef MPDC
	mpdc_init();
#endif
	header_init();

	if (ipod_get_setting (CONTRAST) < 40) // probably no pz.conf file
	    ipod_set_setting (CONTRAST, 96);

	connection_timer = ttk_create_timer (1000, check_connection);

	new_menu_window();

	ttk_run();
	quit_podzilla();

	return 0;
}
