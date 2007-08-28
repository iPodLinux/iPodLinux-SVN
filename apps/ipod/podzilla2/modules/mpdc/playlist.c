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

static TWindow *open_playlist(ttk_menu_item *item)
{
	if (mpdc_tickle() < 0)
		return TTK_MENU_DONOTHING;
	mpd_sendClearCommand(mpdz);
	mpd_finishCommand(mpdz);

	mpd_sendLoadCommand(mpdz, (char *)item->name);
	mpd_finishCommand(mpdz);
	if (mpdz->error) {
		mpdc_tickle();
		return TTK_MENU_DONOTHING;
	}
	mpd_sendPlayCommand(mpdz, -1);
	mpd_finishCommand(mpdz);

	return mpd_currently_playing();
}

static TWidget *populate_playlists()
{
	mpd_InfoEntity entity;
	TWidget *ret;
	
	if (mpdc_tickle() < 0)
		return NULL;

	ret = ttk_new_menu_widget(NULL, ttk_menufont,
			ttk_screen->w - ttk_screen->wx,
			ttk_screen->h - ttk_screen->wy);

	mpd_sendLsInfoCommand(mpdz, "");

	while ((mpd_getNextInfoEntity_st(&entity, mpdz))) {
		mpd_PlaylistFile *playlist;
		ttk_menu_item *item;
		if (entity.type != MPD_INFO_ENTITY_TYPE_PLAYLISTFILE) {
			continue;
		}
		playlist = entity.info.playlistFile;
		item = (ttk_menu_item *)calloc(1, sizeof(ttk_menu_item));
		item->name = (char *)strdup(playlist->path);
		item->free_name = 1;
		item->makesub = open_playlist;

		ttk_menu_append(ret, item);
	}
	ttk_menu_set_i18nable(ret, 0);

	mpd_finishCommand(mpdz);
	return ret;
}

PzWindow *new_playlist_menu()
{
	PzWindow *ret;
	ret = pz_new_window(_("Playlists"), PZ_WINDOW_NORMAL);
	ttk_add_widget(ret, populate_playlists());
	return pz_finish_window(ret);
}

