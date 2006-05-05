/*
 * Copyright (c) 2005 Joshua Oreman, David Carne, and Courtney Cavin
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

#include <string.h>
#include "pz.h"

static TWidget *root_menu;
static int inited = 0;
extern int pz_setting_debounce;

// Structure:
// [menu]
// items:
//   item    (menu -> [menu], data -> whatever)
//   item
//   submenu (menu -> [menu], data -> [sub], makesub -> <mh_sub>)
//     [sub]
//     items:
//       item
//       item
// etc.

static void check_init() 
{
    if (!inited) {
	root_menu = ttk_new_menu_widget (0, ttk_menufont, ttk_screen->w - ttk_screen->wx,
					 ttk_screen->h - ttk_screen->wy);
	ttk_menu_set_closeable (root_menu, 0);
	inited = 1;
    }
}

static TWindow *quit_podzilla() 
{
    pz_uninit();
    exit (0);
}
static TWindow *reboot_ipod() 
{
    pz_ipod_reboot();
    return TTK_MENU_DONOTHING;
}
static TWindow *poweroff_ipod() 
{
    pz_ipod_powerdown();
    return TTK_MENU_DONOTHING;
}
static TWindow *reset_settings() 
{
	pz_blast_config (pz_global_config);
	return TTK_MENU_UPONE;
}

static const char *time1224_options[] = { N_("12-hour"), N_("24-hour"), 0 };

const char * clocks_timezones[] = {
        "United Kingdom 0:00",
        "France +1:00",
        "Greece +2:00",
        "Kenya +3:00",
        "Iran +3:30",

        "UAE +4:00",
        "Afghanistan +4:30",
        "Uzbekistan +5:00",
        "IST India +5:30",
        "Nepal +5:45",

        "Sri Lanka +6:00",
        "Myanmar +6:30",
        "Thailand +7:00",
        "AWST W. Australia +8:00",
        "W. Australia +8:45",

        "JST/KST Japan +9:00",
        "ACST C. Australia +9:30",
        "AEST E. Australia +10:00",
        "New South Wales +10:30",
        "Micronesia +11:00",

        "Norfolk +11:30",
        "Fiji +12:00",
        "Chatham Islands +12:45",
        "Tonga +13:00",
        "Kiribati +14:00",

        "UTC -12:00",
        "Midway Atoll -11:00",
        "HST Hawaii -10:00",
        "Polynesia -9:30",
        "AKST Alaska -9:00",

        "PST US Pacific -8:00",
        "MST US Moutain -7:00",
        "CST US Central -6:00",
        "EST US Eastern -5:00",
        "AST Atlantic -4:00",

        "NST Newfoundland -3:30",
        "Brazil -3:00",
        "Mid-Atlantic -2:00",
        "Portugal -1:00",

	0
};

const char * clocks_dsts[] = {
	"0:00",
	"0:30",
	"1:00",
	0
};

static const char *backlight_options[] = {
	N_("Off"), N_("1 sec"), N_("2 secs"), N_("5 secs"), N_("10 secs"),
	N_("30 secs"), N_("1 min"), N_("On"), 0
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

static const char * transit_options[] = { 
    N_("Off"), N_("Slow"), N_("Fast"), 0
};

static const char * verbosity_options[] = {
    N_("High"), N_("Medium"), N_("Low"), 0
};


/* be sure to keep the "Off" entry lined up with BATTERY_UPDATE_OFF in pz.h */
/* XXXXX
static const char * battery_update_rates[] = { 
		N_("1s"), N_("5s"), N_("15s"),
		N_("30s"), N_("1m"), N_("Off"), 0
};
*/

static const char * title_justifications[] = {
		N_("Center"), N_("Left"), N_("Right"), 0 };

static const char * headerwidget_display_rates[] = {
		N_("1s"), N_("2s"), N_("5s"), 
		N_("10s"), N_("15s"),
		N_("30s"), N_("1m"), 0
};

/* need better words for these */
static const char * headerwidget_display_methods[] = {
		N_("Display All"),	/* display all simultaneously */
		N_("Cycle"), 		/* round-robin through them all */
		N_("Disabled"), 0	/* display none of them */
};



