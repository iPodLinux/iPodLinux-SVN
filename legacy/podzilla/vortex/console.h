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
 *  $Id: vortex.c,v 1.1 2005/05/16 02:53:00 yorgle Exp $
 *
 *  $Log: vortex.c,v $
 *
 */



/* initialize the structures */
void Vortex_Console_Init( void );

/* add a message with a set x/y stepping and text style */
#define VORTEX_STYLE_NORMAL	(0)
#define VORTEX_STYLE_BOLD	(1)
void Vortex_ConsoleMessage( char * text, int xs, int ys, int style );

/* time passes */
void Vortex_ConsoleTick( void );

/* draw the console to the wid/gc */
void Vortex_Console_Render( GR_WINDOW_ID wid, GR_GC_ID gc );
