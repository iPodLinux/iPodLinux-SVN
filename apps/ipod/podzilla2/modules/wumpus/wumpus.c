/*
 * wumpus - a recreation of the old "Hunt The Wumpus" game
 * Copyright (C) 2005 Scott Lawrence
 *
 * Updated 2007 for ttk/pz2
 *
 * Note; this has the rules of "Hunt The Wumpus" but the style
 *       of the display and control is based on a "Dungeons and
 *       Dragons" LCD game I had when I was a kid.  :D
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
 *
 */

/* Revision information:

	2007-Jan-13	version 3.0
		- updated from Legacy calls to TTK
		- added a 'resume' feature to continue a game.  ;)
		- changed all of the display to be vector based
		- adheres to the current color scheme (window.bg/fg/border)

	The legacy version is on the wiki as 2.0, so I decided to
	start this at 3.0.

*/

/*
 * find the arrow, kill the wumpus 
 * use the hook to get out of a pit
 *  "Play" fires the arrow
 *  wheel  changes direction
 * action  moves
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "pz.h"

typedef struct gameInfo {
	int pos_x;		/* current X position */
	int pos_y;		/* current Y position */
	int orientation;	/* current cursor orientation */
	int hooks;		/* number of hooks */
	int arrows;		/* number of arrows */
	int health;		/* current health */

	int map[10][10];	/* our map! */
} GAMEINFO;

typedef struct myGlobals {
        PzModule * module;      /* this. */
        PzWindow * window;      /* our window */
        PzWidget * widget;      /* our display widget */
        PzConfig * config;      /* the config/settings */

        int w, h;               /* current display width and heighth */
	ttk_color bg;		/* bg color */
	ttk_color fg;		/* fg color */
	ttk_color rd;		/* border color */

        int paused;             /* are we paused? 1=yes */
	int blink;		/* for the blinking cursor */

	GAMEINFO game;		/* the current game's information */
	int initialized;	/* have we loaded a new game? */

	char * messagetext;	/* centerbox display text */

	char * noticetext;	/* for bottom notice */
	long noticetime;	/* for bottom notice */
} GLOB;

/* current cursor orientation */
#define WUMPUS_NORTH 	(0)
#define WUMPUS_EAST	(1)
#define WUMPUS_SOUTH	(2)
#define WUMPUS_WEST	(3)

/* map space contents */
#define WUMPUS_MAP_EMPTY	(0)
#define WUMPUS_MAP_PIT		(1)
#define WUMPUS_MAP_ARROW	(2)
#define WUMPUS_MAP_HOOK		(3)
#define WUMPUS_MAP_DRAGON	(4)

static GLOB glob;

#define NOTICE_TIME	(10)

/* hot, steamy, polyline action */

/* the cursor... 0..255 up and down */
static short cursor_north_x[] = { 109, 141, 125, 109 };
static short cursor_north_y[] = { 75, 75, 65, 75 };

static short cursor_south_x[] = { 75, 175, 125, 75 };
static short cursor_south_y[] = { 200, 200, 225, 200 };

static short cursor_west_x[] = { 75, 55, 40, 75 };
static short cursor_west_y[] = { 110, 150, 135, 110 };

static short cursor_east_x[] = { 175, 195, 210, 175 };
static short cursor_east_y[] = { 110, 150, 135, 110 };

static short pit_x[] = { 25, 75, 90, 160, 175, 225 };
static short pit_y[] = { 75, 80, 180, 180, 80, 75 };

static short arrow_x[] = { 50, 25, 75,  50, 200, 200, /* need 3 for polyline */
			  175, 175, 200,  187, 187, 212,  199, 199, 224 };
static short arrow_y[] = { 75, 25, 50,  50, 200, 200,
			  200, 175, 175,  212, 187, 187,  224, 199, 199 };

static short hook_x[] = { 50, 25, 25, 75, 125, 150,
			  50, 200, 200, 225, 225, 200 };
