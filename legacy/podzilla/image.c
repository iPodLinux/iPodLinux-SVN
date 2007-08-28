/*
 * Copyright (C) 2004 Damien Marchal, Bernard Leach
 * Copyright (C) 2004, 2005 Cameron Nishiyama
 *
 * 22 January 2005 - READ ME!
 *     This is a work-in-progress release.  I am still planning to improve
 *     this (namely adding smooth zooming), and will likely largely rework
 *     the structure of this file.  If you would like to make suggestions,
 *     change something, or help out, please e-mail me.
 *     -- Cameron <caaaaaam@gmail.com>
 *     (Expires: 22 February 2005)
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

#include "pz.h"

#define SCROLL_SPEED 5

typedef enum {ZOOM_FIT, ZOOM_ACTUAL, ZOOM_MULTIPLY} PZ_ZOOM_TYPE;

static GR_WINDOW_ID image_wid;
static GR_GC_ID image_gc;
static GR_IMAGE_ID image_id;
static GR_IMAGE_INFO image_info;
static GR_WINDOW_ID image_pixmap;

static int pixmap_width, pixmap_height;
static int pad_x, pad_y, old_pad_x, old_pad_y;
static int loc_x, loc_y;
static int oversized_x, oversized_y;
static float zoom_factor, zoom_change; /* TODO: Remove need for floats? */
static float zoom_fit_level, zoom_min, zoom_max;
static enum {SCROLL_VERT = 0, SCROLL_HORIZ = 1} scroll_dir;
static PZ_ZOOM_TYPE play_btn_toggle;

static void image_blit_pixmap(void)
{
	GrCopyArea(image_wid, image_gc, pad_x, pad_y, screen_info.cols - pad_x,
		screen_info.rows - pad_y, image_pixmap, loc_x, loc_y, 0);
}

static void image_do_draw()
{
	if(image_pixmap)
		image_blit_pixmap();	
}

void image_create_pixmap(void)
{
	if(image_pixmap)
		GrDestroyWindow(image_pixmap);
	pixmap_width = (int)(image_info.width * zoom_factor);
	pixmap_height = (int)(image_info.height * zoom_factor);
	image_pixmap = GrNewPixmap(pixmap_width, pixmap_height, NULL);
	GrDrawImageToFit(image_pixmap, image_gc, 0, 0, pixmap_width, pixmap_height, image_id);
}

void image_set_layout(void)
{
	old_pad_x = pad_x;
	old_pad_y = pad_y;

	if (pixmap_width < screen_info.cols) {
		oversized_x = 0;
		pad_x = (screen_info.cols - pixmap_width) >> 1;
	} else {
		oversized_x = 1;
		pad_x = 0;
	}
	if (pixmap_height < screen_info.rows) {
		oversized_y = 0;
		pad_y = (screen_info.rows - pixmap_height) >> 1;
	} else {
		oversized_y = 1;
		pad_y = 0;
	}

	if (oversized_x == 1)	
		scroll_dir = SCROLL_HORIZ;
	else
		scroll_dir = SCROLL_VERT;

	/* Recenter */
	loc_x = (int)((loc_x - old_pad_x + screen_info.cols / 2.0) *
			zoom_change - (screen_info.cols / 2.0 - pad_x));
	loc_y = (int)((loc_y - old_pad_y + screen_info.rows / 2.0) *
			zoom_change - (screen_info.rows / 2.0 - pad_y));

	if (loc_x < 0)
		loc_x = 0;		
	else if (loc_x > pixmap_width - (screen_info.cols - 2*pad_x))
		loc_x = pixmap_width - (screen_info.cols - 2*pad_x);
	if (loc_y < 0)
		loc_y = 0;
	else if (loc_y > pixmap_height - (screen_info.rows - 2*pad_y))
		loc_y = pixmap_height - (screen_info.rows - 2*pad_y);
}

void image_status_message(char *msg)
{
	GR_SIZE txt_width, txt_height, txt_base;
	GR_COORD txt_x, txt_y;

	GrGetGCTextSize(image_gc, msg, -1, GR_TFASCII, &txt_width, &txt_height,
			&txt_base);
	txt_x = (screen_info.cols - (txt_width + 10)) >> 1;
	txt_y = (screen_info.rows - (txt_height + 10)) >> 1;
	GrSetGCForeground(image_gc, WHITE);
	GrFillRect(image_wid, image_gc, txt_x, txt_y, txt_width + 10,
			txt_height + 10);
	GrSetGCForeground(image_gc, BLACK);
	GrRect(image_wid, image_gc, txt_x, txt_y, txt_width + 10, txt_height +
			10);
	GrText(image_wid, image_gc, txt_x + 5, txt_y + txt_base + 5, msg, -1,
			GR_TFASCII);
}

