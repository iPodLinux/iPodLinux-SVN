/*
 * Vortex - A "Tempest"/"Tempest 2000" style game.
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "pz.h"
#include "console.h"
#include "vglobals.h"
#include "levels.h"
#include "vstars.h"
#include "vgamobjs.h"

/* version number */
#define VORTEX_VERSION	"05122223"

/* change this #define to an #undef if you want the header bar to show */
#define VORTEX_FULLSCREEN

/* the globals */
vortex_globals vglob;

/* padding around the web to the top and bottom */
#define VPADDING (10)


void Vortex_initLevel( void );

int Vortex_Rand( int max )
{
        return (int)((float)max * rand() / (RAND_MAX + 1.0));
}


void Vortex_ChangeToState( int st )
{
	Vortex_Console_WipeAllText();
	Vortex_Console_HiddenStatic( 0 );
	vglob.timer = 0;
	vglob.state = st;
	if( vglob.gameStyle != VORTEX_STYLE_CLASSIC ) Star_GenerateStars();
}


void Vortex_DrawWeb( ttk_surface srf )
{
	short xx[5];
	short yy[5];
	LEVELDATA * lv = &vortex_levels[ vglob.currentLevel ];
	int p, n;
	int maxP = 15;

	if( vortex_levels[ vglob.currentLevel ].flags & LF_CLOSEDWEB ) maxP++;

	/* fill the web */
	if( vglob.gameStyle != VORTEX_STYLE_CLASSIC ) {
		for( p=0 ; p<maxP ; p++ )
		{
			n = (p+1)&0x0f;
			xx[0] = lv->rx[p];	yy[0] = lv->ry[p];
			xx[1] = lv->fx[p];	yy[1] = lv->fy[p];
			xx[2] = lv->fx[n];	yy[2] = lv->fy[n];
			xx[3] = lv->rx[n];	yy[3] = lv->ry[n];

			ttk_fillpoly( srf, 4, xx, yy, vglob.color.web_fill );
		}
	}

	/* draw the back */
	for( p=0 ; p<maxP ; p++ ) {
		n = (p+1)&0x0f;
		ttk_aaline( srf,    lv->rx[p], lv->ry[p],
				    lv->rx[n], lv->ry[n],
				    vglob.color.web_bot );
	}

	/* draw the arms */
	for( p=0 ; p<16 ; p++ )
		ttk_aaline( srf,    lv->fx[p], lv->fy[p],
				    lv->rx[p], lv->ry[p],
				    vglob.color.web_mid );

	/* draw the vector simulation dots */
	for( p=0 ; p<16 ; p++ )
	    ttk_pixel( srf, lv->rx[p], lv->ry[p], vglob.color.web_bot_dot );

	/* draw the front */
	for( p=0 ; p<maxP ; p++ ) {
		n = (p+1)&0x0f;
		ttk_aaline( srf,    lv->fx[p], lv->fy[p],
				    lv->fx[n], lv->fy[n],
				    vglob.color.web_top );
	}

	/* draw the top vector simulation dots */
	for( p=0 ; p<16 ; p++ )
	    ttk_pixel( srf, lv->fx[p], lv->fy[p], vglob.color.web_top_dot );
}

void Vortex_DrawPlayer( ttk_surface srf )
{
	if( vglob.gameStyle == VORTEX_STYLE_CLASSIC )
	{
		ttk_aapoly( srf, 4, vglob.px, vglob.py,
				vglob.color.player_fill );
	} else {
		ttk_fillpoly( srf, 4, vglob.px, vglob.py, 
				vglob.color.player_fill );
		ttk_aapoly( srf, 4, vglob.px, vglob.py,
				vglob.color.player );
	}
}