static short hook_y[] = { 150, 125, 75, 25, 25, 50,
			  50, 200, 225, 225, 200, 200 };

static short walls_x[] = { 	90, 100, 100, 90, 90, 		/* 11:00 */
				65, 90, 90, 65, 65, 		/* 10:00 */
				5, 55, 55, 5, 5,		/* 8:00 */
				35, 55, 55, 35, 35 };		/* 7:00 */

static short walls_y[] = { 	70, 50, 65, 90, 70,		/* 11:00 */
				70, 70, 90, 90, 70,		/* 10:00 */
				170, 170, 205, 205, 170,	/* 8:00 */
				205, 170, 205, 245, 205 };	/* 7:00 */


/* set an item on the map */
static void wumpus_place( int x, int y, int type )
{
	glob.game.map[y][x] = type;
}

/* set an item here */
static void wumpus_place_here( int type )
{
	wumpus_place( glob.game.pos_x, glob.game.pos_y, type );
}

/* place a random piece any unoccupied location on the map */
static void wumpus_random_place( int type )
{
        int x = 0;
        int y = 0;

        while( x==0 && y==0 && glob.game.map[y][x] == WUMPUS_MAP_EMPTY )
        {
            x = rand()%10;
            y = rand()%10;
        }
	wumpus_place( x, y, type );
}

/* return what is underneath us */
static int wumpus_here( void )
{
        return( glob.game.map[glob.game.pos_y][glob.game.pos_x] );
}

/* generate the entire map for a new game */
static void wumpus_start_new_game( void )
{
        int x, y, c;

	if( glob.initialized ) {
		/* ask if we want to continue */
		/* this concept borrowed from courtc's "Puyo" game. Cheers! */
		if( glob.game.health ) {
			if( !pz_dialog(  _("New Game?"), 
				_("You have an unfinished game; Continue?" ),
					2, 0, _("Yes"), _("No"))) {
				glob.initialized = 1; /* state 1 not 2 */
				return;
			}
		}
	}
	glob.initialized = 2; /* no input to absorb */

	/* generate a new dungeon */
	/* clear it out */
        for( x=0 ; x<10 ; x++ ) for( y=0 ; y<10 ; y++ ){
		wumpus_place( x, y, WUMPUS_MAP_EMPTY );
        }

	/* drop 8 pits randomly */
        for( c=0 ; c<8 ; c++ ) { 
                wumpus_random_place( WUMPUS_MAP_PIT );
        }

	/* drop the arrow and the dragon */
        wumpus_random_place( WUMPUS_MAP_ARROW );
        wumpus_random_place( WUMPUS_MAP_DRAGON );

	/* set up game variables */
	glob.game.hooks = 1;			/* grappling hook */
	glob.game.arrows = 0;			/* ammunition */
	glob.game.health = 1;
	glob.game.pos_x = 0;			/* location x */
	glob.game.pos_y = 0;			/* location y */
	glob.game.orientation = WUMPUS_NORTH;	/* orientation */
}

