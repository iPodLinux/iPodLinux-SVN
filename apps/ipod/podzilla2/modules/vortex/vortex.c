/*
 *     Vortex - A "Tempest"/"Tempest 2000" style game.
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

/*
 * Game play, design, and in-game elements are based on the games:
 *	"Tempest" (c)1980 Atari/Dave Theurer
 *	"Tempest 2000", (c)1994,1996 Atari/Llamasoft/Jeff Minter
 *
 *  Mad props to 
 *	Dave Theurer for designing the awesome "Tempest"
 *	Jeff Minter for designing the awesome "Tempest 2000"
 *
 *  This version was written and vaguely designed by 
 *	Scott Lawrence yorgle@gmail.com
 */

/* known bugs:

    sometimes 'ouch' appears on startup

    levels don't self-advance
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "pz.h"
#include "console.h"
#include "vglobals.h"
#include "levels.h"
#include "vgamobjs.h"
#include "render.h"
#include "starfield.h"

/* versioning 
** v0.51 2007-10-02	revival
** v0.50 0604xxxx 	Last major SVN checkin
** v0.xx		Rewrite for podzilla2
*/

/*                       YYMMDDhh */
#define VORTEX_VERSION	"v0.51"

/* change this #define to an #undef if you want the header bar to show */
#define VORTEX_FULLSCREEN

/* the globals */
vortex_globals vglob;

/* padding around the web to the top and bottom */
#define VPADDING (10)


/******************************************************************************/

/* utility random function */
int Vortex_Rand( int max )
{
        return (int)((float)max * rand() / (RAND_MAX + 1.0));
}


/******************************************************************************/

/* finds the x/y position along [vect]or (point on outer edge of web)  
   at depth z (128 == outer edge of web), from the [center] 
 */
#define Vortex_getZ( vect, z, center )\
	((center) + ((((vect)-(center)) * (z) )>>7))


/* compute the claw at the new current position, at the new current level */
void Vortex_generateClawGeometry( void ) /* NOTE: not "cowCompute" */ /* moo. */
{
	LEVELDATA * lv = &vortex_levels[ vglob.currentLevel ];

	vglob.wxC = vglob.usableW>>1;   /* web X center */
	vglob.wyC = lv->y3d;            /* web Y center */

	/* get the point index for the nextcurrent */
	int p2 = vglob.wPosMajor +1;	/* second point in the selected field */
	if( p2 > 15 ) p2 = 0;		/* adjust for wraparound */

	/* store aside the centerpoint of the field edge */
	vglob.pcxC = (lv->fx[ vglob.wPosMajor ] + lv->fx[ p2 ]) >> 1;
	vglob.pcyC = (lv->fy[ vglob.wPosMajor ] + lv->fy[ p2 ]) >> 1;

	/* compute the claw */
	vglob.px[0] = lv->fx[vglob.wPosMajor];
	vglob.py[0] = lv->fy[vglob.wPosMajor];
	vglob.px[1] = Vortex_getZ( vglob.pcxC, 150, vglob.wxC );
	vglob.py[1] = Vortex_getZ( vglob.pcyC, 150, vglob.wyC ); 
	vglob.px[2] = lv->fx[p2];
	vglob.py[2] = lv->fy[p2];
	vglob.px[3] = Vortex_getZ( vglob.pcxC, 140, vglob.wxC ); 
	vglob.py[3] = Vortex_getZ( vglob.pcyC, 140, vglob.wyC ); 
	vglob.px[4] = vglob.px[0];
	vglob.py[4] = vglob.py[0];
}

