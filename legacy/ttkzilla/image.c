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
#define PZMOD
#include "pz.h"

int is_image_type(char *extension)
{
	return strcasecmp(extension, ".jpg") == 0
		|| strcasecmp(extension, ".jpeg") == 0
		|| strcasecmp(extension, ".gif") == 0
		|| strcasecmp(extension, ".bmp") == 0
		|| strcasecmp(extension, ".xpm") == 0
		|| strcasecmp(extension, ".pnm") == 0
	        || strcasecmp(extension, ".ppm") == 0;
}

void new_image_window(char *filename)
{
    TWindow *w = ttk_new_window();
    return ttk_add_widget (w, ttk_new_imgview_widget (w->w, w->h, ttk_load_image (filename)));
}


