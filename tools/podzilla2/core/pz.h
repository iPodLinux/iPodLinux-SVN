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


/*********************************************************************
 * If you're smart enough to be reading this file, you probably want *
 * to know this.                                                     *
 *********************************************************************

 * Since Podzilla can do some things that would only cause confusion
   for newbies, there is a file  /etc/secrets  that unlocks the more
 * confusable (and also more interesting) aspects of podzilla. The file
   contains keywords in any format you want - if the keyword is there,
 * you're allowed to do whatever it is.

 * If there's something you think Podzilla should do but doesn't, check
   the source for a call to pz_has_secret - you may figure out exactly
 * how to fix that.

 * Please don't publicize these secrets, as we want them to remain quasi-
   secret. You are, however, free to discreetly tell people you trust to be
 * intelligent enough to handle them wisely.

 * Have fun.

 ********************************************************************/


#ifndef _PZ_H_
#define _PZ_H_

#ifdef __PZ_H__  // old pz.h symbol
#error Version mismatch.
#endif

#ifndef PZ_COMPAT
#define MWBACKEND /* no mwin emu */
#else
#define MWINCLUDECOLORS
#endif
#include <ttk.h>

/** Compat defs - legacy.c **/
#ifdef PZ_COMPAT
#if !defined(PZ_MOD) && defined(LEGACY_DOT_C)
#define NOWARN
#else
#undef NOWARN
#endif
#ifndef NOWARN
#warning Legacy code alert... please fix ASAP.
#endif
#define HEADER_TOPLINE 19
#define KEY_CLICK 1
#define KEY_UNUSED 2
#define EVENT_UNUSED 2
#define KEY_QUIT 4
extern t_GR_SCREEN_INFO screen_info;
extern long hw_version;
t_GR_WINDOW_ID pz_old_window (int x, int y, int w, int h,
			      void (*do_draw)(void), int (*keystroke)(t_GR_EVENT *event));
void pz_old_close_window (t_GR_WINDOW_ID win);
void pz_old_event_handler (t_GR_EVENT *ev);
void pz_draw_header(char *header);
t_GR_GC_ID pz_get_gc(int copy);
#endif
TWindow *pz_mh_legacy (ttk_menu_item *); // calls the void(*)() in item->data
TWidget *pz_new_legacy_widget (void (*do_draw)(), int (*do_keystroke)(t_GR_EVENT *));

/** Module and .POD/.PCD functions - module.c   XXX MOST NOT DONE**/
/* called from module */
#ifndef NODEF_MODULE
typedef struct _pz_Module PzModule;
#endif
PzModule *pz_register_module (const char *name, void (*cleanup)());
const char *pz_module_get_path (PzModule *mod, const char *filename);
#ifndef PZ_MOD
/* called from core */
void pz_modules_init (void);
PzModule *pz_load_module (const char *name);
void pz_unload_module (PzModule *mod);
void *pz_module_dlsym (PzModule *mod, const char *sym);
int pz_module_check_signature (PzModule *mod);
#endif

/** Configuration stuff - settings.c **/
#ifndef NODEF_CONFIG
typedef struct _pz_Config PzConfig;
#endif

extern PzConfig *pz_global_config;

PzConfig *pz_load_config (const char *filename);
void pz_save_config (PzConfig *conf);
void pz_blast_config (PzConfig *conf);
void pz_free_config (PzConfig *conf);

typedef struct _pz_ConfItem 
{
    unsigned int sid;

#define PZ_SETTING_INT     1
#define PZ_SETTING_STRING  2
#define PZ_SETTING_FLOAT   3
#define PZ_SETTING_ILIST   4
#define PZ_SETTING_SLIST   5
#define PZ_SETTING_BLOB    255
    int type;

    union 
    {
	int ival;
	char *strval;
	double fval;
	struct { int *ivals; int nivals; };
	struct { char **strvals; int nstrvals; };
	struct { void *blobval; int bloblen; };
    };
    struct _pz_ConfItem *next;
} PzConfItem;

#define PZ_SID_FORMAT  0xe0000000

PzConfItem *pz_get_setting (PzConfig *conf, unsigned int sid);
void pz_unset_setting (PzConfig *conf, unsigned int sid);
void pz_config_iterate (PzConfig *conf, void (*fn)(PzConfItem *));

int pz_get_int_setting (PzConfig *conf, unsigned int sid);
const char *pz_get_string_setting (PzConfig *conf, unsigned int sid);
void pz_set_int_setting (PzConfig *conf, unsigned int sid, int val);
void pz_set_string_setting (PzConfig *conf, unsigned int sid, const char *val);
void pz_set_float_setting (PzConfig *conf, unsigned int sid, double val);
void pz_set_ilist_setting (PzConfig *conf, unsigned int sid, int *vals, int nval);
void pz_set_slist_setting (PzConfig *conf, unsigned int sid, char **vals, int nval);
void pz_set_blob_setting (PzConfig *conf, unsigned int sid, void *val, int bytes);
#ifndef PZ_MOD
/* These enable you to set reserved settings (0xf0000000+) only.
 * Don't use in modules.
 */
