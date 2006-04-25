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

static int track_cmp(const void *x, const void *y) 
{
	int a = (int)(*(ttk_menu_item **)x)->cdata;
	int b = (int)(*(ttk_menu_item **)y)->cdata;
	return (a - b);
}

static void queue_song(ttk_menu_item *item)
{
	mpd_InfoEntity entity;

	if (mpdc_tickle() < 0)
		return;
	mpd_sendSearchCommand(mpdz, (long)item->data, item->name);

	if (mpdz->error) {
		mpdc_tickle();
		return;
	}

	while ((mpd_getNextInfoEntity_st(&entity, mpdz))) {
		int found = 1;
		mpd_Song *song = entity.info.song;
		if (entity.type != MPD_INFO_ENTITY_TYPE_SONG) {
				continue;
		}
		found &= (!current_song.artist || (song->artist &&
				strcmp(current_song.artist,song->artist)==0));
		found &= (!current_song.album || (song->album &&
				strcmp(current_song.album, song->album) == 0));
		found &= ((song->title ? (strcmp(item->name, song->title)==0) :
				0) || strcmp(item->name, song->file)==0);
		if (found && song->file) {
			mpd_finishCommand(mpdz);
			mpd_sendAddCommand(mpdz, song->file);
			break;
		}
	}
	mpd_finishCommand(mpdz);
}

static int song_held_handler(TWidget *this, int button)
{
	ttk_menu_item *item;
	if (button == TTK_BUTTON_ACTION) {
		item = ttk_menu_get_selected_item(this);
		ttk_menu_flash(item, 1);
		queue_song(item);
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
			int found = 1;
			mpd_Song *song = entity.info.song;

			if (entity.type != MPD_INFO_ENTITY_TYPE_SONG)
				continue;
			found &= (!current_song.artist || (song->artist &&
				strcmp(current_song.artist,song->artist)==0));
			found &= (!current_song.album || (song->album &&
				strcmp(current_song.album, song->album) == 0));
			found &= ((song->title ?
				(strcmp(item->name, song->title)==0) : 0)
					|| strcmp(item->name, song->file)==0);
			if (found && song->file) {
				mpd_finishCommand(mpdz);
				return mpd_currently_playing();
			}
		}
		mpd_finishCommand(mpdz);
	}

	mpd_sendClearCommand(mpdz);
	mpd_finishCommand(mpdz);

	for (i = 0; (sitem = ttk_menu_get_item(item->menu, i)) != NULL; i++) {
		queue_song(sitem);
		if (item == sitem)
			num = i;
	}

	mpd_sendPlayCommand(mpdz, num);
	mpd_finishCommand(mpdz);

	return mpd_currently_playing();
}
TWidget *populate_songs(char *search)
{
	TWidget *ret;
	mpd_InfoEntity entity;
	char by_album = 0;

	if (mpdc_tickle() < 0)
		return NULL;

	if (search == NULL || (!search[0] && !current_song.artist)) {
		mpd_sendListallInfoCommand(mpdz, "");
	}
	else if (!search[0] && current_song.artist) {
		mpd_sendFindCommand(mpdz, MPD_TABLE_ARTIST,current_song.artist);
	}
	else {
		mpd_sendFindArtistAlbum(mpdz, current_song.artist,
				current_song.album);
		by_album = 1;
	}

	if (mpdz->error) {
		mpdc_tickle();
		return NULL;
	}
	ret = ttk_new_menu_widget(NULL, ttk_menufont,
			ttk_screen->w -	ttk_screen->wx,
			ttk_screen->h - ttk_screen->wy);
	ttk_menu_set_i18nable(ret, 0);
	while ((mpd_getNextInfoEntity_st(&entity, mpdz))) {
		mpd_Song *song = entity.info.song;
		ttk_menu_item *item;
		if (entity.type != MPD_INFO_ENTITY_TYPE_SONG) {
			continue;
		}
		item = (ttk_menu_item *)calloc(1, sizeof(ttk_menu_item));
		item->name = (char *)strdup(song->title?song->title:song->file);
		item->free_name = 1;
		item->makesub = open_song;
		item->cdata = song->track ? atoi(song->track) : 0;
		item->data = (void *)(long)(song->title ?
				MPD_TABLE_TITLE : MPD_TABLE_FILENAME);
		ttk_menu_append(ret, item);
	}
	mpd_finishCommand(mpdz);
	ret->held = song_held_handler;
	ret->holdtime = 500; /* ms */
	if (by_album)
		ttk_menu_sort_my_way(ret, track_cmp);
	else
		ttk_menu_sort(ret);
	if (mpdz->error) {
		mpdc_tickle();
	}
	return ret;
}
PzWindow *new_song_menu()
{
	PzWindow *ret;
	clear_current_song();
	ret = pz_new_window(_("Songs"), PZ_WINDOW_NORMAL);
	ttk_add_widget(ret, populate_songs(NULL));
	return pz_finish_window(ret);
}