void Vortex_generateWebGeometry( void )
{
	int w, p, pp;
	LEVELDATA * lv = &vortex_levels[ vglob.currentLevel ];

	/* get the point index for the nextcurrent */
	int p2 = vglob.wPosMajor +1;	/* second point in the selected field */
	if( p2 > 15 ) p2 = 0;		/* adjust for wraparound */

	vglob.wxC = vglob.usableW>>1;   /* web X center */
	vglob.wyC = lv->y3d;            /* web Y center */

	/* store aside the centerpoint of the field edge */
	vglob.pcxC = (lv->fx[ vglob.wPosMajor ] + lv->fx[ p2 ]) >> 1;
	vglob.pcyC = (lv->fy[ vglob.wPosMajor ] + lv->fy[ p2 ]) >> 1;

	/* compute the top-end points */
	for( w=0 ; w<NUM_SEGMENTS ; w++ )
	{
	    /* the defined points. */
	    vglob.ptsX[w][NUM_Z_POINTS-1][0] = lv->fx[w];
	    vglob.ptsY[w][NUM_Z_POINTS-1][0] = lv->fy[w];

	    /* the mid points */
	    pp = w+1; 
	    if( pp > 15 ) pp = 0;
	    vglob.ptsX[w][NUM_Z_POINTS-1][1] = (lv->fx[w] + lv->fx[pp])>>1;
	    vglob.ptsY[w][NUM_Z_POINTS-1][1] = (lv->fy[w] + lv->fy[pp])>>1;
	}

	/* compute the grid of points... */
	for( w=0 ; w<NUM_SEGMENTS ; w++ )
	    for( p=0; p<(NUM_Z_POINTS-2) ; p++ )
	{
	    int pscaled = (p*128)/NUM_Z_POINTS;

	    /* along lines */
	    vglob.ptsX[w][p][0] = Vortex_getZ( lv->fx[w], pscaled, vglob.wxC );
	    vglob.ptsY[w][p][0] = Vortex_getZ( lv->fy[w], pscaled, vglob.wyC );

	    /* centers */
	    vglob.ptsX[w][p][1] = Vortex_getZ( 
			vglob.ptsX[w][NUM_Z_POINTS-1][1], pscaled, vglob.wxC );
	    vglob.ptsY[w][p][1] = Vortex_getZ( 
			vglob.ptsY[w][NUM_Z_POINTS-1][1], pscaled, vglob.wyC );
	}

	/* put in a copy of [0] to [NUM_SEGMENTS] to make rendering easy */
	for( p=0 ; p<NUM_Z_POINTS ; p++ )
	{
	    vglob.ptsX[NUM_SEGMENTS][p][0] = vglob.ptsX[0][p][0];
	    vglob.ptsY[NUM_SEGMENTS][p][0] = vglob.ptsY[0][p][0];
	    vglob.ptsX[NUM_SEGMENTS][p][1] = vglob.ptsX[0][p][1];
	    vglob.ptsY[NUM_SEGMENTS][p][1] = vglob.ptsY[0][p][1];
	}
}


/* update for a new level... */
void Vortex_selectLevel( int l )
{
	vglob.startLevel = l;

	/* and clip */
	if( vglob.startLevel < 0 ) 
		vglob.startLevel = 0;
	if( vglob.startLevel >= (NLEVELS-1) ) 
		vglob.startLevel = NLEVELS-1;

	if( vglob.currentLevel != vglob.startLevel ) {
		vglob.currentLevel = vglob.startLevel;
		if( vglob.gameStyle != VORTEX_STYLE_CLASSIC )
			Module_Starfield_session( 200, (vglob.startLevel&1)?-1:1 );
	}
}


/* player adjustments - new level stuff */
void Vortex_newClawInPlay( void )
{
	vglob.hasParticleLaser = 0;
	vglob.hasSuperZapper = 1;
	vglob.hasAutoFire = 0;
	vglob.wPosMajor = 8;
	vglob.wPosMinor = 0;
}


/* all of the stuff that needs to get reset at the beginning of a new level */
void Vortex_newLevelSetup( void )
{
	if( vglob.state == VORTEX_STATE_DEATH ) return;
	if( vglob.state == VORTEX_STATE_DEAD ) return;

	Vortex_Powerup_clear();
	Vortex_Bolt_clear();
	Vortex_Enemy_clear();
	Vortex_newClawInPlay();
	Vortex_generateWebGeometry();
	Vortex_generateClawGeometry();

	if( vglob.state == VORTEX_STATE_STARTUP ) return;
	if( vglob.gameStyle != VORTEX_STYLE_CLASSIC )
		Module_Starfield_session( 200, (vglob.startLevel&1)?-1:1 );
}


/******************************************************************************/
/* player movement */

/* number of minor steps per web.  0, 2, etc */
#define MINOR_MAX (0)