void zoom(PZ_ZOOM_TYPE type, float factor)
{
	float new_level;
	char message[30];


	if(type == ZOOM_FIT)
		new_level = zoom_fit_level;
	else if(type == ZOOM_ACTUAL)
		new_level = 1.0;
	else if(type == ZOOM_MULTIPLY)
	{
		new_level = zoom_factor * factor;

		/* Limit zooming */
		if (new_level > zoom_max)
			new_level = zoom_max;
		if (new_level < zoom_min)
			new_level = zoom_min;
	} else
		return;

	if(zoom_factor == 0.0)
		zoom_change = 1.0;
	else	
	{
		zoom_change = new_level / zoom_factor;
		if(zoom_change == 1.0)
			return;
	}
	if(new_level == zoom_fit_level)
	{
		play_btn_toggle = ZOOM_ACTUAL;
		snprintf(message, 30, _("Rescaling image... (Maxpect)"));
	}
	else if (new_level == 1.0)
	{
		play_btn_toggle = ZOOM_FIT;
		snprintf(message, 30, _("Rescaling image... (Actual)"));
	}
	else
	{
		play_btn_toggle = ZOOM_FIT;
		snprintf(message, 30, _("Rescaling image... (%d%%)"),
				(int)(new_level * 100));
	}

	image_status_message(message);

	zoom_factor = new_level;
	image_create_pixmap();
	image_set_layout();

	GrClearWindow(image_wid, GR_TRUE);
	image_blit_pixmap();
}	

static int image_do_keystroke(GR_EVENT * event)
{
	int ret = 0;
	switch(event->type) {
	case GR_EVENT_TYPE_KEY_DOWN:
		switch (event->keystroke.ch) {
		case 'm': /* Free and exit */
			pz_close_window(image_wid);
			GrDestroyWindow(image_pixmap);
			GrFreeImage(image_id);
			GrDestroyGC(image_gc);
			break;
		case '\r': /* Change scroll direction */
			if (scroll_dir == SCROLL_VERT && oversized_x == 1)
				scroll_dir = SCROLL_HORIZ;
			else if (scroll_dir == SCROLL_HORIZ && oversized_y == 1)
				scroll_dir = SCROLL_VERT;
			break;
		case 'r': /* Scroll right/down */
			if(scroll_dir == SCROLL_HORIZ && oversized_x == 1)
			{
				loc_x += SCROLL_SPEED; 
				if (loc_x > pixmap_width - screen_info.cols)
					loc_x = pixmap_width - screen_info.cols;
			} 
			else if (scroll_dir == SCROLL_VERT && oversized_y == 1)
			{
				loc_y += SCROLL_SPEED;
				if (loc_y > pixmap_height - screen_info.rows)
					loc_y = pixmap_height - screen_info.rows;
			}
			image_blit_pixmap();
			break;
		case 'l': /* Scroll left/up */
			if(scroll_dir == SCROLL_HORIZ && oversized_x == 1)
			{
				loc_x -= SCROLL_SPEED;
				if (loc_x < 0)
					loc_x = 0;
			}
			else if (scroll_dir == SCROLL_VERT && oversized_y == 1)
			{
				loc_y -= SCROLL_SPEED;
				if (loc_y < 0)
					loc_y = 0;
			}
			image_blit_pixmap();
			break;		
		case 'w': /* Zoom out */
			zoom(ZOOM_MULTIPLY, 0.5);
			break;
		case 'f': /* Zoom in */
			zoom(ZOOM_MULTIPLY, 2.0);
			break;
		case 'd': /* Zoom fit/actual */
			zoom(play_btn_toggle, 0);
		default:
			ret = 0;
			break;
		}
		break;
	}
	return ret;
}

int is_image_type(char *extension)
{
	return strcasecmp(extension, ".jpg") == 0
		|| strcasecmp(extension, ".jpeg") == 0
		|| strcasecmp(extension, ".gif") == 0
		|| strcasecmp(extension, ".bmp") == 0
		|| strcasecmp(extension, ".xpm") == 0
		|| strcasecmp(extension, ".pnm") == 0;
}

void new_image_window(char *filename)
{
	float horiz_ratio, vert_ratio;

	image_pixmap = 0;
	loc_x = 0;
	loc_y = 0;
	pad_x = 0;
	pad_y = 0;
	zoom_factor=0; zoom_change=0;
	zoom_fit_level=0; zoom_min=0; zoom_max=0;

	image_gc = pz_get_gc(1);
	GrSetGCUseBackground(image_gc, GR_FALSE);
	GrSetGCForeground(image_gc, BLACK);
	
	image_wid = pz_new_window(0, 0, screen_info.cols, screen_info.rows,
			image_do_draw, image_do_keystroke);

	GrSelectEvents(image_wid, GR_EVENT_MASK_EXPOSURE|
			GR_EVENT_MASK_KEY_DOWN| GR_EVENT_MASK_KEY_UP);
	GrMapWindow(image_wid);
	
	GrClearWindow(image_wid, GR_FALSE);
	image_status_message(_("Loading image..."));

	if (!(image_id = GrLoadImageFromFile(filename, 0))) {
		pz_close_window(image_wid);		
		GrDestroyGC(image_gc);
		pz_error(_("Unable to load image %s"), filename);
		return;
	}

	GrGetImageInfo(image_id, &image_info);

	GrClearWindow(image_wid, GR_FALSE);

	/* Calculate zoom fit level */	
	horiz_ratio = (float)screen_info.cols / image_info.width;
	vert_ratio = (float)screen_info.rows / image_info.height;
	if(horiz_ratio > vert_ratio)
		zoom_fit_level = vert_ratio;
	else
		zoom_fit_level = horiz_ratio;	
	
	/* Calculate maximum and minimum zoom levels
	 * Important zoom levels: Half-fit, fit, half-full, full */
	zoom_min = zoom_fit_level / 2.0;
	if(zoom_min > 0.5)
		zoom_min = 0.5;
	zoom_max = zoom_fit_level;
	if(zoom_max < 1.0)
		zoom_max = 1.0;

	zoom(ZOOM_FIT, 0);
}