void pz_setr_int_setting (PzConfig *conf, unsigned int sid, int val);
void pz_setr_string_setting (PzConfig *conf, unsigned int sid, const char *val);
#endif


typedef TWindow PzWindow;
typedef TWidget PzWidget;



/** Menu stuff - menu.c **/
#define PZ_MENU_DONOTHING  (TWindow *(*)())TTK_MENU_DONOTHING
#define PZ_MENU_UPONE      (TWindow *(*)())TTK_MENU_UPONE
#define PZ_MENU_UPALL      (TWindow *(*)())TTK_MENU_UPALL
#define PZ_MENU_ALREADYDONE (TWindow *(*)())TTK_MENU_ALREADYDONE
#define PZ_MENU_QUIT       (TWindow *(*)())TTK_MENU_QUIT
#define PZ_MENU_REPLACE    (TWindow *(*)())TTK_MENU_REPLACE
#define PZ_MENU_DESC_MAX   (TWindow *(*)())TTK_MENU_DESC_MAX

extern TWindow *(*pz_new_menu_window)(TWidget *menu_wid);
TWindow *pz_default_new_menu_window (TWidget *menu_wid);
ttk_menu_item *pz_get_menu_item (const char *path);
TWindow *pz_mh_sub (ttk_menu_item *item);
#ifndef PZ_MOD
void pz_menu_init (void);
TWindow *pz_menu_get (void);
#endif
#ifdef PZ_COMPAT
void pz_menu_add_legacy (const char *menupath, void (*handler)());
#endif
void pz_menu_add_ttkh (const char *menupath, TWindow *(*handler)(), void *data);
void pz_menu_add_after (const char *menupath, PzWindow *(*handler)(), const char *after);
void pz_menu_add_top (const char *menupath, PzWindow *(*handler)());
void pz_menu_add_action (const char *menupath, PzWindow *(*handler)());
void pz_menu_add_option (const char *menupath, const char **choices);
int pz_menu_get_option (const char *menupath);
void pz_menu_set_option (const char *menupath, int choice);
void pz_menu_add_setting (const char *menupath, unsigned int sid, PzConfig *conf, const char **choices);
void pz_menu_sort (const char *menupath);
void pz_menu_remove (const char *menupath);


/** Widget/window/event stuff - gui.c    XXX NOT DONE **/

#define PZ_EVENT_SCROLL       1
#define PZ_EVENT_STAP         2
#define PZ_EVENT_BUTTON_UP    3
#define PZ_EVENT_BUTTON_DOWN  4
#define PZ_EVENT_BUTTON_HELD  5
#define PZ_EVENT_INPUT        6
#define PZ_EVENT_FRAME        7
#define PZ_EVENT_TIMER        8
#define PZ_EVENT_DESTROY      9

typedef struct _pz_Event
{
    int type;
    int arg;
    int time;
} PzEvent;

#define PZ_WINDOW_NORMAL      0
#define PZ_WINDOW_FULLSCREEN  1
#define PZ_WINDOW_POPUP       2
#define PZ_WINDOW_XYWH        3

PzWindow *pz_do_window (const char *name, int geometry,
			void (*draw)(PzWidget *this, ttk_surface srf),
			int (*event)(PzEvent *ev), int timer);

PzWindow *pz_new_window (const char *name, int geometry, ...);
PzWindow *pz_finish_window (PzWindow *win);

PzWidget *pz_add_widget (PzWindow *win, void (*draw)(PzWidget *this, ttk_surface srf),
			 int (*event)(PzEvent *ev));
PzWidget *pz_new_widget (void (*draw)(PzWidget *this, ttk_surface srf), int (*event)(PzEvent *ev));
void pz_resize_widget (PzWidget *wid, int w, int h);
void pz_widget_set_timer (PzWidget *wid, int ms);

void pz_hide_window (PzWindow *win);
void pz_close_window (PzWindow *win);
void pz_show_window (PzWindow *win);

/** Secrets - secrets.c **/
#ifndef PZ_MOD
void pz_secrets_init();
#endif
int pz_has_secret (const char *key);


/** Header - header.c **/
#ifndef PZ_MOD
void pz_header_init (void);
void pz_header_fix_hold (void);
#endif
#define PZ_DEC_PLAIN    (0)
#define PZ_DEC_AMIGA11  (1)
#define PZ_DEC_AMIGA13  (2)
#define PZ_DEC_MROBE    (3)
void pz_header_set_decorations (int decor);
void pz_hwid_put_left (TWidget *wid); // puts wid as leftmost of left widgets, has to be called from mod init
void pz_hwid_pack_left (TWidget *wid); // puts wid as rightmost of left widgets
void pz_hwid_pack_right (TWidget *wid); // puts wid as leftmost of right widgets
void pz_hwid_unpack (TWidget *wid);
void pz_header_set_local (TWidget *left, TWidget *right);
void pz_header_unset_local (void);