void Vortex_OutlinedTextCenter( ttk_surface srf, const char *string, 
			int x, int y, int cw, int ch, int kern, 
			ttk_color col, ttk_color olc )
{
	/* first the outline */
	/* cardinals */
	pz_vector_string_center( srf, string, x+1, y, cw, ch, kern, olc );
	pz_vector_string_center( srf, string, x-1, y, cw, ch, kern, olc );
	pz_vector_string_center( srf, string, x, y-1, cw, ch, kern, olc );
	pz_vector_string_center( srf, string, x, y+1, cw, ch, kern, olc );
	/* diagonals */
	pz_vector_string_center( srf, string, x+1, y-1, cw, ch, kern, olc );
	pz_vector_string_center( srf, string, x+1, y+1, cw, ch, kern, olc );
	pz_vector_string_center( srf, string, x-1, y-1, cw, ch, kern, olc );
	pz_vector_string_center( srf, string, x-1, y+1, cw, ch, kern, olc );

	/* now the fg color */
	pz_vector_string_center( srf, string, x, y, cw, ch, kern, col );
}

void Vortex_DrawAvailableBase( ttk_surface srf, int x, int y )
{
	short xx[4], yy[4];
	xx[0] = x; 	yy[0] = y+9;
	xx[1] = x+5;	yy[1] = y;
	xx[2] = x+10;	yy[2] = y+9;
	xx[3] = x+5;	yy[3] = y+6;

	if( vglob.gameStyle != VORTEX_STYLE_CLASSIC )
	{
		ttk_fillpoly( srf, 4, xx, yy, vglob.color.baseind_fill );
		ttk_aapoly( srf, 4, xx, yy, vglob.color.baseind );
	} else {
		ttk_aapoly( srf, 4, xx, yy, vglob.color.baseind_fill );
	}
}

void Vortex_DrawAvailableBases( ttk_surface srf )
{
	char buf[16];
	int x;

	if( vglob.lives > 4 ) {
		snprintf( buf, 15, "%d", vglob.lives );
		pz_vector_string( srf, buf, 1, 1,
			5, 9, 1, vglob.color.baseind);
		Vortex_DrawAvailableBase( srf, 8, 1 );
		
	} else {
		for( x=0 ; x<vglob.lives ; x++ ) {
			Vortex_DrawAvailableBase( srf, 1+(x*12), 1 );
		}
	}
}


