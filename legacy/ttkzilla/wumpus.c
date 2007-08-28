/*
 * wumpus - a recreation of the old "Hunt The Wumpus" game
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

/*
 * find the arrow, kill the wumpus 
 * use the hook to get out of a pit
 *  "Play" fires the arrow
 *  wheel  changes direction
 * action  moves
 */

/* 
 * $Log: wumpus.c,v $
 * Revision 1.4  2005/08/02 21:14:22  courtc
 * added more i18n
 *
 * Revision 1.3  2005/06/21 16:46:42  yorgle
 * Cleaned up a lot of the code (re-organized it a bit)
 * Made the colors better for Photo.
 * Fixed a bug where getting eaten by the wumpus didn't kill you sometimes
 *
 * Revision 1.2  2005/06/21 04:27:53  yorgle
 * Missed one "Dragon" that should have been "Wumpus".  oops.
 *
 * Revision 1.1  2005/06/21 04:00:27  yorgle
 * "Hunt The Wumpus" game.  find the arrow, find the wumpus, shoot it, win.
 * Fall in a pit, or get eaten by the wumpus, die.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "pz.h"
#include "vectorfont.h"

static GR_TIMER_ID 	wumpus_timer = 0;
static GR_GC_ID		wumpus_gc = 0;
static GR_WINDOW_ID	wumpus_bufwid = 0;
static GR_WINDOW_ID	wumpus_wid = 0;
static GR_SCREEN_INFO	wumpus_screen_info;
static int 		wumpus_height = 0;

static int wumpus_initialized = 0; /* have the polylines been scaled? */
static int wumpus_blink = 1; /* hooked up to the timer for blinking cursors */

/* current map position... */
static int wumpus_x = 0;
static int wumpus_y = 0;

/* current cursor orientation */
#define WUMPUS_NORTH 	(0)
#define WUMPUS_EAST	(1)
#define WUMPUS_SOUTH	(2)
#define WUMPUS_WEST	(3)

static int wumpus_orientation = WUMPUS_NORTH;

/* inventory */
static int wumpus_hooks = 1;	
static int wumpus_arrows = 0;
static int wumpus_health = 1;

/* map stuff */
static int wumpus_map[10][10];
#define WUMPUS_MAP_EMPTY	(0)
#define WUMPUS_MAP_PIT		(1)
#define WUMPUS_MAP_ARROW	(2)
#define WUMPUS_MAP_HOOK		(3)
#define WUMPUS_MAP_DRAGON	(4)


/* display graphics
    - all of these are in 1/32 of the full width/height of the screen, and
      are scaled to fit the ipod screen in the new_game() function below.
*/

static GR_POINT topleft[] = { /* wall element in the top left */
	{12,14}, {12,10}, {6,10}, {6,14},
	{12,14}, {13,12}, {13,8}, {12,10}
};

static GR_POINT topright[] = { /* wall element in the top right */
	{20,14}, {20,10}, {26,10}, {26,14},
	{20,14}, {19,12}, {19,8}, {20,10}
};

static GR_POINT bottomleft[] = { /* wall element in the bottom left */
	{7,26}, {1,26}, {1,20}, {9,20}, {9,26}, {7,30}, {7,26},
	{7,24}, {9,20},
};

static GR_POINT bottomright[] = { /* wall element in the bottom right */
	{25,26}, {31,26}, {31,20}, {23,20}, {23,26}, {25,30}, {25,26},
	{25,24}, {23,20},
};

/* cursor polylines */
static GR_POINT cursor_north[] = { {14,13}, {16,12}, {18,13}, {14,13} };
static GR_POINT cursor_south[] = { {13,26}, {19,26}, {16,28}, {13,26} };
static GR_POINT cursor_west[] = { {8,17}, {11,16}, {10,18}, {8,17} };
static GR_POINT cursor_east[] = { {24,17}, {21,16}, {22,18}, {24,17} };

/* polyline for the big pit graphic, showing we're in a pit */
static GR_POINT bigpit[] = {
    {8,16}, {11,16}, {12,25}, {20,25}, {21,16}, {24,16}
};

static GR_POINT pit_warning[] = { /* icon: a pit is nearby */
    {2,2}, {3,2}, {4,7}, {7,7}, {8,2}, {9,2}
};

static GR_POINT arrow_warning[] = { /* icon: an arrow is nearby */
    {21,7}, {24,3}, {23,3}, {24,3}, {24,5}, {24,3}
};

