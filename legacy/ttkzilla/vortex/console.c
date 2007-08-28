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
 *  $Id: console.c,v 1.1 2005/07/19 02:40:34 yorgle Exp $
 *
 *  $Log: console.c,v $
 *  Revision 1.1  2005/07/19 02:40:34  yorgle
 *  Created the console system (zooms text from the center, then logs it to the bottom left for a little while)
 *  Web rendering rewrite in progress
 *
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "../pz.h"
#include "../vectorfont.h"
#include "globals.h"
#include "console.h"


#define VORTEX_CONSOLE_INACTIVE	(0)	/* item is empty */
#define VORTEX_CONSOLE_ZOOM	(1)	/* item is enlarging on the screen */
#define VORTEX_CONSOLE_STATIC	(2)	/* item is static in bottom left */

#define VORTEX_NCONSOLE_ITEMS	(20)	/* number of console entries */
#define VORTEX_CONSOLE_BUFSIZE	(32)	/* text buffer size */
#define VORTEX_CONSOLE_CAROUSEL	(30)	/* logans run reference */

typedef struct vortex_console {
	int	state;		/* state - see above */
	int	scale;		/* if state is ZOOM, this is the size */
	int 	xc;		/* x center */
	int 	yc;		/* y center */
	int 	xs;		/* x step */
	int 	ys;		/* y step */
	int 	style;		/* text rendering style */
	int 	itemNumber;	/* if state is STATIC, this is log position */
	int	age;		/* how old it is */
	char 	buf[VORTEX_CONSOLE_BUFSIZE];	/* text */
} vortex_console;

static vortex_console consolebuf[VORTEX_NCONSOLE_ITEMS];


/* initialize the structures */
void Vortex_Console_Init( void )
{
	int x;
	for( x=0; x<VORTEX_NCONSOLE_ITEMS; x++ )
	{
		consolebuf[x].state = VORTEX_CONSOLE_INACTIVE;
		consolebuf[x].scale = 0;
		consolebuf[x].xc = 0;
		consolebuf[x].yc = 0;

		consolebuf[x].itemNumber = 0;
		consolebuf[x].buf[0] = '\0';
	}
}

/* find a free entry in the array, or clear one out */
static int Vortex_FindOrClearSpace( void )
{
	int x;
	int oldest_item = -1;
	int itemno = -1;

	/* first, check for empty slot */
	for( x=0 ; x<VORTEX_NCONSOLE_ITEMS; x++ )
	{
		/* check to see if it's inactive */
		if( consolebuf[x].state == VORTEX_CONSOLE_INACTIVE )
			return( x );

		/* check to see if it's oldest */
		if( consolebuf[x].itemNumber > oldest_item ) {
			oldest_item = consolebuf[x].itemNumber;
			itemno = x;
		}
	}

	/* nope, no free slots, relinquish the oldest item */
	return( itemno );
}


/* add a message with a set x/y stepping and text style */
void Vortex_ConsoleMessage( char * text, int xs, int ys, int style )
{
	int pos = Vortex_FindOrClearSpace();
	consolebuf[pos].state = VORTEX_CONSOLE_ZOOM;
	consolebuf[pos].scale = 0;
	consolebuf[pos].itemNumber = 0;
	consolebuf[pos].age = 0;

	consolebuf[pos].xc = Vortex_globals.width>>1;
	consolebuf[pos].yc = Vortex_globals.height>>1;
	consolebuf[pos].xs = xs;
	consolebuf[pos].ys = ys;
	consolebuf[pos].style = style;

	snprintf( consolebuf[pos].buf, VORTEX_CONSOLE_BUFSIZE-1, "%s", text );
	consolebuf[pos].buf[VORTEX_CONSOLE_BUFSIZE-1] = '\0'; //just in case 
}


/* time passes */
void Vortex_ConsoleTick( void )
{
	int x;
	int shift = 0;

	/* first iteration; adjust zooming */
	for( x=0 ; x<VORTEX_NCONSOLE_ITEMS ; x++ )
	{
		if( consolebuf[x].state == VORTEX_CONSOLE_ZOOM ) {
			/* if it's matured past zooming, make it static */
			if( (consolebuf[x].scale++) > 20 ){
				shift++;
				consolebuf[x].state = VORTEX_CONSOLE_STATIC;
				consolebuf[x].itemNumber = -shift;
				consolebuf[x].age = 0;
			} else {
				/* adjust the position */
				consolebuf[x].xc += consolebuf[x].xs;
				consolebuf[x].yc += consolebuf[x].ys;
			}
		}
	}

	/* second iteration; shift static items if applicable */
	for( x=0 ; x<VORTEX_NCONSOLE_ITEMS ; x++ )
	{
		if( consolebuf[x].state == VORTEX_CONSOLE_STATIC ) {
			consolebuf[x].itemNumber += shift;

			/* if it's old, remove it */
			if( (consolebuf[x].age++) > VORTEX_CONSOLE_CAROUSEL ) {
				consolebuf[x].state = VORTEX_CONSOLE_INACTIVE;
			}
		}
	}

}


/* draw the console to the wid/gc */
void Vortex_Console_Render( GR_WINDOW_ID wid, GR_GC_ID gc )
{
	int x;
	int ypos;

	for( x=0 ; x<VORTEX_NCONSOLE_ITEMS ; x++ )
	{
		/* zoomy text, scale it from the center */
		if( consolebuf[x].state == VORTEX_CONSOLE_ZOOM ) {
			vector_render_string_center( 
				Vortex_globals.wid, Vortex_globals.gc,
				consolebuf[x].buf, 1, 
				consolebuf[x].scale, 
				consolebuf[x].xc,
				consolebuf[x].yc );

			if( consolebuf[x].style == VORTEX_STYLE_BOLD ) {
				vector_render_string_center( 
					Vortex_globals.wid, Vortex_globals.gc,
					consolebuf[x].buf, 1, 
					consolebuf[x].scale, 
					consolebuf[x].xc+1,
					consolebuf[x].yc+1 );
			}
		}

		/* static text, draw it in the lower left */
		if( consolebuf[x].state == VORTEX_CONSOLE_STATIC ) {
			ypos = Vortex_globals.height
					- (10*(consolebuf[x].itemNumber+1));
			if( ypos > 16 ) {
				vector_render_string( 
					Vortex_globals.wid, Vortex_globals.gc,
					consolebuf[x].buf, 1, 1, 1, ypos );
			}
		}
	}
}
