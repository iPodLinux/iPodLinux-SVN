/*
 * Copyright (c) 2005 Joshua Oreman, David Carne, and Courtney Cavin
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

void draw_lack_of_text_input_method (TWidget *this, ttk_surface srf) 
{
    ttk_text (srf, ttk_menufont, this->x + 3, this->y + 1, ttk_makecol (BLACK),
              _("No text input method selected."));
    ttk_text (srf, ttk_menufont, this->x + this->w - ttk_text_width (ttk_menufont, _("[press a key]")) - 3,
              this->y + 3 + ttk_text_height (ttk_menufont), ttk_makecol (BLACK), _("[press a key]"));
}

int close_lack_of_text_input_method (TWidget *this, int button, int time) 
{
    ttk_input_end();
    return 0;
}

TWidget *new_lack_of_text_input_method() 
{
    TWidget *ret = ttk_new_widget (0, 0);
    ret->w = ttk_screen->w;
    ret->h = 2 * (ttk_text_height (ttk_menufont) + 2);
    ret->draw = draw_lack_of_text_input_method;
    ret->button = close_lack_of_text_input_method;
    return ret;
}

static TWidget *(*handler)();
static TWidget *(*handler_n)();

void pz_register_input_method (TWidget *(*h)()) { handler = h; }
void pz_register_input_method_n (TWidget *(*h)()) { handler_n = h; }
int pz_start_input() { return ttk_input_start (handler()); }
int pz_start_input_n() { return ttk_input_start (handler_n()); }
int pz_start_input_for (TWindow *win) { return ttk_input_start_for (win, handler()); }
int pz_start_input_n_for (TWindow *win) { return ttk_input_start_for (win, handler_n()); }