/* incPosition - the user moves one tick positive */
void Vortex_incPosition( int steps )
{
	if( steps > 1 ) Vortex_incPosition( steps-1 );

	if( vglob.wPosMinor < MINOR_MAX ) {
		vglob.wPosMinor++;
		Vortex_generateClawGeometry();
		return;
	}

	vglob.wPosMajor++;
	vglob.wPosMinor = 0;

	/* clip or rollover */
	if( vortex_levels[ vglob.currentLevel ].flags & LF_CLOSEDWEB) {
		if( vglob.wPosMajor > 15 )
			vglob.wPosMajor = 0;
	} else {
		if( vglob.wPosMajor > 14 )
			vglob.wPosMajor = 14;
	}

	Vortex_generateClawGeometry();
}

/* decPosition - the user moves one tick negative */
void Vortex_decPosition( int steps )
{
	if( steps > 1 ) Vortex_incPosition( steps-1 );

	if( vglob.wPosMinor > 0 ) {
		vglob.wPosMinor--;
		Vortex_generateClawGeometry();
		return;
	}

	if( vglob.wPosMajor > 0 ) {
		vglob.wPosMajor--;
		vglob.wPosMinor = MINOR_MAX;
		Vortex_generateClawGeometry();
		return;
	}

	if( vortex_levels[ vglob.currentLevel ].flags & LF_CLOSEDWEB ) {
		vglob.wPosMajor = 15;
		vglob.wPosMinor = MINOR_MAX;
		Vortex_generateClawGeometry();
	}
}



/*
   All of this has happened before
   And it will all happen again
*/


/******************************************************************************/
/* State based stuff */


/* just some stuff for printouting of stuffthings */
#define XXdbgprnt( A ) 	printf( A )
#define dbgprnt( ... )

void Vortex_stateChange( int newState )
{
	/* common */
	Vortex_Console_WipeAllText();
	Vortex_Console_HiddenStatic( 1 );
	vglob.timer = 0;

	/* Exit from state */
	switch( vglob.state ){
	case( VORTEX_STATE_STARTUP ):
		break;

	case( VORTEX_STATE_STYLESEL ):
		break;

	case( VORTEX_STATE_LEVELSEL ):
		break;

	case( VORTEX_STATE_GAME ):
		break;

	case( VORTEX_STATE_ADVANCE ):
		break;

	case( VORTEX_STATE_DEATH ):
		break;

	case( VORTEX_STATE_DEAD ):
		break;
	}

	/* the new state... */
	vglob.state = newState;

dbgprnt( "  To " );
	/* entry into the new state */
	switch( vglob.state ){
	case( VORTEX_STATE_STARTUP ):
dbgprnt( "STARTUP" );
		/* clear all globals */
		/* initialize game setup structs */
		Vortex_newLevelSetup();
		break;

	case( VORTEX_STATE_STYLESEL ):
dbgprnt( "STYLE SEL" );
		Vortex_newLevelSetup();
		break;

	case( VORTEX_STATE_LEVELSEL ):
		vglob.wPosMajor = 8;
		Vortex_newLevelSetup();
dbgprnt( "LEVEL SEL" );
		break;

	case( VORTEX_STATE_GAME ):
		Vortex_Console_HiddenStatic( 0 );
		vglob.wPosMajor = 8;
		vglob.wPosMinor = 0;
		Vortex_newLevelSetup();
dbgprnt( "GAME" );
		break;

	case( VORTEX_STATE_ADVANCE ):
dbgprnt( "ADVANCE" );
		break;

	case( VORTEX_STATE_DEATH ):
dbgprnt( "DEATH" );
		break;

	case( VORTEX_STATE_DEAD ):
dbgprnt( "DEAD" );
		break;
	}
dbgprnt( "\n" );
}

