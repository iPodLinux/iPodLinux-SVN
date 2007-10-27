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
#include "pz.h"
#include "mpdc.h"

/* ltinstw == local_ti_new_standard_text_widget */
static TWidget *(*ltinstw)(int x, int y, int w, int h, int absheight,
		char *dt, int (*callback)(TWidget *, char *));

static TWidget *lmenu;
static TWindow *lwindow;

static void queue_song(ttk_menu_item *item)
{
	if (mpdc_tickle() < 0)
		return;

	mpd_sendAddCommand(mpdz, item->data);
	mpd_finishCommand(mpdz);

	if (mpdz->error)
		mpdc_tickle();
}

static int held_handler(TWidget *this, int button)
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

	queue_song(item);

	mpd_sendPlayCommand(mpdz, MPD_PLAY_AT_BEGINNING);
	mpd_finishCommand(mpdz);

	return mpd_currently_playing();
}

static void search_table(long table, const char *string)
{
	mpd_InfoEntity entity;
	int i = 0;

	if (mpdc_tickle() < 0)
		return;
	mpd_sendSearchCommand(mpdz, table, string);

	if (mpdz->error) {
		mpdc_tickle();
		return;
	}
	while ((mpd_getNextInfoEntity_st(&entity, mpdz))) {
		mpd_Song *song = entity.info.song;
		ttk_menu_item *item;
		if (entity.type != MPD_INFO_ENTITY_TYPE_SONG)
				continue;
		item = calloc(1, sizeof(ttk_menu_item));
		item->data = strdup(song->file);
		item->name = strdup(song->title?song->title:song->file);
		item->makesub = open_song;
		item->free_name = 1;
		item->free_data = 1;
		ttk_menu_append(lmenu, item);
		if ((++i % 10) == 0)
			ttk_gfx_update(lwindow->srf);
	}
	mpd_finishCommand(mpdz);
	lmenu->held = held_handler;
	lmenu->holdtime = 500; /* ms */
	if (mpdz->error)
		mpdc_tickle();

}

static int initiate_search(TWidget *wid, char *search)
{
	char title[256];
	sprintf(title, "\"%s\"", search);
	ttk_window_title(lwindow, title);

	wid->destroy(wid);
	ttk_remove_widget(wid->win, wid);
	free(wid);

	search_table(MPD_TABLE_TITLE, search);
	search_table(MPD_TABLE_FILENAME, search);
	search_table(MPD_TABLE_ALBUM, search);
	search_table(MPD_TABLE_ARTIST, search);
	return 0;
}

PzWindow *new_search_window()
{
	int sheight = ttk_text_height(ttk_menufont) + 12;
	TWidget *wid;
	TWindow *ret = lwindow = pz_new_window(_("Search"), PZ_WINDOW_NORMAL);

	lmenu = wid = ttk_new_menu_widget(NULL, ttk_menufont,
			ttk_screen->w - ttk_screen->wx,
			ttk_screen->h - ttk_screen->wy - sheight);
	ttk_add_widget(ret, wid);

	wid = ltinstw(0, ttk_screen->h - ttk_screen->wy - sheight,
			ttk_screen->w - ttk_screen->wx, sheight,
			1, "", initiate_search);
	ttk_add_widget(ret, wid);
	ti_widget_start(wid);

	ret->data = 0x12345678;
	return pz_finish_window(ret);
}

int search_available()
{
	if (!ltinstw)
		ltinstw = pz_module_softdep("tiwidgets",
				"ti_new_standard_text_widget");
	return (!!ltinstw);
}