void draw_vortex (PzWidget *widget, ttk_surface srf) 
{
	char buf[16];
	char * word;
	char * credit;

	ttk_fillrect( srf, 0, 0, ttk_screen->w, ttk_screen->h, vglob.color.bg );

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

	switch( vglob.state ) {
	case VORTEX_STATE_STARTUP:
		if( vglob.gameStyle != VORTEX_STYLE_CLASSIC )
			Star_DrawStars( srf );
		Vortex_Console_Render( srf );
		if( Vortex_Console_GetZoomCount() == 0 )
			Vortex_ChangeToState( VORTEX_STATE_STYLESEL );
			Vortex_initLevel();
			vglob.wPosMajor = 8;
		break;

	case VORTEX_STATE_STYLESEL:
	case VORTEX_STATE_LEVELSEL:
		if( vglob.gameStyle != VORTEX_STYLE_CLASSIC )
			Star_DrawStars( srf );
		Vortex_DrawWeb( srf );

		if( vglob.state == VORTEX_STATE_STYLESEL ) {
			Vortex_DrawPlayer( srf );
			Vortex_Bolt_draw( srf );

/*
			for( x=0 ; x<vglob.lives ; x++ ) {
				Vortex_DrawAvailableBase( srf, 1+(x*12), 1 );
			}
*/
			Vortex_DrawAvailableBases( srf );

			/* let the user select what style to play with */
			switch((vglob.timer>>4) & 0x03 ) {
			case 0: word = "SELECT"; break;
			case 1: word = "GAME";   break;
			case 2: word = "STYLE";  break;
			case 3: word = "";       break;
			}
		} else {
			/* let the user select what level to start on */
			switch((vglob.timer>>4) & 0x03 ) {
			case 0: word = "SELECT"; break;
			case 1: word = "START";  break;
			case 2: word = "LEVEL";  break;
			case 3: word = "";       break;
			}
		}

		Vortex_OutlinedTextCenter( srf, word,
			    vglob.usableW>>1, 20,
			    10, 18, 1, vglob.color.select, vglob.color.bg );

		if( vglob.state == VORTEX_STATE_STYLESEL ) {
			if( vglob.gameStyle == VORTEX_STYLE_CLASSIC )
				snprintf( buf, 16, "CLASSIC %c", 
						VECTORFONT_SPECIAL_RIGHT );
			else if( vglob.gameStyle == VORTEX_STYLE_2K5 )
				snprintf( buf, 16, "%c 2K5", 
						VECTORFONT_SPECIAL_LEFT );
			else
				snprintf( buf, 16, "BURRITO" );
		} else {
			/* level number */
			snprintf( buf, 15, "%c %d %c", 
				(vglob.startLevel>0)?
						VECTORFONT_SPECIAL_LEFT:' ',
				vglob.startLevel,
				(vglob.startLevel<=(NLEVELS-2))?
						VECTORFONT_SPECIAL_RIGHT:' ');
		}
		Vortex_OutlinedTextCenter( srf, buf,
			vglob.usableW>>1, 50,
			10, 18, 1, vglob.color.sellevel, vglob.color.bg );

		/* display some props to the masters */
		switch((vglob.timer>>5) & 0x03 ) {
		case 0: credit = "ALL THANKS TO";    break;
		case 1: credit = "DAVE THEURER"; break;
		case 2: credit = "JEFF MINTER";  break;
		case 3: credit = "";             break;
		}
		Vortex_OutlinedTextCenter( srf, credit, 
			    vglob.usableW>>1, vglob.usableH-10,
			    5, 9, 1, vglob.color.credits,
			    vglob.color.bg );

		/* and the version number */
		pz_vector_string( srf, VORTEX_VERSION, 1,
			    vglob.usableH-6, 3, 5, 1, vglob.color.version );
		break;

	case VORTEX_STATE_GAME:
		/* draw some stars */
		if( vglob.gameStyle != VORTEX_STYLE_CLASSIC )
			Star_DrawStars( srf );

		/* draw the playfield */
		Vortex_DrawWeb( srf );

		/* draw powerups */
		Vortex_Powerup_draw( srf );

		/* draw the player */
		Vortex_DrawPlayer( srf );

		/* draw bolts */
		Vortex_Bolt_draw( srf );

		/* draw enemies */
		Vortex_Enemy_draw( srf );

		/* draw any console text over all of that */
		Vortex_Console_Render( srf );

		/* plop down the score */
		snprintf( buf, 15, "%04d", vglob.score );
		pz_vector_string( srf, buf,
			    vglob.usableW - pz_vector_width( buf, 5, 9, 1 ) -1,
			    1, 5, 9, 1, vglob.color.score );

		/* and lives left */
		Vortex_DrawAvailableBases( srf );

		/* and superzapper indicator, if applicable */
		if( vglob.hasSuperZapper )
			pz_vector_string( srf, "SZ", 1, 12, 5, 9, 1, 
					vglob.color.super );

		/* and current level */
		snprintf( buf, 15, "L%d", vglob.startLevel );
		pz_vector_string( srf, buf, 
			    vglob.usableW -
			     pz_vector_width( buf, 5, 9, 1 ) -1,
			    vglob.usableH-10,
                            5, 9, 1, vglob.color.level );
		break;

	case VORTEX_STATE_ADVANCE:
		/* XXX this needs a lot more code - animations */
		if( vglob.gameStyle != VORTEX_STYLE_CLASSIC )
			Star_DrawStars( srf );
		Vortex_Console_Render( srf );
		break;

	case VORTEX_STATE_DEATH:
		if( vglob.gameStyle != VORTEX_STYLE_CLASSIC )
			Star_DrawStars( srf );
		Vortex_Console_Render( srf );
		if( Vortex_Console_GetZoomCount() == 0 )
		{
			Vortex_ChangeToState( VORTEX_STATE_DEAD );
			pz_close_window( vglob.window );
		}
		break;

	case VORTEX_STATE_DEAD:
		break;

	}
}


void cleanup_vortex( void ) 
{
}