void Vortex_timerTick( PzEvent * ev )
{
	if( !vglob.paused ){
		Vortex_Powerup_poll();
		Vortex_Bolt_poll();
		Vortex_Enemy_poll();
		Vortex_Console_Tick();

		vglob.timer++;
	}

	/* check for change of state */
	switch( vglob.state ){
	case( VORTEX_STATE_STARTUP ):
		/* pseudo animated title screen */
		if( vglob.timer < 5 )
			Vortex_Console_AddItem( "VORTEX", 
				0, 0,
				VORTEX_STYLE_NORMAL, vglob.color.title );
		if( Vortex_Console_GetZoomCount() == 0 )
			Vortex_stateChange( VORTEX_STATE_STYLESEL );
		break;

	case( VORTEX_STATE_STYLESEL ):
		/* just toggle some stuff to make it look interesting */
		if( (vglob.timer & 0x1f) == 0x1f ) 
			vglob.hasParticleLaser ^= 1;
		if( (vglob.timer & 0x1f) == 0x1f ) 
			Vortex_decPosition( 1 );
		else if( (vglob.timer & 0x0f) == 0x0f)
			Vortex_incPosition( 1 );
		if( (vglob.timer & 0x0f) == 0x0f )
			Vortex_Bolt_add();
		break;

	case( VORTEX_STATE_LEVELSEL ):
		break;

	case( VORTEX_STATE_GAME ):
		Vortex_CollisionDetection();
		if( vglob.lives <= 0 ) {
			vglob.state = VORTEX_STATE_DEATH;
		}
		break;

	case( VORTEX_STATE_ADVANCE ):
		if( vglob.timer <= 1 ) 
			Vortex_Console_AddItem( "SUPERZAPPER", 0, 0, 
				VORTEX_STYLE_NORMAL, vglob.color.sz_text1 );

		if( vglob.timer == 20 )
			Vortex_Console_AddItem( "RECHARGE", 0, 0, 
				VORTEX_STYLE_NORMAL, vglob.color.sz_text2 );

		if( vglob.timer > 60 ) {
			Vortex_selectLevel( vglob.currentLevel + 1 );
			Vortex_stateChange( VORTEX_STATE_GAME );
		}
		break;

	case( VORTEX_STATE_DEATH ):
		if( vglob.timer < 5 )
	    		Vortex_Console_AddItem( "GAME OVER", 0, 0, 
				VORTEX_STYLE_NORMAL, vglob.color.title );
		if( Vortex_Console_GetZoomCount() == 0 ) {
			Vortex_stateChange( VORTEX_STATE_DEAD );
			pz_close_window( vglob.window );
		}
		break;

	case( VORTEX_STATE_DEAD ):
		break;
	}
	
	/* every timer tick should cause a redraw */
	ev->wid->dirty = 1;
}


/*
    Are you over the sea?
	And will you come, come back to me?
*/

