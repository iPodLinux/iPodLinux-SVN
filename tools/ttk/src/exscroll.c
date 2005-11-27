/*
 * Copyright (c) 2005 Joshua Oreman
 *
 * This file is a part of TTK.
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

#include "ttk.h"
#include <stdlib.h>

void pw_draw (TWidget *this, ttk_surface screen)
{
    ttk_text (screen, ttk_textfont, this->x, this->y, ttk_makecol (0,0,0),
	      "Hold button pressed.");
}

int pw_button (TWidget *this, int button, int time)
{
    if (button == TTK_BUTTON_HOLD) {
	ttk_popdown_window (this->win);
	return TTK_EV_CLICK;
    }
    return TTK_EV_UNUSED;
}

void new_testpopup_window() 
{
    TWindow *popup = ttk_new_window();
    TWidget *popwid = ttk_new_widget (0, 0);
    popwid->w = ttk_text_width (ttk_textfont, "Hold button pressed.");
    popwid->h = ttk_text_height (ttk_textfont);
    popwid->focusable = 1;
    popwid->dirty = 1;
    popwid->draw = pw_draw;
    popwid->button = pw_button;

    ttk_add_widget (popup, popwid);
    ttk_window_title (popup, "Please Hold");
    ttk_popup_window (popup);
}

typedef struct 
{
    int scrollnum;
    int lasttap;
} sw_data;

void sw_draw (TWidget *this, ttk_surface screen)
{
    sw_data *data = (sw_data *)this->data;
    char buf[64];
    
    sprintf (buf, "%d", data->scrollnum);
    ttk_text (screen, ttk_textfont, this->x, this->y, ttk_makecol (0,0,0), buf);
    sprintf (buf, "%d", data->lasttap);
    ttk_text (screen, ttk_textfont, this->x, this->y + ttk_text_height (ttk_textfont) + 6,
	      ttk_makecol (0,0,0), buf);
}

int sw_scroll (TWidget *this, int dir)
{
    sw_data *data = (sw_data *)this->data;
    data->scrollnum += dir;
    this->dirty++;
    return 0;
}

int sw_resettap (TWidget *this) 
{
    sw_data *data = (sw_data *)this->data;
    data->lasttap = 0;
    this->dirty++;
    return 0;
}

int sw_tap (TWidget *this, int where)
{
    sw_data *data = (sw_data *)this->data;
    data->lasttap = where;
    this->dirty++;
    return TTK_EV_CLICK;
}

int sw_down (TWidget *this, int button)
{
    if (button == TTK_BUTTON_HOLD) {
	new_testpopup_window();
	return TTK_EV_CLICK;
    }
    return TTK_EV_UNUSED;
}

int sw_button (TWidget *this, int button, int time) 
{
    sw_data *data = (sw_data *)this->data;
    switch (button) {
    case TTK_BUTTON_MENU:
	if (ttk_hide_window (this->win) == -1) {
	    ttk_quit();
	    exit (0);
	}
	break;
    case TTK_BUTTON_PREVIOUS:
	data->scrollnum -= 10;
	break;
    case TTK_BUTTON_NEXT:
	data->scrollnum += 10;
	break;
    case TTK_BUTTON_PLAY:
	data->scrollnum = data->lasttap = 0;
	break;
    case TTK_BUTTON_ACTION:
	data->lasttap = 100;
	break;
    }

    this->dirty++;
    return 0;
}

int main (int argc, char **argv) 
{
    TWindow *mainwindow;
    TWidget *scrollwidget;
    mainwindow = ttk_init();
    ttk_menufont = ttk_get_font ("Chicago", 12);
    ttk_textfont = ttk_get_font ("Espy Sans", 10);
    
    scrollwidget = ttk_new_widget (mainwindow->w / 2 - ttk_text_width (ttk_textfont, "9"),
				   mainwindow->h / 2 - ttk_text_height (ttk_textfont) - 3);
    scrollwidget->w = ttk_text_width (ttk_textfont, "99999");
    scrollwidget->h = ttk_text_height (ttk_textfont) * 2 + 6;
    scrollwidget->focusable = 1;
    scrollwidget->dirty = 1;
    scrollwidget->draw = sw_draw;
    scrollwidget->scroll = sw_scroll;
    scrollwidget->stap = sw_tap;
    scrollwidget->button = sw_button;
    scrollwidget->down = sw_down;
    scrollwidget->frame = sw_resettap;
    scrollwidget->data = calloc (sizeof(sw_data), 1);

    ttk_widget_set_inv_fps (scrollwidget, 3); // 3 sec per frame
    
    ttk_add_widget (mainwindow, scrollwidget);
    ttk_window_title (mainwindow, "Scroll Test");
    ttk_run();
}
