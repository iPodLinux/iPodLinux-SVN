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

#define TTK_ICON_WIDTH   8
#define TTK_ICON_HEIGHT  13
extern unsigned char ttk_icon_sub[], ttk_icon_spkr[], ttk_icon_play[], ttk_icon_pause[], ttk_icon_exe[],
    ttk_icon_battery[], ttk_icon_charging[], ttk_icon_hold[];

void ttk_draw_icon (unsigned char *icon, ttk_surface srf, int x, int y, TApItem *ap, ttk_color bgcol);
