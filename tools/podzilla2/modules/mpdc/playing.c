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

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "mpdc.h"
#include "pz.h"

#define IDLE 0
#define DELAYED 1
#define SEEKING 2

#define POSITION 0
#define VOLUME_S 1 /* damn _S because VOLUME is a setting in pz.h */
#define SEEK 2

static struct {
	int rate, /* seek rate */
	    jump; /* how much to jump */
	int dir;  /* direction of seeking */
	short stat; /* status: IDLE, DELAYED, SEEKING */
} seek = {500, 5, 0, IDLE};

static struct {
	short state; /* states: POSITION, VOLUME_S, SEEK */
	int ticker;  /* time ticker */
	int seek_time;
} multibar = {POSITION, 0};

static struct {
	int step;
	int rate;
} accel = {1, 0};

static PzWindow *mcp_wid;
static PzWidget *mcp_widget;

static int inited;
static mpd_Status status;
static mpd_Song *current_song = NULL;

static void mcp_seek(int time);

/* Changes num of seconds to a time string; goes all the way to days:
 * 30:23:59:59 is completly possible.. Negative numbers may(will) give
 * unspecified results
 */
static void num2time(int num, char *tstr)
{
	int seconds, minutes, hours, days;

	seconds = num % 60;
	minutes = (num % 3600) / 60;
	hours = (num % 86400) / 3600;
	days = num / 86400;

	strcpy(tstr, "");
	if (days) {
		sprintf(tstr, "%d:", days);
		if (hours < 10)
			strcat(tstr, "0");
	}
	if (hours || days) {
		sprintf(tstr, "%s%d:", tstr, hours);
		if (minutes < 10)
			strcat(tstr, "0");
	}
	sprintf(tstr, "%s%d:", tstr, minutes);
	if (seconds < 10)
		strcat(tstr, "0");
	sprintf(tstr, "%s%d", tstr, seconds >= 0 ? seconds : 0);
}

static void mcp_draw_percent(PzWidget *wid, ttk_surface srf, int per)
{
	int num = per > 100 ? 100 : per;
	int w = wid->w - (wid->w / 6);
	int x = (wid->w - w) / 2;
	int y = wid->h - 30;
	int pw = (w * num) / 100;
	const int h = 9;

	if (ttk_ap_get("music.bar.bg"))
		ttk_ap_fillrect(srf, ttk_ap_get("music.bar.bg"), x,y,x+w+1,y+h);
	switch (multibar.state) {
	case POSITION:
	case VOLUME_S:
		ttk_ap_fillrect(srf, ttk_ap_get("music.bar"),
				x, y, x + pw + 1, y + h);
		if (ttk_ap_get("music.bar.bar"))
			ttk_ap_fillrect(srf, ttk_ap_get("music.bar.bar"),
					x, y, x + pw + 1, y + h/2);
		break;
	case SEEK:
		ttk_ap_rect(srf, ttk_ap_get("scroll.bar"),
				x, y+h/2, x + w, y+h/2);
		ttk_ap_rect(srf, ttk_ap_get("scroll.bar"), x+pw, y, x+pw,
				y +(h - 1));
		ttk_ellipse(srf, x+pw, y+h/2, h/4, h/4,
			ttk_ap_getx("window.fg")->color);
		break;
	}

	/* these are actually just lines, but the ap_rect function deals
	 * with them appropriatly */
	ttk_ap_rect(srf, ttk_ap_get("window.fg"), x, y - 1, x + w, y - 1);
	ttk_ap_rect(srf, ttk_ap_get("window.fg"), x, y + h, x + w, y + h);
	ttk_ap_rect(srf, ttk_ap_get("window.fg"), x - 1, y, x - 1, y +(h - 1));
	ttk_ap_rect(srf, ttk_ap_get("window.fg"), x+w+1, y, x+w+1, y +(h - 1));
}

static void mcp_place_text(PzWidget *wid, ttk_surface srf, void *text,
		int mod, int num)
{
	int w, h;
	w = ttk_text_width(ttk_menufont,  (char *)text); 
	h = ttk_text_height(ttk_menufont); 
	ttk_text(srf, ttk_menufont, w < wid->w ? (wid->w - w) / 2 : 8,
			mod*num - h/2, ttk_ap_getx("window.fg")->color,
			(char *)text); 
}

static void mcp_draw_volume(PzWidget *wid, ttk_surface srf)
{
	char tmp[20];
	int w, h;

	h = ttk_text_height(ttk_textfont);
	mcp_draw_percent(wid, srf, status.volume);
	sprintf(tmp, "%d%%", status.volume);
	w = ttk_text_width(ttk_textfont,  tmp);
	ttk_text(srf, ttk_textfont, (wid->w - w) / 2, wid->h - (8 + h),
			ttk_ap_getx("window.fg")->color, tmp);
}

