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

#include <stdio.h>
#include <string.h>

#include "mpdc.h"
#include "../pz.h"

#define IDLE 0
#define DELAY 1
#define SEEKING 2

#define POSITION 0
#define VOLUME 1
#define SEEK 2

static struct {
	int rate, /* seek rate */
	    jump; /* how much to jump */
	int dir;  /* direction of seeking */
	short stat; /* status: IDLE, DELAY, SEEKING */
	GR_TIMER_ID tid; /* seek timer id */
} seek = {500, 5, 0, IDLE, 0};

static struct {
	short state; /* states: POSITION, VOLUME, SEEK */
	int ticker;  /* time ticker */
	int seek_time;
} multibar = {POSITION, 0};

static GR_TIMER_ID mcp_tid;
static GR_WINDOW_ID mcp_wid;
static GR_WINDOW_ID mcp_px;
static GR_GC_ID mcp_gc;

static int inited;
static mpd_Status status;
static mpd_Song *current_song = NULL;

static void mcp_seek(int time);

/* Changes num of seconds to a time string; goes all the way to days:
 * 30:23:59:59 is completly possible.. Negative numbers may give unspecified
 * results
 */
static void num2time(int num, char *tstr)
{
	int seconds, minutes, hours, days;

	seconds = num % 60;
	minutes = num % 3600 / 60;
	hours = num % 86400 / 3600;
	days = num / 86400;

	sprintf(tstr, "%s", "");
	if (days) {
		sprintf(tstr, "%d:", days);
		if (hours < 10)
			sprintf(tstr, "%s0", tstr);
		if (hours == 0)
			sprintf(tstr, "%s0", tstr);
	}
	if (hours) {
		sprintf(tstr, "%s%d:", tstr, hours);
		if (minutes < 10)
			sprintf(tstr, "%s0", tstr);
		if (minutes == 0)
			sprintf(tstr, "%s0", tstr);
	}
	sprintf(tstr, "%s%d:", tstr, minutes);
	if (seconds < 10)
		sprintf(tstr, "%s0", tstr);
	sprintf(tstr, "%s%d", tstr, seconds >= 0 ? seconds : 0);
}

static void mcp_draw_percent(int per)
{
	int num = per > 100 ? 100 : per;
	int w = screen_info.cols - (screen_info.cols / 6);
	int x = (screen_info.cols - w) / 2;
	int y = screen_info.rows - HEADER_TOPLINE - 30;
	int pw = (w * num) / 100;
	const int h = 8;

	switch (multibar.state) {
	case POSITION:
	case VOLUME:
		GrSetGCForeground(mcp_gc, GRAY);
		GrFillRect(mcp_px, mcp_gc, x, y, pw - 2 < w/2 ? pw : pw - 2, h);
		GrSetGCForeground(mcp_gc, LTGRAY);
		GrFillRect(mcp_px, mcp_gc, pw + x - 2, y, 2, h);
		GrSetGCForeground(mcp_gc, WHITE);
		GrFillRect(mcp_px, mcp_gc, pw+x-2 > x?pw+x: pw+x+2, y, w-pw, h);
		break;
	case SEEK:
		GrSetGCForeground(mcp_gc, WHITE);
		GrFillRect(mcp_px, mcp_gc, x, y, w, h);
		GrSetGCForeground(mcp_gc, LTGRAY);
		GrLine(mcp_px, mcp_gc, x, y + h/2, x + w, y + h/2);
		GrSetGCForeground(mcp_gc, GRAY);
		GrFillEllipse(mcp_px, mcp_gc, x + pw, y + h/2, h/4, h/4);
		break;
	}

	GrSetGCForeground(mcp_gc, BLACK);
	GrLine(mcp_px, mcp_gc, x, y - 1, x + w, y - 1);
	GrLine(mcp_px, mcp_gc, x, y + h, x + w, y + h);
	GrLine(mcp_px, mcp_gc, x - 1, y, x - 1, y + (h - 1));
	GrLine(mcp_px, mcp_gc, x + w + 1, y, x + w + 1, y + (h - 1));
}

