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

#define KEY_CLICK 1
#define KEY_UNUSED 2
#define EVENT_UNUSED 4

#define FONT_HEIGHT 14

/* pz.c */
extern GR_SCREEN_INFO screen_info;
extern long hw_version;

void pz_draw_header(char *header);
GR_GC_ID pz_get_gc(int copy);
GR_WINDOW_ID pz_new_window(int x, int y, int w, int h, void(*do_draw)(void), int(*keystroke)(GR_EVENT * event));
void pz_close_window(GR_WINDOW_ID wid);
void pz_event_handler(GR_EVENT *event);

/* display.c */
void set_backlight_timer(void);
void set_contrast(void);
void toggle_backlight(void);

/* image.c */
int is_image_type(char *extension);
void new_image_window(char *filename);

/* message.c */
void new_message_window(char *message);
void pz_error(char *fmt, ...);
void pz_perror(char *msg);

void new_slider_widget(int setting, char *title, int slider_min, int slider_max);

/* for the 'handle event' methods. */ 
#define IPOD_BUTTON_ACTION		('\r')
#define IPOD_BUTTON_MENU		('m')
#define IPOD_BUTTON_REWIND		('w')
#define IPOD_BUTTON_FORWARD		('f')
#define IPOD_BUTTON_PLAY		('d')

#define IPOD_SWITCH_HOLD		('h')

#define IPOD_WHEEL_CLOCKWISE		('r')
#define IPOD_WHEEL_ANTICLOCKWISE	('l')
#define IPOD_WHEEL_COUNTERCLOCKWISE	('l')

#define IPOD_REMOTE_PLAY		('1')
#define IPOD_REMOTE_VOL_UP		('2')
#define IPOD_REMOTE_VOL_DOWN		('3')
#define IPOD_REMOTE_FORWARD		('4')
#define IPOD_REMOTE_REWIND		('5')

#endif
