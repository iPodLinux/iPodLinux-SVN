/*
 * Copyright (C) 2004 Bernard Leach, Scott Lawrence, etc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "pz.h"
#include "browser.h"
#include "ipod.h"
#include "vectorfont.h"

/* from pz.c */
extern GR_WINDOW_ID root_wid;
extern GR_GC_ID root_gc;
extern int battery_is_charging;
extern int usb_connected;
extern int old_usb_connected;

/* globals */
int hold_is_on = 0;

#define DEC_WIDG_SZ 	(27)	/* size of the top widgets */

#define BATT_POLY_POINTS 9
#define BATTERY_LEVEL_LOW 50 
#define BATTERY_LEVEL_FULL 512

/*
  +-+
+-+ +-+
|     |
|     |
|     |
+-----+
*/

#define HOLD_X (9)	/* horizontal position of the padlock icon */
			/* (this is a #define now, so it's easy to move if
			   we put in song playing status later) */
static GR_POINT hold_outline[] = {
	{HOLD_X+0, 9},
	{HOLD_X+1, 9},
	{HOLD_X+1, 6},
	{HOLD_X+2, 5},
	{HOLD_X+5, 5},
	{HOLD_X+6, 6},
	{HOLD_X+6, 9},
	{HOLD_X+7, 9},
	{HOLD_X+7, 14},
	{HOLD_X+0, 14},
	{HOLD_X+0, 9},
};
#define HOLD_POLY_POINTS 11



#define BATT_UPDATE_FULL (-1)

