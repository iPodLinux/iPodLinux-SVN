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

#define PZMOD
#include "pz.h"
#include "ipod.h"
#include "slider.h"

// Needed because ipod_set_setting takes a short as first parm
void slider_set_setting (int setting, int val) 
{ ipod_set_setting (setting, val); }

void new_settings_slider_window(char *title, int setting,
				int slider_min, int slider_max)
{
    int tval = ipod_get_setting (setting);
    
    TWindow *win = ttk_new_window();
    TWidget *slider = ttk_new_slider_widget (0, 0, ttk_screen->w * 3 / 5, slider_min, slider_max, &tval, 0);
    ttk_slider_set_callback (slider, slider_set_setting, setting);
    ttk_window_set_title (win, title);
    ttk_add_widget (win, slider);
    ttk_popup_window (win);
    return;
}

void new_int_slider_window(char *title, int *setting,
		int slider_min, int slider_max)
{
    TWindow *win = ttk_new_window();
    TWidget *slider = ttk_new_slider_widget (0, 0, ttk_screen->w * 3 / 5, slider_min, slider_max, setting, 0);
    win->title = title;
    ttk_add_widget (win, slider);
    ttk_popup_window (win);
    return;
}
