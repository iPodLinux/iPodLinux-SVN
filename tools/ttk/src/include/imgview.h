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

#ifndef _TTK_IMGVIEW_H_
#define _TTK_IMGVIEW_H_

TWidget *ttk_new_imgview_widget (int w, int h, ttk_surface img);

void ttk_imgview_draw (TWidget *this, ttk_surface srf);
int ttk_imgview_scroll (TWidget *this, int dir);
int ttk_imgview_down (TWidget *this, int button);
void ttk_imgview_free (TWidget *this);

TWindow *ttk_mh_imgview (struct ttk_menu_item *this);
void *ttk_md_imgview (ttk_surface srf);

#endif