static void slider_set_setting (int set, int val) 
{
	pz_ipod_set (set, val);
}

static TWindow *new_settings_slider_window (char *title, int setting,
                                            int slider_min, int slider_max)
{
    int tval = pz_get_int_setting (pz_global_config, setting);
    
    TWindow *win = ttk_new_window();
    TWidget *slider = ttk_new_slider_widget (0, 0, ttk_screen->w * 3 / 5, slider_min, slider_max, &tval, 0);
    ttk_slider_set_callback (slider, slider_set_setting, setting);
    ttk_window_set_title (win, title);
    ttk_add_widget (win, slider);
    win->data = 0x12345678;
    ttk_set_popup (win);
    return win;
}

static TWindow *set_contrast() 
{
	return new_settings_slider_window (_("Set Contrast"), CONTRAST, 64, 128);
}
static TWindow *set_wheeldebounce() 
{
	TWindow *ret;
	pz_setting_debounce = 1;
	ttk_set_scroll_multiplier (1, 1);
	ret = new_settings_slider_window (_("Wheel Sensitivity"), WHEEL_DEBOUNCE, 0, 19);
	return ret;
}

static int settings_button (TWidget *this, int key, int time) 
{
	if (key == TTK_BUTTON_MENU)
		pz_save_config (pz_global_config);
	return ttk_menu_button (this, key, time);
}

/* XXXXX BleuLlama wants to use this, but can't because he's an idiot.
static int epoch_button( TWidget *this, int key, int time )
{
	ttk_epoch++;
	return( ttk_menu_button( this, key, time ));
}
*/