/* attempt to move in the direction we're pointing, handle collisions */
static void wumpus_move( void )
{
        switch( glob.game.orientation ){
        case( WUMPUS_NORTH ):
		glob.game.pos_y--;
		if( glob.game.pos_y < 0 ) glob.game.pos_y = 9;
		break;
        case( WUMPUS_SOUTH ):
		glob.game.pos_y++;
		if( glob.game.pos_y > 9 ) glob.game.pos_y = 0;
		break;
        case( WUMPUS_EAST ):
		glob.game.pos_x++; 
	    	if( glob.game.pos_x > 9 ) glob.game.pos_x = 0;
		break;
        case( WUMPUS_WEST ):
		glob.game.pos_x--;
		if( glob.game.pos_x < 0 ) glob.game.pos_x = 9;
		break;
        }
        
        /* check for items */
	switch( wumpus_here() ) {
	case( WUMPUS_MAP_DRAGON ):
		glob.messagetext = "THE WUMPUS ATE YOU!";
                glob.game.health = 0;
		break;
        
	case( WUMPUS_MAP_ARROW ):
		wumpus_place_here( WUMPUS_MAP_EMPTY );
                glob.game.arrows++;
		glob.noticetext = "Found an arrow!";
		glob.noticetime = NOTICE_TIME;
		break;
        
	case( WUMPUS_MAP_HOOK ):
		wumpus_place_here( WUMPUS_MAP_EMPTY );
                glob.game.hooks++;
		glob.noticetext = "Found a hook!";
		glob.noticetime = NOTICE_TIME;
		break;
        
	case( WUMPUS_MAP_PIT ):
                if( glob.game.hooks ) {
                        glob.game.hooks--;
                        wumpus_random_place( WUMPUS_MAP_HOOK );
			glob.noticetext = "Using your hook!";
			glob.noticetime = NOTICE_TIME;
                } else {
			glob.messagetext = "YOU DIED IN A PIT!";
                        glob.game.health = 0;
                }
		break;
        }
}

/* find out what item (if any) is in the requested location
   with relation to the current position */
static int wumpus_at( int direction )
{
        int fx = glob.game.pos_x;
        int fy = glob.game.pos_y;

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

        return( glob.game.map[fy][fx] );
}



/* find out if something is nearby */
static int wumpus_nearby( int occupant )
{
	if(    (wumpus_at( WUMPUS_NORTH ) == occupant) 
	    || (wumpus_at( WUMPUS_SOUTH ) == occupant) 
	    || (wumpus_at( WUMPUS_EAST ) == occupant) 
	    || (wumpus_at( WUMPUS_WEST ) == occupant))
	{
		return( 1 );
	}
	return( 0 );
}


/* attempt to fire in the direction that we're facing */
static void wumpus_fire( void )
{
        int tile = wumpus_at( glob.game.orientation );

        if( !glob.game.arrows ) return;
        glob.game.arrows--;

        if( tile == WUMPUS_MAP_DRAGON ) {
                glob.messagetext = "YOU KILLED THE WUMPUS!!";
                glob.game.health = 0;
        } else {
                glob.messagetext = "MISS!";
                wumpus_random_place( WUMPUS_MAP_ARROW );
        }
}


/* antialiased polyline! */
void aapolyline( ttk_surface srf, int nv, short *vx, short *vy, ttk_color col )
{
	int i;
	if( !srf || !vx || !vy ) return;

	for( i=0 ; i<nv-1 ; i++ )
	{
		/* this looks craptastic with libsdl's purple fringing */
		//ttk_aaline( srf, vx[i], vy[i], vx[i+1], vy[i+1], col );
		ttk_line( srf, vx[i], vy[i], vx[i+1], vy[i+1], col );
	}
}


/* draw the cursor (orientation) to the screen */
static void wumpus_draw_cursor( ttk_surface srf )
{
	short * xx = NULL;
	short * yy = NULL;
	short x[4];
	short y[4];
	int i;

	switch( glob.game.orientation ) {
	case WUMPUS_NORTH:
		xx = cursor_north_x;
		yy = cursor_north_y;
		break;
	case WUMPUS_SOUTH:
		xx = cursor_south_x;
		yy = cursor_south_y;
		break;
	case WUMPUS_WEST:
		xx = cursor_west_x;
		yy = cursor_west_y;
		break;
	case WUMPUS_EAST:
	default:
		xx = cursor_east_x;
		yy = cursor_east_y;
		break;
	}

	for( i=0 ; i<4 ; i++ )
	{
		x[i] = 3+(xx[i] * glob.w / 255);
		y[i] = yy[i] * glob.h / 255;
	}

	ttk_fillpoly( srf, 4, x, y, glob.blink?glob.fg:glob.rd );
	aapolyline( srf, 4, x, y, glob.blink?glob.rd:glob.fg );
}


