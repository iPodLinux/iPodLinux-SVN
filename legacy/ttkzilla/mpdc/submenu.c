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
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../pz.h"
#include "../mlist.h"
#include "mpdc.h"

static GR_WINDOW_ID mms_wid;
static GR_GC_ID mms_gc;
static menu_st *mms_menu;
static int mms_wh, mms_ww;

static void mms_save()
{
	char timestr[64];
	time_t now;
	struct tm *l_time;

	now = time((time_t *)NULL);
	time(&now);
	l_time = localtime(&now);
	strftime(timestr, 63, "%Y%m%d-%H%M%S", l_time);
	GrDestroyGC(mms_gc);
	menu_destroy(mms_menu);
	pz_close_window(mms_wid);
	mpd_sendSaveCommand(mpdz, timestr);
	mpd_finishCommand(mpdz);
}

static void mms_move()
{ /* no-op */
	GrDestroyGC(mms_gc);
	menu_destroy(mms_menu);
	pz_close_window(mms_wid);
}

static void mms_remove()
{
	GrDestroyGC(mms_gc);
	mms_menu = menu_destroy(mms_menu);
	pz_close_window(mms_wid);
	menu_delete_item(mms_menu, mms_menu->sel);
	mpd_sendDeleteCommand(mpdz, mms_menu->sel);
	mpd_finishCommand(mpdz);
}

static void mms_draw()
{
	GrSetGCForeground(mms_gc, BLACK);
	GrRect(mms_wid, mms_gc, 1, 1, mms_ww - 2, mms_wh - 2);
}

static void mms_expose()
{
	if (mms_wid == GrGetFocus()) {
		menu_draw(mms_menu);
		mms_draw();
	}
}

static int mms_event(GR_EVENT *e)
{
	int ret = 0;

	switch (e->type) {
	case GR_EVENT_TYPE_TIMER:
		menu_draw_timer(mms_menu);
		break;
	case GR_EVENT_TYPE_KEY_DOWN:
		switch (e->keystroke.ch) {
		case '\r':
		case '\n':
			menu_handle_item(mms_menu, mms_menu->sel);
			ret |= KEY_CLICK;
			break;
		case 'r':
			if (menu_shift_selected(mms_menu, 1)) {
				menu_draw(mms_menu);
				ret |= KEY_CLICK;
			}
			break;
		case 'l':
			if (menu_shift_selected(mms_menu, -1)) {
				menu_draw(mms_menu);
				ret |= KEY_CLICK;
			}
			break;
		case 'm':
			GrDestroyGC(mms_gc);
			menu_destroy(mms_menu);
			pz_close_window(mms_wid);
			ret |= KEY_CLICK;
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

static item_st mod_menu[] = {
	{"move", mms_move, ACTION_MENU},
	{"remove", mms_remove, ACTION_MENU},
	{"save", mms_save, ACTION_MENU},
	{0}
};

void mms_modify_song(menu_st *parent)
{
	GR_SIZE w, h, b;
	int x, y;
	mms_gc = pz_get_gc(1);

	GrGetGCTextSize(mms_gc, "remove", -1, GR_TFASCII, &w, &h, &b);
	mms_ww = w + 8*2 + 6;
	mms_wh = (h + 4)*3 + 6;
	x = (screen_info.cols - mms_ww)/2;
	y = (screen_info.rows - mms_wh)/2;

	mms_wid = pz_new_window(x, y, mms_ww, mms_wh, mms_expose, mms_event);
	GrSelectEvents(mms_wid, GR_EVENT_MASK_EXPOSURE | GR_EVENT_MASK_TIMER |
			GR_EVENT_MASK_KEY_UP | GR_EVENT_MASK_KEY_DOWN);

	mms_menu = menu_init(mms_wid, "?", 3, 3, mms_ww - 6, mms_wh - 6,
			parent, mod_menu, ASCII);
	GrMapWindow(mms_wid);
}

