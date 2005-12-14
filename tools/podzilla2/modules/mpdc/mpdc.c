/*
 * Copyright (C) 2005 Courtney Cavin <courtc@ipodlinux.org>
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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "pz.h"
#include "mpdc.h"

PzModule *module;

mpd_Connection *mpdz = NULL;

int mpdc_status(mpd_Connection *con_fd)
{
	mpd_Status status;
	status.error = NULL;
	if (mpdz == NULL)
		return -2;
	mpd_sendStatusCommand(con_fd);
	if (mpd_getStatus_st(&status, con_fd)) {
		mpd_finishCommand(con_fd);
		return status.state;
	}
	mpd_finishCommand(con_fd);
	return -1;
}

void mpdc_change_volume(mpd_Connection *con_fd, int volume)
{
	if (mpdc_tickle() < 0)
		return;
	mpd_sendSetvolCommand(con_fd, volume);
	mpd_finishCommand(con_fd);
}

void mpdc_next()
{
	if (mpdc_tickle() < 0)
		return;
	mpd_sendNextCommand(mpdz);
	mpd_finishCommand(mpdz);
}

void mpdc_prev()
{
	if (mpdc_tickle() < 0)
		return;
	mpd_sendPrevCommand(mpdz);
	mpd_finishCommand(mpdz);
}

void mpdc_playpause()
{
	int state;
	if (mpdc_tickle() < 0)
		return;
	if ((state = mpdc_status(mpdz)) < 0)
		return;
	if (state == MPD_STATUS_STATE_PLAY)
		mpd_sendPauseCommand(mpdz, 1);
	else if (state == MPD_STATUS_STATE_PAUSE)
		mpd_sendPauseCommand(mpdz, 0);
	else
		mpd_sendPlayCommand(mpdz, -1);
	mpd_finishCommand(mpdz);
}

mpd_Connection *mpd_init_connection()
{
	static mpd_Connection con_fd;
	char *hostname = getenv("MPD_HOST");
 	char *port = getenv("MPD_PORT");
 
 	if (hostname == NULL)
 		hostname = strdup("127.0.0.1");
 	if (port == NULL)
 		port = strdup("6600");
	
	mpd_newConnection_st(&con_fd, hostname, atoi(port), 16);

	free(hostname);
	free(port);

	if (con_fd.error) {
		return NULL;
	}

	return &con_fd;
}

int mpdc_init()
{
	static int warned_once = 0;
	if ((mpdz = mpd_init_connection()) == NULL) {
		if (!warned_once) {
			pz_error(_("Unable to connect to MPD"));
			warned_once = 1;
		}
		return -1;
	}
	return 0;
}

void mpdc_destroy()
{
	mpd_closeConnection(mpdz);
	mpdz = NULL;
}

/*  0 - connected
 *  1 - was error, connection restored
 * -1 - unable to connect to MPD
 */ 
int mpdc_tickle()
{
	char err = 0;

	if (mpdz == NULL) {
		mpdc_init();
	} else if (mpdz->error) {
		pz_error(mpdz->errorStr);
		mpd_clearError(mpdz);
		err = 1;
	}

	if (mpdc_status(mpdz) == -1) {
		mpdc_destroy();
		mpdc_init();
		if (mpdc_status(mpdz) < 0) {
			if (!err) { /* already have an error */
				pz_error(_("Unable to determine MPD status."));
			}
			return -1;
		}
		return 1;
	}

	return (mpdz == NULL) ? -1 : err;
}

#if 0
static int icon_timer(struct TWidget *this)
{
	int state;

	if (mpdc_tickle() < 0)
		return 0;
	state = mpdc_status(mpdz);
	if (state == MPD_STATUS_STATE_PLAY)
		this->data = (void *)&ttk_icon_play;
	else if (state == MPD_STATUS_STATE_PAUSE)
		this->data = (void *)&ttk_icon_pause;
	else
		this->data = NULL;
	this->dirty = 1;
	return 0;
}

static void draw_icon(TWidget *this, ttk_surface srf)
{
	if (this->data) {
		ttk_draw_icon((unsigned char *)this->data, srf, this->x,
				this->y,ttk_ap_getx("battery.border")->color,0);
	}
}

static TWidget *mpdc_icon()
{
	TWidget *ret;

	ret = ttk_new_widget(0, 0);
	ret->w = TTK_ICON_WIDTH;
	ret->h = TTK_ICON_HEIGHT;
	ret->draw = draw_icon;
	ret->data = NULL;
	ret->timer = icon_timer;
	ttk_widget_set_timer(ret, 1000); /* ms */

	return ret;
}
#endif

static int playing_visible(ttk_menu_item *item)
{
	int state;

	if (mpdc_tickle() < 0)
		return 0;
	state = mpdc_status(mpdz);
	return (state == MPD_STATUS_STATE_PLAY ||
			state == MPD_STATUS_STATE_PAUSE);
}

static int mpdc_unused_handler(int button, int time)
{
	switch (button) {
	case PZ_BUTTON_PLAY:
		mpdc_playpause();
		break;
	case PZ_BUTTON_NEXT:
		mpdc_next();
		break;
	case PZ_BUTTON_PREVIOUS:
		mpdc_prev();
		break;
	}
	return 0;
}

static void cleanup_mpdc()
{
	mpdc_destroy();
	pz_unregister_global_unused_handler(PZ_BUTTON_PLAY);
	pz_unregister_global_unused_handler(PZ_BUTTON_NEXT);
	pz_unregister_global_unused_handler(PZ_BUTTON_PREVIOUS);
}

static void init_mpdc()
{
	const int flag = TTK_MENU_ICON_SUB;
	if ((mpdz = mpd_init_connection()) == NULL) {
#ifdef IPOD /* warning gets a bit annoying if you don't have MPD installed */
		pz_error(_("Unable to connect to MPD"));
#endif
		return;
	}
	module = pz_register_module("mpdc", cleanup_mpdc);
	pz_register_global_unused_handler(PZ_BUTTON_PLAY, mpdc_unused_handler);
	pz_register_global_unused_handler(PZ_BUTTON_NEXT, mpdc_unused_handler);
	pz_register_global_unused_handler(PZ_BUTTON_PREVIOUS,
			mpdc_unused_handler);
	pz_menu_add_action("/Music/Playlists", new_playlist_menu)->flags = flag;
	pz_menu_add_action("/Music/Artists", new_artist_menu)->flags = flag;
	pz_menu_add_action("/Music/Albums", new_album_menu)->flags = flag;
	pz_menu_add_action("/Music/Songs", new_song_menu)->flags = flag;
#if 0
	pz_menu_add_action("/Music/Genre", new_genre_menu)->flags = flag;
#endif
	pz_menu_add_action("/Music/Queue", new_queue_menu)->flags = flag;
	pz_menu_add_action("/Now Playing", mpd_currently_playing);
	pz_get_menu_item("/Now Playing")->visible = playing_visible;

#if 0
	ttk_add_header_widget(mpdc_icon());
#endif
}

PZ_MOD_INIT(init_mpdc)