static GR_POINT hook_warning[] = { /* icon: a hook is nearby */
    {29,3}, {28,2}, {28,4}, {27,6}, {28,8}, {27,6}
};



/* a routine to display a text banner - not double-buffered. */
static void wumpus_text( char * text )
{
	int w = wumpus_screen_info.cols;
	int h2 = wumpus_height >> 1;
	int w2 = w >> 1;

	/* backing */
	GrSetGCForeground( wumpus_gc, ( wumpus_screen_info.bpp == 16 ) ? 
				GR_RGB( 230, 0, 148 ) : LTGRAY );
        GrFillRect( wumpus_wid, wumpus_gc, 0, h2-14, w, 28 );
        GrSetGCForeground( wumpus_gc, ( wumpus_screen_info.bpp == 16 ) ? 
				GR_RGB( 0, 140, 204 ) : BLACK );
        GrFillRect( wumpus_wid, wumpus_gc, 0, h2-12, w, 24 );
        GrSetGCForeground( wumpus_gc, WHITE );
        GrFillRect( wumpus_wid, wumpus_gc, 0, h2-10, w, 20 );
	
	/* text */
        GrSetGCForeground( wumpus_gc, BLACK );
	vector_render_string_center( wumpus_wid, wumpus_gc, text,
				1, 1, w2, h2 );
}


/* place a random piece any unoccupied location on the map */
void wumpus_random_place( int type )
{
	int x = 0;
	int y = 0;

	while( x==0 && y==0 && wumpus_map[y][x] == WUMPUS_MAP_EMPTY )
	{
	    x = rand()%10;
	    y = rand()%10;
	}
	wumpus_map[y][x] = type;
}


/* generate the entire map for a new game */
void wumpus_generate_map( void )
{
	int x, y, c;
	for( x=0 ; x<10 ; x++ ) for( y=0 ; y<10 ; y++ ){
		wumpus_map[y][x] = WUMPUS_MAP_EMPTY;
	}

	for( c=0 ; c<8 ; c++ ) { /* 8 pits */
		wumpus_random_place( WUMPUS_MAP_PIT );
	}

	wumpus_random_place( WUMPUS_MAP_ARROW );
	wumpus_random_place( WUMPUS_MAP_DRAGON );
}

/* attempt to move in the direction we're pointing, handle collisions */
static void wumpus_move( void )
{
	switch( wumpus_orientation ){
	case( WUMPUS_NORTH ):
	    wumpus_y--; if( wumpus_y < 0 ) wumpus_y = 9; break;
	case( WUMPUS_SOUTH ):
	    wumpus_y++; if( wumpus_y > 9 ) wumpus_y = 0; break;
	case( WUMPUS_EAST ):
	    wumpus_x++; if( wumpus_x > 9 ) wumpus_x = 0; break;
	case( WUMPUS_WEST ):
	    wumpus_x--; if( wumpus_x < 0 ) wumpus_x = 9; break;
	}

	/* check for items */
	if( wumpus_map[wumpus_y][wumpus_x] == WUMPUS_MAP_DRAGON ) {
		wumpus_text( "THE WUMPUS ATE YOU!");
		wumpus_health = 0;
	}

	if( wumpus_map[wumpus_y][wumpus_x] == WUMPUS_MAP_ARROW ) {
		wumpus_map[wumpus_y][wumpus_x] = WUMPUS_MAP_EMPTY;
		wumpus_arrows++;
	}

	if( wumpus_map[wumpus_y][wumpus_x] == WUMPUS_MAP_HOOK ) {
		wumpus_map[wumpus_y][wumpus_x] = WUMPUS_MAP_EMPTY;
		wumpus_hooks++;
	}

	if( wumpus_map[wumpus_y][wumpus_x] == WUMPUS_MAP_PIT )
	{
		if( wumpus_hooks ) {
			wumpus_hooks--;
			wumpus_random_place( WUMPUS_MAP_HOOK );
		} else {
			wumpus_text( "YOU DIED IN A PIT!" );
			wumpus_health = 0;
		}
	}
}

/* find out what item (if any) is in the requested location 
   with relation to the current position */
static int wumpus_at( int direction )
{
	int fx = wumpus_x;
	int fy = wumpus_y;

	switch( direction ) {
	case( WUMPUS_NORTH ):
	    fy--; if( fy < 0 ) fy = 9; break;
	case( WUMPUS_SOUTH ):
	    fy++; if( fy > 9 ) fy = 0; break;
	case( WUMPUS_EAST ):
	    fx++; if( fx > 9 ) fx = 0; break;
	case( WUMPUS_WEST ):
	    fx--; if( fx < 0 ) fx = 9; break;
	}

	return( wumpus_map[fy][fx] );
}