void pz_menu_init()
{
    ttk_menu_item * item;

    check_init();
    pz_menu_add_stub ("/Music");
    pz_menu_add_stub ("/Extras");
    pz_menu_add_stub ("/Settings/About");
    pz_menu_add_stub ("/Settings/Credits");
    pz_menu_add_stub ("/Settings/Date & Time/Clock");
    pz_menu_add_stub ("/Settings/Date & Time/Set Time");
    pz_menu_add_stub ("/Settings/Date & Time/Set Time & Date");
    pz_menu_add_setting ("/Settings/Date & Time/TZ", TIME_ZONE, pz_global_config, clocks_timezones);
    pz_menu_add_setting ("/Settings/Date & Time/DST Offset", TIME_DST, pz_global_config, clocks_dsts);
    pz_menu_add_setting ("/Settings/Date & Time/Time Style", TIME_1224, pz_global_config, time1224_options);
    pz_menu_add_setting ("/Settings/Date & Time/Time Tick Noise", TIME_TICKER, pz_global_config, 0);
    pz_menu_add_setting ("/Settings/Repeat", REPEAT, pz_global_config, repeat_options);
    pz_menu_add_setting ("/Settings/Shuffle", SHUFFLE, pz_global_config, shuffle_options);
    pz_menu_add_action ("/Settings/Contrast", set_contrast);
    pz_menu_add_action ("/Settings/Wheel Sensitivity", set_wheeldebounce);
    pz_menu_add_setting ("/Settings/Backlight Timer", BACKLIGHT_TIMER, pz_global_config, backlight_options);
    pz_menu_add_setting ("/Settings/Clicker", CLICKER, pz_global_config, 0);

    item = pz_menu_add_action ("/Settings/Appearance/Color Scheme", pz_select_color_scheme);
    item->flags |= TTK_MENU_ICON_SUB;


    /* select from the available widgets */
    item = pz_menu_add_action( "/Settings/Appearance/Widgets/L Sel,Rate", 
		pz_select_left_widgets );
    item->flags |= TTK_MENU_ICON_SUB;

    /* set of display styles */
    pz_menu_add_setting ("/Settings/Appearance/Widgets/L Mode", 
		HEADER_METHOD_L, pz_global_config, 
		headerwidget_display_methods);

    /* how often the cycling rotates the display */
    pz_menu_add_setting ("/Settings/Appearance/Widgets/L Cycle Rate", 
		HEADER_CYC_RATE_L, pz_global_config, 
		headerwidget_display_rates);

    /* should we update active, but cycled out widgets? */
/* XXX future
    pz_menu_add_setting ("/Settings/Appearance/Widgets/L Update Hidden", 
		HEADER_UPD_CYCD_L, pz_global_config, boolean_options );
*/

    /* likewise for these three */
    item = pz_menu_add_action( "/Settings/Appearance/Widgets/R Sel,Rate", 
		pz_select_right_widgets );
    item->flags |= TTK_MENU_ICON_SUB;
    pz_menu_add_setting ("/Settings/Appearance/Widgets/R Mode", 
		HEADER_METHOD_R, pz_global_config, 
		headerwidget_display_methods);
    pz_menu_add_setting ("/Settings/Appearance/Widgets/R Cycle Rate", 
		HEADER_CYC_RATE_R, pz_global_config, 
		headerwidget_display_rates);
/* XXX future
    pz_menu_add_setting ("/Settings/Appearance/Widgets/R Update Hidden", 
		HEADER_UPD_CYCD_R, pz_global_config, boolean_options );
*/

    /* these are for the titlebar behind the widgets */
    item = pz_menu_add_action ("/Settings/Appearance/Decorations", 
		pz_select_decorations );
    item->flags |= TTK_MENU_ICON_SUB;
    pz_menu_add_setting ("/Settings/Appearance/Decoration Update",
		DECORATION_RATE, pz_global_config, headerwidget_display_rates);
    pz_menu_add_setting ("/Settings/Appearance/Text Justification", 
		TITLE_JUSTIFY, pz_global_config, title_justifications);

    pz_menu_add_setting ("/Settings/Appearance/Menu Transition", SLIDE_TRANSIT, pz_global_config, transit_options);
    item = pz_menu_add_ttkh ("/Settings/Appearance/Menu Font", pz_select_font, &ttk_menufont);
    item->cdata = MENU_FONT;;
    item->flags |= TTK_MENU_ICON_SUB;
    item = pz_menu_add_ttkh ("/Settings/Appearance/Text Font", pz_select_font, &ttk_textfont);
    item->cdata |= TEXT_FONT;
    item->flags |= TTK_MENU_ICON_SUB;
    pz_menu_add_setting ("/Settings/Verbosity", VERBOSITY, pz_global_config, verbosity_options);
    pz_menu_add_setting ("/Settings/Browser Path Display", BROWSER_PATH, pz_global_config, 0);
    pz_menu_add_setting ("/Settings/Browser Show Hidden", BROWSER_HIDDEN, pz_global_config, 0);
    pz_menu_add_stub ("/File Browser");
    pz_menu_add_action ("/Power/Quit podzilla", quit_podzilla);
    pz_menu_add_action ("/Power/Reboot iPod/Cancel", PZ_MENU_UPONE);
    pz_menu_add_action ("/Power/Reboot iPod/Absolutely", reboot_ipod);
    pz_menu_add_action ("/Power/Turn Off iPod/Cancel", PZ_MENU_UPONE);
    pz_menu_add_action ("/Power/Turn Off iPod/Absolutely", poweroff_ipod);

    ((TWidget *)pz_get_menu_item ("/Settings")->data)->button = settings_button;
}

TWindow *pz_default_new_menu_window (TWidget *menu_wid) 
{
    // If you don't want to directly use menu_wid, you can use ttk_menu_get_item() and friends.
    TWindow *ret = ttk_new_window();
    ttk_add_widget (ret, menu_wid);
    ttk_window_title (ret, "podzilla");
    return ret;
}

TWindow *(*pz_new_menu_window)(TWidget *) = pz_default_new_menu_window;

TWindow *pz_menu_get() 
{
    // Want to add these at end so modules get stuff put in appropriate place...
    pz_menu_add_action ("/Settings/Exit Without Saving", PZ_MENU_UPONE);
    pz_menu_add_action ("/Settings/Reset All Settings/Cancel", PZ_MENU_UPONE);
    pz_menu_add_action ("/Settings/Reset All Settings/Absolutely", reset_settings);
    return pz_new_menu_window (root_menu);
}

static TWindow *mh_pz (ttk_menu_item *item) 
{
    if ((TWindow*(*)())item->data < PZ_MENU_DESC_MAX)
	return (TWindow *)item->data;
    return (*(TWindow*(*)())item->data)();
}