/* draw valve routine */
void Vortex_draw( PzWidget *widget, ttk_surface srf )
{
	char buf[16];
	int classic = (vglob.gameStyle == VORTEX_STYLE_CLASSIC )?1:0;

	/* backfill! */
	ttk_fillrect( srf, 0, 0, ttk_screen->w, ttk_screen->h, vglob.color.bg );

	/* if we're paused, just render the paused screen and return */
	if( vglob.paused ) {
		Vortex_OutlinedTextCenter( srf, "VORTEX",
			vglob.usableW>>1, (vglob.usableH>>1)-20,
			10, 18, 1, vglob.color.title, vglob.color.bg );

		Vortex_OutlinedTextCenter( srf, "\xFD\xFDpaused\xFB\xFB",
			vglob.usableW>>1, (vglob.usableH>>1)+5,
			10, 18, 1, vglob.color.select, vglob.color.bg );

		Vortex_OutlinedTextCenter( srf, "Coded By", 
			vglob.usableW>>1, vglob.usableH-22,
			5, 9, 1, vglob.color.credits,
			vglob.color.bg );

		Vortex_OutlinedTextCenter( srf, "BleuLlama", 
			vglob.usableW>>1, vglob.usableH-10,
			5, 9, 1, vglob.color.credits,
			vglob.color.bg );
		return;
	}

	switch( vglob.state ){
	case( VORTEX_STATE_STARTUP ):
		if( !classic ) Module_Starfield_draw( srf );
		Vortex_Console_Render( srf );
		break;

	case( VORTEX_STATE_STYLESEL ):
		if( !classic ) Module_Starfield_draw( srf );
		Vortex_RenderWeb( srf );
		Vortex_RenderPlayer( srf );
		Vortex_Bolt_draw( srf );
		Vortex_RenderAvailableBases( srf );

		Vortex_RenderTimedQuad( srf, "SELECT", "GAME", "STYLE", "",
			vglob.usableW>>1, 20, 10, 18, vglob.color.select );
		Vortex_RenderTimedQuad( srf,
			"ALL THANKS TO", "DAVE THEURER", "JEFF MINTER", "",
			vglob.usableW>>1, vglob.usableH-10,
			5, 9, vglob.color.credits );

		pz_vector_string( srf, VORTEX_VERSION, 1,
			    vglob.usableH-6, 3, 5, 1, vglob.color.version );

		if( vglob.gameStyle == VORTEX_STYLE_CLASSIC )
			snprintf( buf, 16, "CLASSIC %c", 
					VECTORFONT_SPECIAL_RIGHT );
		else if( vglob.gameStyle == VORTEX_STYLE_2K5 )
			snprintf( buf, 16, "%c 2K5", 
					VECTORFONT_SPECIAL_LEFT );
		else
			snprintf( buf, 16, "BURRITO" );

		Vortex_OutlinedTextCenter( srf, buf,
			vglob.usableW>>1, 50,
			10, 18, 1, vglob.color.sellevel, vglob.color.bg );
		break;

	case( VORTEX_STATE_LEVELSEL ):
		if( !classic ) Module_Starfield_draw( srf );
		Vortex_RenderWeb( srf );

		Vortex_RenderTimedQuad( srf, "SELECT", "START", "LEVEL", "",
			vglob.usableW>>1, 20, 10, 18, vglob.color.select );
		Vortex_RenderTimedQuad( srf,
			"ALL THANKS TO", "DAVE THEURER", "JEFF MINTER", "",
			vglob.usableW>>1, vglob.usableH-10,
			5, 9, vglob.color.credits );

		pz_vector_string( srf, VORTEX_VERSION, 1,
			    vglob.usableH-6, 3, 5, 1, vglob.color.version );

		/* render level number */
		snprintf( buf, 15, "%c %d %c", 
			(vglob.startLevel>0)?  VECTORFONT_SPECIAL_LEFT:' ',
			vglob.startLevel,
			(vglob.startLevel<=(NLEVELS-2))?
						VECTORFONT_SPECIAL_RIGHT:' ');
		Vortex_OutlinedTextCenter( srf, buf,
			vglob.usableW>>1, 50,
			10, 18, 1, vglob.color.sellevel, vglob.color.bg );
		break;

	case( VORTEX_STATE_GAME ):
		if( !classic ) Module_Starfield_draw( srf );
		Vortex_RenderWeb( srf );
		Vortex_Powerup_draw( srf );
		Vortex_RenderPlayer( srf );
		Vortex_Bolt_draw( srf );
		Vortex_Enemy_draw( srf );
		Vortex_Console_Render( srf );

		/* score */
		snprintf( buf, 15, "%04d", vglob.score );
		pz_vector_string( srf, buf,
			    vglob.usableW - pz_vector_width( buf, 7, 13, 1 ) -1,
			    1, 7, 13, 1, vglob.color.score );

		/* lives left */
		Vortex_RenderAvailableBases( srf );

		/* superzapper indicator */
		if( vglob.hasSuperZapper )
			pz_vector_string( srf, "SZ", 1, 12, 5, 9, 1, 
					vglob.color.super );

		/* render level number */
		snprintf( buf, 15, "L%d", vglob.startLevel );
		pz_vector_string( srf, buf, 
			    vglob.usableW -
			     pz_vector_width( buf, 5, 9, 1 ) -1,
			    vglob.usableH-10,
                            5, 9, 1, vglob.color.level );
		break;

	case( VORTEX_STATE_ADVANCE ):
		if( !classic ) Module_Starfield_draw( srf );
		Vortex_Console_Render( srf );
		break;

	case( VORTEX_STATE_DEATH ):
		if( !classic ) Module_Starfield_draw( srf );
		Vortex_Console_Render( srf );
		break;

	case( VORTEX_STATE_DEAD ):
		break;
	}

	if( 0 ) {
		int w, z;
		for( w=0 ; w<NUM_SEGMENTS ; w++ ) {
			for( z=0 ; z<NUM_Z_POINTS ; z++ ) {
				ttk_pixel( srf, vglob.ptsX[w][z][0],
						    vglob.ptsY[w][z][0], 0xffffff );
				ttk_pixel( srf, vglob.ptsX[w][z][1],
					vglob.ptsY[w][z][1], 0xffffff );
			}
		}

	}
}

/* MUSIC PROFESSOR! */


/******************************************************************************/

