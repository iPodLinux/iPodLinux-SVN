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

#ifndef _PZ_H_
#define _PZ_H_

#ifdef __PZ_H__  // old pz.h symbol
#error Version mismatch.
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern const char *PZ_Developers[];

#ifndef PZ_COMPAT
#define MWBACKEND /* no mwin emu */
#else
#define MWINCLUDECOLORS
#endif
#include <ttk.h>

#define PZ_API_VERSION  0x10000

/** Compat defs - legacy.c **/
#ifdef PZ_COMPAT
#if !defined(PZ_MOD) && defined(LEGACY_DOT_C)
#undef LEGACY_WARN
#endif
#ifdef LEGACY_WARN
#warning Legacy code alert... please fix ASAP.
#endif
#define HEADER_TOPLINE (ttk_screen->wy)
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
void vector_render_char (t_GR_WINDOW_ID win, t_GR_GC_ID gc,
			 char ch, int scale, int x, int y);
void vector_render_string (t_GR_WINDOW_ID win, t_GR_GC_ID gc,
			   const char *string, int kern, int scale, int x, int y);
void vector_render_string_center (t_GR_WINDOW_ID win, t_GR_GC_ID gc,
				  const char *string, int kern, int scale, int x, int y);
void vector_render_string_right (t_GR_WINDOW_ID win, t_GR_GC_ID gc,
				 const char *string, int kern, int scale, int x, int y);
int vector_string_pixel_width (const char *string, int kern, int scale);
#define new_message_window pz_message
#endif
TWindow *pz_mh_legacy (ttk_menu_item *); // calls the void(*)() in item->data
TWidget *pz_new_legacy_widget (void (*do_draw)(), int (*do_keystroke)(t_GR_EVENT *));

/** Module and .POD/.PCD functions - module.c   XXX MOST NOT DONE**/
#ifndef NODEF_MODULE
typedef struct _pz_Module PzModule;
#endif

/* called from module */
extern int _pz_mod_check_version (int ver); /* DO NOT call directly */
#ifdef __PZ_BUILTIN_MODULE
extern void (*__pz_builtin_init_functions[])();
extern const char *__pz_builtin_names[];
extern int __pz_builtin_number_of_init_functions;
#define PZ_MOD_INIT(fn) \
    static void __init_module__ () { \
        __pz_builtin_init_functions[__pz_builtin_number_of_init_functions] = fn; \
        __pz_builtin_names[__pz_builtin_number_of_init_functions++] = __PZ_MODULE_NAME; \
    } \
    static void (*__init_module_constructor_reference__)() __attribute__ ((__section__ (".ctors"))) = \
        __init_module__;
#else
// The _init_module__ one is to make it transparent
// on systems with leading underscores on C symbols.
#ifdef __cplusplus
#define CLINKAGE extern "C"
#else
#define CLINKAGE
#endif
#define PZ_MOD_INIT(fn) \
    CLINKAGE void __init_module__() { \
        if (_pz_mod_check_version(PZ_API_VERSION)) fn(); \
    } \
    CLINKAGE void _init_module__() { \
        if (_pz_mod_check_version(PZ_API_VERSION)) fn(); \
    }
#endif
#ifdef PZ_COMPAT
#define PZ_SIMPLE_MOD(n,f,p) \
    static void init_mod() \
    { \
        (void) pz_register_module (n, 0); \
        pz_menu_add_legacy (p, f); \
    } \
    PZ_MOD_INIT(init_mod)

#define PZ_SIMPLE_MOD_GROUP(n,f,p,g) \
    static void init_mod() \
    { \
        (void) pz_register_module (n, 0); \
        pz_menu_add_legacy_group (p, g, f); \
    } \
    PZ_MOD_INIT(init_mod)

#endif