static void mcp_place_text(void *text, int mod, int num)
{
	GR_SIZE w, h, b;
	GrGetGCTextSize(mcp_gc, text, strlen((char *)text), GR_TFUTF8,
			&w, &h, &b);
	GrText(mcp_px, mcp_gc, w < screen_info.cols ?
			(screen_info.cols - w) / 2 : 8, mod * num + h / 2,
			text, strlen((char*)text), GR_TFUTF8);
}

static void mcp_draw_volume()
{
	char tmp[20];
	GR_SIZE w, h, b;

	mcp_draw_percent(status.volume);
	sprintf(tmp, "%d%%", status.volume);
	GrGetGCTextSize(mcp_gc, tmp, -1, GR_TFASCII, &w, &h, &b);
	GrText(mcp_px, mcp_gc, (screen_info.cols - w) / 2,
			screen_info.rows - HEADER_TOPLINE - 8,
			tmp, -1, GR_TFASCII);
}

static void mcp_draw_position()
{
	char tmp[20];
	GR_SIZE w, h, b;

	mcp_draw_percent(status.elapsedTime * 100 / status.totalTime);
	num2time(status.elapsedTime, tmp);
	GrText(mcp_px, mcp_gc, screen_info.cols / 12,
			screen_info.rows - HEADER_TOPLINE - 8,
			tmp, -1, GR_TFASCII); 
	num2time(status.totalTime - status.elapsedTime, tmp);
	GrGetGCTextSize(mcp_gc, tmp, -1, GR_TFASCII, &w, &h, &b);
	GrText(mcp_px, mcp_gc, screen_info.cols - screen_info.cols /
			12 - w, screen_info.rows - HEADER_TOPLINE - 8,
			tmp, -1, GR_TFASCII);
}

static void mcp_draw_screen()
{
	int posmod, i = 0;

	while (!mpdz->doneProcessing);
	if (!current_song)
		return;

	GrSetGCForeground(mcp_gc, WHITE);
	GrFillRect(mcp_px, mcp_gc, 0, 0, screen_info.cols, screen_info.rows);

	GrSetGCForeground(mcp_gc, BLACK);

	posmod = (screen_info.rows - (HEADER_TOPLINE + 30))
		/ ((current_song->artist ? 1: 0)
		+ ((current_song->album) ? 1: 0) + 2); 

	switch (multibar.state) {
	case VOLUME:
		mcp_draw_volume();
		break;

	case SEEK:
		status.elapsedTime = multibar.seek_time;
	case POSITION:
		mcp_draw_position();
		break;
	}

	if (current_song->artist)
		mcp_place_text(current_song->artist, posmod, ++i);
	if (current_song->album)
		mcp_place_text(current_song->album, posmod, ++i);
	if (current_song->title)
		mcp_place_text(current_song->title, posmod, ++i);
	else
		mcp_place_text(current_song->file, posmod, ++i);
	GrCopyArea(mcp_wid, mcp_gc, 0, 0, screen_info.cols, screen_info.rows -
			(HEADER_TOPLINE + 1), mcp_px, 0, 0, MWROP_USE_GC_MODE);

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
		GrDestroyGC(mcp_gc);
		GrDestroyTimer(mcp_tid);
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
}

static void mcp_update_time()
{
	if (status.elapsedTime < status.totalTime)
		status.elapsedTime++;
	else 
		mcp_update_status();
}

