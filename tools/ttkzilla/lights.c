/*
 * Lights - A Lights Out like game...
 * Copyright (C) 2005 Scott Lawrence
 *
 *   Initially written in one evening, 2005-04-07
 */

/*
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
 */

/*
 * Release versions:
 *
 *  2005-04-07 - initial release, menu based game selection
 *
 *  2005-04-08 - updated to be easier to see on real hardware
 *		 better cursor display (blinking)
 *		 7x7 modes, and dynamic state array
 *
 *  2005-04-12 - Removed dynamic state array (everything static for stability)
 *		 Added 3x3 - 8x8 for all modes (not Merlin)
 *		 reorganized the menus to be cleaner (submenu for size select)
 *		 Quite probably added color support (reddish lights)
 *		 added animation for 'hold' mode (light chasing)
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "pz.h"

static GR_TIMER_ID 	lights_timer = 0;
static GR_GC_ID		lights_gc = 0;
static GR_WINDOW_ID	lights_bufwid = 0;
static GR_WINDOW_ID	lights_wid = 0;
static GR_SCREEN_INFO	lights_screen_info;
static int lights_height = 0;	/* height of the screen */

static char lights_hdr[80];	/* buffer for the header text */
static int lights_count;	/* number of boxes on each side */
static int lights_sel = 4;	/* menu selection (to make merlin independant)*/
static int lights_max;		/* count * count -- total # buttons */
static int lights_size;		/* size of each box on each side */

static int lights_x;		/* x position of top light button */
static int lights_curr;		/* currently selected button */
static int lights_cursblink;	/* for the blinking cursor */

static int lights_hold_engaged;	/* is the hold switch on? */
static int lights_anim = 0;	/* global thing for animating during hold */

static int lights_nmoves;	/* number of moves so far */
static int lights_litcount;	/* number of lit lights */
static int lights_won;		/* did we win? */

static int lights_states[64];		/* one int for each button */
static int lights_states_bak[64];	/* one int for each button */
					/* good for up to 8x8 */

static int lights_mode;		/* what variant of the game are we playing? */
#define LIGHTS_MODE_NORMAL	(0)
#define LIGHTS_MODE_LITONLY	(1)
#define LIGHTS_MODE_WRAP	(2)
#define LIGHTS_MODE_LITWRAP	(3)
#define LIGHTS_MODE_MERLIN	(4)

/* toggle states for merlin mode.
    - we don't need tables like this for the other modes, 
      since those are computed at game time.
 */
static int lights_toggle_merlin[9][6] = {
	{ 0, 1, 3, 4, -1 }, 	/* pressing 0 toggles 0, 1, 3, and 4 */
	{ 1, 0, 2, -1 },	/* pressing 1 toggles 1, 0, and 2 */
	{ 2, 1, 4, 5, -1 },	/* etc... */
	{ 3, 0, 6, -1 },
	{ 4, 1, 3, 5, 7, -1},
	{ 5, 2, 8, -1 },
	{ 6, 3, 4, 7, -1 },
	{ 7, 6, 8, -1 },
	{ 8, 4, 5, 7, -1 },
	/* only 9 entries since merlin is always 3x3 lights */
};


/* a simple random number routine */
int lights_rand( int max )
{
        return (int)((float)max * rand() / (RAND_MAX + 1.0));
}


/* toggle a single light */
void lights_toggle( int id )
{
	if( id > (lights_max-1)) return;
	if( lights_states[id] )
		lights_states[id] = 0;
	else
		lights_states[id] = 1;
}


/* count up the number of lit lights */
void lights_recount( void )
{
	int x;
	lights_litcount = 0;
	for( x=0 ; x<lights_max ; x++ ) {
		if( lights_states[x] )
			lights_litcount++;
	}
}


/* convert X,Y to index */
#define lights_id_at( x, y )   ((y) * lights_count) + (x) 


/* do the appropriate behavior for a button press */
void lights_do_button( int id )
{
	int x, y;
	int w = 0;

	if( lights_mode == LIGHTS_MODE_MERLIN ) {
		for( x=0 ; lights_toggle_merlin[id][x] != -1 ; x++ ) {
			lights_toggle( lights_toggle_merlin[id][x] );
		}
	} else {
		if(    lights_mode == LIGHTS_MODE_LITWRAP 
		    || lights_mode == LIGHTS_MODE_WRAP )  w = 1;

		lights_toggle( id );

		x = id % lights_count;
		y = id / lights_count;

		/* left */
		if( x>0 ) 	
			lights_toggle( lights_id_at( x-1, y ));
		else if( w )
			lights_toggle( lights_id_at( lights_count-1, y ));

		/* right */
		if( x<lights_count-1 )
			lights_toggle( lights_id_at( x+1, y ));
		else if( w )
			lights_toggle( lights_id_at( 0, y ));

		/* up */
		if( y>0 )
			lights_toggle( lights_id_at( x, y-1 ));
		else if( w )
			lights_toggle( lights_id_at( x, lights_count-1 ));

		/* down */
		if( y<lights_count-1 )
			lights_toggle( lights_id_at( x, y+1 ));
		else if( w )
			lights_toggle( lights_id_at( x, 0 ));
	}
}


