/*
 * Vortex - A Tempest-like game
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
 *  $Id: $
 *
 *  $Log: $
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "../pz.h"
#include "../vectorfont.h"
#include "globals.h"
#include "levels.h"
#include "gameobjs.h"


void Vortex_ClearRect(int x, int y, int w, int h)
{
	GrSetGCForeground(Vortex_globals.gc, VORTEX_COLOR_BG);
	GrFillRect(Vortex_globals.wid, Vortex_globals.gc, x, y, w, h);
}

void Vortex_SetLevel( int l )
{
	int p;

	if( l > (16*3) ) l = 16*3;
	if( l < 0 ) l = 0;
	Vortex_globals.currentLevel = l;

	Vortex_globals.minx = 9999;
	Vortex_globals.maxx = 0;
	Vortex_globals.miny = 9999;
	Vortex_globals.maxy = 0;

	/* find the right level */
	for( p=0 ; p<(16*3) ; p++ ) {
	    if( vortex_levels[ p ].order == Vortex_globals.currentLevel )
		Vortex_globals.ld = &vortex_levels[ Vortex_globals.currentLevel ];
	}

	for( p=0 ; p<16 ; p++ ) {
	    Vortex_globals.minx = MIN( Vortex_globals.minx, Vortex_globals.ld->x[p] );
	    Vortex_globals.miny = MIN( Vortex_globals.miny, Vortex_globals.ld->y[p] );
	    Vortex_globals.maxx = MAX( Vortex_globals.maxx, Vortex_globals.ld->x[p] );
	    Vortex_globals.maxy = MAX( Vortex_globals.maxy, Vortex_globals.ld->y[p] );
	}

	Vortex_globals.xskew  = Vortex_globals.minx;
	Vortex_globals.xscale = (Vortex_globals.maxx - Vortex_globals.minx) / 100.0;
	Vortex_globals.xoffs  = (160/2) 
				- (Vortex_globals.maxx - Vortex_globals.minx)/2;

	Vortex_globals.xcenter = //Vortex_globals.minx + 
			    (Vortex_globals.maxx - Vortex_globals.minx); ///2;

	Vortex_globals.yskew  = Vortex_globals.miny;
	Vortex_globals.yscale = (Vortex_globals.maxy - Vortex_globals.miny) / 100.0;
	Vortex_globals.yoffs  = (160/2) 
				- (Vortex_globals.maxy - Vortex_globals.miny)/2;
	Vortex_globals.ycenter = //Vortex_globals.miny + 
			    (Vortex_globals.maxy - Vortex_globals.miny); ///2;
}

int Vortex_XAdjust( int v )
{
    return (int)((float)v/2.5)-15;
/*
    float s = (Vortex_globals.ld->scale)/32.0;

    float r = (float)(v-Vortex_globals.xskew) / Vortex_globals.xscale;
    return( (int) r + Vortex_globals.xoffs );
*/

// values are 0..255, adjust for 0..160 screen
}

int Vortex_YAdjust( int v )
{
    /*
    printf(">> y2d %d\n", Vortex_globals.ld->y2d );
    printf("   y3d %d\n", Vortex_globals.ld->y3d );
    */
	return (int)96-((float)v/2.5) -70; // - (Vortex_globals.ld->y2d/2);
    /*
	float s = (Vortex_globals.ld->scale)/32.0;
	float r = (float)(v-Vortex_globals.yskew) / Vortex_globals.yscale;
	return( (int) r + Vortex_globals.yoffs );
    */
}

int xxxx = 0;

#define SEGMENT_REAR  (0)
#define SEGMENT_ARM   (1)
#define SEGMENT_FRONT (2)

void Vortex_DrawSegment( int _x1, int _y1, int _x2, int _y2, int which)
{
	int xc = Vortex_XAdjust( Vortex_globals.xcenter );
	int yc = Vortex_YAdjust( Vortex_globals.ycenter );

	int x1 = Vortex_XAdjust( _x1 );
	int y1 = Vortex_YAdjust( _y1 );
	int x2 = Vortex_XAdjust( _x2 );
	int y2 = Vortex_YAdjust( _y2 );

	int xi, yi, xj, yj;


	x1 = 80 - (xc/2) + x1;
	y1 = 48 - (yc/2) + y1;
	x2 = 80 - (xc/2) + x2;
	y2 = 48 - (yc/2) + y2;

	xi = 80 + ((x1 - 80)*1/5);// + sin( xxxx ) * 9;
	yi = 48 + ((y1 - 80)*1/5);// + cos( xxxx ) * 9;
	xj = 80 + ((x2 - 80)*1/5);// + sin( xxxx ) * 9;
	yj = 48 + ((y2 - 80)*1/5);// + cos( xxxx ) * 9;


	if( which == SEGMENT_REAR ) {
	    GrSetGCForeground(Vortex_globals.gc, VORTEX_COLOR_WEBREAR);
	    GrLine( Vortex_globals.wid, Vortex_globals.gc, xi, yi, xj, yj );
	}

	if( which == SEGMENT_ARM ) {
	    GrSetGCForeground(Vortex_globals.gc, VORTEX_COLOR_WEBARM);
	    GrLine( Vortex_globals.wid, Vortex_globals.gc, xi, yi, x1, y1 );
	    GrLine( Vortex_globals.wid, Vortex_globals.gc, xj, yj, x2, y2 );
	}

	if( which == SEGMENT_FRONT ) {
	    GrSetGCForeground(Vortex_globals.gc, VORTEX_COLOR_WEBFRONT);
	    GrLine( Vortex_globals.wid, Vortex_globals.gc, x1, y1, x2, y2 );
	}

}