static void mcp_handle_timer()
{
	static int inc = 0;

	switch (multibar.state) {
	case SEEK:
		if (multibar.ticker == 2)
			mcp_seek(multibar.seek_time);
	case VOLUME:
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
	if (inited)
		mcp_draw_screen();
}

static void mcp_set_seek_timer(int direction)
{
	seek.dir = direction;
	seek.tid = GrCreateTimer(mcp_wid, seek.rate);
	seek.stat = DELAY;
}

static void mcp_destroy_seek_timer(int direction)
{
	GrDestroyTimer(seek.tid);
	seek.tid = 0;
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

static void mcp_expose()
{
	mcp_draw_screen();
	pz_draw_header("Now Playing");
}

static int mcp_events(GR_EVENT *e)
{
	int ret = 0;
	switch (e->type) {
	case GR_EVENT_TYPE_TIMER:
		if (((GR_EVENT_TIMER *)e)->tid == seek.tid)
			mcp_seek(-1);
		else
			mcp_handle_timer();
		break;
	case GR_EVENT_TYPE_KEY_DOWN:
		switch (e->keystroke.ch) {
		case '\r':
		case '\n':
			multibar.state = SEEK;
			multibar.ticker = 5;
			multibar.seek_time = status.elapsedTime;
			mcp_draw_screen();
			break;
		case '3':
		case 'l':
			switch (multibar.state) {
			case POSITION:
				multibar.state = VOLUME;
				break;

			case VOLUME:
				if (status.volume > 0) {
					status.volume -= 2;
					status.volume += status.volume % 2;
					mpdc_change_volume(mpdz, status.volume);
				}
				break;
			case SEEK:
				if (multibar.seek_time > 0) {
					multibar.seek_time -= 4;
					multibar.seek_time +=
						multibar.seek_time % 4;
				}
				break;
			}
			multibar.ticker = 5;
			mcp_draw_screen();
			break;
		case '2':
		case 'r':
			switch (multibar.state) {
			case POSITION:
				multibar.state = VOLUME;
				break;

			case VOLUME:
				if (status.volume < 100) {
					status.volume += 2;
					status.volume -= status.volume % 2;
					mpdc_change_volume(mpdz, status.volume);
				}
				break;
			case SEEK:
				if (multibar.seek_time < status.totalTime) {
					multibar.seek_time += 4;
					multibar.seek_time -=
						multibar.seek_time % 4;
				}
				break;
			}
			multibar.ticker = 5;
			mcp_draw_screen();
			break;
		case '4':
		case 'f':
			mcp_set_seek_timer(1);
			mcp_update_status();
			break;
		case '5':
		case 'w':
			mcp_set_seek_timer(-1);
			mcp_update_status();
			break;
		case '1':
		case 'd':
			mpdc_playpause(mpdz);
			mcp_update_status();
			break;
		case 'm':
		case 'q':
			GrDestroyGC(mcp_gc);
			GrDestroyTimer(mcp_tid);
			pz_close_window(mcp_wid);
			if (current_song != NULL) {
				current_song = NULL;
			}
			break;
		default:
			ret |= KEY_UNUSED;
			break;
		}
		break;
	case GR_EVENT_TYPE_KEY_UP:
		switch (e->keystroke.ch) {
		case 'f':
			if (!mcp_seek_timer_active(1)) {
				mpdc_next(mpdz);
				mcp_update_status();
			}
			mcp_destroy_seek_timer(1);
			multibar.state = POSITION;
			break;
		case 'w':
			if (!mcp_seek_timer_active(-1)) {
				mpdc_prev(mpdz);
				mcp_update_status();
			}
			mcp_destroy_seek_timer(-1);
			multibar.state = POSITION;
			break;
		default:
			ret |= KEY_UNUSED;
			break;
		}
	default:
		ret |= EVENT_UNUSED;
		break;
	}
	return ret;
}

void mpd_currently_playing()
{
	inited = 0;
	multibar.state = POSITION;
	status.error = NULL;
	current_song = NULL; /* needed for st mpd_newConnection */
	mcp_gc = pz_get_gc(1);
	
	if (mpdc_tickle() < 0)
		return;
	mcp_update_status();
	if (status.state != MPD_STATUS_STATE_PLAY &&
			status.state != MPD_STATUS_STATE_PAUSE)
		return;

	mcp_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols,
			screen_info.rows - (HEADER_TOPLINE + 1),
			mcp_expose, mcp_events);
	mcp_px = GrNewPixmap(screen_info.cols,
			screen_info.rows - (HEADER_TOPLINE + 1), NULL);

	GrSelectEvents(mcp_wid, GR_EVENT_MASK_EXPOSURE | GR_EVENT_MASK_KEY_UP |
			GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_TIMER);
	GrMapWindow(mcp_wid);
	mcp_tid = GrCreateTimer(mcp_wid, 1000);
	inited = 1;
}

