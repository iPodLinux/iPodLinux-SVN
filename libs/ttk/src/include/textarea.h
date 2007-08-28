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

#ifndef _TTK_TEXTAREA_H_
#define _TTK_TEXTAREA_H_

TWidget *ttk_new_textarea_widget (int w, int h, const char *text, ttk_font font, int baselineskip);

void ttk_textarea_draw (TWidget *_this, ttk_surface srf);
int ttk_textarea_scroll (TWidget *_this, int dir);
int ttk_textarea_down (TWidget *_this, int button);
void ttk_textarea_free (TWidget *_this);

TWindow *ttk_mh_textarea (struct ttk_menu_item *_this);
void *ttk_md_textarea (char *text, int baselineskip);

#endif