void cleanup_vortex( void ) 
{
}


/* event valve routine */
int event_vortex (PzEvent *ev) 
{
	switch (ev->type) {
	case PZ_EVENT_SCROLL:

	    if( !vglob.paused ) {
		if( vglob.state == VORTEX_STATE_STYLESEL ) {
			TTK_SCROLLMOD( ev->arg, 5 );
			if( ev->arg > 0 )
				vglob.gameStyle++;
			else 
				vglob.gameStyle--;

			if( vglob.gameStyle < 0 )
				vglob.gameStyle = 0;
			if( vglob.gameStyle > VORTEX_STYLE_MAX )
				vglob.gameStyle = VORTEX_STYLE_MAX;
			ev->wid->dirty = 1;
		}

		if( vglob.state == VORTEX_STATE_LEVELSEL ) {
			TTK_SCROLLMOD( ev->arg, 5 );
			Vortex_selectLevel( vglob.startLevel + ev->arg );
		}

		if( vglob.state == VORTEX_STATE_GAME ) {
			TTK_SCROLLMOD( ev->arg, 7 );
			if( ev->arg > 0 ) {
				Vortex_decPosition( ev->arg );
			} else {
				Vortex_incPosition( ev->arg * -1 );
			}
		}
	    }
	    break;

	case PZ_EVENT_BUTTON_DOWN:
		switch( ev->arg ) {
		case( PZ_BUTTON_ACTION ):
			if( vglob.state == VORTEX_STATE_GAME ) {
			}
			break;
		case( PZ_BUTTON_HOLD ):
			vglob.paused = 1;
			break;
		}
		break;

	case PZ_EVENT_BUTTON_UP:
		switch( ev->arg ) {
		case( PZ_BUTTON_MENU ):
			if( !vglob.paused ) {
				pz_close_window (ev->wid->win);
			}
			break;

		case( PZ_BUTTON_HOLD ):
			vglob.paused = 0;
			break;

		case( PZ_BUTTON_NEXT ):
		case( PZ_BUTTON_PREVIOUS ):
			Vortex_Bolt_Superzapper();
			break;

		case( PZ_BUTTON_PLAY ):
			Vortex_stateChange( VORTEX_STATE_ADVANCE );
			break;

		case( PZ_BUTTON_ACTION ):
		    if( !vglob.paused ) {
			if( vglob.state == VORTEX_STATE_STARTUP ) {
				Vortex_stateChange( VORTEX_STATE_STYLESEL );
				Vortex_newLevelSetup();
			} else if( vglob.state == VORTEX_STATE_STYLESEL ) {
				Vortex_stateChange( VORTEX_STATE_LEVELSEL );
				vglob.startLevel = 0;
				vglob.currentLevel = 0;
				Vortex_newLevelSetup();
			} else if( vglob.state == VORTEX_STATE_LEVELSEL ) {
				Vortex_stateChange( VORTEX_STATE_GAME );
				Vortex_newLevelSetup();
			} else if( vglob.state == VORTEX_STATE_GAME ) {
				Vortex_Bolt_add();
			}
		    }
		    break;

		default:
			break;
		}
		break;

	case PZ_EVENT_DESTROY:
		cleanup_vortex();
		break;

	case PZ_EVENT_TIMER:
		if( vglob.hasAutoFire ) Vortex_Bolt_add();
		Vortex_timerTick( ev );
		break;
	}
	return 0;
}

/* initialize for when the user first instantiates vortex */
static void Vortex_Initialize( void )
{
	/* globals init */
	vglob.widget = NULL;
	vglob.startLevel = 0;
	vglob.currentLevel = 0;
	vglob.timer = 0;
	vglob.score = 0;
	vglob.lives = 3;
	vglob.hasParticleLaser = 0;
	vglob.hasSuperZapper = 1;
	vglob.hasAutoFire = 0;

	vglob.wPosMajor = 0;
	vglob.wPosMinor = 0;
	vglob.paused = 0;

	Vortex_stateChange( VORTEX_STATE_STARTUP );
}


