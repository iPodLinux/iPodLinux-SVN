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

#ifndef __PZ_H__
#define __PZ_H__

#define MWINCLUDECOLORS
#include "nano-X.h"

#define HEADER_BASELINE 15
#define HEADER_TOPLINE 20

#define FONT_HEIGHT 14

/* pz.c */
void pz_draw_header(char *header);
GR_WINDOW_ID pz_new_window(int x, int y, int w, int h, void(*do_draw), int(*keystroke)(GR_EVENT * event));
void pz_close_window(GR_WINDOW_ID wid);

/* display.c */
void set_backlight_timer(void);
void set_contrast(void);
void toggle_backlight(void);

/* image.c */
int is_image_type(char *extension);
void new_image_window(char *filename);

/* message.c */
void new_message_window(char *message);

void new_slider_widget(int setting, char *title, int slider_min, int slider_max);

#endif