static void wumpus_draw_walls( ttk_surface srf )
{
	int i;
	int j;
	short x[20];
	short y[20];

	for( j=0 ; j<4 ; j++ )
	{
		/* first, do the left side... */
		for( i=0 ; i<20 ; i++ ) {
			x[i] = 3+(walls_x[i] * glob.w / 255);
			y[i] = walls_y[i] * glob.h / 255;
		}
		for( j=0 ; j<4 ; j++ ) {
			ttk_fillpoly( srf, 5, x+(5*j), y+(5*j), glob.rd );
			aapolyline( srf, 5, x+(5*j), y+(5*j), glob.fg );
		}

		/* then do the right side... */
		for( i=0 ; i<20 ; i++ ) {
			x[i] = glob.w - x[i];
		}
		for( j=0 ; j<4 ; j++ ) {
			ttk_fillpoly( srf, 5, x+(5*j), y+(5*j), glob.rd );
			aapolyline( srf, 5, x+(5*j), y+(5*j), glob.fg );
		}
	}

}

/* draw the current player position to the screen */
static void wumpus_draw_position( ttk_surface srf )
{
	char buf[16];
	snprintf( buf, 16, "%c%d", glob.game.pos_y + 'A', glob.game.pos_x );
	pz_vector_string_center( srf, buf, glob.w>>1, glob.h>>1, 
			glob.w>>3, glob.h>>2, 1, glob.fg );
}


/* draw a textbox encompassing the full screen */
static void wumpus_draw_textbox( ttk_surface srf, char * buf )
{
	int texth;
	int textw;
	int inset = 20;

	if( !buf ) return;

	/* tweak the inset for smaller screens. :( */
	/* nano = 176, 1g-4g = 160, mini = 138 */
	if( glob.w < 176 ) {
		inset = 2;	/* mini, 1g-4g mono*/
	} 
	
	/* draw a purty outline box */
	ttk_rect( srf, inset-1, 21, glob.w-(inset-1), glob.h-21, glob.fg );
	ttk_rect( srf, inset,   20, glob.w-inset,     glob.h-20, glob.rd );
	ttk_rect( srf, inset+1, 19, glob.w-(inset+1), glob.h-19, glob.fg );

	/* and plop the text in the middle of it, yo. */
	texth = ttk_text_height( ttk_menufont );
	textw = ttk_text_width( ttk_menufont, buf );

	ttk_text( srf, ttk_menufont,
			(glob.w-textw)>>1, (glob.h-texth)>>1,
			glob.fg, buf );
}

/* draw our polyline arrow */
static void wumpus_draw_arrow( ttk_surface srf, int xp, int yp, 
				int sz, ttk_color c )
{
	int i;
	short x[16];
	short y[16];

	for( i=0 ; i<15 ; i++ )
	{
		x[i] = xp + (arrow_x[i] * sz / 255);
		y[i] = yp + (arrow_y[i] * sz / 255);
	}
	aapolyline( srf, 3, x, y, c ); /* head */
	aapolyline( srf, 3, x+3, y+3, c ); /* stem */
	aapolyline( srf, 3, x+6, y+6, c ); /* feather 1 */
	aapolyline( srf, 3, x+9, y+9, c ); /* feather 2 */
	aapolyline( srf, 3, x+12, y+12, c ); /* feather 3 */
}

/* draw our polyline hook */
static void wumpus_draw_hook( ttk_surface srf, int xp, int yp,
				int sz, ttk_color c )
{
	int i;
	short x[16];
	short y[16];

	for( i=0 ; i<12 ; i++ )
	{
		x[i] = xp + (hook_x[i] * sz / 255);
		y[i] = yp + (hook_y[i] * sz / 255);
	}

	aapolyline( srf, 6, x, y, c ); /* head */
	aapolyline( srf, 6, x+6, y+6, c ); /* stem */
}


