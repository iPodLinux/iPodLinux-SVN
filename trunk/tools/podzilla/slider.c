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

struct slider_widget {
	int setting;
	int value;
	int min;
	int max;
	char *title;
};

static struct slider_widget slider;

#define SHEIGHT	10
#define SBORDER	1
#define SXOFF	15

#define SYOFF	(((screen_info.rows-(HEADER_TOPLINE+1))/2)-(SHEIGHT/2))
#define SXIN	(screen_info.cols-(SXOFF*2))

static void slider_do_draw(void)
{
	int filler;

	GrSetGCForeground(slider_gc, appearance_get_color(CS_BG) );
	GrFillRect(slider_wid, slider_gc, 0, 0, 
		    screen_info.cols, screen_info.rows);
	
	filler = ((int)((slider.value * (SXIN-(SBORDER*2))) / slider.max));
	GrSetGCForeground(slider_gc, appearance_get_color(CS_SLDRBDR) );
	GrRect(slider_wid, slider_gc, SXOFF, SYOFF, SXIN, SHEIGHT);
	GrSetGCForeground(slider_gc, appearance_get_color(CS_SLDRCTNR) );
	GrFillRect(slider_wid, slider_gc, SXOFF+SBORDER+filler, SYOFF+SBORDER,
	           (SXIN-(SBORDER*2))-filler, SHEIGHT-(SBORDER*2));
	GrSetGCForeground(slider_gc, appearance_get_color(CS_SLDRFILL) );
	GrFillRect(slider_wid, slider_gc, SXOFF+SBORDER, SYOFF+SBORDER,
	           filler, SHEIGHT-(SBORDER*2));
}

static int slider_do_keystroke(GR_EVENT * event)
{
	int ret = 0;
	switch (event->type) {
	case GR_EVENT_TYPE_KEY_DOWN:
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

	slider_gc = pz_get_gc(1);
	GrSetGCUseBackground(slider_gc, GR_FALSE);
	GrSetGCForeground(slider_gc, BLACK);

	slider_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1),
		slider_do_draw, slider_do_keystroke);

	GrSelectEvents(slider_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_UP|GR_EVENT_MASK_KEY_DOWN);

	pz_draw_header(slider.title);

	GrMapWindow(slider_wid);
}