PzModule *pz_register_module (const char *name, void (*cleanup)());
const char *pz_module_get_cfgpath (PzModule *mod, const char *file);
const char *pz_module_get_datapath (PzModule *mod, const char *file);
void pz_module_iterate (void (*fn)(const char *name, const char *longname, const char *author));
void *pz_module_softdep (const char *modname, const char *symname);
#ifndef PZ_MOD
/* called from core */
void pz_modules_init (void);
void pz_modules_cleanup (void);
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
void pz_set_blob_setting (PzConfig *conf, unsigned int sid, const void *val, int bytes);
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
ttk_menu_item *pz_menu_add_legacy (const char *menupath, void (*handler)());
ttk_menu_item *pz_menu_add_legacy_group( const char *menupath, const char *group, void (*handler)());
#endif
ttk_menu_item *pz_menu_add_ttkh (const char *menupath, TWindow *(*handler)(), void *data);
ttk_menu_item *pz_menu_add_ttkh_group (const char *menupath, const char *group, TWindow *(*handler)(), void *data);
ttk_menu_item *pz_menu_add_stub (const char *menupath); // adds invisible stub to preserve order
ttk_menu_item *pz_menu_add_after (const char *menupath, PzWindow *(*handler)(), const char *after);
ttk_menu_item *pz_menu_add_top (const char *menupath, PzWindow *(*handler)());
ttk_menu_item *pz_menu_add_action (const char *menupath, PzWindow *(*handler)());
/* the next one also specifies a group */
ttk_menu_item *pz_menu_add_action_group (const char *menupath, const char *group, PzWindow *(*handler)() );
void pz_menu_set_group( const char *menupath, const char *group, int flags );
ttk_menu_item *pz_menu_add_option (const char *menupath, const char **choices);
int pz_menu_get_option (const char *menupath);
void pz_menu_set_option (const char *menupath, int choice);
ttk_menu_item *pz_menu_add_setting (const char *menupath, unsigned int sid, PzConfig *conf, const char **choices);
void pz_menu_sort (const char *menupath);
void pz_menu_remove (const char *menupath);


/** Menuconf stuff - menuconf.c **/
int pz_menuconf_runargs(int argc, char * argv[]);
int pz_menuconf_runstr(char * str);
int pz_menuconf_runfile(char * fn);
void pz_menuconf_init();


/** Widget/window/event stuff and premade GUI stuff - gui.c **/

TWindow *pz_create_stringview (const char *str, const char *title);

#define PZ_EVENT_SCROLL       1
#define PZ_EVENT_STAP         2
#define PZ_EVENT_BUTTON_UP    3
#define PZ_EVENT_BUTTON_DOWN  4
#define PZ_EVENT_BUTTON_HELD  5
#define PZ_EVENT_INPUT        6
#define PZ_EVENT_FRAME        7
#define PZ_EVENT_TIMER        8
#define PZ_EVENT_DESTROY      9

#define PZ_BUTTON_MENU        'm'
#define PZ_BUTTON_PREVIOUS    'w'
#define PZ_BUTTON_NEXT        'f'
#define PZ_BUTTON_PLAY        'd'
#define PZ_BUTTON_HOLD        'h'
#define PZ_BUTTON_ACTION      '\n'
#ifdef PZ_COMPAT
#define IPOD_BUTTON_ACTION		('\r')
#define IPOD_BUTTON_MENU		('m')
#define IPOD_BUTTON_REWIND		('w')
#define IPOD_BUTTON_FORWARD		('f')
#define IPOD_BUTTON_PLAY		('d')

#define IPOD_SWITCH_HOLD		('h')

#define IPOD_WHEEL_CLOCKWISE		('r')
#define IPOD_WHEEL_ANTICLOCKWISE	('l')
#define IPOD_WHEEL_COUNTERCLOCKWISE	('l')

#define IPOD_REMOTE_PLAY		('1')
#define IPOD_REMOTE_VOL_UP		('2')
#define IPOD_REMOTE_VOL_DOWN		('3')
#define IPOD_REMOTE_FORWARD		('4')
#define IPOD_REMOTE_REWIND		('5')
#endif

typedef struct _pz_Event
{
    PzWidget *wid;
    int type;
    int arg;
    int time;
} PzEvent;

#define PZ_WINDOW_NORMAL      0
#define PZ_WINDOW_FULLSCREEN  1
#define PZ_WINDOW_POPUP       2
#define PZ_WINDOW_XYWH        3

PzWindow *pz_do_window (const char *name, int geometry,
			void (*draw)(PzWidget *_this, ttk_surface srf),
			int (*event)(PzEvent *ev), int timer);

PzWindow *pz_new_window (const char *name, int geometry, ...);
PzWindow *pz_finish_window (PzWindow *win);

PzWidget *pz_add_widget (PzWindow *win, void (*draw)(PzWidget *_this, ttk_surface srf),
			 int (*event)(PzEvent *ev));
PzWidget *pz_new_widget (void (*draw)(PzWidget *_this, ttk_surface srf), int (*event)(PzEvent *ev));
void pz_resize_widget (PzWidget *wid, int w, int h);
void pz_widget_set_timer (PzWidget *wid, int ms);

void pz_hide_window (PzWindow *win);
void pz_close_window (PzWindow *win);
void pz_show_window (PzWindow *win);


/** Header - header.c **/
#ifndef PZ_MOD
void pz_header_init (void);
void pz_header_fix_hold (void);
#endif

