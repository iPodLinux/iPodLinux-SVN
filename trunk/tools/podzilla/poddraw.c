/*
 *  Pod Draw
 *
 * Copyright (C) 2005 Scott Lawrence (BleuLlama)
 *
 *  Commands are:
 	PLAY	clear the screen
	CW	right or down
	CCW	left or up
	ACTN	toggle LR/UD
	|<< 	color -
	>>|	color +
	MENU	exit
 
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#include "pz.h"
#include "ipod.h"

static GR_WINDOW_ID	poddraw_wid;
static GR_GC_ID		poddraw_gc;
static GR_TIMER_ID      poddraw_timer;

static int poddraw_color = 0;
static int poddraw_x = 0;
static int poddraw_y = 0;
static int updown = 0;

#define PDMIN( A, B )   (((A)<(B))?(A):(B))
#define PDMAX( A, B )   (((A)>(B))?(A):(B))

static void poddraw_place_point( int c )
{
    if( (c & 0x03) == 0 ) 
	GrSetGCForeground( poddraw_gc, BLACK);
    else if( (c & 0x03) == 1 ) 
	GrSetGCForeground( poddraw_gc, GRAY);
    else if( (c & 0x03) == 2 ) 
	GrSetGCForeground( poddraw_gc, LTGRAY);
    else if( (c & 0x03) == 3 ) 
	GrSetGCForeground( poddraw_gc, WHITE);

    GrPoint( poddraw_wid, poddraw_gc, poddraw_x,   poddraw_y );
    GrPoint( poddraw_wid, poddraw_gc, poddraw_x+1, poddraw_y+1 );
    GrPoint( poddraw_wid, poddraw_gc, poddraw_x+1, poddraw_y );
    GrPoint( poddraw_wid, poddraw_gc, poddraw_x,   poddraw_y+1 );
}

static void poddraw_point( void )
{
    poddraw_place_point( poddraw_color );
}

static void poddraw_cycle_point( void )
{
    static int c = 0;
    poddraw_place_point( c++ );
}

static int poddraw_handle_event(GR_EVENT * event)
{
    int ret = 0;
    switch( event->type )
    {
    case( GR_EVENT_TYPE_TIMER ):
	poddraw_cycle_point();
	break;
	

    case( GR_EVENT_TYPE_KEY_DOWN ):
	switch( event->keystroke.ch )
	{
	    case '\r':
	    case '\n': /* action */
		// toggle lr-ud
		updown = (updown + 1) & 0x01;
		break;

	    case 'p': /* play/pause */
	    case 'd': /*or this */
		// clear screen
		GrSetGCBackground( poddraw_gc, WHITE);
		GrClearWindow( poddraw_wid, 0);
		break;

	    case 'f': /* >>| */
		poddraw_color = (poddraw_color +1 ) & 0x03;
		break;

	    case 'w': /* |<< */
		poddraw_color = (poddraw_color -1 ) & 0x03;
		break;

	    case 'l': /* CCW spin */
		poddraw_point();
		if( updown ) poddraw_x = PDMAX( poddraw_x-2, 0 );
		else         poddraw_y = PDMAX( poddraw_y-2, 0 );
		break;

	    case 'r': /* CW spin */
		poddraw_point();
		if( updown )
		    poddraw_x = PDMIN( poddraw_x+2, screen_info.cols-2 );
		else
		    poddraw_y = PDMIN( poddraw_y+2, 
				  (screen_info.rows - (HEADER_TOPLINE + 1))-2);
		break;

	    case 'm':
		GrDestroyTimer( poddraw_timer );
		pz_close_window( poddraw_wid );
		ret = 1;
		break;
	}
	break;
    }

    return ret;
}

static void poddraw_do_draw( void )
{
    pz_draw_header( "PodDraw" );
    poddraw_cycle_point();
}

void new_poddraw_window( void )
{
    poddraw_gc = pz_get_gc(1);
    GrSetGCUseBackground(poddraw_gc, GR_FALSE);
    GrSetGCForeground(poddraw_gc, BLACK);

    poddraw_wid = pz_new_window(0, HEADER_TOPLINE + 1, 
	screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), 
	poddraw_do_draw, poddraw_handle_event);

    GrSelectEvents( poddraw_wid, GR_EVENT_MASK_TIMER|
	GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_UP|GR_EVENT_MASK_KEY_DOWN);

    poddraw_timer = GrCreateTimer( poddraw_wid, 100 );

    GrMapWindow( poddraw_wid );
    poddraw_color = 0;
    poddraw_x = screen_info.cols/2;
    poddraw_y = (screen_info.rows - (HEADER_TOPLINE + 1))/2;

}