/* finds the x/y position along [vect]or (point on outer edge of web)  
   at depth z (128 == outer edge of web), from the [center] 
 */
#define Vortex_getZ( vect, z, center )\
	((center) + ((((vect)-(center)) * (z) )>>7))


void Vortex_clawGeometryCompute( void ) /* NOTE: not "cowCompute" */ /* moo. */
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

void Vortex_newLevelCompute( void )
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

void Vortex_player_clear( void )
{
	vglob.hasParticleLaser = 0;
	vglob.hasSuperZapper = 1;
	vglob.wPosMajor = 0;
	vglob.wPosMinor = 0;
}

/* all of the stuff that needs to get reset at the beginning of a new level */
void Vortex_initLevel( void )
{
	if( vglob.state == VORTEX_STATE_DEATH ) return;
	if( vglob.state == VORTEX_STATE_DEAD ) return;

	Vortex_newLevelCompute();
	Vortex_Powerup_clear();
	Vortex_Bolt_clear();
	Vortex_Enemy_clear();
	Vortex_clawGeometryCompute();
	Vortex_player_clear();

	if( vglob.state == VORTEX_STATE_STARTUP ) return;
	if( vglob.gameStyle != VORTEX_STYLE_CLASSIC )
		Star_GenerateStars();
}


/* number of minor steps per web.  0, 2, etc */
#define MINOR_MAX (0)

