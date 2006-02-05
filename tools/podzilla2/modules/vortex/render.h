/*
 *     Vortex - render routines
 *	these routines draw stuff.
 *  
 * Copyright (C) 2006 Scott Lawrence
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */


void Vortex_OutlinedTextCenter( ttk_surface srf, const char *string, 
			int x, int y, int cw, int ch, int kern, 
			ttk_color col, ttk_color olc );

void Vortex_RenderWeb( ttk_surface srf );
void Vortex_RenderPlayer( ttk_surface srf );
void Vortex_RenderAvailableBase( ttk_surface srf, int x, int y );
void Vortex_RenderAvailableBases( ttk_surface srf );
void Vortex_RenderTimedQuad( ttk_surface srf,
			     char *buf0, char *buf1, char *buf2, char *buf3,
			     int x, int y, int w, int h,
			     ttk_color color );