/* modular headers-related stuff */
TWindow * pz_select_decorations( void );	/* user selection of deco. */
TWindow * pz_select_left_widgets( void );	/* user selection of left  */
TWindow * pz_select_right_widgets( void );	/* user selection of right */

struct header_info;	/* pre-declare for the callback functions... */

/* you'll need for your draw and update routines to adhere to these */
typedef void (*update_fcn) ( struct header_info * );
typedef void (*draw_fcn)   ( struct header_info *, ttk_surface srf );

typedef struct header_info {
	struct header_info *next; 	/* next on the list */

        /* set by the core, internal to core (treat as private.. hands off!)  */
        char * name;			/* the name of the widget */
        TWidget * widg;			/* widget struct */
        int side;			/* L / R display side */
        int LZorder;			/* left order */
        int RZorder;			/* right order */
        int LURate;			/* countdown timeout for udpating */
        int RURate;			/* countdown timeout for udpating */
        int LCountdown;			/* countdown timer for udpating */
        int RCountdown;			/* countdown timer for udpating */
	int group;			/* grouping - for transients */
        update_fcn updfcn;		/* update fcn */
        draw_fcn drawfcn;		/* draw fcn */

        /* set/changable by the widget (treat as public)*/
        void * data;                    /* user data */
} header_info;

#define HEADER_SIDE_LEFT        (0x01)
#define HEADER_SIDE_RIGHT       (0x02)
#define HEADER_SIDE_DECORATION  (0x80)

void pz_add_header_widget( char * widgetDisplayName,
                            update_fcn update_function,
                            draw_fcn draw_function,
                            void * data );
void pz_add_header_decoration( char * decorationDisplayName,
                                update_fcn update_function,
                                draw_fcn draw_function,
                                void * data );
void pz_header_justification_helper( int lx, int rx );

header_info * find_header_item( header_info * list, char * name );
void pz_enable_widget_on_side( int side, char * name );
void pz_enable_header_decorations( char * name );
void pz_force_update_of_widget( char * name );
void pz_clear_header_lists( void );
void pz_header_widget_set_rate( int milliseconds, char * name );

void pz_header_settings_load( void ); /* force a load of all settings */

/* for transient widget groups (game stats, etc.) */
int pz_header_group_create( void );
void pz_header_group_destroy( int group );
void pz_header_group_activate( int group );
void pz_header_group_deactivate( int group );
void pz_header_group_add_widget( char * displayName,
				update_fcn update_function,
				draw_fcn draw_function,
				void * data,
				int group );

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


/** Vector text - vector.c **/
void pz_vector_string (ttk_surface srf, const char *string, int x, int y, int cw, int ch, int kern, ttk_color col);
void pz_vector_string_center (ttk_surface srf, const char *string, int x, int y, int cw, int ch, int kern, ttk_color col);
int pz_vector_width (const char *string, int cw, int ch, int kern);
#define VECTORFONT_SPECIAL_UP		(250)
#define VECTORFONT_SPECIAL_LEFT		(251)
#define VECTORFONT_SPECIAL_DOWN		(252)
#define VECTORFONT_SPECIAL_RIGHT	(253)


/** iPod stuff - ipod.c **/
void pz_ipod_set (int sid, int value);
void pz_ipod_fix_setting (int sid, int value); // sets on iPod w/o setting in config
void pz_ipod_fix_settings (PzConfig *conf);
int ipod_get_contrast(void);
int pz_ipod_get_battery_level(void);
int pz_ipod_is_charging(void);
long pz_ipod_get_hw_version(void);
void pz_ipod_go_to_diskmode(void);
void pz_ipod_reboot(void);
void pz_ipod_powerdown(void);
int pz_ipod_usb_is_connected(void);
int pz_ipod_fw_is_connected(void);


/** Fonts - fonts.c **/
void pz_load_font (ttk_font *f, const char *def, int setting, PzConfig *conf);
// menu handler, data = &font_to_set, data2 = config, cdata = setting_to_set:
TWindow *pz_select_font (ttk_menu_item *item);


/** DSP functions - oss.c **/
#define PZ_DSP_LINEIN 0
#define PZ_DSP_LINEOUT 1
#define PZ_DSP_MIC 2

#define PZ_DSP_MAX_VOL 100	/* apparently this isn't obvious */

typedef struct _dsp_st {
	int dsp;	/* /dev/dsp file descriptor */
	int mixer;	/* /dev/mixer file descriptor */
	int volume;	/* pcm volume 0 - 100 (or DSP_MAX_VOL) */
} pz_dsp_st;

