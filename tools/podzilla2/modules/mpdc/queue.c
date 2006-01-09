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
#include <time.h>
#include "mpdc.h"

extern void mpdc_queue_data(int, void *, void (*action)(void *, void *),
		int (*cmp)(void *, void *));

static void add_to_menu(void *menu, void *item)
{
	ttk_menu_append((TWidget *)menu, (ttk_menu_item *)item);
}

static int save_playlist(TWidget *wid, char *filename)
{
	if (mpdc_tickle() < 0)
		return 0;
	mpd_sendSaveCommand(mpdz, filename);
	mpd_finishCommand(mpdz);
	if (wid) pz_close_window(wid->win);
	return 0;
}

static TWindow *save_queue(ttk_menu_item *item)
{
	/* ltinstw == local_ti_new_standard_text_widget */
	TWidget *(*ltinstw)(int x, int y, int w, int h, int absheight,
			char *dt, int (*callback)(TWidget *, char *));

	ltinstw = pz_module_softdep("tiwidgets",
			"ti_new_standard_text_widget");
	if (ltinstw) {
		TWindow *eightywindowslater;
		eightywindowslater = pz_new_window("Enter name...",
				PZ_WINDOW_POPUP);
		/* more magic numbers.. these I made up... */
		ttk_add_widget(eightywindowslater, ltinstw(0, 0,
					ttk_screen->w - ttk_screen->wx - 12,
					ttk_text_height(ttk_textfont) + 10,
					0, "untitled_playlist", save_playlist));
		return pz_finish_window(eightywindowslater);
	}
	else {
		char filename[64];
		time_t now;
		struct tm *l_time;

		now = time((time_t *)NULL);
		time(&now);
		l_time = (struct tm *)localtime(&now);
		strftime(filename, 63, "%Y%m%d-%H%M%S", l_time);
		save_playlist(NULL, filename);
	}

	return TTK_MENU_UPONE;
}

static void remove_queued_item(int num)
{
	if (mpdc_tickle() < 0)
		return;
	mpd_sendDeleteCommand(mpdz, num);
	mpd_finishCommand(mpdz);
}

static TWindow *remove_song(ttk_menu_item *it)
{
	ttk_menu_item *citem, *sitem, *item;
	int i;

	item = (ttk_menu_item *)it->data;
	sitem = ttk_menu_get_selected_item(item->menu);
	for (i = 0;(citem = ttk_menu_get_item(item->menu, i)) != NULL; i++) {
		if (citem == sitem) {
			ttk_menu_remove(item->menu, i);
			remove_queued_item(i);
			break;
		}
	}
	return TTK_MENU_UPONE;
}

static TWindow *clear_queue(ttk_menu_item *item)
{
	ttk_menu_clear(((ttk_menu_item *)item->data)->menu);
	mpd_sendClearCommand(mpdz);
	mpd_finishCommand(mpdz);
	return TTK_MENU_UPONE;
}

#define END 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
static ttk_menu_item queue_menu[] = {
	{N_("Save"), {save_queue,0}, END},
	{N_("Remove"), {remove_song,0}, END},
	{N_("Clear"), {clear_queue,0}, END},
	{0, {0,0}, END}
};
static TWindow *queue_actions(ttk_menu_item *item)
{
	TWindow *ret;
	TWidget *menu;
	int maxlen = 0, i, tmp;
	
	for (i = 0; queue_menu[i].name != 0; i++) {
		/* item gets translated when it's drawn so translation needed */
		tmp = ttk_text_width(ttk_menufont, gettext(queue_menu[i].name));
		if (tmp > maxlen) maxlen = tmp;
		queue_menu[i].data = (void *)item;
	}
	
	ret = pz_new_window(_("Queue"), PZ_WINDOW_POPUP);

	/* sorry for the magic numbers... 6 and 4 are the horizontal and
	 * vertical padding (respectively) for menu item text */
	menu = ttk_new_menu_widget(queue_menu, ttk_menufont,
			maxlen + 6, (ttk_text_height(ttk_menufont) + 4) * i);
	ttk_add_widget(ret, menu);
	return pz_finish_window(ret);
}

static int queue_held_handler(TWidget *this, int button)
{
	ttk_menu_item *item;
	if (button == TTK_BUTTON_ACTION) {
		item = ttk_menu_get_selected_item(this);
		ttk_popup_window(queue_actions(item));
	}
	return 0;
}

static void fix_playing_icon(ttk_menu_item *item)
{
	mpd_Status status;

	status.error = NULL;
	if (mpdc_tickle() < 0)
		return;
	mpd_sendStatusCommand(mpdz);
	mpd_getStatus_st(&status, mpdz);
	mpd_finishCommand(mpdz);

	if (item == ttk_menu_get_item(item->menu, status.song) &&
			(status.state == MPD_STATUS_STATE_PLAY ||
			 status.state == MPD_STATUS_STATE_PAUSE))
		item->flags = TTK_MENU_ICON_SND;
	else
		item->flags = 0;
}
static TWindow *open_queued_song(ttk_menu_item *item)
{
	ttk_menu_item *sitem;
	mpd_Status status;
	int i, id;

	status.error = NULL;
	if (mpdc_tickle() < 0)
		return TTK_MENU_DONOTHING;

	mpd_sendStatusCommand(mpdz);
	mpd_getStatus_st(&status, mpdz);
	id = status.song;
	mpd_finishCommand(mpdz);

	for (i = 0; (sitem = ttk_menu_get_item(item->menu, i)) != NULL; i++) {
		if (item == sitem) {
			if (i != id || (status.state != MPD_STATUS_STATE_PLAY &&
				    status.state != MPD_STATUS_STATE_PAUSE)) {
				mpd_sendPlayCommand(mpdz, i);
				mpd_finishCommand(mpdz);
			}
			return mpd_currently_playing();
		}
	}
	return TTK_MENU_DONOTHING;
}

static TWidget *populate_queue()
{
	TWidget *ret;
	mpd_InfoEntity entity;

	if (mpdc_tickle() < 0)
		return NULL;
	mpd_sendPlaylistInfoCommand(mpdz, -1);
	if (mpdz->error) {
		mpdc_tickle();
		return NULL;
	}
	ret = ttk_new_menu_widget(NULL, ttk_menufont,
			ttk_screen->w -	ttk_screen->wx,
			ttk_screen->h - ttk_screen->wy);

	while ((mpd_getNextInfoEntity_st(&entity, mpdz))) {
		mpd_Song *song = entity.info.song;
		ttk_menu_item *item;
		if (entity.type != MPD_INFO_ENTITY_TYPE_SONG) {
			continue;
		}
		item = (ttk_menu_item *)calloc(1, sizeof(ttk_menu_item));
		item->name = (char *)strdup(song->title?song->title:song->file);
		item->free_name = 1;
		item->predraw = fix_playing_icon;
		item->makesub = open_queued_song;
		mpdc_queue_data(0, item, NULL, NULL);
	}
	mpd_finishCommand(mpdz);
	if (mpdz->error) {
		mpdc_tickle();
	}
	mpdc_queue_data(0, ret, add_to_menu, NULL);
	ttk_menu_set_i18nable(ret, 0);

	ret->held = queue_held_handler;
	ret->holdtime = 500; /* ms */
	return ret;
}
PzWindow *new_queue_menu()
{
	PzWindow *ret;
	ret = pz_new_window(_("Queue"), PZ_WINDOW_NORMAL);
	ttk_add_widget(ret, populate_queue());
	ret->data = 0x12345678;
	return pz_finish_window(ret);
}
