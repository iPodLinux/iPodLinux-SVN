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

#ifndef __IPOD_H__
#define __IPOD_H__

/* Global Settings */

#ifdef IPOD
#define IPOD_SETTINGS_FILE	"/etc/podzilla.conf"
#else
#define IPOD_SETTINGS_FILE	"podzilla.conf"
#endif

/* DISPLAY SETINGS 0 - 9 */

#define CONTRAST 0
#define BACKLIGHT 1
#define BACKLIGHT_TIMER 2

/* AUDIO SETTIGNS 10 - 19 */

#define CLICKER 10
#define VOLUME 11
#define EQUILIZER 12
#define DSPFREQUENCY 13

/* PLAYLIST SETTINGS 20 - 29 */
 
#define SHUFFLE 20
#define REPEAT 21

/* OTHER SETTINGS 30 - 99 */

#define LANGUAGE 30
#define WHEEL_DEBOUNCE  34
#define ACTION_DEBOUNCE 32
#define KEY_DEBOUNCE 33

#define MIN_CONTRAST	0
#define MAX_CONTRAST	128

int ipod_get_contrast(void);
int ipod_set_contrast(int contrast);

int ipod_get_backlight(void);
int ipod_set_backlight(int backlight);

int ipod_load_settings(void);
int ipod_save_settings(void);

void ipod_reset_settings(void);

int ipod_set_setting(int setting, int value);
int ipod_get_setting(int setting);
void ipod_touch_settings(void);

int ipod_set_blank_mode(int blank);

void ipod_beep(void);

long ipod_get_hw_version(void);

#endif