/* attempt to fire in the direction that we're facing */
static void wumpus_fire( void )
{
	int tile = wumpus_at( wumpus_orientation );

	if( !wumpus_arrows ) return;
	wumpus_arrows--;

	if( tile == WUMPUS_MAP_DRAGON ) {
		wumpus_text( "YOU KILLED THE WUMPUS!!" );
		wumpus_health = 0;
	} else {
		new_message_window( "MISS!" );
		wumpus_random_place( WUMPUS_MAP_ARROW );
	}
}


/* initialize everything for a new game */
void wumpus_new_game( void )
{
	int x;

	/* i hate to use doubles, but this only gets computed the 
	   first time that wumpus is run.  Otherwise, we end up 
	   with massive rounding errors for mini/photo rendering,
	   which looks *really* craptastic. */

	double h32 = (double)wumpus_height/32.0;
	double w32 = (double)screen_info.cols/32.0;

	if( !wumpus_initialized ) {
		wumpus_initialized = 1;
		for( x=0 ; x<8 ; x++ ) {
		 	topright[x].x *= w32;
			topright[x].y *= h32;   
		 	topleft[x].x *= w32;
			topleft[x].y *= h32;   
		}
		for( x=0 ; x<9 ; x++ ) {
		 	bottomleft[x].x *= w32;
			bottomleft[x].y *= h32;   
		 	bottomright[x].x *= w32;
			bottomright[x].y *= h32;   
		}
		for( x=0 ; x<4 ; x++ ) {
			cursor_north[x].x *= w32;
			cursor_north[x].y *= h32;
			cursor_south[x].x *= w32;
			cursor_south[x].y *= h32;
			cursor_east[x].x *= w32;
			cursor_east[x].y *= h32;
			cursor_west[x].x *= w32;
			cursor_west[x].y *= h32;
		}
		for( x=0 ; x<6 ; x++ ) {
			bigpit[x].x *= w32;
			bigpit[x].y *= h32;
			pit_warning[x].x *= w32;
			pit_warning[x].y *= h32;
			arrow_warning[x].x *= w32;
			arrow_warning[x].y *= h32;
			hook_warning[x].x *= w32;
			hook_warning[x].y *= h32;
		}
	}

	/* now set up the game variables... */
	wumpus_x = wumpus_y = 0;		/* location */
	wumpus_orientation = WUMPUS_NORTH;	/* orientation */
	wumpus_hooks = 1;			/* grappling hook */
	wumpus_arrows = 0;			/* ammunition */
	wumpus_health = 1;

	wumpus_generate_map();
}