/* which can be -1 to update all things, 0/1 to update just the blinky bits */
static void draw_batt_status( int which )
{
	char buf[32];
	static int battery_fill = 512;
	static int battery_fill_16 = 0;
	static int last_level = 0;
	static int old_battery_is_charging = 0;	

	GR_POINT batt_outline[] = {
		{screen_info.cols-22, 5},
		{screen_info.cols-4, 5},
		{screen_info.cols-4, 8},
		{screen_info.cols-3, 8},
		{screen_info.cols-3, 12},
		{screen_info.cols-4, 12},
		{screen_info.cols-4, 15},
		{screen_info.cols-22, 15},
		{screen_info.cols-22, 6}
	};

	GR_POINT charging_bolt_outline[] = { /* 12 */
		{screen_info.cols-8, 2},
		{screen_info.cols-15, 2},
		{screen_info.cols-17, 9},
		{screen_info.cols-12, 9},

		{screen_info.cols-13, 12},
		{screen_info.cols-14, 12},
		{screen_info.cols-17, 9},
		{screen_info.cols-18, 18},

		{screen_info.cols-15, 14},
		{screen_info.cols-13, 16},
		{screen_info.cols-7, 7},
		{screen_info.cols-11, 7},

		{screen_info.cols-8, 2},
	};

	GR_POINT charging_bolt[] = {
		{screen_info.cols-8, 3},
		{screen_info.cols-14, 3},
		{screen_info.cols-16, 8},
		{screen_info.cols-11, 8},

		{screen_info.cols-13, 13},
		{screen_info.cols-16, 10},
		{screen_info.cols-17, 18},
		{screen_info.cols-14, 13},

		{screen_info.cols-12, 16},
		{screen_info.cols-7, 8},
		{screen_info.cols-11, 8},
		{screen_info.cols-9, 3},
	};

	/* we may need to only draw the digits to the screen */
	if( ipod_get_setting( BATTERY_DIGITS )) {
		battery_fill = ipod_get_battery_level();
		battery_is_charging = ipod_is_charging();	

		snprintf( buf, 32, "%3d", battery_fill );
		if( battery_is_charging ) strcat( buf, "C" );
		
		GrSetGCForeground(root_gc, appearance_get_color(CS_TITLEBG) );
		GrFillRect(root_wid, root_gc,
					screen_info.cols-DEC_WIDG_SZ, 0,
					DEC_WIDG_SZ, HEADER_TOPLINE );

		GrSetGCForeground(root_gc, appearance_get_color(CS_TITLEFG) );

		vector_render_string_center( root_wid, root_gc, buf, 1, 1,
					screen_info.cols-(DEC_WIDG_SZ/2)-1,
					HEADER_TOPLINE/2 );
		return;
	}

	if( which == BATT_UPDATE_FULL ) {
		/* get the level from the hardware */
		battery_fill = ipod_get_battery_level();

		/* if read failed, return */
		if (battery_fill > 0x200) return;

		/* adjust battery level to be 1..15 */
		battery_fill_16 = (battery_fill>>5)-1;
		if( battery_fill_16 < 1 ) battery_fill_16=1;
		if( battery_fill_16 > 15 ) battery_fill_16=15;
		
		/* set battery charging indicator */
		battery_is_charging = ipod_is_charging();	
	}

	/* eliminiate unnecessary redraw/flicker */
	/* this could be eliminated by doublebuffering... */
	if( !battery_is_charging
	    && (which >= 0)
	    && (last_level == battery_fill_16)
	    && (battery_fill > BATTERY_LEVEL_LOW))
		return;
	last_level = battery_fill_16;

	/* draw it */

	/* fill the battery outline */
	GrSetGCForeground(root_gc, appearance_get_color(CS_BATTCTNR) );
	if( which == BATT_UPDATE_FULL ) /* try to eliminate some blink */
		GrFillPoly(root_wid, root_gc, BATT_POLY_POINTS, batt_outline);
	else
		GrFillRect(root_wid, root_gc, screen_info.cols-21, 6, 17, 9 );

	/* draw the outline */
	GrSetGCForeground(root_gc, appearance_get_color(CS_BATTBDR) );
	GrPoly(root_wid, root_gc, BATT_POLY_POINTS, batt_outline);

	if( !battery_is_charging ) {

	    /* erase the outer bits if necessary */
	    if (old_battery_is_charging!=battery_is_charging)
	    {
		    GrSetGCForeground(root_gc, appearance_get_color(CS_TITLEBG));
		    GrFillRect(root_wid, root_gc, screen_info.cols-16,2, 9,3);
		    GrFillRect(root_wid, root_gc, screen_info.cols-18,16, 6,3);
	    }
	    /* draw fullness level */
	    GrSetGCForeground(root_gc, appearance_get_color(CS_BATTFILL));

	    /* change color to low color if appropriate */
	    if( battery_fill <= BATTERY_LEVEL_LOW )
		GrSetGCForeground(root_gc, appearance_get_color(CS_BATTLOW));

	    /* draw amount of battery fullness */
	    if(    (battery_fill > BATTERY_LEVEL_LOW)
		|| (which < 1 ) ) {
		    GrFillRect(root_wid, root_gc, screen_info.cols-20, 7, 
			battery_fill_16, 7);
	    }
	} else {
	    /* draw charging bolt, if applicable */
	    if( which < 1 ) {
		    /* erase charging bolt overlap */

		    GrSetGCForeground(root_gc, appearance_get_color(CS_TITLEBG));
		    GrFillRect(root_wid, root_gc, screen_info.cols-16,2, 9,3);
		    GrFillRect(root_wid, root_gc, screen_info.cols-18,16, 6,3);
		    return;
	    }

	    /* draw charging bolt */
	    GrSetGCForeground(root_gc, appearance_get_color(CS_BATTCHRG) );
	    GrFillPoly(root_wid, root_gc, 11, charging_bolt);
	    /* and a few cleanups... */
	    GrLine(root_wid, root_gc, screen_info.cols-17, 8,
				      screen_info.cols-8, 8);
	    GrLine(root_wid, root_gc, screen_info.cols-18, 16,
				      screen_info.cols-18, 17 );

	    GrSetGCForeground(root_gc, appearance_get_color(CS_BATTCTNR) );
	    GrPoly(root_wid, root_gc, 13, charging_bolt_outline);
	}
	old_battery_is_charging = battery_is_charging;
}

static double get_load_average( void )
{
#ifdef __linux__
	FILE * file;
	float f;
#else
	double avgs[3];
#endif
	double ret = 0.00;

#ifdef __linux__
	file = fopen( "/proc/loadavg", "r" );
	if( file ) {
		fscanf( file, "%f", &f );
		ret = f;
		fclose( file );
	} else {
		ret = 0.50;
	}
#else
	if( getloadavg( avgs, 3 ) < 0 ) return( 0.50 );
	ret = avgs[0];
#endif
	return( ret );
}

