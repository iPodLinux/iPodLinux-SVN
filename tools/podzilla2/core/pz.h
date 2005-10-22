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

/** Compat defs **/
#ifdef PZ_COMPAT
#define HEADER_TOPLINE 19
#define KEY_CLICK 1
#define KEY_UNUSED 2
#define EVENT_UNUSED 2
#define KEY_QUIT 4
extern t_GR_SCREEN_INFO screen_info;
extern long hw_version;
t_GR_WINDOW_ID pz_old_window (int x, int y, int w, int h,
			      void (*do_draw)(void), int (*keystroke)(t_GR_EVENT *event));
void pz_old_event_handler (t_GR_EVENT *ev);
#endif

/** Module and .POD/.PCD functions - module.c **/
/* called from module */
PzModule *pz_register_module (const char *name, void (*cleanup)());
const char *pz_module_get_path (PzModule *mod, const char *filename);
#ifndef PZ_MOD
/* called from core */
PzModule *pz_load_module (const char *name);
void pz_unload_module (PzModule *mod);
void *pz_module_dlsym (PzModule *mod, const char *sym);
int pz_module_check_signature (PzModule *mod);
#endif

/** Configuration stuff - config.c **/
extern PzConfig *pz_global_config;

PzConfig *pz_load_config (const char *filename);
void pz_save_config (PzConfig *conf);
void pz_blast_config (PzConfig *conf);
void pz_free_config (PzConfig *conf);

typedef struct _pz_ConfItem 
{
    unsigned int sid;

#define PZ_CONF_INT     1
#define PZ_CONF_STRING  2
#define PZ_CONF_FLOAT   3
#define PZ_CONF_ILIST   4
#define PZ_CONF_SLIST   5
#define PZ_CONF_BLOB    255
    int type;

    union 
    {
	int ival;
	const char *strval;
	double fval;
	int *ivals; int nivals;
	const char **strvals; int nstrvals;
	void *blobval; int bloblen;
    };
} PzConfItem;

#define PZ_SID_FORMAT  0xe0000000

PzConfItem *pz_get_setting (PzConfig *conf, unsigned int sid);
void pz_unset_setting (PzConfig *conf, unsigned int sid);

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
void pz_reallyset_int_setting (PzConfig *conf, unsigned int sid, int val);
void pz_reallyset_string_setting (PzConfig *conf, unsigned int sid, const char *val);
#endif


/** Menu stuff - menu.c **/
void pz_menu_add_action (const char *menupath, PzWindow *(*handler)());
void pz_menu_add_option (const char *menupath, const char **choices);
int pz_menu_get_option (const char *menupath);
void pz_menu_set_option (const char *menupath, int choice);
void pz_menu_add_setting (const char *menupath, unsigned int sid, PzConfig *conf, const char **choices);
void pz_menu_sort (const char *menupath);
void pz_menu_remove (const char *menupath);


/** Widget/window stuff - gui.c **/
typedef TWindow PzWindow;
typedef TWidget PzWidget;

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

void pz_hide_window (PzWindow *win);
void pz_close_window (PzWindow *win);
void pz_show_window (PzWindow *win);


/** Secrets - secrets.c **/
#ifndef PZ_MOD
void pz_init_secrets();
#endif
int pz_has_secret (const char *key);


#endif
