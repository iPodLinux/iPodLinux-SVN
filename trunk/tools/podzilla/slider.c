/*
 * Copyright (C) 2004 Bernard Leach
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

#include "pz.h"
#include "ipod.h"

static GR_WINDOW_ID slider_wid;
static GR_GC_ID slider_gc;
static GR_SCREEN_INFO screen_info;

struct slider_widget {
	int setting;
	int value;
	int min;
	int max;
	char *title;
};

static struct slider_widget slider;

static void slider_do_draw(void)
{
	int filler;

	filler = ((int)((slider.value * 128) / slider.max));
	GrSetGCForeground(slider_gc, WHITE);
	GrRect(slider_wid, slider_gc, 15, 40, 130, 10);
	GrFillRect(slider_wid, slider_gc, 15, 40, 130, 10);
	GrSetGCForeground(slider_gc, BLACK);
	GrFillRect(slider_wid, slider_gc, 16 , 41, filler, 8);
	GrSetGCForeground(slider_gc, WHITE);
}

static int slider_do_keystroke(GR_EVENT * event)
{
	int ret = 0;
	switch (event->keystroke.ch) {
	case '\r':
	case 'm':
		pz_close_window(slider_wid);
		ret = 1;
		break;

	case 'l':
		if (slider.value) {
			slider.value--;
			ipod_set_setting(slider.setting, slider.value);
			slider_do_draw();
			ret = 1;
		}
		break;

	case 'r':
		if (slider.value < slider.max - 1) {
			slider.value++;
			ipod_set_setting(slider.setting, slider.value);
			slider_do_draw();
			ret = 1;
		}
		break;
	}
	return ret;
}

void new_slider_widget(int SETTING, char *title, int slider_min, int slider_max)
{
	slider.setting = SETTING;
	slider.value = ipod_get_setting(slider.setting);
	slider.min = slider_min;
	slider.max = slider_max;
	slider.title = title;

	GrGetScreenInfo(&screen_info);

	slider_gc = GrNewGC();
	GrSetGCUseBackground(slider_gc, GR_TRUE);
	GrSetGCForeground(slider_gc, WHITE);

	slider_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), 
		slider_do_draw, slider_do_keystroke);

	GrSelectEvents(slider_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN);

	pz_draw_header(slider.title);

	GrMapWindow(slider_wid);
}
