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
#include <sys/time.h>
#include <math.h>
#include "pz.h"
#include "mpdc.h"

/* ltinstw == local_ti_new_standard_text_widget */
static TWidget *(*ltinstw)(int x, int y, int w, int h, int absheight,
		char *dt, int (*callback)(TWidget *, char *));
static int (*ltws)(TWidget * wid);


static TWidget *lmenu;
static TWindow *lwindow;

static unsigned int get_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (unsigned int)((tv.tv_sec % 0x3fffff)*1000 + tv.tv_usec/1000);
}

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

static void spinner(ttk_surface srf, int x, int y)
{
#if YOU_NEED_TO_RECALCULATE
	int px, py, vx, vy, tx, ty, zx, zy;
	px = 17 * cos(angle), py = 17 * sin(angle);
	vx = 17 * cos(M_PI/4 + angle), vy = 17 * sin(M_PI/4 + angle);
	tx = 17 * cos(M_PI/2 + angle), ty = 17 * sin(M_PI/2 + angle);
	zx = 17 * cos(M_PI/2 + M_PI/4 + angle), zy = 17 * sin(M_PI/2 + M_PI/4+ angle);

	printf("(%d,%d), (%d,%d), (%d,%d), (%d,%d)\n",
	         px,py,   vx,vy,   tx,ty,   zx,zy);
#endif
	static int pos;
	int i;
	struct cir {
		int x, y;
	} c[] = {
		{ 17,  0},
		{ 12, 12},
		{  0, 17},
		{-12, 12}
	};

	if (++pos == 8) pos = 0;

	for (i = 0; i < sizeof(c) / sizeof(c[0]); ++i) {
		int s = (i - pos)*31;
		int t = (((i + 4) % 8) - pos)*31;
		ttk_color col0 = ttk_makecol(s,s,s);
		ttk_color col1 = ttk_makecol(t,t,t);
		ttk_aafillellipse(srf, x + c[i].x, y + c[i].y, 5, 5, col0);
		ttk_aafillellipse(srf, x - c[i].x, y - c[i].y, 5, 5, col1);
	}
}

static void draw_loading(TWindow *win, ttk_surface srf)
{
	spinner(srf, win->w / 2, win->h / 2);
}

static void search_table(long table, const char *string)
{
	mpd_InfoEntity entity;
	unsigned int time = get_ms();

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
		if (get_ms() - time > 100) {
			time = get_ms();
			ttk_menu_draw(lmenu, lwindow->srf);
			draw_loading(lwindow, lwindow->srf);
			ttk_draw_window(lwindow);
			ttk_gfx_update(ttk_screen->srf);
		}
		if (entity.type != MPD_INFO_ENTITY_TYPE_SONG)
			continue;
		item = calloc(1, sizeof(ttk_menu_item));
		item->data = strdup(song->file);
		item->name = strdup(song->title?song->title:song->file);
		item->makesub = open_song;
		item->free_name = 1;
		item->free_data = 1;
		ttk_menu_append(lmenu, item);
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
	if (!search || !search[0])
		return 0;

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
			ttk_screen->h - ttk_screen->wy);
	ttk_add_widget(ret, wid);

	wid = ltinstw(0, ttk_screen->h - ttk_screen->wy - sheight,
			ttk_screen->w - ttk_screen->wx, sheight,
			1, "", initiate_search);
	ttk_add_widget(ret, wid);
	ltws(wid);

	ret->data = 0x12345678;
	return pz_finish_window(ret);
}

int search_available()
{
	if (!ltws)
		ltws = pz_module_softdep("tiwidgets", "ti_widget_start");
	if (!ltinstw)
		ltinstw = pz_module_softdep("tiwidgets",
				"ti_new_standard_text_widget");
	return (!!ltinstw & !!ltws);
}