void Vortex_DrawWeb( void )
{
	int p, q;
	LEVELDATA * ld = Vortex_globals.ld;

	xxxx++;

	/* draw the web */
	for( p=0 ; p<15 ; p++ )
	    for( q=0 ; q<3 ; q++ ) 
		Vortex_DrawSegment( ld->x[p], ld->y[p], ld->x[p+1], ld->y[p+1], q );

	if( ld->flags & LF_CLOSEDWEB )
	    for( q=0 ; q<3 ; q++ ) 
		Vortex_DrawSegment( ld->x[0], ld->y[0], ld->x[15], ld->y[15], q );
}


/* initialize a new game */
static void Vortex_NewGame( void )
{
	Vortex_globals.gameState = VORTEX_STATE_PLAY;
	Vortex_SetLevel( 0 );
}


int ssss = 1;

/* draw the playfield */
static void Vortex_GameLoop( void )
{
	char buf[100];

	/* clear the playfield */
	Vortex_ClearRect( 0, 0, Vortex_globals.width, Vortex_globals.height );

	Vortex_DrawWeb();

	sprintf( buf, ":%02d:", Vortex_globals.currentLevel );
	GrSetGCForeground(Vortex_globals.gc, VORTEX_COLOR_SCORE);
     //   GrText( Vortex_globals.wid, Vortex_globals.gc, 0, Vortex_globals.height-10, buf, -1, GR_TFASCII );

	if( ssss < 20 )
	{ 
	    vector_render_string_center( 
			    Vortex_globals.wid, Vortex_globals.gc,
			    buf, 1, ssss++,
			    Vortex_globals.width/2, 
			    Vortex_globals.height/2 );

    /*
	    vector_render_string_center(
			    Vortex_globals.wid, Vortex_globals.gc,
			    "VORTEX", 2, (20-ssss)/2, 
			    Vortex_globals.width/2 + sin(ssss)*10.0, 
			    Vortex_globals.height/2 + cos(ssss)*10.0 );
    */
	} else 
	    vector_render_string( 
			    Vortex_globals.wid, Vortex_globals.gc,
			    buf, 1, 1, 1, Vortex_globals.height-10 );

	GrCopyArea( Vortex_globals.display_wid,
		   Vortex_globals.gc,
		   0, 0,
		   screen_info.cols, (screen_info.rows - (HEADER_TOPLINE + 1)),
		   Vortex_globals.wid,
		   0, 0,
		   MWROP_SRCCOPY);

}

static void vortex_drawTop()
{
	char buf[16];
	GrSetGCBackground( Vortex_globals.tgc, WHITE);

	GrClearWindow( Vortex_globals.twid, 0);

    /*
	GrSetGCForeground( Vortex_globals.tgc, LTGRAY);
	vector_render_string_center( Vortex_globals.twid, Vortex_globals.tgc,
			    "VORTEX", 2, 1, Vortex_globals.width/2+1, 9+1 );
    */

	GrSetGCForeground( Vortex_globals.tgc, BLACK);
	vector_render_string_center( Vortex_globals.twid, Vortex_globals.tgc,
			    "VORTEX", 2, 1, Vortex_globals.width/2, 9 );


	/* number of bases */
	snprintf( buf, 16, "%02d", Vortex_globals.lives );

    /*
	GrSetGCForeground( Vortex_globals.tgc, LTGRAY);
	vector_render_string( Vortex_globals.twid, Vortex_globals.tgc,
			buf, 1, 1,   2+1, 5+1 );
	vector_render_polystruct( Vortex_globals.twid, Vortex_globals.tgc,
			V_OBJECT_CLAW, V_gameobjs, V_points,
			2, 15+1, 1+1 );
    */
			

	GrSetGCForeground( Vortex_globals.tgc, BLACK);
	vector_render_string( Vortex_globals.twid, Vortex_globals.tgc,
			buf, 1, 1,   2, 5 );
	vector_render_polystruct( Vortex_globals.twid, Vortex_globals.tgc,
			V_OBJECT_CLAW, V_gameobjs, V_points,
			2, 15, 1 );


	/* score */
	Vortex_globals.score += 20;
	snprintf( buf, 16, "%d", Vortex_globals.score );

    /*
	GrSetGCForeground( Vortex_globals.tgc, LTGRAY);
	vector_render_string_right( Vortex_globals.twid, Vortex_globals.tgc,
			    buf, 1, 1, Vortex_globals.width-3, 5 );
    */
	GrSetGCForeground( Vortex_globals.tgc, BLACK);
	vector_render_string_right( Vortex_globals.twid, Vortex_globals.tgc,
			    buf, 1, 1, Vortex_globals.width-3+1, 5+1 );
}