/* do the appropriate behavior for the user pressing a button */
void lights_press_button( int id )
{
	if( 	( lights_mode == LIGHTS_MODE_LITONLY )
	    || 	( lights_mode == LIGHTS_MODE_LITWRAP ) ) {
		if( !lights_states[id] ) return;
	}

	/* inc the counter */
	if( !lights_won )
		lights_nmoves++;

	/* toggle the button */
	lights_do_button( id );

	/* now re-count the number lit */
	lights_recount();
}


/* start up a new game... */
void lights_new_game( void )
{
	int x;

	/* reset the game board */
	lights_won = 0;
	for( x=0 ; x<lights_max ; x++ ) {
		lights_states[x] = 0;
		lights_states_bak[x] = 0;
	}

	lights_litcount = 0;

	/* this insures that all puzzles generated are solvable */
	for( x=0 ; x<((lights_max)>>1) ; x++ )
	{
		lights_do_button( lights_rand( lights_max ));
	}

	/* update the count for the screen */
	lights_recount();

	/* initialize game values */
	lights_curr = lights_max >> 1;
	lights_nmoves = 0;
}


/* draw a button to the screen at x,y using info from id */
void lights_draw_button( int x, int y, int id )
{
	int ls2 = lights_size/2;

	/* change color based on if it's lit or not */
	if( lights_states[id] ) {
		if( lights_screen_info.bpp == 16 )
		    GrSetGCForeground( lights_gc, GR_RGB( 255, 100, 50 ));
		else 
		    GrSetGCForeground( lights_gc, WHITE );
	} else {
		GrSetGCForeground( lights_gc, BLACK );
	}
	GrFillEllipse( lights_bufwid, lights_gc, 
		ls2+x, ls2+y, ls2-2, ls2-2 );

	/* draw the container */
	if( lights_screen_info.bpp == 16 )
	    GrSetGCForeground( lights_gc, GR_RGB( 102, 0, 0 ));
	else 
	    GrSetGCForeground( lights_gc, GRAY );
	GrEllipse( lights_bufwid, lights_gc, 
		    ls2+x, ls2+y, ls2-1, ls2-1 );

	/* draw the cursor if applicable */
	if( id == lights_curr  &&  !lights_hold_engaged )
	{
		if( lights_cursblink ) {
		    if( lights_screen_info.bpp == 16 )
			GrSetGCForeground( lights_gc, GR_RGB( 0, 255, 0 ));
		    else
			GrSetGCForeground( lights_gc, WHITE );
		} else {
		    GrSetGCForeground( lights_gc, BLACK );
		}
		GrEllipse( lights_bufwid, lights_gc, 
			ls2+x, ls2+y, ls2, ls2 );
		GrEllipse( lights_bufwid, lights_gc, 
			ls2+x, ls2+y, ls2-1, ls2-1 );
		GrEllipse( lights_bufwid, lights_gc, 
			ls2+x, ls2+y, ls2-1, ls2 );
		GrEllipse( lights_bufwid, lights_gc, 
			ls2+x, ls2+y, ls2, ls2-1 );

		if( lights_cursblink ) {
			GrSetGCForeground( lights_gc, BLACK );
		} else {
		    if( lights_screen_info.bpp == 16 )
			GrSetGCForeground( lights_gc, GR_RGB( 0, 255, 255 ));
		    else
			GrSetGCForeground( lights_gc, WHITE );
		}
		GrEllipse( lights_bufwid, lights_gc, 
			ls2+x, ls2+y, ls2-2, ls2-2 );
	}
}


