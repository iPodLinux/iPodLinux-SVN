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

static GR_WINDOW_ID disp_wid;
static GR_GC_ID disp_gc;
static GR_SCREEN_INFO screen_info;
static int contrast;

static void contrast_do_draw(void)
{
	GrSetGCForeground(disp_gc, WHITE);
	GrRect(disp_wid, disp_gc, 10, 40, MAX_CONTRAST, 10);
	GrFillRect(disp_wid, disp_gc, 10, 40, contrast, 10);
	GrSetGCForeground(disp_gc, BLACK);
	GrFillRect(disp_wid, disp_gc, 11 + contrast, 41,
		   MAX_CONTRAST - contrast - 2, 8);
	GrSetGCForeground(disp_gc, WHITE);
}

static void contrast_do_keystroke(GR_EVENT * event)
{
	switch (event->keystroke.ch) {
	case '\r':
	case '\n':
	case 'm':
		pz_close_window(disp_wid);
		break;

	case 'l':
		if (contrast) {
			contrast--;
			ipod_set_contrast(contrast);
			contrast_do_draw();
		}
		break;

	case 'r':
		if (contrast < MAX_CONTRAST - 1) {
			contrast++;
			ipod_set_contrast(contrast);
			contrast_do_draw();
		}
		break;
	}
}

static void contrast_event_handler(GR_EVENT *event)
{
	int i;

	switch (event->type) {
	case GR_EVENT_TYPE_EXPOSURE:
		contrast_do_draw();
		break;

	case GR_EVENT_TYPE_KEY_DOWN:
		contrast_do_keystroke(event);
		break;
	}
}

void new_contrast_window(void)
{
	contrast = ipod_get_contrast();
	if (contrast < 0) {
		contrast = 50;
	}

	GrGetScreenInfo(&screen_info);

	disp_gc = GrNewGC();
	GrSetGCUseBackground(disp_gc, GR_TRUE);
	GrSetGCForeground(disp_gc, WHITE);

	disp_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), contrast_event_handler);

	GrSelectEvents(disp_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN);

	pz_draw_header("Contrast");

	GrMapWindow(disp_wid);
}

void toggle_backlight(void)
{
	int backlight;

	printf("backlight toggle\n");
	backlight = ipod_get_backlight();
	if (backlight >= 0) {
		backlight = 1 - backlight;
		ipod_set_backlight(backlight);
	}
}

