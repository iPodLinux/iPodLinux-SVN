/*
 * binary clock faces
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
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include "pz.h"
#include "clocks.h"


static void clock_draw_light( ttk_surface srf, int x, int y, int dia,
				ttk_color light, ttk_color container )
{
	ttk_fillellipse( srf, x, y, dia, dia, light );	
	ttk_aaellipse( srf, x, y, dia, dia, container );	
	if( ttk_screen->bpp >= 16 )
		ttk_aaellipse( srf, x, y, dia+1, dia+1, container );	
}

static void clock_bcd_nibble_vert( ttk_surface srf, clocks_globals *glob,
				    int x, int val, 
				    ttk_color light, ttk_color dark, 
				    ttk_color container )
{
	int ld = glob->h/4-4;
	int xv = glob->w/6;
	int r = (xv-2)>>1;
	int vc;

	for( vc=0 ; vc<4 ; vc++ ){
		clock_draw_light( srf, x+r, r+4+((ld+3)*vc), r-2,
				(val & 0x08)?light:dark, container );
		val = val << 1;
	}
}

static void clock_draw_bcd_common( ttk_surface srf, clocks_globals *glob, 
					int isRed )
{
	ttk_color light;
	ttk_color dark;
	ttk_color container;

	char buf[16];
	int xv = glob->w/6;
	int col;

	/* make the colors */
	if( ttk_screen->bpp >= 16 ) {
		if( isRed ) {
			light = ttk_makecol( 255, 0, 0 );
			dark = ttk_makecol( 75, 0, 0 );
			container = ttk_makecol( 50, 0, 0 );
		} else {
			light = ttk_makecol( 0, 0, 255 );
			dark = ttk_makecol( 0, 0, 75 );
			container = ttk_makecol( 0, 0, 50 );
		}
	} else {
		light = ttk_makecol( WHITE );
		dark = ttk_makecol( BLACK );
		container = ttk_makecol( GREY );
	}

	/* clear to black */
	ttk_fillrect( srf, 0, 0, glob->w, glob->h, ttk_makecol( 0, 0, 0 ));

	/* generate the digit string */
	snprintf( buf, 16, "%02d%02d%02d", glob->dispTime->tm_hour,
					   glob->dispTime->tm_min,
					   glob->dispTime->tm_sec );

	/* display the lights */
	for( col=0 ; col < 6 ; col++ ){
		clock_bcd_nibble_vert( srf, glob, 2+col*xv, buf[col] - '0', 
					light, dark, container );
	}
}

void clock_draw_bcd_red( ttk_surface srf, clocks_globals *glob )
{
	clock_draw_bcd_common( srf, glob, 1 );
}

void clock_draw_bcd_blue( ttk_surface srf, clocks_globals *glob )
{
	clock_draw_bcd_common( srf, glob, 0 );
}