/* redraw everything */
static void lights_do_draw( void )
{
	int w;
	int x, y;
	char buf[32];

	/* start clear */
	GrSetGCForeground( lights_gc, BLACK );
	GrFillRect( lights_bufwid, lights_gc, 0, 0,
		    lights_screen_info.cols, lights_height );

	/* draw the buttons */
	w=0;
	for( y=0 ; y<lights_count ; y++ )
	{
		for( x=0 ; x<lights_count ; x++ )
		{
			lights_draw_button( lights_x+(lights_size*x),
				     1+(lights_size*y), w );
			w++;
		}
	}

	/* draw the stats */
	GrSetGCForeground( lights_gc, WHITE );
	GrText( lights_bufwid, lights_gc, 3, 20, "Mov", 3, GR_TFASCII );
	snprintf( buf, 16, "%2d", lights_nmoves );
	GrText( lights_bufwid, lights_gc, 5, 35, buf, 3, GR_TFASCII );

	GrText( lights_bufwid, lights_gc, 3, 70, "Lit", 3, GR_TFASCII );
	snprintf( buf, 16, "%2d", lights_litcount );
	GrText( lights_bufwid, lights_gc, 5, 85, buf, 3, GR_TFASCII );

	/* copy the buffer into place */
	GrCopyArea( lights_wid, lights_gc, 0, 0,
		    lights_screen_info.cols, 
		    (screen_info.rows - (HEADER_TOPLINE + 1)),
		    lights_bufwid, 0, 0, MWROP_SRCCOPY);

	if( !lights_won && lights_litcount==0 )
	{
		lights_won = 1;
		snprintf( buf, 32, "You won in %d moves!", lights_nmoves );
		new_message_window( buf );
	}
}


/* exit, release all created stuff */
void lights_exit( void ) 
{
	GrDestroyTimer( lights_timer );
	GrDestroyGC( lights_gc );
	pz_close_window( lights_wid );
	GrDestroyWindow( lights_bufwid );
}


/* user switched on hold */
void lights_engage_hold( void )
{
	int x;
	lights_hold_engaged = 1;
	lights_anim = 0;

	/* store aside the states */
	for( x=0 ; x<lights_max ; x++ ) {
		lights_states_bak[x] = lights_states[x];
	}
}


/* user switched off hold */
void lights_release_hold( void )
{
	int x;

	/* restore the states */
	for( x=0 ; x<lights_max ; x++ ) {
		lights_states[x] = lights_states_bak[x];
	}

	lights_hold_engaged = 0;
}


/* do something fun with the lights */
void lights_animate( void )
{
	int x,y,z;

	/* do a vertical light chasing like thing */
	for( z=0 ; z<lights_count ; z++ ) {
		if( lights_states[lights_anim] )
		{
			x = lights_anim % lights_count;
			y = (lights_anim / lights_count)+1;
			if( y>= lights_max )  y = 0;
			
			lights_do_button( lights_id_at( x, y ));
		}
		lights_anim++;
		if( lights_anim >= lights_max ) lights_anim = 0;
	}

	/* keep it from showing a solution - randomly toggle a bit */
	lights_toggle( lights_rand( lights_max-1 ));
}


/* event handler */
static int lights_handle_event (GR_EVENT *event)
{
	switch( event->type )
	{
	    case GR_EVENT_TYPE_TIMER:
		if( !lights_hold_engaged ) {
		    if( lights_cursblink )	lights_cursblink=0;
		    else			lights_cursblink=1;
		} else {
		    lights_animate( );
		}
		lights_do_draw( );
		break;

	    case GR_EVENT_TYPE_KEY_UP:
		switch (event->keystroke.ch)
		{
		case IPOD_SWITCH_HOLD: // hold released
		    lights_release_hold();
		    break;
		}
		break;

	    case GR_EVENT_TYPE_KEY_DOWN:
		switch (event->keystroke.ch)
		{
		case IPOD_BUTTON_ACTION: // Wheel button
		    lights_press_button( lights_curr );
		    lights_do_draw( );
		    break;

		case IPOD_WHEEL_ANTICLOCKWISE: // Wheel left
		    lights_curr--;
		    if( lights_curr < 0 )
		    {
			    lights_curr = lights_max-1;
		    }
		    lights_cursblink = 1; /* force the cursor visible */
		    lights_do_draw( );
		    break;

		case IPOD_WHEEL_CLOCKWISE: // Wheel right
		    lights_curr++;
		    if( lights_curr >= lights_max )
		    {
			    lights_curr = 0;
		    }
		    lights_cursblink = 1; /* force the cursor visible */
		    lights_do_draw( );
		    break;

		case IPOD_SWITCH_HOLD: // Hold engaged
		    lights_engage_hold();
		    break;

		case IPOD_BUTTON_MENU: // Menu button
		case 'q': // Quit
		    lights_exit();
		    break;

		case IPOD_BUTTON_PLAY: // Play/pause button
		case IPOD_BUTTON_REWIND: // Rewind button
		case IPOD_BUTTON_FORWARD: // Fast forward button
		default:
		     return EVENT_UNUSED;
		    break;
		} // keystroke
		break;   // key down

	} // event type

	return 1;
}


