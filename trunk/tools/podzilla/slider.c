/*
 * Copyright (C) 2004 Bernard Leach
 * Copyright (C) 2005 Courtney Cavin
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
#include "slider.h"

static GR_WINDOW_ID slider_wid;

struct {
	short type;
	int *pointer;
	int setting;
	int value;
	int min;
	int max;
	char *title;
	char window;
	GR_WINDOW_ID wid;
	GR_GC_ID gc;
} slider;

#define INT	1
#define SETTING	2

#define SHEIGHT	10
#define SBORDER	1
#define SXOFF	15

#define SYOFF	(((screen_info.rows-(HEADER_TOPLINE+1))/2)-(SHEIGHT/2))
#define SXIN	(screen_info.cols-(SXOFF*2))

static void slider_set_value()
{
	if (slider.type == INT)
		*slider.pointer = slider.value;
	else if (slider.type == SETTING)
		ipod_set_setting(slider.setting, slider.value);
}

void destroy_slider()
{
	GrDestroyGC(slider.gc);
}

void slider_draw()
{
	int filler;

	GrSetGCForeground(slider.gc, appearance_get_color(CS_BG) );
	GrFillRect(slider_wid, slider.gc, 0, 0, 
		    screen_info.cols, screen_info.rows);
	
	filler = ((int)((slider.value * (SXIN-(SBORDER*2))) / slider.max));
	GrSetGCForeground(slider.gc, appearance_get_color(CS_SLDRBDR) );
	GrRect(slider_wid, slider.gc, SXOFF, SYOFF, SXIN, SHEIGHT);

	GrSetGCForeground(slider.gc, appearance_get_color(CS_SLDRCTNR) );
	GrFillRect(slider_wid, slider.gc, SXOFF+SBORDER+filler, SYOFF+SBORDER,
	           (SXIN-(SBORDER*2))-filler, SHEIGHT-(SBORDER*2));

	GrSetGCForeground(slider.gc, appearance_get_color(CS_SLDRFILL) );
	GrFillRect(slider_wid, slider.gc, SXOFF+SBORDER, SYOFF+SBORDER,
	           filler, SHEIGHT-(SBORDER*2));
}

int slider_event(GR_EVENT * event)
{
	int ret = 0;
	switch (event->type) {
	case GR_EVENT_TYPE_KEY_DOWN:
		switch (event->keystroke.ch) {
		case '\r':
		case 'm':
			if (slider.window) {
				slider.window = 0;
				destroy_slider();
				pz_close_window(slider_wid);
				ret |= KEY_CLICK;
			}
			break;
	
		case 'l':
			if (slider.value >= slider.min) {
				slider.value--;
				slider_set_value();
				slider_draw();
				ret |= KEY_CLICK;
			}
			break;

		case 'r':
			if (slider.value <= slider.max) {
				slider.value++;
				slider_set_value();
				slider_draw();
				ret |= KEY_CLICK;
			}
			break;
		default:
			ret |= KEY_UNUSED;
		}
		break;
	default:
		ret |= EVENT_UNUSED;
	}
	return ret;
}

static void new_slider(GR_WINDOW_ID wid, int slider_min, int slider_max)
{
	slider.min = slider_min;
	slider.max = slider_max;
	slider.wid = wid;
	slider.gc = pz_get_gc(1);

	GrSetGCUseBackground(slider.gc, GR_FALSE);
	GrSetGCForeground(slider.gc, appearance_get_color(CS_FG));
}

static void create_slider_window()
{
	slider_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols,
			screen_info.rows - (HEADER_TOPLINE + 1),
			slider_draw, slider_event);
	GrSetWindowBackgroundColor(slider_wid, appearance_get_color(CS_BG));
}
static void init_slider_window(char *title)
{
	GrSelectEvents(slider_wid, GR_EVENT_MASK_EXPOSURE |
			GR_EVENT_MASK_KEY_UP | GR_EVENT_MASK_KEY_DOWN);
	GrMapWindow(slider_wid);
	pz_draw_header(title);
}

void new_settings_slider(GR_WINDOW_ID wid, int setting,
		int slider_min, int slider_max)
{
	slider.type = SETTING;
	slider.setting = setting;
	slider.value = ipod_get_setting(slider.setting);
	new_slider(wid, slider_min, slider_max);
}

void new_int_slider(GR_WINDOW_ID wid, int *setting,
		int slider_min, int slider_max)
{
	slider.type = INT;
	slider.pointer = setting;
	slider.value = *setting;
	new_slider(wid, slider_min, slider_max);
}

void new_settings_slider_window(char *title, int setting,
		int slider_min, int slider_max)
{
	slider.window = 1;
	create_slider_window();
	new_settings_slider(slider_wid, setting, slider_min, slider_max);	
	init_slider_window(title);
}

void new_int_slider_window(char *title, int *setting,
		int slider_min, int slider_max)
{
	slider.window = 1;
	create_slider_window(title);
	new_int_slider(slider_wid, setting, slider_min, slider_max);	
	init_slider_window(title);
}