void Vortex_incPosition( int steps )
{
	if( steps > 1 ) Vortex_incPosition( steps-1 );

	if( vglob.wPosMinor < MINOR_MAX ) {
		vglob.wPosMinor++;
		Vortex_clawGeometryCompute();
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

	Vortex_clawGeometryCompute();
}

void Vortex_decPosition( int steps )
{
	if( steps > 1 ) Vortex_incPosition( steps-1 );

	if( vglob.wPosMinor > 0 ) {
		vglob.wPosMinor--;
		Vortex_clawGeometryCompute();
		return;
	}

	if( vglob.wPosMajor > 0 ) {
		vglob.wPosMajor--;
		vglob.wPosMinor = MINOR_MAX;
		Vortex_clawGeometryCompute();
		return;
	}

	if( vortex_levels[ vglob.currentLevel ].flags & LF_CLOSEDWEB ) {
		vglob.wPosMajor = 15;
		vglob.wPosMinor = MINOR_MAX;
		Vortex_clawGeometryCompute();
	}
}


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
			Star_GenerateStars();
	}
}

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
			TTK_SCROLLMOD( ev->arg, 3 );
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

		case( PZ_BUTTON_PLAY ):
			Vortex_ChangeToState( VORTEX_STATE_ADVANCE );
			break;

		case( PZ_BUTTON_ACTION ):
		    if( !vglob.paused ) {
			if( vglob.state == VORTEX_STATE_STARTUP ) {
				Vortex_ChangeToState( VORTEX_STATE_STYLESEL );
				Vortex_initLevel();
				vglob.wPosMajor = 8;
			} else if( vglob.state == VORTEX_STATE_STYLESEL ) {
				Vortex_ChangeToState( VORTEX_STATE_LEVELSEL );
				vglob.startLevel = 0;
				vglob.currentLevel = 0;
				Vortex_initLevel();
			} else if( vglob.state == VORTEX_STATE_LEVELSEL ) {
				Vortex_ChangeToState( VORTEX_STATE_GAME );
				Vortex_initLevel();
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
		switch( vglob.state ){
		case( VORTEX_STATE_STARTUP ):
			/* generate a splash screen */
			if( vglob.timer < 5 )
				Vortex_Console_AddItem( "VORTEX", 0, 0, 
				    VORTEX_STYLE_NORMAL, vglob.color.title );
			break;

		case( VORTEX_STATE_STYLESEL ):
			/* just move stuff around */
			if( (vglob.timer & 0x3f) == 0x3f ) {
				if( vglob.startLevel > 16 ) 
					vglob.startLevel = -1;
				Vortex_selectLevel( vglob.startLevel + 1 );
				Vortex_initLevel();
			}

			if( (vglob.timer & 0x1f) == 0x1f ) 
				vglob.hasParticleLaser ^= 1;

			if( (vglob.timer & 0x1f) == 0x1f ) 
				Vortex_decPosition( 1 );
			else if( (vglob.timer & 0x0f) == 0x0f)
				Vortex_incPosition( 1 );
			if( (vglob.timer & 0x0f) == 0x0f )
				Vortex_Bolt_add();
			break;

		case( VORTEX_STATE_ADVANCE ):
			/* xxx */
			if( vglob.timer == 0 ) {
			    Vortex_Console_WipeAllText();
			    Vortex_Console_HiddenStatic( 1 );
			    Vortex_Console_AddItem( "SUPERZAPPER", 0, 0, 
				VORTEX_STYLE_NORMAL, vglob.color.sz_text1 );
			}

			if( vglob.timer == 20 ) {
			    Vortex_Console_AddItem( "RECHARGE", 0, 0, 
				VORTEX_STYLE_NORMAL, vglob.color.sz_text2 );
			}

			if( vglob.timer > 60 ) {
				Vortex_ChangeToState( VORTEX_STATE_GAME );
				Vortex_selectLevel( vglob.currentLevel + 1 );
				Vortex_initLevel();
				Vortex_Console_WipeAllText();
				Vortex_Console_HiddenStatic( 0 );
			}
			break;

		case( VORTEX_STATE_DEATH ):
			if( vglob.timer < 5 )
				Vortex_Console_AddItem( "GAME OVER", 0, 0, 
				    VORTEX_STYLE_NORMAL, vglob.color.title );
			break;
		}

		if( !vglob.paused ) {
			Vortex_Powerup_poll();
			Vortex_Bolt_poll();
			Vortex_Enemy_poll();
			Vortex_Console_Tick();
			Vortex_CollisionDetection();

			/* generate a death screen - state changer */
			if( (vglob.lives <= 0)
			    && (vglob.state != VORTEX_STATE_DEATH) ) {
				Vortex_ChangeToState( VORTEX_STATE_DEATH );
				Vortex_Console_HiddenStatic( 1 );
				Vortex_Console_AddItem( "GAME OVER", 0, 0, 
					VORTEX_STYLE_NORMAL, vglob.color.title );
			}

			vglob.timer++;
		}
		ev->wid->dirty = 1;
		break;
	}
	return 0;
}

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

	vglob.wPosMajor = 0;
	vglob.wPosMinor = 0;
	vglob.paused = 0;

	Vortex_ChangeToState( VORTEX_STATE_STARTUP );
}


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

	vglob.widget = pz_add_widget( vglob.window, draw_vortex, event_vortex );
	pz_widget_set_timer( vglob.widget, 30 );

	vglob.gameStyle = VORTEX_STYLE_2K5;
	Vortex_Initialize( );
	Vortex_Console_HiddenStatic( 1 );
	Vortex_Console_AddItem( "VORTEX", 0, 0, 
				VORTEX_STYLE_BOLD, vglob.color.title );
	Star_GenerateStars();

	return pz_finish_window( vglob.window );
}