/* initialize the window */
void new_lights_window(void)
{
	/* Init randomizer */
	srand(time(NULL));

	lights_gc = pz_get_gc (1);
        GrSetGCUseBackground(lights_gc, GR_FALSE);
        GrSetGCForeground(lights_gc, BLACK);

	lights_height = (lights_screen_info.rows - (HEADER_TOPLINE + 1));

	lights_wid = pz_new_window( 0, HEADER_TOPLINE + 1,
		    screen_info.cols, lights_height,
		    lights_do_draw, lights_handle_event );

	lights_bufwid = GrNewPixmap( lights_screen_info.cols, 
					lights_height, NULL );

        GrSelectEvents( lights_wid, GR_EVENT_MASK_TIMER
					| GR_EVENT_MASK_EXPOSURE
					| GR_EVENT_MASK_KEY_UP
					| GR_EVENT_MASK_KEY_DOWN );

	lights_timer = GrCreateTimer( lights_wid, 300 );

	GrMapWindow( lights_wid );

	/* intialize game state */
	lights_new_game();
}


/* do common startup thing for all variants */
static void lights_common_start( int mode, char * hdr )
{
    lights_hold_engaged = 0;
    GrGetScreenInfo(&lights_screen_info);
    lights_height = (lights_screen_info.rows - (HEADER_TOPLINE + 1));

    lights_mode  = mode;
    lights_max   = lights_count * lights_count;
    lights_size  = lights_height/lights_count;
    lights_x     = (lights_screen_info.cols/2) - (lights_count*lights_size/2); 

    pz_draw_header( hdr );
    new_lights_window();
}



/* variant entry points for the menu */
static void lights_start_merlin( void ) {
    lights_count = 3;
    snprintf( lights_hdr, 80, "Lights: Merlin" );
    lights_common_start( LIGHTS_MODE_MERLIN, lights_hdr );
}

static void lights_start_original( void ) {
    lights_count = lights_sel;
    snprintf( lights_hdr, 80, "Lights: %dx%d, Wrap", 
		lights_count, lights_count );
    lights_common_start( LIGHTS_MODE_WRAP, lights_hdr );
}

static void lights_start_lit_only( void ) {
    lights_count = lights_sel;
    snprintf( lights_hdr, 80, "Lights: %dx%d, Lit Only, Wrap", 
		lights_count, lights_count );
    lights_common_start( LIGHTS_MODE_LITWRAP, lights_hdr );
}

static void lights_start_original_nw( void ) {
    lights_count = lights_sel;
    snprintf( lights_hdr, 80, "Lights: %dx%d, No Wrap", 
		lights_count, lights_count );
    lights_common_start( LIGHTS_MODE_NORMAL, lights_hdr );
}

static void lights_start_lit_only_nw( void ) {
    lights_count = lights_sel;
    snprintf( lights_hdr, 80, "Lights: %dx%d, Lit Only, No Wrap", 
		lights_count, lights_count );
    lights_common_start( LIGHTS_MODE_LITONLY, lights_hdr );
}



/* size selectors... */
static TWindow* lights_set_size_3( ttk_menu_item *i ) { lights_sel = 3; return TTK_MENU_UPONE; }
static TWindow* lights_set_size_4( ttk_menu_item *i ) { lights_sel = 4; return TTK_MENU_UPONE; }
static TWindow* lights_set_size_5( ttk_menu_item *i ) { lights_sel = 5; return TTK_MENU_UPONE; }
static TWindow* lights_set_size_6( ttk_menu_item *i ) { lights_sel = 6; return TTK_MENU_UPONE; }
static TWindow* lights_set_size_7( ttk_menu_item *i ) { lights_sel = 7; return TTK_MENU_UPONE; }
static TWindow* lights_set_size_8( ttk_menu_item *i ) { lights_sel = 8; return TTK_MENU_UPONE; }

/* size selection menu */
ttk_menu_item lights_size_menu[] = {
	{ "3x3 Lights", {lights_set_size_3} },
	{ "4x4 Lights", {lights_set_size_4} },
	{ "5x5 Lights", {lights_set_size_5} },
	{ "6x6 Lights", {lights_set_size_6} },
	{ "7x7 Lights", {lights_set_size_7} },
	{ "8x8 Lights", {lights_set_size_8} },
	{ 0 }
};


/* menu hook for letting the user select a variant */
ttk_menu_item lights_menu[] = {
	{ "Play Merlin 3x3", {pz_mh_legacy}, 0, lights_start_merlin },
	{ "Play Original - With Wrap", {pz_mh_legacy}, 0, lights_start_original },
	{ "Play Lit Only - With Wrap", {pz_mh_legacy}, 0, lights_start_lit_only },
	{ "Play Original - No Wrap", {pz_mh_legacy}, 0, lights_start_original_nw },
	{ "Play Lit Only - No Wrap", {pz_mh_legacy}, 0, lights_start_lit_only_nw },
	{ "Select Game Size...", {ttk_mh_sub}, TTK_MENU_ICON_SUB, lights_size_menu },
	{ 0 }
};