/* draw the playfield to the screen */
static void wumpus_redraw( void )
{
	int w2 = wumpus_screen_info.cols >> 1;
	int h2 = wumpus_height >> 1;

	if( wumpus_health == 0 ) return; /* don't draw anything if we're dead */

        /* start clear */
        GrSetGCForeground( wumpus_gc, WHITE );
        GrFillRect( wumpus_bufwid, wumpus_gc, 0, 0,
                    wumpus_screen_info.cols, wumpus_height );

	/* do graphics routines here */
	/* draw to wumpus_bufwid */

	/* http://www.so2.sys-techs.com/mwin/microwin/nxapi/ */

	/* fill */
	GrSetGCForeground( wumpus_gc, ( wumpus_screen_info.bpp == 16 ) ? 
				GR_RGB( 186, 129, 81 ) : LTGRAY );
	GrFillPoly( wumpus_bufwid, wumpus_gc, 8, topleft );
	GrFillPoly( wumpus_bufwid, wumpus_gc, 8, topright );
	GrFillPoly( wumpus_bufwid, wumpus_gc, 7, bottomleft );
	GrFillPoly( wumpus_bufwid, wumpus_gc, 7, bottomright );

	/* outlines */
        GrSetGCForeground( wumpus_gc, BLACK );
	GrPoly( wumpus_bufwid, wumpus_gc, 8, topleft );
	GrPoly( wumpus_bufwid, wumpus_gc, 8, topright );
	GrPoly( wumpus_bufwid, wumpus_gc, 9, bottomleft);
	GrPoly( wumpus_bufwid, wumpus_gc, 9, bottomright );

	/* pit, if applicable... */
	if( wumpus_map[wumpus_y][wumpus_x] == WUMPUS_MAP_PIT ) 
		GrPoly( wumpus_bufwid, wumpus_gc, 6, bigpit );


	/* current position */
	vector_render_char( wumpus_bufwid, wumpus_gc, wumpus_y + 'A', 
				3, w2-12-4, h2 );
	vector_render_char( wumpus_bufwid, wumpus_gc, wumpus_x + '0', 
				3, w2+4, h2 );

	/* cursor */
	switch( wumpus_orientation ) {
	case( WUMPUS_NORTH ):
		if( wumpus_blink ) {
		    GrSetGCForeground( wumpus_gc, 
			    ( wumpus_screen_info.bpp == 16 ) ? RED : BLACK );
		    GrFillPoly( wumpus_bufwid, wumpus_gc, 4, cursor_north );
		}
		GrSetGCForeground( wumpus_gc, GRAY );
		GrPoly( wumpus_bufwid, wumpus_gc, 4, cursor_north );
		break;

	case( WUMPUS_SOUTH ):
		if( wumpus_blink ) {
		    GrSetGCForeground( wumpus_gc, 
			    ( wumpus_screen_info.bpp == 16 ) ? RED : BLACK );
		    GrFillPoly( wumpus_bufwid, wumpus_gc, 4, cursor_south );
		}
		GrSetGCForeground( wumpus_gc, GRAY );
		GrPoly( wumpus_bufwid, wumpus_gc, 4, cursor_south );
		break;

	case( WUMPUS_WEST ):
		if( wumpus_blink ) {
		    GrSetGCForeground( wumpus_gc, 
			    ( wumpus_screen_info.bpp == 16 ) ? RED : BLACK );
		    GrFillPoly( wumpus_bufwid, wumpus_gc, 4, cursor_west );
		}
		GrSetGCForeground( wumpus_gc, GRAY );
		GrPoly( wumpus_bufwid, wumpus_gc, 4, cursor_west );
		break;

	case( WUMPUS_EAST ):
		if( wumpus_blink ) {
		    GrSetGCForeground( wumpus_gc, 
			    ( wumpus_screen_info.bpp == 16 ) ? RED : BLACK );
		    GrFillPoly( wumpus_bufwid, wumpus_gc, 4, cursor_east );
		}
		GrSetGCForeground( wumpus_gc, GRAY );
		GrPoly( wumpus_bufwid, wumpus_gc, 4, cursor_east );
		break;

	default:
		break;
	}

	if( !wumpus_blink ) {
		GrSetGCForeground( wumpus_gc, 
			( wumpus_screen_info.bpp == 16 ) ? RED : BLACK );
	} else {
		GrSetGCForeground( wumpus_gc, 
			( wumpus_screen_info.bpp == 16 ) ? LTGRAY : LTGRAY );
	}

	/* pit warning */
	if( 	wumpus_at( WUMPUS_NORTH ) == WUMPUS_MAP_PIT 
	    ||	wumpus_at( WUMPUS_SOUTH ) == WUMPUS_MAP_PIT 
	    ||	wumpus_at( WUMPUS_EAST ) == WUMPUS_MAP_PIT 
	    ||	wumpus_at( WUMPUS_WEST ) == WUMPUS_MAP_PIT )
	{
		GrPoly( wumpus_bufwid, wumpus_gc, 6, pit_warning );
	}

	/* hook warning */
	if( 	wumpus_at( WUMPUS_NORTH ) == WUMPUS_MAP_HOOK 
	    ||	wumpus_at( WUMPUS_SOUTH ) == WUMPUS_MAP_HOOK 
	    ||	wumpus_at( WUMPUS_EAST ) == WUMPUS_MAP_HOOK 
	    ||	wumpus_at( WUMPUS_WEST ) == WUMPUS_MAP_HOOK )
	{
		GrPoly( wumpus_bufwid, wumpus_gc, 6, hook_warning );
	}

	/* arrow warning */
	if( 	wumpus_at( WUMPUS_NORTH ) == WUMPUS_MAP_ARROW 
	    ||	wumpus_at( WUMPUS_SOUTH ) == WUMPUS_MAP_ARROW 
	    ||	wumpus_at( WUMPUS_EAST ) == WUMPUS_MAP_ARROW 
	    ||	wumpus_at( WUMPUS_WEST ) == WUMPUS_MAP_ARROW )
	{
		GrPoly( wumpus_bufwid, wumpus_gc, 6, arrow_warning );
	}

	/* dragon warning */
	if( 	wumpus_at( WUMPUS_NORTH ) == WUMPUS_MAP_DRAGON 
	    ||	wumpus_at( WUMPUS_SOUTH ) == WUMPUS_MAP_DRAGON 
	    ||	wumpus_at( WUMPUS_EAST ) == WUMPUS_MAP_DRAGON 
	    ||	wumpus_at( WUMPUS_WEST ) == WUMPUS_MAP_DRAGON )
	{
		vector_render_string(  wumpus_bufwid, wumpus_gc, "[W]",
					1, 2, w2-16, 4 );
	}


	/* inventory display */
	GrSetGCForeground( wumpus_gc, BLACK );
	if( wumpus_arrows ) {
		vector_render_string( wumpus_bufwid, wumpus_gc, "ARROW",
				1, 1, 1, wumpus_height-10 );
	}

	if( wumpus_hooks ) {
		vector_render_string_right( wumpus_bufwid, wumpus_gc, "HOOK", 
				1, 1,
				wumpus_screen_info.cols-1,
				wumpus_height-10 );
	}

        /* copy the buffer into place */
        GrCopyArea( wumpus_wid, wumpus_gc, 0, 0,
                    wumpus_screen_info.cols,
                    (screen_info.rows - (HEADER_TOPLINE + 1)),
                    wumpus_bufwid, 0, 0, MWROP_SRCCOPY);
}

