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
 *	"Tempest" (c)1980 Atari
 *	"Tempest 2000", (c)1994,1996 Atari/Llamasoft
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

static vortex_globals vglob;
static int startcount;
#define VPADDING (10)

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
}

void Vortex_DrawWeb( ttk_surface srf )
{
	LEVELDATA * lv = &vortex_levels[ vglob.currentLevel ];
	int p;

	/* draw the back */
	for( p=0 ; p<15 ; p++ )
	    ttk_aaline( srf,	lv->rx[p], lv->ry[p],
				lv->rx[p+1], lv->ry[p+1],
				vglob.color.web_bot );

	if( vortex_levels[ vglob.currentLevel ].flags & LF_CLOSEDWEB )
	    ttk_aaline( srf,	lv->rx[0], lv->ry[0],
				lv->rx[15], lv->ry[15],
				vglob.color.web_bot );

	/* draw the arms */
	for( p=0 ; p<16 ; p++ )
	    ttk_aaline( srf,	lv->fx[p], lv->fy[p],
				lv->rx[p], lv->ry[p],
				vglob.color.web_mid );

	/* draw the vector simulation dots */
	for( p=0 ; p<16 ; p++ )
	    ttk_pixel( srf, lv->rx[p], lv->ry[p], vglob.color.web_bot_dot );

	/* draw the front */
	for( p=0 ; p<15 ; p++ )
	    ttk_aaline( srf,	lv->fx[p], lv->fy[p],
				lv->fx[p+1], lv->fy[p+1],
				vglob.color.web_top );

	if( vortex_levels[ vglob.currentLevel ].flags & LF_CLOSEDWEB )
	    ttk_aaline( srf,	lv->fx[0], lv->fy[0],
				lv->fx[15], lv->fy[15],
				vglob.color.web_top );

	/* draw the top vector simulation dots */
	for( p=0 ; p<16 ; p++ )
	    ttk_pixel( srf, lv->fx[p], lv->fy[p], vglob.color.web_top_dot );
}