/* pcm volume changer: */
int pz_dsp_vol_change(pz_dsp_st *oss, int delta);
/* and for the lazy: */
int pz_dsp_vol_up(pz_dsp_st *oss);
int pz_dsp_vol_down(pz_dsp_st *oss);
int pz_dsp_get_volume(pz_dsp_st *oss);
/* set the dsp.. umm... fmt (whatever that is), channels and rate: */
int pz_dsp_setup(pz_dsp_st *oss, int channels, int rate);
/* dsp read and (blocking) write: */
size_t pz_dsp_read(pz_dsp_st *oss, void *ptr, size_t size);
void pz_dsp_write(pz_dsp_st *oss, void *ptr, size_t size);
/* dsp open and close:  mode takes DSP_LINEIN, DSP_LINEOUT and DSP_MIC */
int pz_dsp_open(pz_dsp_st *oss, int mode);
void pz_dsp_close(pz_dsp_st *oss);


/** File browser helper functions - browser.c **/
TWindow *pz_browser_open (const char *path);
TWidget *pz_browser_get_actions (const char *path); // returns a menu widget
void pz_browser_set_handler (int (*pred)(const char *), TWindow *(*handler)());
void pz_browser_remove_handler (int (*pred)(const char *));
void pz_browser_add_action (int (*pred)(const char *), ttk_menu_item *action); // action->data will be set to file's full name
void pz_browser_remove_action (int (*pred)(const char *));


/** Text input functions - input.c **/
void pz_register_input_method (TWidget *(*handler)());
void pz_register_input_method_n (TWidget *(*handler)()); // _n = numeric
int pz_start_input();
int pz_start_input_n();
int pz_start_input_for (TWindow *win);
int pz_start_input_n_for (TWindow *win);


/** Appearance - appearance.c **/
TWindow *pz_select_color_scheme();

/** Icons - icons.c **/
extern unsigned char pz_icon_play[], pz_icon_pause[];
extern unsigned char pz_icon_usb[],  pz_icon_fw[];
extern unsigned char pz_icon_hold[], pz_icon_dot[];
extern unsigned char pz_icon_battery_horiz[], pz_icon_battery_vert[];
extern unsigned char pz_icon_charging[];


/** Other things - pz.c **/
void pz_register_global_hold_button (unsigned char ch, int ms, void (*handler)());
void pz_register_global_unused_handler (unsigned char ch, int (*handler)(int, int)); // args = (button, time)
void pz_unregister_global_hold_button (unsigned char ch);
void pz_unregister_global_unused_handler (unsigned char ch);
void pz_handled_hold (unsigned char ch); // call from the hold handler or all hell *WILL* break loose :-)
#ifndef PZ_MOD
void pz_uninit();
#endif

#define PZ_BL_OFF    -2
#define PZ_BL_RESET  -1
#define PZ_BL_ON      0


/** Priority priority.c **/
#define PZ_PRIORITY_IDLE   1
#define PZ_PRIORITY_ACTIVE 3
#define PZ_PRIORITY_VITAL  7

void pz_set_priority(unsigned char st);
unsigned char pz_get_priority();
void pz_register_idle(int (*func)(void *), void *data);
void pz_unregister_idle(int (*func)(void *));
void pz_reset_idle_timer();


/** Locale **/
#if defined( IPOD ) || defined( __linux__ ) /* hopefully fix for OS X */
  #define LOCALE
#endif
#ifdef IPOD
  #ifndef __UCLIBC_HAS_LOCALE__
    // insert warning here
    #undef LOCALE
  #endif
  #define LOCALEDIR "/usr/share/locale"
#else
  #define LOCALEDIR "./locale"
#endif
#ifdef LOCALE
  #define _(str) gettext(str)
  #include "libintl.h"
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

/* PLAYLIST SETTINGS 20 - 29, with their associated values */
 
#define SHUFFLE         (20)
#define SHUFFLE_OFF     (0)
#define SHUFFLE_SONGS   (1)
#define SHUFFLE_ALBUMS	(2)	/* not implemented yet. */

#define REPEAT          (21)
#define REPEAT_OFF      (0)
#define REPEAT_ONE      (1)
#define REPEAT_ALL      (2)

/* OTHER SETTINGS 30 - 99 */

#define LANGUAGE        (30)
#define USB_FW_POPUP    (33)
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
#define BATTERY_DIGITS	(46)	/*  -- DEPRECATED --  */
#define DISPLAY_LOAD	(47)	/*  -- DEPRECATED --  */
#define TEXT_FONT	(48)
#define SLIDE_TRANSIT	(49)
#define MENU_FONT	(50)
#define BATTERY_UPDATE	(51)	/*  -- DEPRECATED --  */
#define TITLE_JUSTIFY	(52)	/* appearance */

