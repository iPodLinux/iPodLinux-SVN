/*
 * Copyright (C) 2004 Damien Marchal,Bernard Leach
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

#include <string.h>
#include <stdio.h>
#include "pz.h"


static GR_WINDOW_ID image_wid;
static GR_GC_ID image_gc;
static GR_IMAGE_ID image_id;
static GR_SCREEN_INFO screen_info;

static void image_do_draw(GR_EVENT * event)
{
	GR_IMAGE_INFO image_info;
	int offx = 0, offy = 0;

	GrGetImageInfo(image_id, &image_info);
	if (image_info.width < screen_info.cols) {
		offx = (screen_info.cols - image_info.width) >> 1;
	}
	if (image_info.height < screen_info.rows) {
		offy = (screen_info.rows - image_info.height) >> 1;
	}

	GrDrawImageToFit(image_wid, image_gc, offx, offy, image_info.width, image_info.height, image_id);
}

static int image_do_keystroke(GR_EVENT * event)
{
	int ret = 1;
	pz_close_window(image_wid);
	return ret;
}

int is_image_type(char *extension)
{
	return strcmp(extension, ".jpg") == 0
		|| strcmp(extension, ".gif") == 0
		|| strcmp(extension, ".bmp") == 0;
}


void new_image_window(char *filename)
{
	image_id = GrLoadImageFromFile(filename, 0);
	if (!image_id) {
		printf("cannot open image %s\n", filename);
		return;
	}

	image_gc = GrNewGC();
	GrSetGCUseBackground(image_gc, GR_FALSE);
	GrSetGCForeground(image_gc, BLACK);
	GrGetScreenInfo(&screen_info);

	image_wid = pz_new_window(0, 0, screen_info.cols, screen_info.rows, image_do_draw, image_do_keystroke);

	GrSelectEvents(image_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_UP|GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(image_wid);
}