/* draw the messagetext in a textbox */
static int wumpus_draw_text( ttk_surface srf )
{
	if( !glob.messagetext ) return 0;
	wumpus_draw_textbox( srf, glob.messagetext );
	return 1;
}

/* list out the inventory on the screen */
static void wumpus_draw_inventory( ttk_surface srf )
{
	int sz = glob.h/4;
	int off = 10;

	pz_vector_string( srf, "Inventory", 2, 2, 4, 6, 1, glob.fg );

	if( glob.game.arrows ) {
		wumpus_draw_arrow( srf, 0, off, sz, glob.fg );
		off += sz;
		pz_vector_string( srf, "arrow", 2, off, 4, 6, 1, glob.fg );
		off += 10;
	}
	if( glob.game.hooks ) {
		wumpus_draw_hook( srf, 0, off, sz, glob.fg );
		off += sz;
		pz_vector_string( srf, "hook", 2, off, 4, 6, 1, glob.fg );
		off += 10;
	}
}

/* draw the pit, if applicable */
static void wumpus_draw_pit( ttk_surface srf )
{
	int i;
	short x[6];
	short y[6];

	if( wumpus_here() != WUMPUS_MAP_PIT )  return;

	for( i=0 ; i<6 ; i++ )
	{
		x[i] = 3+ (pit_x[i] * glob.w / 255);
		y[i] = pit_y[i] * glob.h / 255;
	}
	aapolyline( srf, 6, x, y, glob.rd );

	for( i=0 ; i<6 ; i++ ) y[i]--;
	aapolyline( srf, 6, x, y, glob.fg );
}


/* draw the pit-nearby icon if applicable */
static void wumpus_draw_near( ttk_surface srf )
{
	int off = 12;
	int i;
	int sz = glob.h/3;
	short x[16];
	short y[16];
	ttk_color c = (glob.blink)?glob.rd:glob.fg;

	pz_vector_string( srf, "Near", glob.w-25, 2, 4, 6, 1, glob.fg );

	if( wumpus_nearby( WUMPUS_MAP_DRAGON )) {
		pz_vector_string( srf, "WUMPUS", glob.w-70, off, 8, 16, 1, c );
		pz_vector_string( srf, "WUMPUS", glob.w-70, off+1, 8, 16, 1, c );
		off += 20;
	}

	if( wumpus_nearby( WUMPUS_MAP_PIT )) {
		off -= (sz/4);
		for( i=0 ; i<6 ; i++ )
		{
			x[i] = (glob.w-sz) + (pit_x[i] * sz / 255);
			y[i] = off+ (pit_y[i] * sz / 255);
		}
		aapolyline( srf, 6, x, y, c );
		for( i=0 ; i<6 ; i++ )  y[i]++;
		aapolyline( srf, 6, x, y, c );
		off += sz - (sz/4);;
	}

	if( wumpus_nearby( WUMPUS_MAP_ARROW )) {
		wumpus_draw_arrow( srf, (glob.w-sz), off, sz, c );
		off += sz;
	}

	if( wumpus_nearby( WUMPUS_MAP_HOOK )) {
		wumpus_draw_hook( srf, (glob.w-sz), off, sz, c );
	}
}

/* draw the timed notice at the bottom of the screen */
static void wumpus_draw_notice( ttk_surface srf )
{
	if( glob.noticetime > 0 && glob.noticetext ) {
		pz_vector_string_center( srf, glob.noticetext,
				glob.w>>1, glob.h-10, 4, 6, 1, glob.fg );
	}
}

/* draw all of our stuff to the screen */
static void wumpus_draw( PzWidget * widget, ttk_surface srf )
{
	if( glob.paused ) {
		wumpus_draw_textbox( srf, "PAUSED" );
		return;
	}
	if( wumpus_draw_text( srf )) return;

	wumpus_draw_inventory( srf );
	wumpus_draw_near( srf );

	wumpus_draw_position( srf );
	wumpus_draw_cursor( srf );
	wumpus_draw_walls( srf );

	wumpus_draw_pit( srf );

	wumpus_draw_notice( srf );
}