#define VERBOSITY	(53)	/* startup verbosity */
#define VERBOSITY_ALL		(0)	/* info, warnings and errors */
#define VERBOSITY_WARNINGS	(1)	/* only warnings and errors */
#define VERBOSITY_ERRORS	(2) 	/* only errors */

/****  modular header widget stuff  *** */
#define HEADER_METHOD_L   (54)	 /*  display method - LEFT  */
#define HEADER_CYC_RATE_L (55)	 /*  cycle rate - LEFT  */
/* 56  -- DEPRECATED --  */
#define HEADER_UPD_CYCD_L (57)   /*  update cycled out widgets? - LEFT  */

#define HEADER_METHOD_R   (58)	 /*  display method - RIGHT */
#define HEADER_CYC_RATE_R (59)	 /*  cycle rate - RIGHT */
/* 60  -- DEPRECATED --  */
#define HEADER_UPD_CYCD_R (61)   /*  update cycled out widgets? - RIGHT */

#define DECORATION_RATE   (62)   /* update rate for decorations */
#define HEADER_WIDGETS    (63)	 /* settings for header widgets */

/**** ADVANCED SETTINGS ****/
#define MODULE_TESTING    (70)   /* don't warn for modules in beta */
#define ENABLE_VTSWITCH   (71)   /* Enable VT switching. */
#define ENABLE_WINDOWMGMT (72)   /* Enable window management. */

#define 	BATTERY_UPDATE_OFF (5)

#if defined(PZ_COMPAT) && !defined(LEGACY_DOT_C)
#define pz_new_window(x,y,w,h,d,k) pz_old_window(x,y,w,h,d,k) /* hopefully no conflict with new_win(title,XYWH,x,y,w,h) */
#define pz_close_window(w) pz_old_close_window(w)
#endif

/*********** Appearance legacy stuff *************/
#ifdef PZ_COMPAT
#define CS_BG		"window.bg"	/* standard background */
#define CS_FG		"window.fg"	/* standard foreground (text) */

#define CS_SELBG	"menu.selbg"	/* selected background */
#define CS_SELBGBDR	"menu.selborder" /* selected background border */
#define CS_SELFG	"menu.selfg"	/* selected foreground (text) */

#define CS_ARROW0	"menu.selfg"	/* arrow > color 0 */
#define CS_ARROW1	"menu.selbg"	/* arrow > color 1 */
#define CS_ARROW2	"menu.selfg"	/* arrow > color 2 */
#define CS_ARROW3	"menu.selbg"	/* arrow > color 3*/

#define CS_TITLEBG	"header.bg"	/* titlebar background */
#define CS_TITLEFG	"header.fg"	/* titlebar foreground */
#define CS_TITLEACC	"notanyyet"	/* titlebar accent */
#define CS_TITLELINE	"header.line"	/* titlebar separator line */

/* UI Widgets */
#define CS_SCRLBDR	"scroll.border"	/* scrollbar border */
#define CS_SCRLCTNR	"scroll.bg"	/* scrollbar container */
#define CS_SCRLKNOB	"scroll.bar"	/* scrollbar knob */

#define CS_SLDRBDR	"slider.border"	/* slider border */
#define CS_SLDRCTNR	"slider.bg"	/* slider container */
#define CS_SLDRFILL	"slider.full"	/* slider filled */

/* podzilla customized widgets */
#define CS_BATTBDR	"battery.border"	/* battery icon border */
#define CS_BATTCTNR	"battery.bg"	/* battery icon container */
#define CS_BATTFILL	"battery.fill.normal"	/* battery icon filled (normal) */
#define CS_BATTLOW	"battery.fill.low"	/* battery icon filled (low) */
#define CS_BATTCHRG	"battery.fill.charge"	/* battery is charging */

#define CS_HOLDBDR	"lock.border"	/* hold icon border */
#define CS_HOLDFILL	"lock.fill"	/* hold icon fill */

/* error/warning messages */
#define CS_MESSAGEFG	"dialog.fg"	/* pz_message forground text */
#define CS_MESSAGELINE	"dialog.line"	/* highlight line */
#define CS_MESSAGEBG	"dialog.bg"	/* pz_message background */
#define CS_ERRORBG	"error.bg"	/* pz_error background */

/* load average display */
#define CS_LOADBG	"loadavg.bg"	/* load average container */
#define CS_LOADFG	"loadavg.fg"	/* load average meter */

t_GR_COLOR appearance_get_color (const char *prop);
#endif

#ifdef __cplusplus
}
#endif

#endif