static void draw_load_average( void )
{
	double avg;
	int level = 0;

	if( !ipod_get_setting( DISPLAY_LOAD )) return;
	if( hold_is_on ) return;

	/* clear the background */
	GrSetGCForeground(root_gc, appearance_get_color(CS_TITLEBG) );
	GrFillRect( root_wid, root_gc, 0, 0, DEC_WIDG_SZ, HEADER_TOPLINE );

	/* get the load average */
	avg = get_load_average();

	/* adjust the display level */
	if( avg > 1.0 ) avg = 1.0;
	level = (HEADER_TOPLINE-3) - (HEADER_TOPLINE-4)*avg;
	if( level< 0 ) level = 0;
	if( level>(HEADER_TOPLINE-2) ) level = HEADER_TOPLINE-2;

	GrSetGCForeground(root_gc, appearance_get_color(CS_LOADBG) );
	GrFillRect( root_wid, root_gc, 2, 2,
			DEC_WIDG_SZ-4, level-1 );

	GrSetGCForeground(root_gc, appearance_get_color(CS_LOADFG) );
	GrFillRect( root_wid, root_gc, 2, level+1,
			DEC_WIDG_SZ-4, HEADER_TOPLINE-4-level+1 );
}


void draw_hold_status( void )
{
	int decorations = appearance_get_decorations();

	if (hold_is_on) {
		if(    decorations == DEC_AMIGA13
		    || decorations == DEC_AMIGA11
		    || ipod_get_setting( DISPLAY_LOAD ) ){
			/* erase any decoration-specific imagery */
			GrSetGCForeground(root_gc,
					appearance_get_color(CS_TITLEBG));
			GrFillRect( root_wid, root_gc, 0, 0,
					DEC_WIDG_SZ, HEADER_TOPLINE );
		    
		}

		/* draw the body of the padlock */
		GrSetGCForeground(root_gc, appearance_get_color(CS_HOLDFILL));
		GrFillRect(root_wid, root_gc, HOLD_X+1, 9, 7, 5);

		/* draw the outline of the padlock */
		GrSetGCForeground(root_gc, appearance_get_color(CS_HOLDBDR));
		GrPoly(root_wid, root_gc, HOLD_POLY_POINTS, hold_outline);
	}
	else {
		/* erase the padlock */
		GrSetGCForeground(root_gc, appearance_get_color(CS_TITLEBG));
		GrFillPoly(root_wid, root_gc, HOLD_POLY_POINTS, hold_outline);
		GrPoly(root_wid, root_gc, HOLD_POLY_POINTS, hold_outline);

		/* erase out the body bit */
		GrFillRect(root_wid, root_gc, HOLD_X+1, 9, 7, 5);

		if( ipod_get_setting( DISPLAY_LOAD )) {
			draw_load_average();
			return;
		}

		if( decorations == DEC_AMIGA13 || decorations == DEC_AMIGA11 ){
		    /* draw the vertical bar on the left */
		    GrSetGCForeground(root_gc, 
				appearance_get_color(CS_TITLEACC) );
		    GrFillRect( root_wid, root_gc, 3, 0, 2, HEADER_TOPLINE );

		    /* draw the close box */
		    GrFillRect( root_wid, root_gc, 8+0, 2, 16, 16 );
		    GrSetGCForeground(root_gc, 
				appearance_get_color(CS_TITLEBG) );
		    GrFillRect( root_wid, root_gc, 8+2, 4, 12, 12 );
		    GrSetGCForeground(root_gc, 
				appearance_get_color(CS_TITLEFG) );
		    GrFillRect( root_wid, root_gc, 8+6, 8, 4, 4 );
		}
	}
}