#define NO_ADD    -2
#define LOC_START  0
#define LOC_END   -1
static void add_at_loc (TWidget *menu, ttk_menu_item *item, int loc) 
{
    check_init();
    
    if (!item->name) item->name = ""; // avoid errors

    if (loc == LOC_END)
	ttk_menu_append (menu, item);
    else if (loc == LOC_START)
	ttk_menu_insert (menu, item, 0);
    else
	ttk_menu_insert (menu, item, loc);
}

TWindow *pz_mh_sub (ttk_menu_item *item)
{
    TWindow *ret = ttk_new_window();
    ttk_add_widget (ret, item->data);
    ttk_window_title (ret, item->name);
    ret->widgets->v->draw (ret->widgets->v, ret->srf);
    return ret;
}

static ttk_menu_item root_item = {"<Root>", {pz_mh_sub}, TTK_MENU_ICON_SUB};
ttk_menu_item *resolve_menupath (const char *path, int loc)
{
    const char *p = path;
    TWidget *menu;
    ttk_menu_item *item = 0;
    int i, len;

    if (!strcmp (path, "/")) {
	root_item.data = root_menu;
	return &root_item;
    }

    check_init();
    menu = root_menu;

    while (p && *p) {
	while (*p == '/') p++;
	if (strchr (p, '/'))
	    len = strchr (p, '/') - p;
	else
	    len = strlen (p);
	
	for (i = 0; (item = ttk_menu_get_item (menu, i)) != 0; i++) {
	    if (!strncmp (item->name, p, len))
		break;
	}
	
	if (!strchr (p, '/') || !((strchr(p, '/'))[1])) break;

	if (!item) {
	    if (loc == NO_ADD) return 0;
	    
	    item = calloc (1, sizeof(ttk_menu_item));
	    item->name = malloc (len + 1);
	    strncpy ((char *)item->name, p, len);
	    ((char *)item->name)[len] = 0;
            item->free_name = 1;
	    item->makesub = pz_mh_sub;
	    item->flags = TTK_MENU_ICON_SUB;
	    item->data = ttk_new_menu_widget (0, ttk_menufont, menu->w, menu->h);
	    add_at_loc (menu, item, loc);
	} else if (item->makesub != pz_mh_sub) {
	    item->makesub = pz_mh_sub;
	    item->flags = TTK_MENU_ICON_SUB;
	    item->data = ttk_new_menu_widget (0, ttk_menufont, menu->w, menu->h);
	}

        item->visible = 0;
	p = strchr (p, '/');
	menu = item->data;
    }

    if (!item) {
	if (loc == NO_ADD) return 0;
	
	// allocate new item
	item = calloc (1, sizeof(ttk_menu_item));
	if (p && !p[1]) { // asking for a submenu
	    item->makesub = pz_mh_sub;
	    item->flags = TTK_MENU_ICON_SUB;
	    item->data = ttk_new_menu_widget (0, ttk_menufont, menu->w, menu->h);
	}

        if (strrchr (path, '/')) {
            if (p && !p[1]) {
                item->name = path + strlen (path);
                while (item->name != path && item->name[0] != '/')
                    item->name--;
                if (item->name != path)
                    item->name++;
            } else {
                item->name = strrchr (path, '/') + 1;
            }
        } else {
            item->name = path;
        }
        item->name = strdup (item->name);
        item->free_name = 1;
        add_at_loc (menu, item, loc);
    }
    item->visible = 0; // always visible 
    return item;
}
ttk_menu_item *pz_get_menu_item (const char *path) 
{
    return resolve_menupath (path, LOC_END);
}

ttk_menu_item *pz_menu_add_after (const char *menupath, PzWindow *(*handler)(), const char *after) 
{
    int i;
    ttk_menu_item *item;
    TWidget *menu;
    
    if (strchr (menupath, '/')) {
	char *parentpath = strdup (menupath);
	*(strrchr (parentpath, '/') + 1) = 0;
	item = resolve_menupath (parentpath, LOC_END);
	menu = item->data;
    } else {
	menu = root_menu;
    }

    for (i = 0; (item = ttk_menu_get_item (menu, i)) != 0; i++) {
	if (!strcmp (item->name, after))
	    break;
    }
    if (!item)
	i = LOC_END;
    
    item = resolve_menupath (menupath, i);
    item->makesub = mh_pz;
    item->data = handler;
    item->flags = 0;
    ttk_menu_item_updated (item->menu, item);
    return item;
}

