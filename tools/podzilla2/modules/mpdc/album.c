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

#include <string.h>
#include "mpdc.h"

extern mpd_Song current_song;
extern void clear_current_song();
extern void mpdc_queue_data(int, void *, void (*action)(void *, void *),
		int (*cmp)(void *, void *));

typedef struct _ASong {
	char *name;
	int track;
} ASong;

static void add_track(void *nil, void *song)
{
	mpd_sendAddCommand(mpdz, ((ASong *)song)->name);	
	mpd_finishCommand(mpdz);

	free(((ASong *)song)->name);
	free((ASong *)song);
}

static int track_cmp(void *x, void *y)
{
	int a = (int)((ASong *)x)->track;
	int b = (int)((ASong *)y)->track;
	return (a - b);
}

void queue_album(ttk_menu_item *item)
{
	mpd_InfoEntity entity;
	
	if (mpdc_tickle() < 0)
		return;
	mpd_sendFindArtistAlbum(mpdz, current_song.artist, item->name);
	if (mpdz->error) {
		mpdc_tickle();
		return;
	}
	
	while ((mpd_getNextInfoEntity_st(&entity, mpdz))) {
		mpd_Song *song = entity.info.song;
		ASong *asong;
		if (entity.type != MPD_INFO_ENTITY_TYPE_SONG) {
			continue;
		}
		asong = (ASong *)malloc(sizeof(ASong));
		asong->name = strdup(song->file);
		asong->track = song->track ? atoi(song->track) : 0;
		mpdc_queue_data(0, asong, NULL, NULL);
	}
	mpdc_queue_data(0, NULL, NULL, track_cmp);
	mpdc_queue_data(0, NULL, add_track, NULL);
	mpd_finishCommand(mpdz);
}

static int album_held_handler(TWidget *this, int button)
{
	ttk_menu_item *item;
	if (button == TTK_BUTTON_ACTION) {
		item = ttk_menu_get_selected_item(this);
		if (((char *)item->data)[0]) {
			ttk_menu_flash(item, 1);
			queue_album(item);
		}
	}
	return 0;
}

static TWindow *open_album(ttk_menu_item *item)
{
	TWindow *ret;
	if (mpdc_tickle() < 0)
		return TTK_MENU_DONOTHING;
	current_song.album = ((char *)item->data)[0]? (char *)item->data: NULL;
	ret = pz_new_window(_("Songs"), PZ_WINDOW_NORMAL);
	ttk_add_widget(ret, populate_songs((char *)item->data));
	return pz_finish_window(ret);
}

TWidget *populate_albums(char *search)
{
	TWidget *ret;
	char *album;
	if (mpdc_tickle() < 0)
		return NULL;
	mpd_sendListCommand(mpdz, MPD_TABLE_ALBUM, search);
	if (mpdz->error) {
		mpdc_tickle();
		return NULL;
	}
	ret = ttk_new_menu_widget(NULL, ttk_menufont,
			ttk_screen->w -	ttk_screen->wx,
			ttk_screen->h - ttk_screen->wy);
	ttk_menu_set_i18nable(ret, 0);
	while ((album = mpd_getNextAlbum(mpdz))) {
		ttk_menu_item *item;
		item = (ttk_menu_item *)calloc(1, sizeof(ttk_menu_item));
		item->name = album;
		item->data = (void *)item->name;
		item->free_name = 1;
		item->makesub = open_album;
		item->flags = TTK_MENU_ICON_SUB;
		ttk_menu_append(ret, item);
	}
	mpd_finishCommand(mpdz);
	ret->held = album_held_handler;
	ret->holdtime = 500; /* ms */

	{
		ttk_menu_item *item;
		item = (ttk_menu_item *)calloc(1, sizeof(ttk_menu_item));
		item->name = _("All Albums");
		item->data = "";
		item->makesub = open_album;
		item->flags = TTK_MENU_ICON_SUB;
		ttk_menu_insert(ret, item, 0);
	}
	return ret;
}
PzWindow *new_album_menu()
{
	PzWindow *ret;
	clear_current_song();
	ret = pz_new_window(_("Albums"), PZ_WINDOW_NORMAL);
	ttk_add_widget(ret, populate_albums(""));
	return pz_finish_window(ret);
}

