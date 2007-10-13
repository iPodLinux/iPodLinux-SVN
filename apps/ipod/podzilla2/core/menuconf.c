/*
 * Copyright (C) 2006 Rebecca G. Bettencourt
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

#include "pz.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define use_str_switch      char * _str_switch_var_
#define str_switch(v)       _str_switch_var_ = (v); if (0)
#define str_case(s)         } else if (!strcmp(_str_switch_var_, (s))) {
#define str_break           while(0)
#define str_default         } else {

static TWindow * pz_menuconf_browser_open(ttk_menu_item * item)
{
	return pz_browser_open(item->data);
}

#define CHKPWR(a)    ( (!strcmp((a),"/Power")) || (!strncmp((a),"/Power/",7)) )

int pz_menuconf_runargs(int argc, char * argv[])
{
	use_str_switch;
	str_switch(argv[0]) {
	str_case("rm")
		if (argc < 2) {
			pz_error("Too few arguments to %s in menu.conf.", argv[0]);
		} else if (CHKPWR(argv[1])) {
			pz_error("Cannot modify the Power menu from menu.conf.");
		} else {
			pz_menu_remove(argv[1]);
		}
	str_case("hide")
		if (argc < 2) {
			pz_error("Too few arguments to %s in menu.conf.", argv[0]);
		} else if (CHKPWR(argv[1])) {
			pz_error("Cannot modify the Power menu from menu.conf.");
		} else {
			pz_menu_remove(argv[1]);
		}
	str_case("show")
		if (argc < 2) {
			pz_error("Too few arguments to %s in menu.conf.", argv[0]);
		} else if (CHKPWR(argv[1])) {
			pz_error("Cannot modify the Power menu from menu.conf.");
		} else {
			ttk_menu_item * item = pz_get_menu_item(argv[1]);
			if (item) item->visible = 0;
		}
	str_case("mv")
		if (argc < 3) {
			pz_error("Too few arguments to %s in menu.conf.", argv[0]);
		} else if (CHKPWR(argv[1]) || CHKPWR(argv[2])) {
			pz_error("Cannot modify the Power menu from menu.conf.");
		} else {
			ttk_menu_item * item = pz_get_menu_item(argv[1]);
			if (item) {
				ttk_menu_item * nitem;
				pz_menu_add_stub(argv[2]);
				nitem = pz_get_menu_item(argv[2]);
				if (!nitem) {
					pz_error("Could not create menu item %s in menu.conf.", argv[2]);
				} else {
					nitem->makesub = item->makesub;
					nitem->sub = item->sub;
					nitem->flags = item->flags;
					nitem->data = item->data;
					nitem->choices = item->choices;
					nitem->cdata = item->cdata;
					nitem->choicechanged = item->choicechanged;
					nitem->choiceget = item->choiceget;
					nitem->choice = item->choice;
					nitem->visible = item->visible;
					nitem->free_data = item->free_data;
					item->free_data = 0;
				}
				pz_menu_remove(argv[1]);
			}
		}
	str_case("cp")
		if (argc < 3) {
			pz_error("Too few arguments to %s in menu.conf.", argv[0]);
		} else if (CHKPWR(argv[2])) {
			pz_error("Cannot modify the Power menu from menu.conf.");
		} else {
			ttk_menu_item * item = pz_get_menu_item(argv[1]);
			if (item) {
				ttk_menu_item * nitem;
				pz_menu_add_stub(argv[2]);
				nitem = pz_get_menu_item(argv[2]);
				if (!nitem) {
					pz_error("Could not create menu item %s in menu.conf.", argv[2]);
				} else {
					nitem->makesub = item->makesub;
					nitem->sub = item->sub;
					nitem->flags = item->flags;
					nitem->data = item->data;
					nitem->choices = item->choices;
					nitem->cdata = item->cdata;
					nitem->choicechanged = item->choicechanged;
					nitem->choiceget = item->choiceget;
					nitem->choice = item->choice;
					nitem->visible = item->visible;
					nitem->free_data = 0;
				}
			}
		}
	str_case("ln")
		if (argc < 3) {
			pz_error("Too few arguments to %s in menu.conf.", argv[0]);
		} else if (CHKPWR(argv[2])) {
			pz_error("Cannot modify the Power menu from menu.conf.");
		} else {
			ttk_menu_item * nitem;
			pz_menu_add_ttkh(argv[2], pz_menuconf_browser_open, 0);
			nitem = pz_get_menu_item(argv[2]);
			if (!nitem) {
				pz_error("Could not create menu item %s in menu.conf.", argv[2]);
			} else {
				nitem->data = strdup(argv[1]);
				nitem->free_data = 1;
			}
		}
	str_case("sort")
		if (argc < 2) {
			pz_error("Too few arguments to %s in menu.conf.", argv[0]);
		} else if (CHKPWR(argv[1])) {
			pz_error("Cannot modify the Power menu from menu.conf.");
		} else {
			pz_menu_sort(argv[1]);
		}
	str_case("title")
		if (argc < 2) {
			pz_error("Too few arguments to %s in menu.conf.", argv[0]);
		} else if (ttk_windows && ttk_windows->w) {
			ttk_window_title(ttk_windows->w, argv[1]);
		}
	str_default
		pz_error("Invalid command %s in menu.conf.", argv[0]);
	}
	return 0;
}

int pz_menuconf_runstr(char * str)
{
	int argc;
	char * argv[64];
	char * s = strdup(str);
	char * argstart = s;
	char * argend = s;
	int inquote = 0;
	int ret = 0;
	
	/* find first instruction */
	if ((*argstart) == '#') {
		while (!( (*argstart == 10) || (*argstart == 13) || (*argstart == 0) )) argstart++;
	}
	while ( (*argstart == ' ') || (*argstart == 10) || (*argstart == 13) ) {
		argstart++;
		if ((*argstart) == '#') {
			while (!( (*argstart == 10) || (*argstart == 13) || (*argstart == 0) )) argstart++;
		}
	}
	argend = argstart;
	/* start processing */
	while ((*argstart) && (*argend)) {
		/* reset argv */
		argc = 0;
		inquote = 0;
		ret = 0;
		/* parse arguments */
		while (1) {
			int a = *argend;
			if (a == 0) {
				argv[argc++] = argstart;
				argstart = argend;
				break;
			} else if (a == 10 || a == 13) {
				*(argend++) = 0;
				argv[argc++] = argstart;
				argstart = argend;
				break;
			} else if (a == '\"') {
				if (inquote) {
					*(argend++) = 0;
					argv[argc++] = argstart;
					inquote = 0;
					while ((*argend) == ' ') argend++;
					argstart = argend;
					if ((*argend) == 0 || (*argend) == 10 || (*argend) == 13) break;
				} else {
					argstart = ++argend;
					inquote = 1;
				}
			} else if ((a == ' ') && (!inquote)) {
				*(argend++) = 0;
				argv[argc++] = argstart;
				while ((*argend) == ' ') argend++;
				argstart = argend;
				if ((*argend) == 0 || (*argend) == 10 || (*argend) == 13) break;
			} else {
				argend++;
			}
		}
		/* execute the instruction */
		if (argc && argv[0] && argv[0][0]) {
			ret = pz_menuconf_runargs(argc, argv);
			if (ret) break;
		}
		/* find next instruction */
		if ((*argstart) == '#') {
			while (!( (*argstart == 10) || (*argstart == 13) || (*argstart == 0) )) argstart++;
		}
		while ( (*argstart == ' ') || (*argstart == 10) || (*argstart == 13) ) {
			argstart++;
			if ((*argstart) == '#') {
				while (!( (*argstart == 10) || (*argstart == 13) || (*argstart == 0) )) argstart++;
			}
		}
		argend = argstart;
	}
	free(s);
	return ret;
}

int pz_menuconf_runfile(char * fn)
{
	FILE * fp = fopen(fn, "r");
	int ret = -1;
	if (fp) {
		char * buf;
		int len;
		fseek(fp, 0, SEEK_END);
		len = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		buf = malloc(len+1);
		if (!buf) {
			pz_error("Ran out of memory to run the script %s.", fn);
		} else {
			fread(buf, 1, len, fp);
			buf[len] = 0;
			ret = pz_menuconf_runstr(buf);
			free(buf);
		}
		fclose(fp);
	}
	return ret;
}

#ifdef IPOD
#define MENUCONF_CONFIG_FILE "/etc/podzilla/menu.conf"
#else
#define MENUCONF_CONFIG_FILE "config/menu.conf"
#endif

void pz_menuconf_init()
{
	if (!access(MENUCONF_CONFIG_FILE, R_OK)) {
		pz_menuconf_runfile(MENUCONF_CONFIG_FILE);
	}
}
