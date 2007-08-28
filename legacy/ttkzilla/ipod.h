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

#define TIME_ZONE	(35)
#define TIME_DST	(36)
#define TIME_IN_TITLE	(37)
#define TIME_TICKER	(38)
#define TIME_1224	(39)
#define TIME_WORLDTZ	(40)
#define TIME_WORLDDST	(41)

#define BROWSER_PATH	(42)
#define BROWSER_HIDDEN	(43)

#define COLORSCHEME	(44)	/* appearance */
#define DECORATIONS	(45)	/* appearance */
#define BATTERY_DIGITS	(46)	/* appearance */
#define DISPLAY_LOAD	(47)	/* appearance */
#define FONT_FILE	(48)
#define SLIDE_TRANSIT	(49)

#define MIN_CONTRAST	0
#define MAX_CONTRAST	128

// for pz_set_backlight_timer
#define BL_RESET -1
#define BL_OFF   -2
#define BL_ON     0

int ipod_get_contrast(void);
int ipod_set_contrast(int contrast);

int ipod_get_backlight(void);
int ipod_set_backlight(int backlight);

int ipod_load_settings(void);
int ipod_save_settings(void);

void ipod_reset_settings(void);

int ipod_set_setting(short setting, int value);
int ipod_get_setting(short setting);
void ipod_touch_settings(void);

int ipod_set_blank_mode(int blank);

void ipod_beep(void);

#define BATTERY_LEVEL_LOW 50 
#define BATTERY_LEVEL_FULL 512

int ipod_get_battery_level(void);
int ipod_is_charging(void);
void ipod_turn_off(void);


int usb_is_connected(void);
void usb_check_goto_diskmode(void);

int fw_is_connected(void);
void fw_check_goto_diskmode(void);

long ipod_get_hw_version(void);

#endif