void pz_draw_header(char *header)
{
	GR_SIZE width, height, base, elwidth;
	int decorations;
	int centered_text = 1;
	int len, i;
	
	//GrClearWindow(root_wid, 0);
	GrSetGCForeground(root_gc, appearance_get_color(CS_TITLEBG) );
	GrFillRect( root_wid, root_gc, 0, 0, screen_info.cols, HEADER_TOPLINE);

	GrSetGCForeground(root_gc, appearance_get_color(CS_BG));
	GrLine(root_wid, root_gc, 0, HEADER_TOPLINE + 1, screen_info.cols,
			HEADER_TOPLINE + 1);

	GrSetGCBackground(root_gc, appearance_get_color(CS_TITLEBG) );
	GrSetGCForeground(root_gc, appearance_get_color(CS_TITLEACC) );

	GrGetGCTextSize(root_gc, header, -1, GR_TFASCII, &width, &height,
			&base);

	decorations = appearance_get_decorations();
	/* draw amiga dragbars */
	if( decorations == DEC_AMIGA13 || decorations == DEC_AMIGA11 ) {
		/* drag bars */
		if( decorations == DEC_AMIGA13 ) {
			GrFillRect( root_wid, root_gc,
				DEC_WIDG_SZ+6, 4,
				screen_info.cols - (DEC_WIDG_SZ*2)-12, 4 );
			GrFillRect( root_wid, root_gc,
				DEC_WIDG_SZ+6, HEADER_TOPLINE-4-4, 
				screen_info.cols - (DEC_WIDG_SZ*2)-12, 4 );
		} else {
			for( i=2 ; i<(HEADER_TOPLINE) ; i+=2 ){
				GrLine( root_wid, root_gc, DEC_WIDG_SZ+2, i,
					screen_info.cols - DEC_WIDG_SZ-3, i );
			}
		}

		/* vertical widget separators */
		GrFillRect( root_wid, root_gc, DEC_WIDG_SZ,
						0, 2, HEADER_TOPLINE );
		GrFillRect( root_wid, root_gc, screen_info.cols-DEC_WIDG_SZ-2,
						0, 2, HEADER_TOPLINE );
		centered_text = 0;
	}
	if( decorations == DEC_MROBE ) {
		/* to make this symmetrical, we draw the left half, left to 
		    right, then we draw the right half, right to left.
		    This should make it such that it will fill properly,
		    and fill with only complete dots. no more slivers.
		- this isn't an OCD thing, it's a 'making it look good' thing.
		*/
		/* draw left side */
		for( 	i = DEC_WIDG_SZ+2 ;
			i < ((screen_info.cols - width)>>1)-5 ;
			i+=11 ) {
		    GrFillEllipse( root_wid, root_gc, i, HEADER_TOPLINE>>1,
				2, 2);
		}

		/* draw right side */
		for( 	i = screen_info.cols-DEC_WIDG_SZ-4 ;
			i > ((screen_info.cols + width)>>1)+5 ;
			i-=11 ) {
		    GrFillEllipse( root_wid, root_gc, i, HEADER_TOPLINE>>1,
				2, 2);
		}
	}

	GrSetGCForeground(root_gc, appearance_get_color(CS_TITLEFG) );
	if (width > screen_info.cols - 46) {
		GrGetGCTextSize(root_gc, "...", -1, GR_TFASCII, &elwidth,
				&height, &base);
		len = strlen(header);
		for (; elwidth + width > screen_info.cols - 46; len--) {
			GrGetGCTextSize(root_gc, header, len, GR_TFASCII,
					&width, &height, &base);
		}
		GrText(root_wid, root_gc, (screen_info.cols - (width + elwidth))
				/ 2, HEADER_BASELINE, header, len, GR_TFASCII);
		GrText(root_wid, root_gc, ((screen_info.cols -
				(width + elwidth)) / 2) + width,
				HEADER_BASELINE, "...", -1, GR_TFASCII);
	}
	else {
		if( decorations != DEC_PLAIN ) {
			GrSetGCForeground(root_gc, 
				appearance_get_color(CS_TITLEBG) );
			GrFillRect( root_wid, root_gc, 
			(centered_text) ? ((screen_info.cols - width)>>1) - 4
					: DEC_WIDG_SZ+2,
				0, width + 8, HEADER_TOPLINE );
		}

		GrSetGCForeground(root_gc, appearance_get_color(CS_TITLEFG) );
		GrText(root_wid, root_gc,
			(centered_text) ? (screen_info.cols - width) / 2
					: DEC_WIDG_SZ+6,
			HEADER_BASELINE, header, -1, GR_TFASCII);
	}

	GrSetGCForeground(root_gc, appearance_get_color(CS_TITLELINE) );
	GrLine(root_wid, root_gc, 0, HEADER_TOPLINE, screen_info.cols,
			HEADER_TOPLINE);

	draw_batt_status( BATT_UPDATE_FULL );
	draw_hold_status();
}


void header_update_hold_status( int hold_status )
{
	hold_is_on = hold_status;
	draw_hold_status();
}


void header_timer_update( void )
{
	static int count = 0;
	count++;

	if( count > 30 ) {
		count = 0;
		draw_batt_status( BATT_UPDATE_FULL );
	} else if (!(count%10)) {
		if (ipod_is_charging()!=battery_is_charging)
		{
			count = 0;
			draw_batt_status(BATT_UPDATE_FULL);
		}
	} else {
		draw_batt_status( count & 1 );
	}

	if( (count & 0xf) == 0 ) {
		draw_load_average();
	}

}
