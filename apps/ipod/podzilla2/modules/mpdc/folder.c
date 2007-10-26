/*
 * Copyright (c) 2005, 2007 Courtney Cavin
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

#include <string.h>
#include <libgen.h>
#include "mpdc.h"

extern mpd_Song current_song;
extern void clear_current_song();

static int item_cmp(const void *x, const void *y) 
{
	ttk_menu_item *a = (ttk_menu_item *)x;
	ttk_menu_item *b = (ttk_menu_item *)y;
	if  ((a->cdata - b->cdata) != 0)
		return a->cdata - b->cdata;
	return strcmp(a->name, b->name);
}

static void queue_song(const char *path)
{
	if (mpdc_tickle() < 0)
		return;

	mpd_sendAddCommand(mpdz, path);
	mpd_finishCommand(mpdz);

	if (mpdz->error)
		mpdc_tickle();
}

static void queue_song_item(ttk_menu_item *item)
{
	queue_song(item->data);
}

static void queue_dir(const char *path)
{
	mpd_InfoEntity entity;
	struct ll {
		char *path;
		struct ll *next;
	} *head = 0;

	if (mpdc_tickle() < 0)
		return;

	mpd_sendListallInfoCommand(mpdz, path);

	if (mpdz->error) {
		mpdc_tickle();
		return;
	}
	while ((mpd_getNextInfoEntity_st(&entity, mpdz))) {
		if (entity.type == MPD_INFO_ENTITY_TYPE_SONG) {
			struct ll *it, *ll = calloc(1, sizeof(struct ll));
			mpd_Song *song = entity.info.song;
			if ((it = head)) {
				while (it->next)
					it = it->next;
				it->next = ll;
			} else head = ll;
			ll->path = strdup(song->file);
		}
	}
	mpd_finishCommand(mpdz);
	if (mpdz->error)
		mpdc_tickle();

	while (head) {
		struct ll *o;
		queue_song(head->path);
		free(head->path);
		o = head->next;
		free(head);
		head = o;
	}
}

static void queue_dir_item(ttk_menu_item *item)
{
	queue_dir((const char *)item->data);
}

static void queue_item(ttk_menu_item *item)
{
	if (item->cdata == MPD_INFO_ENTITY_TYPE_SONG)
		queue_song_item(item);
	else if (item->cdata == MPD_INFO_ENTITY_TYPE_DIRECTORY)
		queue_dir_item(item);
}

static int item_held_handler(TWidget *this, int button)
{
	ttk_menu_item *item;
	if (button == TTK_BUTTON_ACTION) {
		item = ttk_menu_get_selected_item(this);
		ttk_menu_flash(item, 1);
		queue_item(item);
	}
	return 0;
}

static TWindow *open_song(ttk_menu_item *item)
{
	int i, num = 0;
	ttk_menu_item *sitem;
	int state;

	if (mpdc_tickle() < 0)
		return TTK_MENU_DONOTHING;

	state = mpdc_status(mpdz);
	if (state == MPD_STATUS_STATE_PLAY || state == MPD_STATUS_STATE_PAUSE) {
		mpd_InfoEntity entity;

		mpd_sendCurrentSongCommand(mpdz);
		if (mpdz->error) {
			mpdc_tickle();
			return TTK_MENU_DONOTHING;
		}
		while ((mpd_getNextInfoEntity_st(&entity, mpdz))) {
			mpd_Song *song = entity.info.song;
			if (entity.type != MPD_INFO_ENTITY_TYPE_SONG)
				continue;
			if (!strcmp(song->file, (const char *)item->data)) {
				mpd_finishCommand(mpdz);
				return mpd_currently_playing();
			}
		}
		mpd_finishCommand(mpdz);
	}

	mpd_sendClearCommand(mpdz);
	mpd_finishCommand(mpdz);

	for (i = 0; (sitem = ttk_menu_get_item(item->menu, i)) != NULL; i++) {
		queue_song_item(sitem);
		if (item == sitem)
			num = i;
	}

	mpd_sendPlayCommand(mpdz, num);
	mpd_finishCommand(mpdz);

	return mpd_currently_playing();
}

PzWindow *new_folder_menu_path(const char *path);

static TWindow *open_dir(ttk_menu_item *item)
{
	return new_folder_menu_path(item->data);
}

TWidget *populate_folder(const char *search)
{
	TWidget *ret;
	mpd_InfoEntity entity;

	if (mpdc_tickle() < 0)
		return NULL;

	mpd_sendLsInfoCommand(mpdz, search);

	if (mpdz->error) {
		mpdc_tickle();
		return NULL;
	}
	ret = ttk_new_menu_widget(NULL, ttk_menufont,
			ttk_screen->w -	ttk_screen->wx,
			ttk_screen->h - ttk_screen->wy);
	ttk_menu_set_i18nable(ret, 0);
	while ((mpd_getNextInfoEntity_st(&entity, mpdz))) {
		ttk_menu_item *item;
		if (entity.type == MPD_INFO_ENTITY_TYPE_DIRECTORY) {
			mpd_Directory *dir = entity.info.directory;
			item = calloc(1, sizeof(ttk_menu_item));
			item->data = strdup(dir->path);
			item->name = strdup(basename(dir->path));
			item->flags = TTK_MENU_ICON_SUB;
			item->makesub = open_dir;
			item->cdata = entity.type;
			item->free_data = 1;
			item->free_name = 1;
			ttk_menu_append(ret, item);
		}
		else if (entity.type == MPD_INFO_ENTITY_TYPE_SONG) {
			mpd_Song *song = entity.info.song;
			item = calloc(1, sizeof(ttk_menu_item));
			item->name = (char *)strdup(song->title ? song->title :
							basename(song->file));
			item->free_name = 1;
			item->makesub = open_song;
			item->cdata = entity.type;
			item->data = strdup(song->file);
			ttk_menu_append(ret, item);
		}
	}
	mpd_finishCommand(mpdz);
	ret->held = item_held_handler;
	ret->holdtime = 500; /* ms */
	ttk_menu_sort_my_way(ret, item_cmp);
	if (mpdz->error)
		mpdc_tickle();
	return ret;
}

PzWindow *new_folder_menu_path(const char *path)
{
	PzWindow *ret;
	clear_current_song();
	ret = pz_new_window(_("Folder"), PZ_WINDOW_NORMAL);
	ttk_add_widget(ret, populate_folder(path));
	return pz_finish_window(ret);
}


PzWindow *new_folder_menu()
{
	return new_folder_menu_path("/");
}