void init_vortex() 
{
	int p,q;

	/* internal module name */
	vglob.module = pz_register_module ("vortex", cleanup_vortex);

	/* menu item display name */
	pz_menu_add_action ("/Extras/Games/Vortex", new_vortex_window);

	Vortex_Initialize();

	/* setup colors */
	if( ttk_screen->bpp >= 16 ) {
		/* full color! */
		vglob.color.bg          = ttk_makecol(   0,   0,   0 );
		vglob.color.title       = ttk_makecol( 128, 255, 255 );
		vglob.color.select      = ttk_makecol( 255, 255,   0 );
		vglob.color.sellevel    = ttk_makecol(   0, 255,   0 );
		vglob.color.credits     = ttk_makecol(   0,   0, 255 );
		vglob.color.version     = ttk_makecol(  64,  32,  32 );
		vglob.color.con         = ttk_makecol( 255, 255,   0 );
		vglob.color.bonus       = ttk_makecol( 255,   0,   0 );
		vglob.color.web_top     = ttk_makecol(   0, 128, 255 );
		vglob.color.web_mid     = ttk_makecol(   0,   0, 255 );
		vglob.color.web_bot     = ttk_makecol(   0,   0, 128 );
		vglob.color.web_top_sel = ttk_makecol( 128, 255, 255 );
		vglob.color.web_top_dot = ttk_makecol( 128, 255, 255 );
		vglob.color.web_bot_dot = ttk_makecol(   0, 128, 255 );
		vglob.color.web_fill    = ttk_makecol(   0,   0,  30 );
		vglob.color.baseind     = ttk_makecol( 255, 128,   0 );
		vglob.color.baseind_fill= ttk_makecol( 255, 255,   0 );
		vglob.color.score       = ttk_makecol(   0, 255, 255 );
		vglob.color.level       = ttk_makecol(   0,   0, 128 );
		vglob.color.player      = ttk_makecol( 255, 128,   0 );
		vglob.color.player_fill = ttk_makecol( 255, 255,   0 );
		vglob.color.bolts       = ttk_makecol( 255, 255, 255 );
		vglob.color.plaser      = ttk_makecol( 255, 128,   0 );
		vglob.color.super       = ttk_makecol(   0, 255, 255 );
		vglob.color.flippers    = ttk_makecol( 255,   0,   0 );
		vglob.color.edeath      = ttk_makecol( 255, 255,   0 );
		vglob.color.powerup	= ttk_makecol(  64, 225,  64 );
		vglob.color.sz_text1	= ttk_makecol(   0, 255, 128 );
		vglob.color.sz_text2	= ttk_makecol( 128, 255,   0 );
	} else {
		/* monochrome iPod */
		vglob.color.bg          = ttk_makecol( WHITE );
		vglob.color.title       = ttk_makecol( BLACK );
		vglob.color.select      = ttk_makecol( BLACK );
		vglob.color.sellevel    = ttk_makecol( BLACK );
		vglob.color.credits     = ttk_makecol( DKGREY );
		vglob.color.version     = ttk_makecol( GREY );
		vglob.color.con         = ttk_makecol( BLACK );
		vglob.color.bonus       = ttk_makecol( DKGREY );
		vglob.color.web_top     = ttk_makecol( BLACK );
		vglob.color.web_mid     = ttk_makecol( DKGREY );
		vglob.color.web_bot     = ttk_makecol( GREY );
		vglob.color.web_top_sel = ttk_makecol( GREY );
		vglob.color.web_top_dot = ttk_makecol( BLACK );
		vglob.color.web_bot_dot = ttk_makecol( GREY );
		vglob.color.web_fill    = ttk_makecol( WHITE );
		vglob.color.baseind     = ttk_makecol( BLACK );
		vglob.color.baseind_fill= ttk_makecol( BLACK );
		vglob.color.score       = ttk_makecol( BLACK );
		vglob.color.level       = ttk_makecol( BLACK );
		vglob.color.player      = ttk_makecol( DKGREY );
		vglob.color.player_fill = ttk_makecol( GREY );
		vglob.color.bolts       = ttk_makecol( BLACK );
		vglob.color.plaser      = ttk_makecol( BLACK );
		vglob.color.super       = ttk_makecol( DKGREY );
		vglob.color.flippers    = ttk_makecol( BLACK );
		vglob.color.edeath      = ttk_makecol( GREY );
		vglob.color.powerup     = ttk_makecol( DKGREY );
		vglob.color.sz_text1	= ttk_makecol( BLACK );
		vglob.color.sz_text2	= ttk_makecol( BLACK );
	}


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

	/* flip all of the Y pixels. */
	for( p=0 ; p<NLEVELS ; p++ )
	{
		/* web pixels... */
	    	for( q=0 ; q<16 ; q++ )
			vortex_levels[p].y[q] = 256 - vortex_levels[p].y[q];

		/* flip y3d */
		vortex_levels[p].y3d = 256 - vortex_levels[p].y3d;

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
