/*
 * Clocks - 7 segment display
 *
 *  a 7-segment LED clock, and also an example of an external clock face
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
#include "../clocks/clocks.h" /* for the glob and register_face() */


void draw_colon( ttk_surface srf, int x1, int y1, int x2, int y2, 
		 ttk_color fill, ttk_color container )
{
	int x = (x2 + x1) >> 1;	/* x pos center */
	int d = (x2 - x1)/6;	/* diameter */
	int y = (y2 - y1) >> 2;	/* y increments */

	/* remember...  a colon is just an umlaut that fell over. */

	/* top dot */
	ttk_fillellipse( srf, x, y1+y, d, d, fill );
	ttk_aaellipse( srf, x, y1+y, d, d, container );

	/* bottom dot */
	ttk_fillellipse( srf, x, y1+(y*3), d, d, fill );
	ttk_aaellipse( srf, x, y1+(y*3), d, d, container );

	/* umlaut umlaut umlaut umlaut umlaut umlaut umlaut umlaut! */
}

/* this simulates the 74LS7447 chip, which you shove in an 8 bit number,
** and you get out the A-G bits which drive a 7 segment LED display directly.
**
**	 ***		  A
**	*   *		F   B
**	 ***		  G
**	*   *		E   C
**	 ***		  D
*/

static int ls7447[10] = {
        /* #   - a b c  d e f g */
        /* 0   0 1 1 1  1 1 1 0 */      0x7e,
        /* 1   0 0 1 1  0 0 0 0 */      0x30,
        /* 2   0 1 1 0  1 1 0 1 */      0x6d,
        /* 3   0 1 1 1  1 0 0 1 */      0x79,
        /* 4   0 0 1 1  0 0 1 1 */      0x33,
        /* 5   0 1 0 1  1 0 1 1 */      0x5b,
        /* 6   0 1 0 1  1 1 1 1 */      0x5f,
        /* 7   0 1 1 1  0 0 0 0 */      0x70,
        /* 8   0 1 1 1  1 1 1 1 */      0x7f,
        /* 9   0 1 1 1  1 0 1 1 */      0x7b
};

#define LS_A	(0x40)
#define LS_B	(0x20)
#define LS_C	(0x10)
#define LS_D	(0x08)
#define LS_E	(0x04)
#define LS_F	(0x02)
#define LS_G	(0x01)


