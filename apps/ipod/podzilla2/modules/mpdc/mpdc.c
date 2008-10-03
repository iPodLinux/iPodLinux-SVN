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

char mpdc_is_playing()
{
	int state;
	if ((state = mpdc_status(mpdz)) == -1) {
		mpdc_destroy();
		mpdc_init();
		if ((state = mpdc_status(mpdz)) < 0)
			return 0;
	}
	return (state == MPD_STATUS_STATE_PLAY);
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
 		hostname = "127.0.0.1";
 	if (port == NULL)
 		port = "6600";
	
	mpd_newConnection_st(&con_fd, hostname, atoi(port), 16);

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

void mpd_widg_icons_update( struct header_info * hdr )
{
	int state = mpdc_status( mpdz );
	if( !hdr ) return;

	if( state == MPD_STATUS_STATE_PLAY )
		hdr->data = pz_icon_play;
	else if( state == MPD_STATUS_STATE_PAUSE )
		hdr->data = pz_icon_pause;
	else
		hdr->data = NULL;

	if( hdr->data ) {
		hdr->widg->w = pz_icon_play[0] + 6;
	} else {
		hdr->widg->w = 0;
	}
}

void mpd_widg_icons_draw( struct header_info * hdr, ttk_surface srf )
{
	unsigned char * icon = (unsigned char *)hdr->data;
	if( !hdr || !srf || !hdr->data ) return;

	ttk_draw_icon( icon, srf,
			hdr->widg->x + 3,
			hdr->widg->y + ((hdr->widg->h - icon[1])>>1),
			ttk_ap_getx( "battery.border" ),
			ttk_ap_getx( "header.bg" )->color );
}


static mpd_Status widget_inf;
void mpd_widg_progress_update( struct header_info * hdr )
{
	char buf[16];
	int mins,secs;
	mpd_Status * inf = (mpd_Status *)hdr->data;

	if( !hdr ) return;

	inf->error = NULL;

	mpd_sendStatusCommand(mpdz);
	if (mpdz->error) { 
		mpdc_tickle(); 
		return;
	}
	if( !mpd_getStatus_st( inf, mpdz) )
		return;
	else
		mpd_finishCommand(mpdz);

	/* compute this all out, but it's just to determine the string */
	mins = inf->elapsedTime / 60;
	secs = inf->elapsedTime - (mins * 60);
	if(    inf->state == MPD_STATUS_STATE_PLAY 
	    || inf->state == MPD_STATUS_STATE_PAUSE )
	{
		snprintf( buf, 16, "%d:%02d", mins, secs );
	} else {
		snprintf( buf, 16, "----" );
	}
        hdr->widg->w = pz_vector_width( buf, 5, 9 ,1 ) + 4;
}

extern ttk_color pz_dec_ap_get_solid( char * name );

#define WIDG_INSET_X (1)
void mpd_widg_progress_draw( struct header_info * hdr, ttk_surface srf )
{
	char buf[16];
	int mins,secs;
	mpd_Status * inf = (mpd_Status *)hdr->data;

        ttk_ap_fillrect( srf, ttk_ap_get( "slider.bg" ),
				hdr->widg->x+WIDG_INSET_X,
				hdr->widg->y + hdr->widg->h - 7,
                                hdr->widg->x + hdr->widg->w - (WIDG_INSET_X),
                                hdr->widg->y + hdr->widg->h - 2 );

	/* i really want to show this differently if we're paused, but 
	    this will be good enough for now... */
	if(    inf->state == MPD_STATUS_STATE_PLAY 
	    || inf->state == MPD_STATUS_STATE_PAUSE )
	{
		int wid = inf->elapsedTime 
			    * (hdr->widg->w-(WIDG_INSET_X*2)) 
			    / inf->totalTime;

		ttk_ap_fillrect( srf, ttk_ap_get( "slider.full" ),
				hdr->widg->x + WIDG_INSET_X,
				hdr->widg->y + hdr->widg->h - 7,
                                hdr->widg->x + WIDG_INSET_X + wid,
                                hdr->widg->y + hdr->widg->h - 2 );
	} 

	ttk_rect( srf,  hdr->widg->x+WIDG_INSET_X,
			hdr->widg->y + hdr->widg->h - 7,
			hdr->widg->x + hdr->widg->w - (WIDG_INSET_X),
			hdr->widg->y + hdr->widg->h - 2,
			pz_dec_ap_get_solid( "slider.border" ));

	/* and dump the time string to the screen */
	if(    inf->state == MPD_STATUS_STATE_PLAY 
	    || inf->state == MPD_STATUS_STATE_PAUSE )
	{
		mins = inf->elapsedTime / 60;
		secs = inf->elapsedTime - (mins * 60);
		snprintf( buf, 16, "%d:%02d", mins, secs );
	} else {
		snprintf( buf, 16, "stop" );
	}

	pz_vector_string( srf, buf, 
			hdr->widg->x + 3, hdr->widg->y + 4,
			5, 9, 1,
			pz_dec_ap_get_solid( "header.bg" ));
	pz_vector_string( srf, buf, 
			hdr->widg->x + 2, hdr->widg->y + 3,
			5, 9, 1,
			pz_dec_ap_get_solid( "header.fg" ));
}


static int playing_visible(ttk_menu_item *item)
{
	int state;

	if (mpdc_tickle() < 0)
		return 0;
	state = mpdc_status(mpdz);
	return (state == MPD_STATUS_STATE_PLAY ||
			state == MPD_STATUS_STATE_PAUSE);
}

static int init_shuffle()
{
	mpd_Status status;
	status.error = NULL;
	mpd_sendStatusCommand(mpdz);
	mpd_getStatus_st(&status, mpdz);
	mpd_finishCommand(mpdz);
	return status.random;
}
static void set_shuffle(ttk_menu_item *item, int cdata)
{
	if (mpdc_tickle() < 0)
		return;
	mpd_sendRandomCommand(mpdz, (item->choice == 1));
	mpd_finishCommand(mpdz);
}

static int init_repeat()
{
	mpd_Status status;
	status.error = NULL;
	mpd_sendStatusCommand(mpdz);
	mpd_getStatus_st(&status, mpdz);
	mpd_finishCommand(mpdz);
	return status.repeat ? 2 : 0;
}
static void set_repeat(ttk_menu_item *item, int cdata)
{
	if (mpdc_tickle() < 0)
		return;
	if (item->choice == 1) item->choice = 2;
	mpd_sendRepeatCommand(mpdz, (item->choice == 2));
	mpd_finishCommand(mpdz);
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

#define SETUP_MENUITEM(a,b,c) \
  do { ttk_menu_item *item; \
    item = pz_get_menu_item(a); \
    item->choicechanged = b; \
    item->choice = c; } while (0)

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
	pz_menu_add_action_group("/Music/Playlists", "Playlists", new_playlist_menu)->flags = flag;
	pz_menu_add_action_group("/Music/Artists", "Browse", new_artist_menu)->flags = flag;
	pz_menu_add_action_group("/Music/Albums", "Browse", new_album_menu)->flags = flag;
	pz_menu_add_action_group("/Music/Songs", "Browse", new_song_menu)->flags = flag;
#if 0
	pz_menu_add_action_group("/Music/Genre", "Browse", new_genre_menu)->flags = flag;
#endif
	pz_menu_add_action_group("/Music/Folders", "Browse", new_folder_menu)->flags = flag;
	if (search_available())
		pz_menu_add_action_group("/Music/Search...", "Browse", new_search_window);
	pz_menu_add_action_group("/Music/Queue", "Playlists", new_queue_menu)->flags = flag;
	pz_menu_add_action_group("/Now Playing", "Media", mpd_currently_playing);
	pz_get_menu_item("/Now Playing")->visible = playing_visible;

	SETUP_MENUITEM("/Settings/Shuffle", set_shuffle, init_shuffle());
	SETUP_MENUITEM("/Settings/Repeat", set_repeat, init_repeat());
#if 0
	ttk_add_header_widget(mpdc_icon());
#endif
	pz_add_header_widget( "MPD Progress", mpd_widg_progress_update,
					      mpd_widg_progress_draw, 
					      &widget_inf );
	pz_add_header_widget( "MPD Icons", mpd_widg_icons_update,
					   mpd_widg_icons_draw, NULL );
}

PZ_MOD_INIT(init_mpdc)