void Vortex_DrawPlayer( ttk_surface srf )
{
	LEVELDATA * lv = &vortex_levels[ vglob.currentLevel ];
	int p2 = vglob.wPosMajor +1;

	if( p2 > 15 ) p2 = 0;

	/* draw the claw */
	ttk_aaline( srf, lv->fx[vglob.wPosMajor], lv->fy[vglob.wPosMajor],
		       vglob.pcx1, vglob.pcy1,
		       vglob.color.player );

	ttk_aaline( srf, vglob.pcx1, vglob.pcy1,
		       lv->fx[p2], lv->fy[p2],
		       vglob.color.player );

	ttk_aaline( srf, lv->fx[p2], lv->fy[p2],
		       vglob.pcx2, vglob.pcy2,
		       vglob.color.player );

	ttk_aaline( srf, vglob.pcx2, vglob.pcy2,
		       lv->fx[vglob.wPosMajor], lv->fy[vglob.wPosMajor],
		       vglob.color.player );
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


void draw_vortex (PzWidget *widget, ttk_surface srf) 
{
	char buf[16];
	char * word;
	char * credit;

	ttk_fillrect( srf, 0, 0, ttk_screen->w, ttk_screen->h, vglob.color.bg );

	switch( vglob.state ) {
	case VORTEX_STATE_STARTUP:
		Vortex_Console_Render( srf );
		if( Vortex_Console_GetZoomCount() == 0 )
			Vortex_ChangeToState( VORTEX_STATE_LEVELSEL );
		break;

	case VORTEX_STATE_LEVELSEL:
		Vortex_DrawWeb( srf );

		/* let the user select what level to start on */
		switch((vglob.timer>>3) & 0x03 ) {
		case 0: word = "SELECT"; break;
		case 1: word = "START";  break;
		case 2: word = "LEVEL";  break;
		case 3: word = "";       break;
		}

		Vortex_OutlinedTextCenter( srf, word,
			    (ttk_screen->w - ttk_screen->wx)>>1, 20,
			    10, 18, 1, vglob.color.select, vglob.color.bg );

		/* level number */
		snprintf( buf, 15, "%c %d %c", 
			(vglob.startLevel>0)?
					VECTORFONT_SPECIAL_LEFT:' ',
			vglob.startLevel,
			(vglob.startLevel<=(NLEVELS-2))?
					VECTORFONT_SPECIAL_RIGHT:' ');
		Vortex_OutlinedTextCenter( srf, buf,
			    (ttk_screen->w - ttk_screen->wx)>>1, 50,
			    10, 18, 1, vglob.color.level, vglob.color.bg );

		/* display some props to the masters */
		switch((vglob.timer>>4) & 0x03 ) {
		case 0: credit = "THANKS TO";    break;
		case 1: credit = "DAVE THEURER"; break;
		case 2: credit = "JEFF MINTER";  break;
		case 3: credit = "";             break;
		}
		Vortex_OutlinedTextCenter( srf, credit, 
			    (ttk_screen->w - ttk_screen->wx)>>1, 
			    (ttk_screen->h - ttk_screen->wy)-10, 
			    5, 9, 1, vglob.color.credits,
			    vglob.color.bg );
		break;

	case VORTEX_STATE_GAME:
		/* do game stuff in here */
		pz_vector_string_center( srf, "[menu] to exit.",
                            (ttk_screen->w - ttk_screen->wx)>>1, 10,
                            5, 9, 1, vglob.color.credits );

		/* draw the playfield */
		Vortex_DrawWeb( srf );

		/* draw the player */
		Vortex_DrawPlayer( srf );

		/* draw any console text over all of that */
		Vortex_Console_Render( srf );

		/* plop down the score */
		snprintf( buf, 15, "%04d", vglob.score );
		pz_vector_string( srf, buf,
                            (ttk_screen->w - ttk_screen->wx) -
			     pz_vector_width( buf, 5, 9, 1 ) -1, 1,
                            5, 9, 1, vglob.color.score );

		/* and lives left */
		snprintf( buf, 15, "%d", vglob.lives );
		pz_vector_string( srf, buf, 1, 1,
                            5, 9, 1, vglob.color.baseind );
		break;

	case VORTEX_STATE_DEATH:
		break;

	case VORTEX_STATE_DEAD:
		break;

	}
}


void cleanup_vortex() 
{
}

#define MINOR_MAX (0)

void Vortex_clawCompute( void )
{
	LEVELDATA * lv = &vortex_levels[ vglob.currentLevel ];
	int wxC = ttk_screen->w>>1;   /* web X center */
	int wyC = lv->y3d;            /* web Y center */

	int p2 = vglob.wPosMajor +1;  /* second point in the selected field */
	if( p2 > 15 ) p2 = 0;	      /* adjust for wraparound */

	/* store aside the centerpoint of the field edge */
	vglob.pcxC = (lv->fx[ vglob.wPosMajor ] + lv->fx[ p2 ]) >> 1;
	vglob.pcyC = (lv->fy[ vglob.wPosMajor ] + lv->fy[ p2 ]) >> 1;

	/* compute the two center points of the claw (extrapolation) */
	vglob.pcx1 = wxC + (((vglob.pcxC - wxC) * 150)>>7);
	vglob.pcy1 = wyC + (((vglob.pcyC - wyC) * 150)>>7);

	vglob.pcx2 = wxC + (((vglob.pcxC - wxC) * 140)>>7);
	vglob.pcy2 = wyC + (((vglob.pcyC - wyC) * 140)>>7);
}

void Vortex_incPosition( int steps )
{
	if( steps > 1 ) Vortex_incPosition( steps-1 );

	if( vglob.wPosMinor < MINOR_MAX ) {
		vglob.wPosMinor++;
		Vortex_clawCompute();
		return;
	}

	if( vglob.wPosMajor < 14 ) {
		vglob.wPosMajor++;
		vglob.wPosMinor = 0;
		Vortex_clawCompute();
		return;
	}

	if( vortex_levels[ vglob.currentLevel ].flags & LF_CLOSEDWEB ) {
		vglob.wPosMajor = 0;
		vglob.wPosMinor = 0;
		Vortex_clawCompute();
	}
}

void Vortex_decPosition( int steps )
{
	if( steps > 1 ) Vortex_incPosition( steps-1 );

	if( vglob.wPosMinor > 0 ) {
		vglob.wPosMinor--;
		Vortex_clawCompute();
		return;
	}

	if( vglob.wPosMajor > 0 ) {
		vglob.wPosMajor--;
		vglob.wPosMinor = MINOR_MAX;
		Vortex_clawCompute();
		return;
	}

	if( vortex_levels[ vglob.currentLevel ].flags & LF_CLOSEDWEB ) {
		vglob.wPosMajor = 15;
		vglob.wPosMinor = MINOR_MAX;
		Vortex_clawCompute();
	}
}

int event_vortex (PzEvent *ev) 
{
	switch (ev->type) {
	case PZ_EVENT_SCROLL:

		if( vglob.state == VORTEX_STATE_LEVELSEL ) {
			TTK_SCROLLMOD( ev->arg, 4 );
	   		/* adjust */ 
			vglob.startLevel += ev->arg;

			/* and clip */
			if( vglob.startLevel < 0 ) 
				vglob.startLevel = 0;
			if( vglob.startLevel >= (NLEVELS-1) ) 
				vglob.startLevel = NLEVELS-1;
			vglob.currentLevel = vglob.startLevel;
		}

		if( vglob.state == VORTEX_STATE_GAME ) {
			TTK_SCROLLMOD( ev->arg, 1 );
			if( ev->arg > 0 ) {
				Vortex_decPosition( ev->arg );
			} else {
				Vortex_incPosition( ev->arg * -1 );
			}

			/* change the level for now too... */
/*
			vglob.currentLevel += ev->arg;
			if( vglob.currentLevel < 0 ) 
				vglob.currentLevel = 0;
			if( vglob.currentLevel >= (NLEVELS-1) ) 
				vglob.currentLevel = NLEVELS-1;
*/
		}
		break;

	case PZ_EVENT_BUTTON_UP:
		switch( ev->arg ) {
		case( PZ_BUTTON_MENU ):
			pz_close_window (ev->wid->win);
			break;

		case( PZ_BUTTON_ACTION ):
			if( vglob.state == VORTEX_STATE_LEVELSEL ) {
				Vortex_ChangeToState( VORTEX_STATE_GAME );
				Vortex_clawCompute();
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
		if( (vglob.state == VORTEX_STATE_STARTUP) &&
		    (++startcount < 3 ) )
			Vortex_Console_AddItem( "VORTEX", 0, 0, 
				VORTEX_STYLE_NORMAL, vglob.color.title );

		Vortex_Console_Tick();
		vglob.timer++;
		ev->wid->dirty = 1;
		break;
	}
	return 0;
}

static void Vortex_Initialize( void )
{
	/* globals init */
	vglob.widget = NULL;
	vglob.state = VORTEX_STATE_STARTUP;
	vglob.level = 0;
	vglob.startLevel = 0;
	vglob.currentLevel = 0;
	vglob.timer = 0;
	vglob.score = 0;
	vglob.lives = 3;

	vglob.wPosMajor = 0;
	vglob.wPosMinor = 0;

	startcount = 0;
}

PzWindow *new_vortex_window()
{
	//fp = fopen (pz_module_get_datapath (vglob.module, "message.txt"), "r");

	srand( time( NULL ));
	Vortex_Console_Init();

	vglob.window = pz_new_window( "Vortex", PZ_WINDOW_NORMAL );
	vglob.widget = pz_add_widget( vglob.window, draw_vortex, event_vortex );
	pz_widget_set_timer( vglob.widget, 40 );

	Vortex_Initialize( );
	Vortex_Console_HiddenStatic( 1 );
	Vortex_Console_AddItem( "VORTEX", 0, 0, 
				VORTEX_STYLE_BOLD, vglob.color.title );

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
		vglob.color.level       = ttk_makecol(   0, 255,   0 );
		vglob.color.credits     = ttk_makecol(   0,   0, 255 );
		vglob.color.con         = ttk_makecol( 255, 255,   0 );
		vglob.color.bonus       = ttk_makecol( 255,   0,   0 );
		vglob.color.web_top     = ttk_makecol(   0, 128, 255 );
		vglob.color.web_mid     = ttk_makecol(   0,   0, 255 );
		vglob.color.web_bot     = ttk_makecol(   0,   0, 128 );
		vglob.color.web_top_sel = ttk_makecol( 128, 255, 255 );
		vglob.color.web_top_dot = ttk_makecol( 128, 255, 255 );
		vglob.color.web_bot_dot = ttk_makecol(   0, 128, 255 );
		vglob.color.baseind     = ttk_makecol(   0, 255,   0 );
		vglob.color.score       = ttk_makecol(   0, 255, 255 );
		vglob.color.player      = ttk_makecol( 255, 255,   0 );
		vglob.color.bolts       = ttk_makecol( 255, 255, 255 );
		vglob.color.super       = ttk_makecol(   0, 255, 255 );
		vglob.color.flippers    = ttk_makecol( 255,   0,   0 );
		vglob.color.stars       = ttk_makecol( 180, 210, 190 );
	} else {
		/* monochrome iPod */
		vglob.color.bg          = ttk_makecol( WHITE );
		vglob.color.title       = ttk_makecol( BLACK );
		vglob.color.select      = ttk_makecol( BLACK );
		vglob.color.level       = ttk_makecol( DKGREY );
		vglob.color.credits     = ttk_makecol( GREY );
		vglob.color.con         = ttk_makecol( BLACK );
		vglob.color.bonus       = ttk_makecol( DKGREY );
		vglob.color.web_top     = ttk_makecol( BLACK );
		vglob.color.web_mid     = ttk_makecol( DKGREY );
		vglob.color.web_bot     = ttk_makecol( GREY );
		vglob.color.web_top_sel = ttk_makecol( GREY );
		vglob.color.web_top_dot = ttk_makecol( BLACK );
		vglob.color.web_bot_dot = ttk_makecol( GREY );
		vglob.color.baseind     = ttk_makecol( BLACK );
		vglob.color.score       = ttk_makecol( BLACK );
		vglob.color.player      = ttk_makecol( DKGREY );
		vglob.color.bolts       = ttk_makecol( BLACK );
		vglob.color.super       = ttk_makecol( DKGREY );
		vglob.color.flippers    = ttk_makecol( BLACK );
		vglob.color.stars       = ttk_makecol( GREY );
	}

	/* precompute all of the web scaling... */
	/* convert from rom coordinates to screen coordinates */

	/* flip all of the Y pixels. */
	for( p=0 ; p<NLEVELS ; p++ )
	{
		/* web pixels... */
	    	for( q=0 ; q<16 ; q++ )
			vortex_levels[p].y[q] = 256 - vortex_levels[p].y[q];

		/* flip y3d */
		vortex_levels[p].y3d = 256 - vortex_levels[p].y3d;

		/* and scale y3d while we're at it... */
		vortex_levels[p].y3d = 
			    ( vortex_levels[p].y3d
			    * (ttk_screen->h-VPADDING-ttk_screen->wy) ) >> 8;
		vortex_levels[p].y3d = vortex_levels[p].y3d
			    - ((ttk_screen->h-VPADDING-ttk_screen->wy)>>1)
			    + (ttk_screen->h>>1);
	}


	/* now, compute the front pixels (fx, fy) */
	for( p=0 ; p<NLEVELS ; p++ ) {
	    	for( q=0 ; q<16 ; q++ ) {
			/* scale */
			vortex_levels[p].fx[q] = 
			    ( vortex_levels[p].x[q] 
			    * (ttk_screen->h-VPADDING-ttk_screen->wy) ) >> 8;
			vortex_levels[p].fy[q] = 
			    ( vortex_levels[p].y[q] 
			    * (ttk_screen->h-VPADDING-ttk_screen->wy) ) >> 8;

			/* and translate */
			vortex_levels[p].fx[q] = vortex_levels[p].fx[q] 
			    - ((ttk_screen->h-VPADDING/2-ttk_screen->wy)>>1)
			    + (ttk_screen->w>>1);
			vortex_levels[p].fy[q] = vortex_levels[p].fy[q] 
			    - ((ttk_screen->h-VPADDING/2-ttk_screen->wy)>>1)
			    + ((ttk_screen->h-VPADDING)>>1);
		}
	}

	/* and finally, compute the rear pixels (rx, ry) */ 
	for( p=0 ; p<NLEVELS ; p++ ) {
	    	for( q=0 ; q<16 ; q++ ) {
			/* swipe the results from above and scale them*/
			vortex_levels[p].rx[q] = 
			    (   (vortex_levels[p].fx[q]) 
			      + ((ttk_screen->w>>1)*3)
			    )>>2;

			vortex_levels[p].ry[q] = 
			    (   (vortex_levels[p].fy[q]) 
			      + (vortex_levels[p].y3d *3)
			    )>>2;
		}
	}
}

PZ_MOD_INIT (init_vortex)