/* the full draw function */
static void wumpus_do_draw( void )
{
	pz_draw_header(_("Hunt The Wumpus"));
	wumpus_redraw();
}

/* quit.  dispose of everything */
void wumpus_exit( void ) 
{
	GrDestroyTimer( wumpus_timer );
	GrDestroyGC( wumpus_gc );
	pz_close_window( wumpus_wid );
	GrDestroyWindow( wumpus_bufwid );
}

/* event handler */
static int wumpus_handle_event (GR_EVENT *event)
{
	switch( event->type )
	{
	    case GR_EVENT_TYPE_TIMER:
		wumpus_blink ^= 1;
		wumpus_redraw();
		break;

	    case GR_EVENT_TYPE_KEY_DOWN:
		switch (event->keystroke.ch)
		{
		case( IPOD_BUTTON_ACTION ):
		    if( wumpus_health ) {
			    wumpus_move();
			    wumpus_redraw();
		    }
		    break;

		case( IPOD_BUTTON_PLAY ):
		    if( wumpus_health ) {
			    wumpus_fire();
			    wumpus_redraw();
		    }
		    break;

		case( IPOD_WHEEL_ANTICLOCKWISE ):
		    if( wumpus_health ) {
			    wumpus_orientation--;
			    if( wumpus_orientation < 0 ) wumpus_orientation = 3;
			    wumpus_blink = 0;
			    wumpus_redraw();
		    }
		    break;

		case( IPOD_WHEEL_CLOCKWISE ):
		    if( wumpus_health ) {
			    wumpus_orientation++;
			    if( wumpus_orientation > 3 ) wumpus_orientation = 0;
			    wumpus_blink = 0;
			    wumpus_redraw();
		    }
		    break;

		case 'q': // Quit
		case( IPOD_BUTTON_MENU ):
		    wumpus_exit();
		    break;

		case( IPOD_BUTTON_REWIND ):
		case( IPOD_BUTTON_FORWARD ):
		default:
		    return EVENT_UNUSED;
		    break;
		} // keystroke
		break;   // key down

	} // event type

	return 1;
}

/* entry point - get everything set up for a new game */
void new_wumpus_window(void)
{
	/* Init randomizer */
	srand(time(NULL));

	GrGetScreenInfo(&wumpus_screen_info);

	wumpus_gc = GrNewGC();
        GrSetGCUseBackground(wumpus_gc, GR_FALSE);
        GrSetGCForeground(wumpus_gc, BLACK);

	wumpus_height = (screen_info.rows - (HEADER_TOPLINE + 1));

	wumpus_wid = pz_new_window( 0, HEADER_TOPLINE + 1,
		    screen_info.cols, wumpus_height,
		    wumpus_do_draw, wumpus_handle_event );

	wumpus_bufwid = GrNewPixmap( screen_info.cols, wumpus_height, NULL );

        GrSelectEvents( wumpus_wid, GR_EVENT_MASK_TIMER
					| GR_EVENT_MASK_EXPOSURE
					| GR_EVENT_MASK_KEY_UP
					| GR_EVENT_MASK_KEY_DOWN );

	wumpus_timer = GrCreateTimer( wumpus_wid, 300 );

	GrMapWindow( wumpus_wid );

	/* intialize game state */
	wumpus_new_game();
}
