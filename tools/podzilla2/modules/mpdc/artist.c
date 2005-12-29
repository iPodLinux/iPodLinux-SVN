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
extern void queue_album(ttk_menu_item *item);
extern void mpdc_queue_data(int, void *, void (*action)(void *, void *),
		int (*cmp)(void *, void *));

static void enqueue_album(void *name, void *album)
{
	ttk_menu_item item_st;
	current_song.artist = (char *)name;
	item_st.name = (char *)album;
	queue_album(&item_st);
	current_song.artist = NULL;
	free(album);
}

static void queue_artist(ttk_menu_item *item)
{
	char *album;
	if (mpdc_tickle() < 0)
		return;
	mpd_sendListCommand(mpdz, MPD_TABLE_ALBUM, item->name);
	if (mpdz->error) {
		mpdc_tickle();
		return;
	}
	while ((album = mpd_getNextAlbum(mpdz))) {
		mpdc_queue_data(1, strdup(album), NULL, NULL);
	}
	mpd_finishCommand(mpdz);
	mpdc_queue_data(1, (void *)item->name, enqueue_album, NULL);
}

static int artist_held_handler(TWidget *this, int button)
{
	ttk_menu_item *item;
	if (button == TTK_BUTTON_ACTION) {
		item = ttk_menu_get_selected_item(this);
		if (((char *)item->data)[0]) {
			ttk_menu_flash(item, 1);
			queue_artist(item);
		}
	}
	return 0;
}

static TWindow *open_artist(ttk_menu_item *item)
{
	TWindow *ret;
	if (mpdc_tickle() < 0)
		return TTK_MENU_DONOTHING;
	current_song.artist = ((char *)item->data)[0]? (char *)item->data:NULL;
	ret = pz_new_window(_("Albums"), PZ_WINDOW_NORMAL);
	ret->data = 0x12345678;
	ttk_add_widget(ret, populate_albums((char *)item->data));
	return pz_finish_window(ret);
}
static TWidget *populate_artists(char *search)
{
	TWidget *ret;
	char *artist;

	if (mpdc_tickle() < 0)
		return NULL;
	mpd_sendListCommand(mpdz, MPD_TABLE_ARTIST, search);
	if (mpdz->error) {
		mpdc_tickle();
		return NULL;
	}
	ret = ttk_new_menu_widget(NULL, ttk_menufont,
			ttk_screen->w -	ttk_screen->wx,
			ttk_screen->h - ttk_screen->wy);
	ttk_menu_set_i18nable(ret, 0);

	while ((artist = mpd_getNextArtist(mpdz))) {
		ttk_menu_item *item;
		item = (ttk_menu_item *)calloc(1, sizeof(ttk_menu_item));
		item->name = artist;
		item->data = (void *)artist;
		item->free_name = 1;
		item->makesub = open_artist;
		item->flags = TTK_MENU_ICON_SUB;
		ttk_menu_append(ret, item);
	}
	mpd_finishCommand(mpdz);

	{
		ttk_menu_item *item;
		item = (ttk_menu_item *)calloc(1, sizeof(ttk_menu_item));
		item->name = _("All Artists");
		item->data = "";
		item->makesub = open_artist;
		item->flags = TTK_MENU_ICON_SUB;
		ttk_menu_insert(ret, item, 0);
	}
	ret->held = artist_held_handler;
	ret->holdtime = 500; /* ms */

	return ret;
}

PzWindow *new_artist_menu()
{
	PzWindow *ret;
	clear_current_song();
	ret = pz_new_window(_("Artists"), PZ_WINDOW_NORMAL);
	ttk_add_widget(ret, populate_artists(""));
	return pz_finish_window(ret);
}