static void draw_number( ttk_surface srf,
		int x1, int y1, int x2, int y2, /* screen coords for the num */
		int value, /* value is actually an ascii character */
		ttk_color bright, ttk_color dark, ttk_color container )
{
	int mask = 0x00;
	int yc = y1+((y2-y1)>>1);

	int inset = 1;	/* how far in the segments are shortened */
	int thick = 7;	/* how thick the segment bars are */
	int thickc = 3; /* how thick the center segment bar is */

	/* heuristics for smaller displays */
	if( ttk_screen->w < 220 ) {
		thick = 5;
		thickc = 3;
	}

	/* and for larger displays */
	if( ttk_screen->w > 220 ) {
		thick = 9;
		thickc = 5;
	}

	/* A - top */
	short polyAx[] = { x1+inset, x2-inset, x2-inset-thick, x1+inset+thick };
	short polyAy[] = { y1, y1, y1+thick, y1+thick };

	/* B - top right */
	short polyBx[] = { x2, x2, x2-thick, x2-thick };
	short polyBy[] = { y1+inset, yc-inset, yc-inset-thick, y1+inset+thick };

	/* C - bottom right */
	short polyCx[] = { x2, x2, x2-thick, x2-thick };
	short polyCy[] = { yc+inset, y2-inset, y2-inset-thick, yc+inset+thick };

	/* D - bottom */
	short polyDx[] = { x1+inset, x2-inset, x2-inset-thick, x1+inset+thick };
	short polyDy[] = { y2, y2, y2-thick, y2-thick };

	/* E - bottom left */
	short polyEx[] = { x1, x1, x1+thick, x1+thick };
	short polyEy[] = { yc+inset, y2-inset, y2-inset-thick, yc+inset+thick };

	/* F - top left */
	short polyFx[] = { x1, x1, x1+thick, x1+thick };
	short polyFy[] = { y1+inset, yc-inset, yc-inset-thick, y1+inset+thick };

	/* G - center */
	short polyGx[] = { x1+inset, x1+inset+thickc, x2-inset-thickc, x2-inset,
			   x2-inset-thickc, x1+inset+thickc };
	short polyGy[] = { yc, yc-thickc, yc-thickc, yc,
			   yc+thickc, yc+thickc };

	/* convert the value to the segment mask */
	if( value >= 0 && value <= 9 ) mask = ls7447[ value ];

	ttk_rect( srf, x2, y2, x2, y2, container );

	ttk_fillpoly( srf, 4, polyAx, polyAy, (mask & LS_A)?bright:dark );
	ttk_aapoly( srf, 4, polyAx, polyAy, container );
	ttk_fillpoly( srf, 4, polyBx, polyBy, (mask & LS_B)?bright:dark );
	ttk_aapoly( srf, 4, polyBx, polyBy, container );
	ttk_fillpoly( srf, 4, polyCx, polyCy, (mask & LS_C)?bright:dark );
	ttk_aapoly( srf, 4, polyCx, polyCy, container );
	ttk_fillpoly( srf, 4, polyDx, polyDy, (mask & LS_D)?bright:dark );
	ttk_aapoly( srf, 4, polyDx, polyDy, container );
	ttk_fillpoly( srf, 4, polyEx, polyEy, (mask & LS_E)?bright:dark );
	ttk_aapoly( srf, 4, polyEx, polyEy, container );
	ttk_fillpoly( srf, 4, polyFx, polyFy, (mask & LS_F)?bright:dark );
	ttk_aapoly( srf, 4, polyFx, polyFy, container );
	ttk_fillpoly( srf, 6, polyGx, polyGy, (mask & LS_G)?bright:dark );
	ttk_aapoly( srf, 6, polyGx, polyGy, container );
}


/* this basically just calls the current clock face routine */
void draw_7seg( ttk_surface srf, clocks_globals *glob )
{
	static int blink = 0;
	int i;
	char buf[16];
	int w5 = glob->w/5;
	int h = glob->w/3;
	int t = (glob->h>>1) - (h>>1);
	int b = (glob->h>>1) + (h>>1);
	int offs;

	ttk_color bright;
	ttk_color dark;
	ttk_color container;;

	if( ttk_screen->bpp >=16) {
		bright = ttk_makecol( 255, 0, 0 );
		dark = ttk_makecol( 50, 0, 0 );
		container = ttk_makecol( 25, 0, 0 );
	} else {
		bright = ttk_makecol( WHITE );
		/* these don't look good as anything other than black. */
		dark = ttk_makecol( BLACK );
		container = ttk_makecol( BLACK );
	}

	/* clear to black */
        ttk_fillrect( srf, 0, 0, glob->w, glob->h, ttk_makecol( BLACK ));

	snprintf( buf, 16, "%2d %02d",
			clock_convert_1224( glob->dispTime->tm_hour),
			glob->dispTime->tm_min );

	for( i=0 ; i<5 ; i++ )
	{
		if( i == 2 ) {
			draw_colon( srf, (i*w5)+2, t,
				((i+1)*w5)-2, b, 
				(blink)?bright:dark, container );
			blink ^= 1;
		} else {
			/* offset to scooch the numbers in a little */
			offs = w5>>2;
			if( i>2 ) offs *= -1;
			
			draw_number( srf,
				(i*w5)+2+offs, t, ((i+1)*w5)-2+offs, b,
				buf[i] - '0', bright, dark, container );
		}
	}
}


void init_7seg() 
{
	clocks_register_face( draw_7seg, "7 Segment Display" );
}

PZ_MOD_INIT (init_7seg)
