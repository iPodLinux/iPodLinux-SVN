/*
 * console - a text zoom, scroll thingy for messages
 * Copyright (C) 2005 Scott Lawrence
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

#include "pz.h"


/* initialize the structures */
void Vortex_Console_Init( void );

/* add a message with a set x/y stepping and text style */
#define VORTEX_STYLE_NORMAL	(0)
#define VORTEX_STYLE_BOLD	(1)
void Vortex_Console_AddItem( char * text, int xs, int ys, int style );

/* time passes */
void Vortex_Console_Tick( void );

/* set the hidden state of the static text */
void Vortex_Console_HiddenStatic( int hidden );

/* draw the console to the wid/gc */
void Vortex_Console_Render( ttk_surface srf, ttk_color col );

/* "accessors" for various statistics */
int Vortex_Console_GetZoomCount( void );
int Vortex_Console_GetStaticCount( void );

