/*
 * console - a text zoom, scroll thingy for messages
 *
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "pz.h"
#include "console.h"


#define VORTEX_CONSOLE_INACTIVE	(0)	/* item is empty */
#define VORTEX_CONSOLE_ZOOM	(1)	/* item is enlarging on the screen */
#define VORTEX_CONSOLE_STATIC	(2)	/* item is static in bottom left */

#define VORTEX_NCONSOLE_ITEMS	(20)	/* number of console entries */
#define VORTEX_CONSOLE_BUFSIZE	(32)	/* text buffer size */

#define VORTEX_CONSOLE_SCALE    (2)     /* how fast it gets larger */
#define VORTEX_CONSOLE_MATURITY (40)	/* at what age to they go static? */
#define VORTEX_CONSOLE_CAROUSEL	(30)	/* logans run reference */

typedef struct vortex_console {
	int		state;		/* state - see above */
	int		scale;		/* if state is ZOOM, this is the size */
	int 		xc;		/* x center */
	int 		yc;		/* y center */
	int 		xs;		/* x step */
	int 		ys;		/* y step */
	int 		style;		/* text rendering style */
	int 		itemNumber;	/* if state is STATIC; log position */
	int		age;		/* how old it is */
	char 		buf[VORTEX_CONSOLE_BUFSIZE];	/* text */
	ttk_color	color;		/* color to render this item */
} vortex_console;

static vortex_console consolebuf[VORTEX_NCONSOLE_ITEMS];

static int static_is_hidden = 0;

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

		consolebuf[x].color = ttk_makecol( BLACK );
	}
	static_is_hidden = 0;
}

int Vortex_Console_GetZoomCount( void )
{
	int x, c=0;
	for( x=0 ; x<VORTEX_NCONSOLE_ITEMS; x++ )
		if( consolebuf[x].state == VORTEX_CONSOLE_ZOOM ) c++;
	return c;
}
int Vortex_Console_GetStaticCount( void )
{
	int x, c=0;
	for( x=0 ; x<VORTEX_NCONSOLE_ITEMS; x++ )
		if( consolebuf[x].state == VORTEX_CONSOLE_STATIC ) c++;
	return c;
}

void Vortex_Console_WipeAllText( void )
{
	int x;
	for( x=0 ; x<VORTEX_NCONSOLE_ITEMS; x++ )
		consolebuf[x].state = VORTEX_CONSOLE_INACTIVE;
}


/* find a free entry in the array, or clear one out */
static int Vortex_Console_FindOrClearSpace( void )
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
void Vortex_Console_AddItem( char * text, int xs, int ys,
				int style, ttk_color color )
{
	int pos = Vortex_Console_FindOrClearSpace();
	consolebuf[pos].state = VORTEX_CONSOLE_ZOOM;
	consolebuf[pos].scale = 0;
	consolebuf[pos].itemNumber = 0;
	consolebuf[pos].age = 0;

	consolebuf[pos].xc = (ttk_screen->w - ttk_screen->wx)>>1;
	consolebuf[pos].yc = (ttk_screen->h - ttk_screen->wy)>>1;
	consolebuf[pos].xs = xs;
	consolebuf[pos].ys = ys;
	consolebuf[pos].style = style;
	consolebuf[pos].color = color;

	snprintf( consolebuf[pos].buf, VORTEX_CONSOLE_BUFSIZE-1, "%s", text );
	consolebuf[pos].buf[VORTEX_CONSOLE_BUFSIZE-1] = '\0'; //just in case 
}



/* time passes */
void Vortex_Console_Tick( void )
{
	int x;
	int shift = 0;

	/* first iteration; adjust zooming */
	for( x=0 ; x<VORTEX_NCONSOLE_ITEMS ; x++ )
	{
		if( consolebuf[x].state == VORTEX_CONSOLE_ZOOM ) {
			/* if it's matured past zooming, make it static */
			consolebuf[x].scale += VORTEX_CONSOLE_SCALE;
			if( consolebuf[x].scale > VORTEX_CONSOLE_MATURITY ){
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

/* hide the static console? */
void Vortex_Console_HiddenStatic( int hidden )
{
	static_is_hidden = hidden;
}

/* draw the console to the wid/gc */
void Vortex_Console_Render( ttk_surface srf )
{
	int x;
	int ypos;

	for( x=0 ; x<VORTEX_NCONSOLE_ITEMS ; x++ )
	{
		/* zoomy text, scale it from the center */
		if( consolebuf[x].state == VORTEX_CONSOLE_ZOOM ) {
			pz_vector_string_center( srf,
				consolebuf[x].buf, 
				consolebuf[x].xc,
				consolebuf[x].yc,
				consolebuf[x].scale, 
				consolebuf[x].scale<<1, 
				1, consolebuf[x].color );

			if( consolebuf[x].style == VORTEX_STYLE_BOLD ) {
				pz_vector_string_center( srf,
					consolebuf[x].buf,
					consolebuf[x].xc+1,
					consolebuf[x].yc+1,
					consolebuf[x].scale, 
					consolebuf[x].scale<<1, 
					1, consolebuf[x].color );
			}
		}

		/* static text, draw it in the lower left */
		if( !static_is_hidden ) {
		    if( consolebuf[x].state == VORTEX_CONSOLE_STATIC ) {
			    ypos = ttk_screen->h - ttk_screen->wy 
				    - (10*(consolebuf[x].itemNumber+1)) - 1;
			    if( ypos > 16 ) {
				    pz_vector_string( srf, consolebuf[x].buf,
					    1, ypos, 5, 9, 1,
					    consolebuf[x].color );
			    }
		    }
		}
	}
}