/* create the vortex window, etc */
PzWindow *new_vortex_window()
{
	srand( time( NULL ));

	vglob.window = pz_new_window( "Vortex", PZ_WINDOW_NORMAL );

	/* make it full screen */
#ifdef VORTEX_FULLSCREEN
	ttk_window_hide_header( vglob.window );
	vglob.window->x = 0;
	vglob.window->y = 0;
	vglob.window->w = ttk_screen->w;
	vglob.window->h = ttk_screen->h;
#endif
	Vortex_Console_Init();

	vglob.widget = pz_add_widget( vglob.window, Vortex_draw, event_vortex );
	pz_widget_set_timer( vglob.widget, 30 );

	vglob.gameStyle = VORTEX_STYLE_2K5;
	Vortex_Initialize( );
	Vortex_Console_HiddenStatic( 1 );
	Vortex_Console_AddItem( "VORTEX", 0, 0, 
				VORTEX_STYLE_BOLD, vglob.color.title );
/*
	Star_SetStyle( STAR_MOTION_STATIC );
*/
	Module_Starfield_session( 200, (vglob.startLevel&1)?-1:1 );

	return pz_finish_window( vglob.window );
}




/* this will help simplify the creation of colors for mono or color */

ttk_color mc_color( cr, cg, cb,  mr, mg, mb )
{
	if( ttk_screen->bpp >= 16 ) {
		return( ttk_makecol( cr, cg, cb ));
	}
	return( ttk_makecol( mr, mg, mb ));
}

