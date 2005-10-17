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
#include "slider.h"

void set_backlight_timer(void)
{
	new_settings_slider_window(_("Backlight Timer"), 
			BACKLIGHT_TIMER, 0, 180);
}

void set_contrast(void)
{
	new_settings_slider_window(_("Set Contrast"), CONTRAST, 48, 112);
}

void toggle_backlight(void)
{
        ipod_set_setting(BACKLIGHT, !ipod_get_setting(BACKLIGHT));
}