static void wumpus_destroy( void )
{
}


static int wumpus_event( PzEvent *ev )
{
	switch( ev->type ) {
	case PZ_EVENT_SCROLL:
		if( glob.game.health ) {
			TTK_SCROLLMOD( ev->arg, 10 );
			if( ev->arg > 0 ) {
				glob.game.orientation++;
			} else {
				glob.game.orientation--;
			}
			if( glob.game.orientation < 0 ) 
				glob.game.orientation = 3;
			if( glob.game.orientation > 3 ) 
				glob.game.orientation = 0;
			glob.blink = 0;
                        ev->wid->dirty = 1;
		}
		break;

        case PZ_EVENT_BUTTON_DOWN:
                switch( ev->arg ) {
                case( PZ_BUTTON_HOLD ):
                        glob.paused = 1;
                        ev->wid->dirty = 1;
                        break;          
		}
		break;

	case PZ_EVENT_BUTTON_UP:
                switch( ev->arg ) {     
		case( PZ_BUTTON_HOLD ):
                        glob.paused = 0;
                        ev->wid->dirty = 1;
                        break;

                case( PZ_BUTTON_MENU ):
			if( !glob.paused ) {
                        	pz_close_window (ev->wid->win);
			}
                        break;  

                case( PZ_BUTTON_ACTION ):
			/* make sure the first input gets eaten... */
			if( !glob.paused ) {
				if( glob.initialized == 1 ) {
					glob.initialized = 2;
				} else {
					if( glob.game.health ) {
						wumpus_move();
						ev->wid->dirty = 1;
					} else {
						pz_close_window (ev->wid->win);
					}
				}
			}
			break;

                case( PZ_BUTTON_PLAY ):
			if( glob.game.health && !glob.paused ) {
				wumpus_fire();
				ev->wid->dirty = 1;
			}
			break;

                case( PZ_BUTTON_NEXT ):
                case( PZ_BUTTON_PREVIOUS ):
			break;
		}
		break;

        case PZ_EVENT_DESTROY:
                wumpus_destroy();
                break;

	case PZ_EVENT_TIMER:
		if( !glob.paused ) {
			if( glob.noticetime > 0 ) glob.noticetime--;
			glob.blink ^= 1;
			ev->wid->dirty = 1;
		}
		break;
	}
	return 0;
}


static PzWindow * wumpus_new_game( void )
{
	srand( time( NULL ));

	/* build our map, init stuff */
	wumpus_start_new_game();

	/* other minutae */
	glob.messagetext = NULL;
	glob.paused = 0;

	glob.noticetext = NULL;
	glob.noticetime = 0;

	/* set up our window */
	glob.window = pz_new_window( "Wumpus", PZ_WINDOW_NORMAL );
	glob.w = glob.window->w;
	glob.h = glob.window->h;
	glob.widget = pz_add_widget( glob.window, wumpus_draw, wumpus_event );

	/* set up the colors */
	glob.fg = ttk_ap_getx( "window.fg" )->color;
        glob.bg = ttk_ap_getx( "window.bg" )->color;
        glob.rd = ttk_ap_getx( "window.border" )->color;

	/* our timer - for cursor blink */
	pz_widget_set_timer( glob.widget, 300 );
	
	/* finish and return it */
	return pz_finish_window( glob.window );
}

void wumpus_module_init( void )
{
	glob.module = pz_register_module( "wumpus", wumpus_destroy );
	glob.window = NULL;
	glob.widget = NULL;

	/* stuff we need for saving games -- necessary? nah. BUT FUN! */
	glob.config = NULL;
	glob.initialized = 0;

	pz_menu_add_action( "/Extras/Games/Hunt the Wumpus", wumpus_new_game );
}

PZ_MOD_INIT( wumpus_module_init )