/** Dialog and message - dialog.c **/
extern int (*pz_do_dialog) (const char *title, const char *text,
			    const char *b0, const char *b1, const char *b2,
			    int timeout, int is_err);
int pz_default_do_dialog (const char *title, const char *text,
			  const char *b0, const char *b1, const char *b2,
			  int timeout, int is_err);

int pz_dialog (const char *title, const char *text,
	       int nbuttons, int timeout, ...); // supply [nbuttons] const char *'s in the ... for buttons
int pz_errdialog (const char *title, const char *text,
		  int nbuttons, int timeout, ...); // supply [nbuttons] const char *'s in the ... for buttons
void pz_message_title (const char *title, const char *text);
void pz_message (const char *text);
void pz_warning (const char *fmt, ...);
void pz_error (const char *fmt, ...);
void pz_perror (const char *firstpart);


/** Vector text - vector.c    XXX NOT DONE **/
void pz_vector_string (ttk_surface srf, const char *string, int x, int y, int cw, int ch, int kern, ttk_color col);
void pz_vector_string_center (ttk_surface srf, const char *string, int x, int y, int cw, int ch, int kern, ttk_color col);
int pz_vector_width (const char *string, int cw, int ch, int kern);


/** iPod stuff - ipod.c **/
void pz_ipod_set (int sid, int value);
void pz_ipod_fix_setting (int sid, int value); // sets on iPod w/o setting in config
void pz_ipod_fix_settings (PzConfig *conf);
int pz_ipod_get_battery_level(void);
int pz_ipod_is_charging(void);
long pz_ipod_get_hw_version(void);
void pz_ipod_go_to_diskmode(void);
void pz_ipod_reboot(void);
void pz_ipod_powerdown(void);
int pz_ipod_usb_is_connected(void);
int pz_ipod_fw_is_connected(void);


/** Fonts - fonts.c **/
void pz_load_font (ttk_font *f, int setting);
// menu handler, data = &font_to_set, cdata = setting_to_set:
TWindow *pz_select_font (ttk_menu_item *item);


/** Other things - pz.c **/
void pz_register_global_hold_button (char ch, int ms, void (*handler)());
void pz_register_global_unused_button (char ch, void (*handler)(int, int)); // args = (button, time)
void pz_unregister_global_hold_button (char ch);
void pz_unregister_global_unused_button (char ch);
void pz_handled_hold (char ch); // call from the hold handler or all hell *WILL* break loose :-)
#ifndef PZ_MOD
void pz_uninit();
#endif

#define PZ_BL_OFF    -2
#define PZ_BL_RESET  -1
#define PZ_BL_ON      0


/** Locale **/
#define LOCALE
#ifdef IPOD
#ifndef __UCLIBC_HAS_LOCALE__
#warning You need to update your toolchain if you wish to have locale support. ("http://so2.sys-techs.net/ipod/toolchain/")
#undef LOCALE
#endif
#define LOCALEDIR "/usr/share/locale"
#else
#define LOCALEDIR "./locale"
#endif
#ifdef LOCALE
#define _(str) gettext(str)
#include <libintl.h>
#include <locale.h>
#else
#define _(str) str
#define gettext(str) str
#endif
#define N_(str) str

/************* Global settings values *************/

/* DISPLAY SETINGS 0 - 9 */

#define CONTRAST        (0)
#define BACKLIGHT       (1)
#define BACKLIGHT_TIMER (2)

/* AUDIO SETTIGNS 10 - 19 */

#define CLICKER         (10)
#define VOLUME          (11)
#define EQUALIZER       (12)
#define DSPFREQUENCY    (13)

/* PLAYLIST SETTINGS 20 - 29 */
 
#define SHUFFLE         (20)
#define REPEAT          (21)

/* OTHER SETTINGS 30 - 99 */

#define LANGUAGE        (30)
#define WHEEL_DEBOUNCE  (34)

#define TIME_ZONE	(35)
#define TIME_DST	(36)
#define TIME_IN_TITLE	(37)
#define TIME_TICKER	(38)
#define TIME_1224	(39)
#define TIME_WORLDTZ	(40)
#define TIME_WORLDDST	(41)

#define BROWSER_PATH	(42)
#define BROWSER_HIDDEN	(43)

#define COLORSCHEME	(44)	/* appearance */
#define DECORATIONS	(45)	/* appearance */
#define BATTERY_DIGITS	(46)	/* appearance */
#define DISPLAY_LOAD	(47)	/* appearance */
#define TEXT_FONT	(48)
#define SLIDE_TRANSIT	(49)
#define MENU_FONT	(50)

/* MODULE METASETTINGS 100 - 110 */
#define MODULE_LIST     (100)

#if defined(PZ_COMPAT) && !defined(LEGACY_DOT_C)
#define pz_new_window(x,y,w,h,d,k) pz_old_window(x,y,w,h,d,k) /* hopefully no conflict with new_win(title,XYWH,x,y,w,h) */
#define pz_close_window(w) pz_old_close_window(w)
#endif

#endif