static void mcp_draw_position(PzWidget *wid, ttk_surface srf)
{
	char tmp[20];
	int w, h;

	h = ttk_text_height(ttk_textfont);
	mcp_draw_percent(wid, srf, status.elapsedTime * 100 / status.totalTime);
	num2time(status.elapsedTime, tmp);
	ttk_text(srf, ttk_textfont, wid->w / 12, wid->h -
			(8 + h), ttk_ap_getx("window.fg")->color, tmp);
	num2time(status.totalTime - status.elapsedTime, tmp);
	w = ttk_text_width(ttk_textfont,  tmp);
	ttk_text(srf, ttk_textfont, wid->w - wid->w / 12 - w, wid->h - (8 + h),
			ttk_ap_getx("window.fg")->color, tmp);
}

static void mcp_draw_screen(PzWidget *wid, ttk_surface srf)
{
	int posmod, i = 0;

	while (!mpdz->doneProcessing);
	if (!current_song)
		return;

	posmod = (wid->h - 30)
		/ ((current_song->artist ? 1: 0)
		+ ((current_song->album) ? 1: 0) + 2); 

	switch (multibar.state) {
	case VOLUME_S:
		mcp_draw_volume(wid, srf);
		break;

	case SEEK:
		status.elapsedTime = multibar.seek_time;
	case POSITION:
		mcp_draw_position(wid, srf);
		break;
	}

	if (current_song->artist)
		mcp_place_text(wid, srf, current_song->artist, posmod, ++i);
	if (current_song->album)
		mcp_place_text(wid, srf, current_song->album, posmod, ++i);
	if (current_song->title)
		mcp_place_text(wid, srf, current_song->title, posmod, ++i);
	else
		mcp_place_text(wid, srf, current_song->file, posmod, ++i);
}

static void mcp_update_status()
{
	mpd_InfoEntity entity;

	mpd_sendStatusCommand(mpdz);
	if (mpdz->error) {
		mpdc_tickle();
		return;
	}
	if (!mpd_getStatus_st(&status, mpdz))
		return;
	mpd_finishCommand(mpdz);

	if (inited && status.state != MPD_STATUS_STATE_PLAY &&
			status.state != MPD_STATUS_STATE_PAUSE) {
		pz_close_window(mcp_wid);
		if (current_song != NULL) {
			current_song = NULL;
		}
		inited = 0;
		return;
	}
	
	mpd_sendCurrentSongCommand(mpdz);
	if (mpdz->error) {
		mpdc_tickle();
		return;
	}
	while ((mpd_getNextInfoEntity_st(&entity, mpdz))) {
		mpd_Song *song = entity.info.song;

		if (entity.type != MPD_INFO_ENTITY_TYPE_SONG) {
			continue;
		}
		current_song = song;
	}
	if (mpdz->error) {
		mpdc_tickle();
		return;
	}
	mpd_finishCommand(mpdz);
	if (mcp_widget)
		mcp_widget->dirty = 1;
}

static void mcp_update_time()
{
	if (status.elapsedTime < status.totalTime)
		status.elapsedTime++;
	else 
		mcp_update_status();
	mcp_widget->dirty = 1;
}

static int mcp_handle_timer(struct TWidget *this)
{
	static int inc = 0;

	if (accel.rate == 0)
		accel.step = 1;
	else if (accel.rate > 25 || accel.rate < 25)
		accel.step++;
	accel.rate = 0;

	switch (multibar.state) {
	case SEEK:
		if (multibar.ticker == 2)
			mcp_seek(multibar.seek_time);
		/* fall through */
	case VOLUME_S:
		if (multibar.ticker-- <= 0)
			multibar.state = POSITION;
		break;

	case POSITION:
	default:
		break;
	}

	if (!(inc++ % 10)) {
		mcp_update_status();
	} else if (status.state == MPD_STATUS_STATE_PLAY) {
		mcp_update_time();
	}
	if (inc == INT_MAX) inc = 0;
	return 0;
}

void mcp_seek_callback()
{
	if (seek.stat != IDLE) {
		mcp_seek(-1);
		ttk_create_timer(seek.rate, mcp_seek_callback);
	}
}

static void mcp_set_seek_timer(int direction)
{
	seek.dir = direction;
	seek.stat = DELAYED;
	ttk_create_timer(seek.rate, mcp_seek_callback);
}

static void mcp_destroy_seek_timer(int direction)
{
	seek.stat = IDLE;
	seek.dir = 0;
}

