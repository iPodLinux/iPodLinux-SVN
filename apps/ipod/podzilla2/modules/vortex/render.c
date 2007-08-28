/*
 *     Vortex - render routines
 *	these routines draw stuff.
 *  
 * Copyright (C) 2006 Scott Lawrence
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
#include <errno.h>
#include <time.h>
#include "pz.h"
#include "console.h"
#include "vglobals.h"
#include "levels.h"
#include "vgamobjs.h"
#include "render.h"


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

void Vortex_RenderWeb( ttk_surface srf )
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

void Vortex_RenderPlayer( ttk_surface srf )
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


void Vortex_RenderAvailableBase( ttk_surface srf, int x, int y )
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

void Vortex_RenderAvailableBases( ttk_surface srf )
{
	char buf[16];
	int x;

	if( vglob.lives > 4 ) {
		snprintf( buf, 15, "%d", vglob.lives );
		pz_vector_string( srf, buf, 1, 1,
			5, 9, 1, vglob.color.baseind);
		Vortex_RenderAvailableBase( srf, 8, 1 );
		
	} else {
		for( x=0 ; x<vglob.lives ; x++ ) {
			Vortex_RenderAvailableBase( srf, 1+(x*12), 1 );
		}
	}
}


void Vortex_RenderTimedQuad( ttk_surface srf,
				char *buf0, char *buf1, char *buf2, char *buf3,
				int x, int y, int w, int h,
				ttk_color color )
{
	char * word = "";

	switch((vglob.timer>>4) & 0x03 ) {
	    case 0:	word = buf0; 	break;
	    case 1:	word = buf1;	break;
	    case 2:	word = buf2;	break;
	    case 3:	word = buf3;	break;
	    default:			break;
	}

	Vortex_OutlinedTextCenter( srf, word, x, y, w, h, 1,
					color, vglob.color.bg );
}