ttk_menu_item *pz_menu_add_top (const char *menupath, PzWindow *(*handler)()) 
{
    ttk_menu_item *item = resolve_menupath (menupath, LOC_START);
    item->makesub = mh_pz;
    item->data = handler;
    item->flags = 0;
    ttk_menu_item_updated (item->menu, item);
    return item;
}

ttk_menu_item *pz_menu_add_legacy (const char *menupath, void (*handler)()) 
{
    ttk_menu_item *item = resolve_menupath (menupath, LOC_END);
    item->makesub = pz_mh_legacy;
    item->data = handler;
    item->flags = 0;
    ttk_menu_item_updated (item->menu, item);
    return item;
}

ttk_menu_item *pz_menu_add_ttkh (const char *menupath, TWindow *(*handler)(ttk_menu_item *), void *data) 
{
    ttk_menu_item *item = resolve_menupath (menupath, LOC_END);
    item->makesub = handler;
    item->data = data;
    item->flags = 0;
    ttk_menu_item_updated (item->menu, item);
    return item;
}

ttk_menu_item *pz_menu_add_action (const char *menupath, PzWindow *(*handler)()) 
{
    ttk_menu_item *item = resolve_menupath (menupath, LOC_END);
    item->makesub = mh_pz;
    item->data = handler;
    item->flags = 0;
    ttk_menu_item_updated (item->menu, item);
    return item;
}

static int invisible() { return 0; }
static TWindow *stub (ttk_menu_item *item) 
{
	pz_message ("Nothing to see here, please move along...");
	return TTK_MENU_DONOTHING;
}
ttk_menu_item *pz_menu_add_stub (const char *menupath) 
{
	ttk_menu_item *item = resolve_menupath (menupath, LOC_END);
	item->makesub = stub;
	item->visible = invisible;
	item->flags = 0;
	ttk_menu_item_updated (item->menu, item);
	return item;
}


ttk_menu_item *pz_menu_add_option (const char *menupath, const char **choices) 
{
    ttk_menu_item *item = resolve_menupath (menupath, LOC_END);
    if (!choices) choices = boolean_options;
    item->choices = choices;
    ttk_menu_item_updated (item->menu, item);
    return item;
}

int pz_menu_get_option (const char *menupath) 
{
    ttk_menu_item *item = resolve_menupath (menupath, NO_ADD);
    if (!item) return 0;
    return item->choice;
}

void pz_menu_set_option (const char *menupath, int choice) 
{
    ttk_menu_item *item = resolve_menupath (menupath, NO_ADD);
    if (!item) return;
    item->choice = choice;
}

static void ch_set_setting (ttk_menu_item *item, int sid) 
{
    if (item->data == pz_global_config)
        pz_ipod_set (sid, item->choice);
    else
        pz_set_int_setting (item->data, sid, item->choice);
}
static int ch_get_setting (ttk_menu_item *item, int sid) 
{ return pz_get_int_setting (item->data, sid); }

ttk_menu_item *pz_menu_add_setting (const char *menupath, unsigned int sid, PzConfig *conf, const char **choices) 
{
    ttk_menu_item *item = resolve_menupath (menupath, LOC_END);
    if (!choices) choices = boolean_options;
    item->choices = choices;
    item->choicechanged = ch_set_setting;
    item->choiceget = ch_get_setting;
    item->cdata = sid;
    item->data = conf;
    ttk_menu_item_updated (item->menu, item);
    return item;
}

void pz_menu_sort (const char *menupath)
{
    ttk_menu_item *item = resolve_menupath (menupath, NO_ADD);
    if (!item) return;
    if (item->makesub != pz_mh_sub) {
	pz_error ("Can't sort a non-menu");
	return;
    }
    ttk_menu_sort (item->data);
}

void pz_menu_remove (const char *menupath)
{
    ttk_menu_item *item = resolve_menupath (menupath, NO_ADD);
    if (!item) return;
    ttk_menu_remove_by_ptr (item->menu, item);
}

