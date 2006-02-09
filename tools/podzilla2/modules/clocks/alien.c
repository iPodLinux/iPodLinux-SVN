/*
 * ALIEN CLOCK
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
#include "pz.h"
#include "clocks.h"

/*  each 'digit' is a 3x3 array

*/

#define INS (4)		/* inset */

static void draw_digit( ttk_surface srf, clocks_globals *glob, int w, int v, 
			int use_ap, 
			ttk_color c_ol, ttk_color c_fill, ttk_color c_dark )
{
	/*  0  1
            2  3  */
	int array[9];		/* the bits - one for each light */
	TApItem *boxOn = NULL, *boxOnB = NULL;
	TApItem *boxOff = NULL, *boxOffB = NULL;
	int x, z;		/* misc */
	int sx=0, sy=0;		/* start x/ start y for the box */
	int w2 = glob->w>>1;	/* w/2 */
	int h2 = glob->h>>1;	/* h/2 */

	int w6 = glob->w/6;	/* w/6 */
	int h6 = glob->h/6;	/* h/6 */

	/* the positions in the 3x3 grid for each of the 9 lights */
	int xp[9] = { 0, w6, w6+w6, 0, w6, w6+w6, 0, w6, w6+w6 };
	int yp[9] = { 0, 0, 0, h6, h6, h6, h6+h6, h6+h6, h6+h6 };

	/* determine sx/sy */
	if( w & 0x01 )  sx = w2;  else  sx = 0;
	if( w & 0x02 )  sy = h2;  else  sy = 0;

	/* use the apearance colorscheme */
	if( use_ap ) {
		if( w == 0 || w == 3 ) {
			boxOff = ttk_ap_getx( "box.default.bg" );
			boxOffB = ttk_ap_getx( "box.default.border" );
			boxOn = ttk_ap_getx( "box.selected.bg" );
			boxOnB = ttk_ap_getx( "box.selected.border" );
		} else {
			boxOff = ttk_ap_getx( "box.default.bg" );
			boxOffB = ttk_ap_getx( "box.default.border" );
			boxOn = ttk_ap_getx( "box.special.bg" );
			boxOnB = ttk_ap_getx( "box.special.border" );
		}
	}

	/* generate the array */
	for( x=0 ; x<9 ; x++ ) array[x] = 0;
	for( x=0 ; x<v ; ) {
		int idx = rand()%9;
		if( !array[idx]) {
			array[idx] = 1;
			x++;
		}
	}

	/* draw the number */
	/* 0 1 2
	   3 4 5
	   6 7 8 */
	for( z=0 ; z<9 ; z++ ) {
		int x1 = sx+xp[z]+INS;
		int y1 = sy+yp[z]+INS;
		int x2 = sx+xp[z]+w6-INS;
		int y2 = sy+yp[z]+h6-INS;

		if( use_ap ) {
			ttk_ap_fillrect( srf, (array[z])? boxOn : boxOff,
				x1, y1, x2, y2 );
			ttk_ap_rect( srf, (array[z])? boxOnB : boxOffB,
				x1, y1, x2, y2 );
		} else {
			if( array[z] ) {
				ttk_fillrect( srf, x1, y1, x2, y2, c_fill );
				ttk_rect( srf, x1, y1, x2, y2, c_ol );
			} else {
				ttk_fillrect( srf, x1, y1, x2, y2, c_dark );
			}
		}
	}
}

#define ALIEN_AP	1, 0, 0, 0

void clock_draw_alien_ap( ttk_surface srf, clocks_globals *glob )
{
	int x;
	char buf[16];

	snprintf( buf, 16, "%02d%02d", glob->dispTime->tm_hour,
				       glob->dispTime->tm_min );
	for( x=0 ; x<4 ; x++ ) {
		draw_digit( srf, glob, x, buf[x]-'0', ALIEN_AP );
	}
}


static ttk_color mc_color( cr, cg, cb,  mr, mg, mb )
{       
        if( ttk_screen->bpp >= 16 ) {
                return( ttk_makecol( cr, cg, cb ));
        }
        return( ttk_makecol( mr, mg, mb ));
}       

#define ALIEN_RGB	0

void clock_draw_alien_rgb( ttk_surface srf, clocks_globals *glob )
{
	int x;
	char buf[16];
	ttk_color fills[4];
	ttk_color outlines[4];
	ttk_color darks[4];

	/* fill the color array */
	fills[0]    = mc_color( 255,   0,   0,   WHITE );	/* R */
	fills[1]    = mc_color(   0, 255,   0,   GREY );	/* G */
	fills[2]    = mc_color( 255, 255,   0,   GREY );	/* Y */
	fills[3]    = mc_color(   0,   0, 255,   WHITE );	/* B */
	outlines[0] = mc_color( 128,   0,   0,   GREY );	/* R */
	outlines[1] = mc_color(   0, 128,   0,   DKGREY );	/* G */
	outlines[2] = mc_color( 128, 128,   0,   DKGREY );	/* Y */
	outlines[3] = mc_color(   0,   0, 128,   GREY );	/* B */
	darks[0]    = mc_color(  50,   0,   0,   DKGREY );	/* R */
	darks[1]    = mc_color(   0,  50,   0,   DKGREY );	/* G */
	darks[2]    = mc_color(  50,  50,   0,   DKGREY );	/* Y */
	darks[3]    = mc_color(   0,   0,  50,   DKGREY );	/* B */

        /* clear to black */
        ttk_fillrect( srf, 0, 0, glob->w, glob->h, ttk_makecol( 0, 0, 0 ));

	snprintf( buf, 16, "%02d%02d", glob->dispTime->tm_hour,
				       glob->dispTime->tm_min );
	for( x=0 ; x<4 ; x++ ) {
		draw_digit( srf, glob, x, buf[x]-'0', 
				ALIEN_RGB, outlines[x], fills[x], darks[x]);
	}
}
