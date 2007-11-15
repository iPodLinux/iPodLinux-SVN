/*
 * Copyright (c) 2005 Courtney Cavin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "pz.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 512
#endif

typedef struct _Entry {
	char *name;
	mode_t mode;
} Entry;

static PzModule *module;

static TWindow *open_directory(const char *filename);

static TWindow *previous_directory(ttk_menu_item *item)
{
	fchdir(*(int *)item->menu->data2); /* last working directory */
	close(*(int *)item->menu->data2);
	free(item->menu->data2);
	return TTK_MENU_UPONE;
}

static TWindow *handle_file(ttk_menu_item *item)
{
	char path[MAXPATHLEN];
	snprintf(path, MAXPATHLEN-1, "%s/%s",
		getcwd(NULL, MAXPATHLEN), item->name);
	return pz_browser_open( path );
}

static TWindow *list_actions(const char *lpath)
{
	TWindow *ret;
	TWidget *menu;
	char path[MAXPATHLEN];

	snprintf( path, MAXPATHLEN, "%s/%s", getcwd(path, MAXPATHLEN), lpath);
	ret = pz_new_window(path, PZ_WINDOW_NORMAL);
	menu = pz_browser_get_actions(path);
	ttk_add_widget(ret, menu);
	return pz_finish_window(ret);
}

static int is_dir(const char *filename)
{
	struct stat st;
	stat(filename, &st);
	return S_ISDIR(st.st_mode);
}

static int entry_cmp(const void *x, const void *y)
{
	Entry *a = (Entry *)(*(ttk_menu_item **)x)->data;
	Entry *b = (Entry *)(*(ttk_menu_item **)y)->data;

	if (a == NULL) return -1;
	if (b == NULL) return 1;
	if (S_ISDIR(a->mode) != S_ISDIR(b->mode)) {
		return S_ISDIR(a->mode) ? -1 : 1;
	}

	return strcasecmp(a->name, b->name);
}

static int button_handler(TWidget *this, int button, int time)
{
	if (button == TTK_BUTTON_MENU) {
		fchdir(*(int *)this->data2); /* last working directory */
		close(*(int *)this->data2);
		free(this->data2);
	}
	return ttk_menu_button(this, button, time);
}

static int held_handler(TWidget *this, int button)
{
	ttk_menu_item *item;

	if (button == TTK_BUTTON_ACTION) {
		item = ttk_menu_get_selected_item(this);
		if (item->data) {
			ttk_show_window(list_actions(((Entry *)item
							->data)->name));
		}
	}
	return 0;
}

static int check_hidden(ttk_menu_item *item)
{
	if (!pz_get_int_setting(pz_global_config, BROWSER_HIDDEN)) {
		if (item->name[0] == '.') {
			return 0;
		}
	}
	return 1;
}

static TWidget *read_directory(const char *dirname)
{
	TWidget *ret;
	DIR *dir;
	struct stat st;
	struct dirent *subdir;

	ret = ttk_new_menu_widget(NULL, ttk_menufont, ttk_screen->w -
			ttk_screen->wx, ttk_screen->h - ttk_screen->wy);
	ttk_menu_set_i18nable(ret, 0);
	dir = opendir(dirname);

	while ((subdir = readdir(dir))) {
		ttk_menu_item *item;
		Entry *entry;
		if (strncmp(subdir->d_name,".", strlen(subdir->d_name)) == 0) {
			continue;
		}
		
		stat(subdir->d_name, &st);

		item = (ttk_menu_item *)calloc(1, sizeof(ttk_menu_item));
		if (strncmp(subdir->d_name,"..",strlen(subdir->d_name)) == 0) {
			item->name = _(".. [Parent Directory]");
			item->makesub = previous_directory;
		}
		else {
			entry = (Entry *)malloc(sizeof(Entry));
			entry->name = (char *)strdup(subdir->d_name);
			entry->mode = st.st_mode;

			item->name = (char *)entry->name;
			item->visible = check_hidden;
			item->data = (void *)entry;
			item->free_data = 1;
			item->free_name = 1;
			item->makesub = handle_file;
		}

		if (S_ISDIR(st.st_mode)) {
			item->flags = TTK_MENU_ICON_SUB;
		}
		else if (st.st_mode & S_IXUSR) {
			item->flags = TTK_MENU_ICON_EXE;
		}
		ttk_menu_append(ret, item);
	}
	ttk_menu_sort_my_way(ret, entry_cmp);
	closedir(dir);

	ret->button = button_handler;
	ret->held = held_handler;
	ret->holdtime = 500; /* ms */
	return ret;
}

static TWindow *open_directory(const char *filename)
{
	TWindow *ret;
	TWidget *menu;
	int lwd;

	lwd = open(".", O_RDONLY);

	chdir(filename);
	if (pz_get_int_setting(pz_global_config, BROWSER_PATH)) {
		char wd[MAXPATHLEN], *p;
		p = getcwd(wd, MAXPATHLEN);
		while (ttk_text_width(ttk_menufont, p) > ttk_screen->w - 60)
			memcpy((void *)++p, "...", 3);
		ret = pz_new_window(p, PZ_WINDOW_NORMAL);
	}
	else
		ret = pz_new_window(_("File Browser"), PZ_WINDOW_NORMAL);

	menu = read_directory("./");
	menu->data2 = malloc(sizeof(int));
	*(int *)menu->data2 = lwd;
	ttk_add_widget(ret, menu);

	ret->data = 0x12345678;
	return pz_finish_window(ret);
}

PzWindow *new_browser_window()
{
	return open_directory("/");
}

static void cleanup_browser()
{
	pz_browser_remove_handler(is_dir);
}

void init_browser()
{
	module = pz_register_module("browser", cleanup_browser);
	pz_browser_set_handler(is_dir, open_directory);
	pz_menu_add_action_group("/File Browser", "Utility", new_browser_window);
}

PZ_MOD_INIT(init_browser)
