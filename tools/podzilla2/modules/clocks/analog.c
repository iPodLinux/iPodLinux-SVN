/*
 * analog clock faces
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


/* draw an angular line for the analog clock, with a ball at the end */

static void aclock_angular_line_angle(
			ttk_surface srf, ttk_color col,
			int cx, int cy,
                        double angle,
                        int radius, int circdiam, int thick  )
{
        int px, py;

        px = cx + ( radius * cos( angle ));
        py = cy + ( radius * sin( angle ));

        if( thick ) {
		ttk_aaline( srf, cx+1, cy   , px+1, py   , col );
		ttk_aaline( srf, cx-1, cy   , px-1, py   , col );
		ttk_aaline( srf, cx  , cy+1 , px  , py+1 , col );
		ttk_aaline( srf, cx  , cy-1 , px  , py-1 , col );
        }
	ttk_aaline( srf, cx, cy, px, py, col );

        if( circdiam ) {
		ttk_aaellipse( srf, px, py, circdiam, circdiam, col );
		ttk_fillellipse( srf, px, py, circdiam, circdiam, col );
        }
}


static void aclock_angular_line(
			ttk_surface srf, ttk_color col,
			int cx, int cy,
                        int val, int max,
                        int radius, int circdiam, int thick  )
{
        double angle;

        angle = (3.14159265 * ((( (val%max) * 360 ) / max) - 90)) / 180;

	aclock_angular_line_angle( srf, col, cx, cy, angle, radius,
                                circdiam, thick );
}


#define ARM_NORMAL (0)
#define ARM_THICK  (1)

void clock_draw_simple_analog( ttk_surface srf, clocks_globals *glob )
{
	int cx = glob->w>>1;	/* X center */
	int cy = glob->h>>1;	/* Y center */
	int cr = cy-5;		/* backing circle radius */
	int hhr = cy>>1;	/* hour hand radius */
	int mhr = cy-20;	/* minute hand radius */
	int shr = cy-22;	/* second hand radius */

	/* clock face .. do a thick AA, simulated... */
	ttk_aaellipse( srf, cx, cy, cr, cr, glob->fg );
	ttk_fillellipse( srf, cx, cy, cr, cr, glob->fg );
	ttk_fillellipse( srf, cx, cy, cr-5, cr-5, glob->bg );
	ttk_aaellipse( srf, cx, cy, cr-5, cr-5, glob->fg );

	/* 12:00 dot */
	ttk_fillellipse( srf, cx, cy-cr+10, 3, 3, glob->border );
	ttk_aaellipse( srf, cx, cy-cr+10, 3, 3, glob->border );
	
	/* minutes */
        aclock_angular_line( srf, glob->fg, cx, cy,
			(glob->dispTime->tm_min*60)+glob->dispTime->tm_sec,
			60*60, mhr, 4, ARM_THICK );

	/* hours */
        aclock_angular_line( srf, glob->fg, cx, cy,
				(((glob->dispTime->tm_hour > 12)?
					glob->dispTime->tm_hour-12:
					glob->dispTime->tm_hour) *60 ) + 
				    glob->dispTime->tm_min,
				12*60, hhr, 3, ARM_THICK );

	/* seconds */
        aclock_angular_line( srf, glob->fg, cx, cy,
				glob->dispTime->tm_sec, 60, shr, 
				2, ARM_NORMAL );
}


void clock_draw_nelson_analog( ttk_surface srf, clocks_globals *glob )
{
	int cx = glob->w>>1;	/* X center */
	int cy = glob->h>>1;	/* Y center */
	int lr = cy-10;		/* lollipop radius */
	int hhr = cy>>1;	/* hour hand radius */
	int mhr = cy-20;	/* minute hand radius */
	int shr = cy-22;	/* second hand radius */
	int cr = glob->h>>3;	/* center circle radius */
	int x;

	int lollipop_r = 8;	/* radius of the lollipop circles */
	int minutes_r  = 5;	/* radius of the minute hand circles */
	int hours_r    = 4;	/* radius of the hour hand circles */

	/* make it look non-stupid on small screens Mini/Nano/1g-4g */
	if( glob->w < 200 ) {
		lollipop_r = 6;
		minutes_r = 4;
		hours_r = 4;
	}
	
	/* lollipops */
	for( x=0 ; x<12 ; x++ ) {
		aclock_angular_line( srf, glob->border, cx, cy,
				x, 12, lr, lollipop_r, ARM_NORMAL );
		
	}

	/* center region */
	ttk_aaellipse( srf, cx, cy, cr, cr, glob->border );
	ttk_fillellipse( srf, cx, cy, cr, cr, glob->border );
	
	/* minutes */
        aclock_angular_line( srf, glob->fg, cx, cy,
			(glob->dispTime->tm_min*60)+glob->dispTime->tm_sec,
			60*60, mhr, minutes_r, ARM_THICK );

	/* hours */
        aclock_angular_line( srf, glob->fg, cx, cy,
				(((glob->dispTime->tm_hour > 12)?
					glob->dispTime->tm_hour-12:
					glob->dispTime->tm_hour) *60 )+ 
				    glob->dispTime->tm_min,
				12*60, hhr, hours_r, ARM_THICK );

	/* seconds */
        aclock_angular_line( srf, glob->fg, cx, cy,
			    glob->dispTime->tm_sec, 60, shr, 0, ARM_NORMAL );
}



/******************************************************************************
**  oversized faces
*/

static void clock_common_oversized( ttk_surface srf, clocks_globals *glob,
					int showdate )
{
	int cx = glob->w>>1;	/* X center */
	int cy = glob->h>>1;	/* Y center */
	int hlen = (cy*3)>>2;	/* hand length */
	char buf[16];	/* should be big enough for the format */

	/* draw the crosshairs */
	ttk_line( srf, 0, cy, glob->w, cy, glob->border );
	ttk_line( srf, cx, 0, cx, glob->h, glob->border );

	/* draw the date */
	if( showdate ) {
		int w34 = (glob->w*3)>>2;
		int wid, wid2;;

		/* date text */
		strftime( buf, 16, "%a %d", glob->dispTime );
		wid = pz_vector_width( buf, 5, 9, 1 );
		wid2 = wid>>1;

		/* backing box */
		ttk_fillrect( srf, w34-wid2-2, cy-6,
				   w34+wid2+2, cy+7, glob->bg );
		ttk_rect( srf, w34-wid2-3, cy-7,
				   w34+wid2+3, cy+8, glob->border );

		/* draw the text */
		pz_vector_string_center( srf, buf, w34, cy,
					5, 9, 1, glob->border );

	}
	
	/* draw the hands */
	/* minutes */
        aclock_angular_line( srf, glob->fg, cx, cy,
			(glob->dispTime->tm_min*60)+glob->dispTime->tm_sec,
			60*60, glob->w, 0, ARM_THICK );

	/* hours */
        aclock_angular_line( srf, glob->fg, cx, cy,
				(((glob->dispTime->tm_hour > 12)?
					glob->dispTime->tm_hour-12:
					glob->dispTime->tm_hour) *60) +
				    glob->dispTime->tm_min,
				12*60, hlen, 0, ARM_THICK );
	
	/* seconds */
        aclock_angular_line( srf, glob->fg, cx, cy,
			glob->dispTime->tm_sec, 60, glob->w, 0, ARM_NORMAL );
}

void clock_draw_oversized( ttk_surface srf, clocks_globals *glob )
{
	clock_common_oversized( srf, glob, 0 );
}

void clock_draw_oversized_watch( ttk_surface srf, clocks_globals *glob )
{
	clock_common_oversized( srf, glob, 1 );
}