static int vortex_handleTop (GR_EVENT *event)
{
	return 0;
}



static void vortex_do_draw()
{
	vortex_drawTop();
	//pz_draw_header("Vortex");
}

/* event handler */
static int vortex_handle_event (GR_EVENT *event)
{
	vortex_drawTop();
	switch( Vortex_globals.gameState )
	{
	    case VORTEX_STATE_PLAY:
		switch( event->type )
		{
		    case GR_EVENT_TYPE_TIMER:
			Vortex_GameLoop();
			break;

		    case GR_EVENT_TYPE_KEY_DOWN:
			switch (event->keystroke.ch)
			{
			case '\r': // Wheel button
			    // Fire:
			    break;

			case 'd': // Play/pause button
			    break;

			case 'w': // Rewind button
			    break;

			case 'f': // Fast forward button
			    // Engine thrust:
			    break;

			case 'l': // Wheel left
			    // Rotate ship clockwise
			    if( Vortex_globals.currentLevel > 0 )
			    {
				Vortex_SetLevel( Vortex_globals.currentLevel-1 );
				ssss=0;
			    }
			    break;

			case 'r': // Wheel right
			    // Rotate ship counter clockwise
			    if( Vortex_globals.currentLevel < (16*3)-1 )
			    {
				Vortex_SetLevel( Vortex_globals.currentLevel+1 );
				ssss=0;
			    }
			    break;

			case 'q':
			case 'm': // Menu button
			    Vortex_globals.gameState = VORTEX_STATE_EXIT;
			    break;

			default:
			    break;
			} // keystroke
			break;   // key down

		} // event type
		break; // GAME_STATE_PLAY

	    case VORTEX_STATE_EXIT:
		GrDestroyTimer( Vortex_globals.timer_id );
		GrDestroyGC( Vortex_globals.tgc );
		GrDestroyGC( Vortex_globals.gc );
		//pz_close_window( Vortex_globals.wid );
		pz_close_window( Vortex_globals.display_wid );
		pz_close_window( Vortex_globals.twid );
		break;
	} // game type

	return 1;
}

void new_vortex_window(void)
{
	/* Init randomizer */
	srand(time(NULL));

	Vortex_resetGlobals();

	GrGetScreenInfo(&Vortex_globals.screen_info);

	Vortex_globals.width = Vortex_globals.screen_info.cols;
	Vortex_globals.height = Vortex_globals.screen_info.rows - (HEADER_TOPLINE + 1);


    /* -- */
	Vortex_globals.tgc = GrNewGC();
	GrSetGCUseBackground(Vortex_globals.tgc, GR_FALSE);
	Vortex_globals.twid = pz_new_window( 0, 0,
				     Vortex_globals.width,
				     HEADER_TOPLINE,
				     vortex_drawTop, vortex_handleTop );

	Vortex_globals.gc = GrNewGC();
	GrSetGCUseBackground(Vortex_globals.gc, GR_FALSE);

	Vortex_globals.wid = GrNewPixmap(screen_info.cols,
				(screen_info.rows - (HEADER_TOPLINE + 1)), NULL);

	Vortex_globals.display_wid = pz_new_window(	 0,
				     HEADER_TOPLINE + 1,
				     Vortex_globals.width,
				     Vortex_globals.height,
				     vortex_do_draw,
				     vortex_handle_event);

	GrSelectEvents( Vortex_globals.display_wid, 0);
	GrSelectEvents( Vortex_globals.display_wid,
			 GR_EVENT_MASK_EXPOSURE
		       | GR_EVENT_MASK_KEY_DOWN
		       | GR_EVENT_MASK_KEY_UP
		       | GR_EVENT_MASK_TIMER);

	GrMapWindow( Vortex_globals.twid );
	GrMapWindow( Vortex_globals.display_wid );


	/* intialise game state */
	Vortex_NewGame();

	Vortex_globals.timer_id = GrCreateTimer(Vortex_globals.display_wid, 100);
}