static int mcp_seek_timer_active(int direction)
{
	if (direction == seek.dir)
		return (seek.stat == SEEKING);
	return 0;
}

static void mcp_seek(int time)
{
	seek.stat = SEEKING;
	if (time < 0) {
		if (seek.dir > 0)
			status.elapsedTime += seek.jump;
		else
			status.elapsedTime -= seek.jump;
		mpd_sendSeekCommand(mpdz, status.song, status.elapsedTime);
		mpd_finishCommand(mpdz);
	} else  {
		mpd_sendSeekCommand(mpdz, status.song, time);
		mpd_finishCommand(mpdz);
	}
}

#define NMIN(x,y) if (x < y) x = y
#define NMAX(x,y) if (x > y) x = y
#define BOUNDS(x,y,z) NMIN(x,y); NMAX(x,z)

static int mcp_events(PzEvent *e)
{
	int ret = 0;
	switch (e->type) {
	case PZ_EVENT_BUTTON_DOWN:
		switch (e->arg) {
		case PZ_BUTTON_ACTION:
			switch (multibar.state) {
			case POSITION:
				multibar.state = SEEK;
				multibar.ticker = 5;
				multibar.seek_time = status.elapsedTime;
				break;
			case SEEK:
				mcp_seek(multibar.seek_time);
				mcp_destroy_seek_timer(0);
				/* fall through */
			case VOLUME_S:
				multibar.state = POSITION;
				multibar.ticker = 0;
				break;
			}
			e->wid->dirty = 1;
			break;
		case '4':
		case PZ_BUTTON_NEXT:
			mcp_set_seek_timer(1);
			mcp_update_status();
			break;
		case '5':
		case PZ_BUTTON_PREVIOUS:
			mcp_set_seek_timer(-1);
			mcp_update_status();
			break;
		case '1':
		case PZ_BUTTON_PLAY:
			mpdc_playpause();
			mcp_update_status();
			break;
		case PZ_BUTTON_MENU:
			current_song = NULL;
			pz_close_window(e->wid->win);
			break;
		default:
			break;
		}
		break;

	case PZ_EVENT_SCROLL:
		switch (multibar.state) {
		case POSITION:
			multibar.state = VOLUME_S;
			break;
		case VOLUME_S:
			status.volume += e->arg * 2;
			status.volume -= status.volume % (e->arg * 2);
			BOUNDS(status.volume, 0, 100);
			mpdc_change_volume(mpdz, status.volume);
			break;
		case SEEK:
			if (accel.rate && (!(e->arg < 0 && accel.rate < 0) &&
					!(e->arg > 0 && accel.rate > 0))) {
				accel.step = 1;
				accel.rate = 0;
			}
			accel.rate += e->arg;
			multibar.seek_time += e->arg * (4 * accel.step);
			BOUNDS(multibar.seek_time, 0, status.totalTime);
			multibar.seek_time -= multibar.seek_time %
				(e->arg * (4 * accel.step));
			break;
		}
		e->wid->dirty = 1;
		multibar.ticker = 5;
		break;

	case PZ_EVENT_BUTTON_UP:
		switch (e->arg) {
		case PZ_BUTTON_NEXT:
			if (!mcp_seek_timer_active(1)) {
				mpdc_next();
				mcp_update_status();
			}
			mcp_destroy_seek_timer(1);
			multibar.state = POSITION;
			break;
		case PZ_BUTTON_PREVIOUS:
			if (!mcp_seek_timer_active(-1)) {
				mpdc_prev();
				mcp_update_status();
			}
			mcp_destroy_seek_timer(-1);
			multibar.state = POSITION;
			break;
		default:
			break;
		}
		break;
	default:
		ret |= TTK_EV_UNUSED;
		break;
	}
	return ret;
}

PzWindow *mpd_currently_playing()
{
	PzWindow *ret;
	inited = 0;
	accel.step = 1;
	multibar.state = POSITION;
	status.error = NULL;
	current_song = NULL; /* needed for st mpd_newConnection */
	
	if (mpdc_tickle() < 0)
		return TTK_MENU_DONOTHING;
	mcp_update_status();
	if (status.state != MPD_STATUS_STATE_PLAY &&
			status.state != MPD_STATUS_STATE_PAUSE)
		return TTK_MENU_DONOTHING;

	mcp_wid = ret = pz_new_window(_("Now Playing"), PZ_WINDOW_NORMAL);
	mcp_widget = pz_add_widget(ret, mcp_draw_screen, mcp_events);
	ttk_widget_set_timer(mcp_widget, 1000); /* ms */
	mcp_widget->timer = mcp_handle_timer;

	inited = 1;
	return pz_finish_window(ret);
}