/* initialize at module init time */
void init_vortex() 
{
	int p,q;
	int i;

	/* internal module name */
	vglob.module = pz_register_module ("vortex", cleanup_vortex);

	/* menu item display name */
	pz_menu_add_action ("/Extras/Games/Vortex", new_vortex_window);

	/* Starfield setup */
	Module_Starfield_init();

	Vortex_Initialize();

	/* setup colors */
	vglob.color.bg           = mc_color(   0,   0,   0,  WHITE );
	vglob.color.title        = mc_color( 128, 255, 255,  BLACK );
	vglob.color.select       = mc_color( 255, 255,   0,  BLACK );
	vglob.color.sellevel     = mc_color(   0, 255,   0,  BLACK );
	vglob.color.credits      = mc_color(   0,   0, 255,  DKGREY );
	vglob.color.version      = mc_color(  64,  32,  32,  GREY );
	vglob.color.con          = mc_color( 255, 255,   0,  BLACK );
	vglob.color.bonus        = mc_color( 255,   0,   0,  DKGREY );
	vglob.color.web_top      = mc_color(   0, 128, 255,  BLACK );
	vglob.color.web_mid      = mc_color(   0,   0, 255,  DKGREY );
	vglob.color.web_bot      = mc_color(   0,   0, 128,  GREY );
	vglob.color.web_top_sel  = mc_color( 128, 255, 255,  GREY );
	vglob.color.web_top_dot  = mc_color( 128, 255, 255,  BLACK );
	vglob.color.web_bot_dot  = mc_color(   0, 128, 255,  GREY );
	vglob.color.web_fill     = mc_color(   0,   0,  30,  WHITE );
	vglob.color.baseind      = mc_color( 255, 128,   0,  BLACK );
	vglob.color.baseind_fill = mc_color( 255, 255,   0,  BLACK );
	vglob.color.score        = mc_color(   0, 255,   0,  BLACK );
	vglob.color.level        = mc_color(   0,   0, 128,  BLACK );
	vglob.color.player       = mc_color( 255, 128,   0,  DKGREY );
	vglob.color.player_fill  = mc_color( 255, 255,   0,  GREY );
	vglob.color.bolts        = mc_color( 255, 255, 255,  BLACK );

	vglob.color.plaser       = mc_color( 255, 128,   0,  BLACK );
	vglob.color.super        = mc_color( 255, 255, 255,  BLACK );
	vglob.color.flippers     = mc_color( 255,   0,   0,  BLACK );
	vglob.color.edeath       = mc_color( 255, 255,   0,  GREY );
	vglob.color.powerup	 = mc_color(  64, 225,  64,  DKGREY );
	vglob.color.sz_text1	 = mc_color(   0, 255, 128,  BLACK );
	vglob.color.sz_text2	 = mc_color( 128, 255,   0,  BLACK );
	vglob.color.spikers	 = mc_color(   0, 255, 128,  BLACK );
	vglob.color.spikes	 = mc_color(   0, 255,   0,  BLACK );
	vglob.color.spike_death	 = mc_color( 255, 255, 255,  GREY );

	/* precompute all of the web scaling... */
	/* convert from rom coordinates to screen coordinates */

	/* set up the screen sizes. */
#ifdef VORTEX_FULLSCREEN
	vglob.usableW   = ttk_screen->w;
	vglob.usableH   = ttk_screen->h;
#else
	vglob.usableW   = ttk_screen->w - ttk_screen->wx;
	vglob.usableH   = ttk_screen->h - ttk_screen->wy;
#endif

	/* scale the tubes appropriately */
	for( p=0 ; p<NLEVELS ; p++ )
	{
#ifdef NEVER
/* DUMP */
printf( "    {\n" );
printf( "        %d, /* order */\n", vortex_levels[p].order );

printf( "        { ");
for( i=0 ; i<8 ; i++ ) printf( "%d, ", vortex_levels[p].x[i] ); 
printf( "\n          " );
for( i=8 ; i<15 ; i++ ) printf( "%d, ", vortex_levels[p].x[i] );
printf( "%d ", vortex_levels[p].x[i] );
printf( "}, /* x */\n");

printf( "        { ");
for( i=0 ; i<8 ; i++ ) printf( "%d, ", vortex_levels[p].y[i] ); 
printf( "\n          " );
for( i=8 ; i<15 ; i++ ) printf( "%d, ", vortex_levels[p].y[i] );
printf( "%d ", vortex_levels[p].y[i] );
printf( "}, /* y */\n");

printf( "        { ");
for( i=0 ; i<8 ; i++ ) printf( "%d, ", vortex_levels[p].angle[i] ); 
printf( "\n          " );
for( i=8 ; i<15 ; i++ ) printf( "%d, ", vortex_levels[p].angle[i] );
printf( "%d ", vortex_levels[p].angle[i] );
printf( "}, /* angles */\n");

printf( "        %d, /* scale */\n", vortex_levels[p].scale );
printf( "        %d, /* fscale */\n", vortex_levels[p].fscale );
printf( "        %d, /* y3d */\n", vortex_levels[p].y3d );
printf( "        %d, /* y2d */\n", vortex_levels[p].y2d );
printf( "        %s /* FLAGS */\n", 
	    (vortex_levels[p].flags == LF_CLOSEDWEB)?
			"LF_CLOSEDWEB" : "LF_OPENWEB" );
printf( "    },\n" );
#endif


		/* and scale y3d while we're at it... */
		vortex_levels[p].y3d = ( vortex_levels[p].y3d 
                            * (vglob.usableH - VPADDING)) >> 8;
		vortex_levels[p].y3d = vortex_levels[p].y3d
			    - ((vglob.usableH - VPADDING )>>1)
			    + (ttk_screen->h>>1);
	}


	/* now, compute the front pixels (fx, fy) */
	for( p=0 ; p<NLEVELS ; p++ ) {
	    	for( q=0 ; q<16 ; q++ ) {
			/* scale */
			vortex_levels[p].fx[q] = ( vortex_levels[p].x[q] 
				* (vglob.usableH - VPADDING )) >> 8;
			vortex_levels[p].fy[q] = ( vortex_levels[p].y[q] 
				* (vglob.usableH - VPADDING )) >> 8;

			/* and translate */
			vortex_levels[p].fx[q] = vortex_levels[p].fx[q] 
			    - ( (vglob.usableH-VPADDING)>>1 )
			    + (vglob.usableW>>1);
			vortex_levels[p].fy[q] = vortex_levels[p].fy[q] 
			    - ( (vglob.usableH-VPADDING)>>1 )
			    + (vglob.usableH>>1);
		}
	}

	/* and finally, compute the rear pixels (rx, ry) */ 
	for( p=0 ; p<NLEVELS ; p++ ) {
	    	for( q=0 ; q<16 ; q++ ) {
			/* swipe the results from above and scale them*/
			vortex_levels[p].rx[q] = 
			    ( (vortex_levels[p].fx[q]) + ((vglob.usableW>>1)*3)
			    )>>2;

			vortex_levels[p].ry[q] = 
			    (   (vortex_levels[p].fy[q]) 
			      + (vortex_levels[p].y3d *3)
			    )>>2;
		}
	}
}

PZ_MOD_INIT (init_vortex)
