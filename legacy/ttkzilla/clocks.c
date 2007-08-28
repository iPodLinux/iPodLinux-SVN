/*
 * Various clocks 
 * Copyright (C) 2005 Scott Lawrence
 *
 *   Time setting routines
 *   Various clock display faces:
 *	Traditional Analog clock
 *	Nelson Ball-like Art Deco Analog clock (google for it)
 *	Oversized analog clock (looks nice fullscreen)
 *	Oversized analog watch (same, but with DOW, DOM )
 *	Vectorfont Clock (digital)
 *	Binary - Desktop BCD clock
 *	Binary - Binary Watch clock
 *	Digital Bedside clock
 *
 *   $Id: clocks.c,v 1.12 2005/08/02 21:14:22 courtc Exp $
 *
 */

/*
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
 * $Id: clocks.c,v 1.12 2005/08/02 21:14:22 courtc Exp $
 *
 * $Log: clocks.c,v $
 * Revision 1.12  2005/08/02 21:14:22  courtc
 * added more i18n
 *
 * Revision 1.11  2005/06/20 13:48:01  yorgle
 * Cleaned up event stuff to use the new #defines
 *
 * Revision 1.10  2005/05/31 16:02:22  yorgle
 * Cleaned up some colors.  Why didn't anyone tell me this looked like crap? ;)
 *
 * Revision 1.9  2005/05/30 02:00:33  yorgle
 * Clockzilla strikes again!
 * Added another face (a "watch" face, based on the "OVERSIZED" face), which displays the day of week and day of month, much like many watches do.
 * Converted over the vector text display to use strftime() instead of a char[][].
 * removed the months[] array.
 * This should work better for non-english based systems, I hope.
 *
 * Revision 1.8  2005/05/26 05:30:30  yorgle
 * Sproing tweaks.  Hopefully, sproingy hands should look a little better on 'photo
 * The table may need to be tweaked.
 *
 * Revision 1.7  2005/05/26 04:27:00  yorgle
 * Timezone arrays moved into clocks.c from menu.c
 * DST arrays added
 * World clock complete and hooked in.  (ipodlinux.org/WorldClock for info)
 * Added WORLD DST into the settings array
 * Re-ordered the time-based settings items
 *
 *
 * 2005-05-25 - switched to cvs log above...
 *
 * 2005-05-24 - Sproingy seconds hand for analog clocks
 *
 * 2005-05-08 - Added TIME_TICKER, TIME_1224 settings
 *
 * 2005-05-01 - Cleaned up Digital clock look (for monochrome ipods)
 *		Oversized clock added
 *
 * 2005-04-27 - Analog clock visual enhancements
 *		Binary clock bugfix (0..23 doesn't fit in 4 bits)
 *
 * 2005-04-26 - Time and Date setting added (Scott Lawrence)
 *		it's not perfect, but it's good enough for now.
 *
 * 2005-04-17 - Binary clock face added  (Scott Lawrence)
 *
 * 2005-04-16 - BCD clock face added  (Scott Lawrence)
 *
 * 2005-04-15 - integration (Scott Lawrence)
 *		- added hold switch sensing for full-screen view
 *		- added in my old vectorfont clock (2005-02)
 *		- action button toggles face style
 *
 * 2005-04-13 - initial Analog version  (Scott Lawrence)
 *		- basic version done.
 *		- deco, normal look
 *		- ready for time adjustment code (get/set time)
 *		- (update changed from 16ms to 1000ms)
 *		- (courtc's titlebar update fix)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>

#include "pz.h"
#include "ipod.h"
#include "vectorfont.h"

static GR_TIMER_ID 	Clocks_timer = 0;	/* our timer. 1000ms */
static GR_GC_ID		Clocks_gc = 0;		/* graphics context */
static GR_WINDOW_ID	Clocks_bufwid = 0;	/* second buffer */
static GR_WINDOW_ID	Clocks_bakwid = 0;	/* third backup buffer */
static GR_WINDOW_ID	Clocks_wid = 0;		/* screen window */
static GR_SCREEN_INFO	Clocks_screen_info;	/* screen info */
static int 		Clocks_height = 0;	/* visible screen height */

/*
 *  to add another clock face...
 *  1. add another CLOCKS_STYLE_(face name) entry in this list
 *  2. set CLOCKS_STYLE_MAX to be the same value.
 *  -- be sure to understand how this works before you hack it.
 *  3. add in a hook in the valve (search on __3__ below)
 *  4. add in the face render routine
 *  -- many examples below, look to see how the other valved routines do it.
 */

#define CLOCKS_STYLE_ANALOG	(0)	/* display a traditional analog */
#define CLOCKS_STYLE_DECO	(1)	/* display a nelson ball clock */
#define CLOCKS_STYLE_OVERSIZED	(2)	/* oversized analog clock */
#define CLOCKS_STYLE_WATCH	(3)	/* oversized analog watch */
#define CLOCKS_STYLE_VECTOR	(4)	/* display the vectorfont clock */
#define CLOCKS_STYLE_BCD	(5)	/* BCD digital clock */
#define CLOCKS_STYLE_BINARY	(6)	/* Binary digital clock */
#define CLOCKS_STYLE_DIGITAL	(7)	/* stnadard 7-segment digital clock */
#define CLOCKS_STYLE_MAX	(7)

static int Clocks_style;	/* what kind of clock ? */
static int Clocks_bak = 0; 			/* backup */


#define CLOCKS_SEL_DISPLAY 	(0)	/* not editing */
#define CLOCKS_SEL_HOURS	(1)	/* editing hours */
#define CLOCKS_SEL_MINUTES	(2)	/* editing minutes */
#define CLOCKS_SEL_SECONDS	(3)	/* editing seconds */
#define CLOCKS_SEL_YEARS	(4)	/* editing year */
#define CLOCKS_SEL_MONTHS	(5)	/* editing month */
#define CLOCKS_SEL_DAYS		(6)	/* editing day */
#define CLOCKS_SEL_MAX		(6)

static int Clocks_sel = CLOCKS_SEL_DISPLAY;	/* how are we editing? */
static int Clocks_set = 0;

static int Clocks_offset = 0;		/* timezone offset for display */

/* timezone information gleaned from:
        http://en.wikipedia.org/wiki/Timezone
*/
char * clocks_timezones[] = {
        "United Kingdom 0:00",
        "France +1:00",
        "Greece +2:00",
        "Kenya +3:00",
        "Iran +3:30",

        "UAE +4:00",
        "Afghanistan +4:30",
        "Uzbekistan +5:00",
        "IST India +5:30",
        "Nepal +5:45",

        "Sri Lanka +6:00",
        "Myanmar +6:30",
        "Thailand +7:00",
        "AWST W. Australia +8:00",
        "W. Australia +8:45",

        "JST/KST Japan +9:00",
        "ACST C. Australia +9:30",
        "AEST E. Australia +10:00",
        "New South Wales +10:30",
        "Micronesia +11:00",

        "Norfolk +11:30",
        "Fiji +12:00",
        "Chatham Islands +12:45",
        "Tonga +13:00",
        "Kiribati +14:00",

        "UTC -12:00",
        "Midway Atoll -11:00",
        "HST Hawaii -10:00",
        "Polynesia -9:30",
        "AKST Alaska -9:00",

        "PST US Pacific -8:00",
        "MST US Moutain -7:00",
        "CST US Central -6:00",
        "EST US Eastern -5:00",
        "AST Atlantic -4:00",

        "NST Newfoundland -3:30",
        "Brazil -3:00",
        "Mid-Atlantic -2:00",
        "Portugal -1:00",

	0
};

static int clocks_tz_offsets[] = { /* minutes associated with the above */
           0,
          60,  120,  180,  210,  240,
         270,  300,  330,  345,  360,
         390,  420,  480,  525,  540,
         570,  600,  630,  660,  690,
         720,  765,  780,  840,
        -720, -660, -600, -570, -540,
        -480, -420, -360, -300, -240,
        -210, -180, -120, -60
};

char * clocks_dsts[] = {
	"0:00",
	"0:30",
	"1:00",
	0
};

static int clocks_dst_offsets[] = {
	0, 30, 60
};

static int monthlens[] = {
	31, 28, 31, 30, 31, 30,
	31, 31, 30, 31, 30, 31 
};

/* copy the buffer into place */
static void Clocks_blit() 
{
        GrCopyArea( Clocks_wid, Clocks_gc, 0, 0,
                    Clocks_screen_info.cols, 
                    Clocks_height,
                    Clocks_bufwid, 0, 0, MWROP_SRCCOPY);
}

static void Clocks_buf2bak()
{
        GrCopyArea( Clocks_bakwid, Clocks_gc, 0, 0,
                    Clocks_screen_info.cols,
                    Clocks_height,
                    Clocks_bufwid, 0, 0, MWROP_SRCCOPY);
}

static void Clocks_bak2buf()
{
        GrCopyArea( Clocks_bufwid, Clocks_gc, 0, 0,
                    Clocks_screen_info.cols,
                    Clocks_height,
                    Clocks_bakwid, 0, 0, MWROP_SRCCOPY);
}



/* draw an angular line for the analog clock, with a ball at the end */

static void Analog_angular_line_angle( int cx, int cy, 
			double angle,
			int length, int circdiam,
			int selected, int thick  )
{
	int px, py;

	px = cx + ( length * cos( angle ));
	py = cy + ( length * sin( angle ));
	if( thick ) {
		GrLine( Clocks_bufwid, Clocks_gc, cx+1, cy  , px+1, py   );
		GrLine( Clocks_bufwid, Clocks_gc, cx-1, cy  , px-1, py   );
		GrLine( Clocks_bufwid, Clocks_gc, cx  , cy+1, px  , py+1 );
		GrLine( Clocks_bufwid, Clocks_gc, cx  , cy-1, px  , py-1 );
	}
	GrLine( Clocks_bufwid, Clocks_gc, cx, cy, px, py );

	if( circdiam ) {
		GrFillEllipse( Clocks_bufwid, Clocks_gc, px, py, 
				circdiam, circdiam); 
	}
	if( selected ) {
	    GrEllipse( Clocks_bufwid, Clocks_gc, px, py, 
				circdiam+2, circdiam+2); 
	}
}

static void Analog_angular_line( int cx, int cy,
			int val, int max, 
			int length, int circdiam,
			int selected, int thick  )
{
	double angle;

	angle = (3.14159265 * ((( (val%max) * 360 ) / max) - 90)) / 180;

	Analog_angular_line_angle( cx, cy,
				angle, length, 
				circdiam, selected, thick );
}

static void Analog_angular_line_offset( int cx, int cy, 
			int val, int max, double da,
			int length, int circdiam,
			int selected, int thick  )
{
	double angle;

	angle = (3.14159265 * ((( (val%max) * 360.0 ) / max) - 90.0 + da))
					/ 180.0;

	Analog_angular_line_angle( cx, cy,
				angle, length, 
				circdiam, selected, thick );
}


/* some helpers for drawing the second hand.  these aid in sproingy display */
static void Clocks_secondhand( int cx, int cy, int pos, int max,
                                double da, int sz )
{
        if( Clocks_screen_info.bpp == 16 )
            GrSetGCForeground( Clocks_gc, GR_RGB( 96, 96, 0 ) );
        else
            GrSetGCForeground( Clocks_gc, GRAY );
        Analog_angular_line_offset( cx, cy, pos, max, da,
				sz, 2,
                                (Clocks_sel == CLOCKS_SEL_SECONDS)?1:0, 0 );
}

static void Clocks_center( int cx, int cy, int cd )
{
        /* pretty-up the center */
        if ( Clocks_style == CLOCKS_STYLE_OVERSIZED
	  || Clocks_style == CLOCKS_STYLE_WATCH ) {
            cd = 3;
        }
        GrSetGCForeground( Clocks_gc, GRAY );
        GrFillEllipse( Clocks_bufwid, Clocks_gc, cx, cy, cd, cd );
        GrSetGCForeground( Clocks_gc, BLACK );
        GrFillEllipse( Clocks_bufwid, Clocks_gc, cx, cy, cd-1, cd-1 );
}


/* draw the analog or deco clock */
static void Clocks_draw_analog_clocks( struct tm *dispTime )
{
	char buf[16];
	int cx, cy;		/* center of clock */
	int ls, lm, lh;
	int x = 0;
	int cd = 5;		/* diameter of center circle */

        /* for the seconds hand sproingyness */
        /* each timer tick, it draws the second hand at the timer position
            plus each of these offsets (angles) in turn.  It simulates the
            second hand mechanically settling into the correct location */
        double sproingypos[2][3][5] = {
	{
		/* for mini and mono iPod */
		{ -1.2, 1.2, -0.6, 0.6, 0 },	/* analog clock 0 sproing */
		{ -1.2, 1.2, -0.6, 0.6, 0 },	/* analog clock 1 sproing */
		{ -0.6, 0.6, -0.3, 0.3, 0 }	/* analog clock 2 sproing */
	}, {
		/* for iPod photo */    /* XXXXXXX Untested XXXXXXX */
		{ -1, 1, -0.5, 0.5, 0 },	/* analog clock 0 sproing */
		{ -1, 1, -0.5, 0.5, 0 },	/* analog clock 1 sproing */
		{ -0.4, 0.4, -0.2, 0.2, 0 }	/* analog clock 2 sproing */
	}
	};
        int sproingy;
	int sp = 0;

	cx = Clocks_screen_info.cols>>1;
	cy = Clocks_height>>1;

	ls = cy-12; 		/* second hand length */
	lm = cy-12;		/* minute hand length */
	lh = cy>>1;		/* hour hand length */

	/* draw the face */
	if( Clocks_style == CLOCKS_STYLE_DECO ) {
// 210  95 100   red
//  80 100  70   green
//  50 100 170   blue
		for( x=0 ; x<12 ; x++ )
		{
		    if( Clocks_screen_info.bpp == 16 )
		    {
			if( x%3 ) 
			    GrSetGCForeground( Clocks_gc, GR_RGB( 50, 100, 170) );
			else /* 12, 3, 6, 9 */
			    GrSetGCForeground( Clocks_gc, GR_RGB( 80, 100, 70) );
		    } else {
			GrSetGCForeground( Clocks_gc, LTGRAY );
		    }
		    Analog_angular_line( cx, cy, x, 12, 
				(x==0)?cy-6:	/* 12:00 */
				(x==6)?cy-6:	/*  6:00 */
				cy-5,		/* otherwise */
				4, (x%3)?0:1, 0 );
		}
	} else if( Clocks_style == CLOCKS_STYLE_ANALOG ) {
		/* give the illusion of anti-alisedness for the outer ring */
		GrSetGCForeground( Clocks_gc, LTGRAY );
		GrFillEllipse( Clocks_bufwid, Clocks_gc, cx, cy, cy-1, cy-1 );
		GrSetGCForeground( Clocks_gc, GRAY );
		GrFillEllipse( Clocks_bufwid, Clocks_gc, cx, cy, cy-2, cy-2 );

		GrSetGCForeground( Clocks_gc, BLACK );
		GrFillEllipse( Clocks_bufwid, Clocks_gc, cx, cy, cy-3, cy-3 );

		GrSetGCForeground( Clocks_gc, GRAY );
		GrFillEllipse( Clocks_bufwid, Clocks_gc, cx, cy, cy-4, cy-4 );
		GrSetGCForeground( Clocks_gc, LTGRAY );
		GrFillEllipse( Clocks_bufwid, Clocks_gc, cx, cy, cy-5, cy-5 );
		GrSetGCForeground( Clocks_gc, WHITE );
		GrFillEllipse( Clocks_bufwid, Clocks_gc, cx, cy, cy-6, cy-6 );
	}

	/* draw some tickmark thingies */
	if( Clocks_screen_info.bpp == 16 )
	    GrSetGCForeground( Clocks_gc, GR_RGB( 0, 128, 0 ));
	else 
	    GrSetGCForeground( Clocks_gc, LTGRAY );
	if( Clocks_style == CLOCKS_STYLE_ANALOG ) {
		/* horizontal */
		GrLine( Clocks_bufwid, Clocks_gc, 
					(cx-cy)+8, cy, (cx-cy)+16, cy );
		GrLine( Clocks_bufwid, Clocks_gc, 
					(cx+cy)-16, cy, (cx+cy)-8, cy );

		/* vertical */
		GrLine( Clocks_bufwid, Clocks_gc, 
					cx, (cy-cy)+8, cx, (cy-cy)+16 );
		GrLine( Clocks_bufwid, Clocks_gc, 
					cx, (cy+cy)-16, cx, (cy+cy)-8 );
	} else if ( Clocks_style == CLOCKS_STYLE_OVERSIZED 
		 || Clocks_style == CLOCKS_STYLE_WATCH ) {
		GrLine( Clocks_bufwid, Clocks_gc, 0, cy, Clocks_screen_info.cols, cy);
		GrLine( Clocks_bufwid, Clocks_gc, cx, 0, cx, Clocks_height);
	}

	/* draw the DOW, DOM on the face, if we're watchface */
	if( Clocks_style == CLOCKS_STYLE_WATCH ) {
	    int wq = Clocks_screen_info.cols>>2;
	    int wq3 = wq+wq+wq;
	    int tw, tw2;

	    strftime( buf, 16, "%a %d", dispTime );

	    tw = vector_string_pixel_width( buf, 1, 1 );
	    tw2 = tw>>1;

	    /* draw the background-matching backing box */
	    GrSetGCForeground( Clocks_gc, WHITE );
	    GrFillRect( Clocks_bufwid, Clocks_gc, wq3-tw2-3, cy-7, tw+6, 15);

	    /* draw a box the same color as the lines, like above */
	    if( Clocks_screen_info.bpp == 16 )
		GrSetGCForeground( Clocks_gc, GR_RGB( 0, 128, 0 ));
	    else 
		GrSetGCForeground( Clocks_gc, LTGRAY );
	    GrRect( Clocks_bufwid, Clocks_gc, wq3-tw2-3, cy-7, tw+6, 15);

	    /* and finally render the text */
	    GrSetGCForeground( Clocks_gc, BLACK );
	    vector_render_string_center( Clocks_bufwid, Clocks_gc, 
			buf, 1, 1, wq3, cy );
	}


	/* draw the hands */
	if ( Clocks_style == CLOCKS_STYLE_OVERSIZED
	  || Clocks_style == CLOCKS_STYLE_WATCH ) {
	    lh = cy*3/4;			/* hours */
	    lm = Clocks_screen_info.cols<<1;	/* minutes */
	    ls = Clocks_screen_info.cols<<1;	/* seconds */
	}

	/* -- minutes -- */
	if( Clocks_screen_info.bpp == 16 )
	    GrSetGCForeground( Clocks_gc, GR_RGB( 0, 45, 0 ) );
	else
	    GrSetGCForeground( Clocks_gc, GRAY );
	Analog_angular_line( cx, cy,
				(dispTime->tm_min*60)+dispTime->tm_sec, 60*60,
				lm, 
				(Clocks_style == CLOCKS_STYLE_DECO)?3:2 ,
				(Clocks_sel == CLOCKS_SEL_MINUTES)?1:0, 1 );

	if( Clocks_screen_info.bpp == 16 )
	    GrSetGCForeground( Clocks_gc, GR_RGB( 0, 96, 0 ) );
	else
	    GrSetGCForeground( Clocks_gc, BLACK );
	Analog_angular_line( cx, cy,
				(dispTime->tm_min*60)+dispTime->tm_sec, 60*60,
				lm, 
				(Clocks_style == CLOCKS_STYLE_DECO)?3:2 ,
				(Clocks_sel == CLOCKS_SEL_MINUTES)?1:0, 0 );

	/* -- hours -- */
	if( Clocks_screen_info.bpp == 16 )
	    GrSetGCForeground( Clocks_gc, GR_RGB( 0, 45, 0 ) );
	else
	    GrSetGCForeground( Clocks_gc, GRAY );
	Analog_angular_line( cx, cy, 
				(dispTime->tm_hour*60)+dispTime->tm_min, 12*60,
				lh, 
				(Clocks_style == CLOCKS_STYLE_DECO)?4:
				  (Clocks_style == CLOCKS_STYLE_ANALOG)?2:0,
				(Clocks_sel == CLOCKS_SEL_HOURS)?1:0, 1 );

	if( Clocks_screen_info.bpp == 16 )
	    GrSetGCForeground( Clocks_gc, GR_RGB( 0, 96, 0 ) );
	else
	    GrSetGCForeground( Clocks_gc, BLACK );
	Analog_angular_line( cx, cy, 
				(dispTime->tm_hour*60)+dispTime->tm_min, 12*60,
				lh, 
				(Clocks_style == CLOCKS_STYLE_DECO)?4:
				  (Clocks_style == CLOCKS_STYLE_ANALOG)?2:0,
				(Clocks_sel == CLOCKS_SEL_HOURS)?1:0, 0 );
	/* note in the above, the *60+mins is so that the hour hand slowly
	    progresses from hour to hour appropriately, as time passes through
	    the hour.  Otherwise, at 3:59, it will have the hours hand on 3,
	    instead of almost on 4, where it should be. */

	/* -- seconds -- */
	/* do a little sproingyness */
        Clocks_buf2bak(); /* store aside the backing screen */
	for( sproingy = (Clocks_set)?4:0 ;
	      sproingy<5 ;
	      sproingy++ )
        {
            Clocks_bak2buf(); /* restore the backing screen */
            Clocks_secondhand( cx, cy, dispTime->tm_sec, 60,
                                sproingypos[sp][Clocks_style][sproingy],
				ls );
            Clocks_center( cx, cy, cd ); /* draw */
            Clocks_blit(); /* blit */
        }

        // instead of the above seconds code, this will just draw it solid
        // Clocks_secondhand( cx, cy, dispTime->tm_sec, 60, 0, ls );
        // Clocks_center( cx, cy, cd );
}


/* convert 24 hour time to 12 hour time */
static int Clocks_c12( int hours )
{
	if( hours == 0 )
		hours = 12;
	else if( hours > 12 )
		hours -= 12;
	return( hours );
}

/* adjust for 12/24 hour time */
static int Clocks_convert_to_12( int hours )
{
	if( ipod_get_setting( TIME_1224 ))
		return( hours );
	return( Clocks_c12( hours ));
}


/* draw the clock using my vector font... */
static void Clocks_draw_vector_clock( struct tm *dispTime )
{
        char buf[80];
        
        int scale_time = 3; 
        int scale_date = 2;
        
        int w = Clocks_screen_info.cols;
        int h = Clocks_height;

        /* figure out the correct ipod screen size/model */
        if( w < 160 ) { /* MINI */
            scale_time = 2;
            scale_date = 1;
        } else if ( w > 160 ) { /* PHOTO */
            scale_time = 4; /* untested. */
            scale_date = 3; /* untested. */
        }

        /* clear the screen */
        GrSetGCForeground( Clocks_gc, WHITE);
        GrFillRect( Clocks_bufwid, Clocks_gc, 0, 0, w, h);

        /* draw some text */
        GrSetGCForeground( Clocks_gc, BLACK);

        sprintf( buf, "%02d:%02d:%02d",
			Clocks_convert_to_12( dispTime->tm_hour ),
			dispTime->tm_min,
                        dispTime->tm_sec );
        vector_render_string_center( Clocks_bufwid, Clocks_gc,
                    buf, 1, scale_time, w/2, h/3 );

	if( Clocks_sel != CLOCKS_SEL_DISPLAY ) {
		char * fmtt = "";
		char * fmtd = "";
		GrSetGCForeground( Clocks_gc, GRAY );

		if( Clocks_sel == CLOCKS_SEL_HOURS ) {
			fmtt = "%c%c      ";
		} else if( Clocks_sel == CLOCKS_SEL_MINUTES ) {
			fmtt = "   %c%c   ";
		} else if( Clocks_sel == CLOCKS_SEL_SECONDS ) {
			fmtt = "      %c%c";
		} else if( Clocks_sel == CLOCKS_SEL_YEARS ) {
			fmtd = "%c%c%c%c       ";
		} else if( Clocks_sel == CLOCKS_SEL_MONTHS ) {
			fmtd = "     %c%c%c   ";
		} else if( Clocks_sel == CLOCKS_SEL_DAYS ) {
			fmtd = "         %c%c";
		}

		sprintf( buf, fmtt,
			VECTORFONT_SPECIAL_UP, VECTORFONT_SPECIAL_UP  );
		vector_render_string_center( Clocks_bufwid, Clocks_gc,
			buf, 1, scale_time, w/2, (h/3) - (scale_time*8) - 2 );
		sprintf( buf, fmtt,
			VECTORFONT_SPECIAL_DOWN, VECTORFONT_SPECIAL_DOWN  );
		vector_render_string_center( Clocks_bufwid, Clocks_gc,
			buf, 1, scale_time, w/2, (h/3) + (scale_time*8) + 2 );

		sprintf( buf, fmtd,
			VECTORFONT_SPECIAL_UP, VECTORFONT_SPECIAL_UP,
			VECTORFONT_SPECIAL_UP, VECTORFONT_SPECIAL_UP);
		vector_render_string_center( Clocks_bufwid, Clocks_gc,
			buf, 1, scale_date, w/2, (h*2/3) - (scale_date*8) - 2 );
		sprintf( buf, fmtd,
			VECTORFONT_SPECIAL_DOWN, VECTORFONT_SPECIAL_DOWN,
			VECTORFONT_SPECIAL_DOWN, VECTORFONT_SPECIAL_DOWN );
		vector_render_string_center( Clocks_bufwid, Clocks_gc,
			buf, 1, scale_date, w/2, (h*2/3) + (scale_date*8) + 2 );

	}

	GrSetGCForeground( Clocks_gc, BLACK );
	strftime( buf, 80, "%Y %b %d", dispTime );

        vector_render_string_center( Clocks_bufwid, Clocks_gc,
                    buf, 1, scale_date, w/2, (h*2)/3 );
}

/* draws a single light */
static void Clocks_draw_light( int x, int y, int dia,
				GR_COLOR mono_foreground,
				GR_COLOR color_foreground,
				GR_COLOR mono_container,
				GR_COLOR color_container )
{
	/* draw fill */
	GrSetGCForeground( Clocks_gc, 
		(Clocks_screen_info.bpp == 16)?
			  color_foreground
			: mono_foreground );
	GrFillEllipse( Clocks_bufwid, Clocks_gc, x, y, dia, dia );

	/* draw container */
	GrSetGCForeground( Clocks_gc, 
		(Clocks_screen_info.bpp == 16)?
			  color_container
			: mono_container );
	GrEllipse( Clocks_bufwid, Clocks_gc, x, y, dia, dia );
}

/* draws a nibble vertically */
static void Clocks_draw_nibble_vert( int x, int val )
{
	int ld = Clocks_height/4-4;
	int xv = Clocks_screen_info.cols/6;
	int r = (xv-2)>>1;
	int vc;

	for( vc=0 ; vc<4 ; vc++ )
	{
		Clocks_draw_light( x+r, r+4+((ld+3)*vc), r-2,
			(val & 0x08)? WHITE : BLACK,
			(val & 0x08)? GR_RGB( 255, 0, 0 ) : BLACK,
			GRAY, GR_RGB( 100, 0, 0 ) );
		val = val << 1;
	}
}

/* draw the BCD clock */
static void Clocks_draw_bcd_clock( struct tm *dispTime )
{
	char buf[8];
	int xv = Clocks_screen_info.cols/6;

	/* render the lights */
	snprintf( buf, 8, "%02d", dispTime->tm_hour );
	Clocks_draw_nibble_vert( 2+0*xv, buf[0] - '0' );
	Clocks_draw_nibble_vert( 2+1*xv, buf[1] - '0' );
	snprintf( buf, 8, "%02d", dispTime->tm_min );
	Clocks_draw_nibble_vert( 2+2*xv, buf[0] - '0' );
	Clocks_draw_nibble_vert( 2+3*xv, buf[1] - '0' );
	snprintf( buf, 8, "%02d", dispTime->tm_sec );
	Clocks_draw_nibble_vert( 2+4*xv, buf[0] - '0' );
	Clocks_draw_nibble_vert( 2+5*xv, buf[1] - '0' );
}


/* draws a 6-bit pseudobyte horizontally */
static void Clocks_draw_6bit_horiz( int y, int val )
{
	int xv = Clocks_screen_info.cols/6;
	int r = (xv-2)>>1;
	int vc;

	for( vc=0 ; vc<6 ; vc++ )
	{
		Clocks_draw_light( 2+r+vc*xv, y, r-2,
			(val & 0x20)? WHITE : BLACK,
			(val & 0x20)? GR_RGB( 255, 0, 0 ) : BLACK,
			GRAY, GR_RGB( 100, 0, 0 ) );
		val = val << 1;
	}
}

/* draws a 4-bit nibble horizontally */
static void Clocks_draw_4bit_horiz( int y, int val )
{
	int xv = Clocks_screen_info.cols/6;
	int r = (xv-2)>>1;
	int xv4 = (Clocks_screen_info.cols-(r*2))/3;
	int vc;
	int xp;

	/* note... there's a whole lot of magic going on in here to
	   get the 4 and 6 light rows to look correct. */
	for( vc=0 ; vc<4 ; vc++ )
	{
		switch( vc ) {
		    case( 0 ):
			xp = 2+r+0*xv;
			break;
		    case( 1 ):
		    case( 2 ):
			xp = r+ (vc*xv4);
			break;
		    case( 3 ):
			xp = 2+r+5*xv;
			break;
		}

		Clocks_draw_light( xp, y, r-2,
			(val & 0x08)? WHITE : BLACK,
			(val & 0x08)? GR_RGB( 255, 0, 0 ) : BLACK,
			GRAY, GR_RGB( 100, 0, 0 ) );
		val = val << 1;
	}
}


/* draw the BINARY clock (like the watch my wonderful wife bought for me) */
static void Clocks_draw_binary_clock( struct tm * dispTime )
{
	int h3 = Clocks_height/3;
	int h32 = h3>>2;
	int xv = Clocks_screen_info.cols/6;
	int r = (xv-2)>>1;
	int h = Clocks_c12( dispTime->tm_hour );

	Clocks_draw_4bit_horiz( h32+r+h3*0, h );
	Clocks_draw_6bit_horiz( h32+r+h3*1, dispTime->tm_min );

	/* real watch doesn't have seconds, but we will.. */
	Clocks_draw_6bit_horiz( h32+r+h3*2, dispTime->tm_sec );
}


/* 7-segment display 
 *      a
 *    f   b
 *      g
 *    e   c
 *      d
 *
 */

static int ls7447[10] = {
	/* #   - a b c  d e f g */
	/* 0   0 1 1 1  1 1 1 0 */	0x7e,
	/* 1   0 0 1 1  0 0 0 0 */	0x30,
 	/* 2   0 1 1 0  1 1 0 1 */	0x6d,
 	/* 3   0 1 1 1  1 0 0 1 */	0x79,
	/* 4   0 0 1 1  0 0 1 1 */	0x33,
 	/* 5   0 1 0 1  1 0 1 1 */	0x5b,
 	/* 6   0 1 0 1  1 1 1 1 */	0x5f,
 	/* 7   0 1 1 1  0 0 0 0 */	0x70,
 	/* 8   0 1 1 1  1 1 1 1 */	0x7f,
 	/* 9   0 1 1 1  1 0 1 1 */	0x7b
};

/* draw a 7-segment display digit */
static void Clocks_draw_7segment( int x, int y, int w, int h, char value,
				GR_COLOR dark, GR_COLOR bright )
{
	int mask = 0x00;
	int h2 = h>>1;

	if( value >= '0' && value <= '9' )  mask = ls7447[ value-'0' ];

        GrSetGCForeground( Clocks_gc, (mask & 0x40)?bright:dark ); /* A */
	GrLine( Clocks_bufwid, Clocks_gc, x+3, y  , x+w-3, y);
	GrLine( Clocks_bufwid, Clocks_gc, x+4, y+1, x+w-4, y+1);
	GrLine( Clocks_bufwid, Clocks_gc, x+5, y+2, x+w-5, y+2);

        GrSetGCForeground( Clocks_gc, (mask & 0x20)?bright:dark ); /* B */
	GrLine( Clocks_bufwid, Clocks_gc, x+w-1, y+2, x+w-1, y+h2-2);
	GrLine( Clocks_bufwid, Clocks_gc, x+w-2, y+3, x+w-2, y+h2-3);
	GrLine( Clocks_bufwid, Clocks_gc, x+w-3, y+4, x+w-3, y+h2-4);

        GrSetGCForeground( Clocks_gc, (mask & 0x10)?bright:dark ); /* C */
	GrLine( Clocks_bufwid, Clocks_gc, x+w-1, y+h2+2, x+w-1, y+h2+h2-2);
	GrLine( Clocks_bufwid, Clocks_gc, x+w-2, y+h2+3, x+w-2, y+h2+h2-3);
	GrLine( Clocks_bufwid, Clocks_gc, x+w-3, y+h2+4, x+w-3, y+h2+h2-4);

        GrSetGCForeground( Clocks_gc, (mask & 0x08)?bright:dark ); /* D */
	GrLine( Clocks_bufwid, Clocks_gc, x+3, y+h,   x+w-3, y+h);
	GrLine( Clocks_bufwid, Clocks_gc, x+4, y+h-1, x+w-4, y+h-1);
	GrLine( Clocks_bufwid, Clocks_gc, x+5, y+h-2, x+w-5, y+h-2);

        GrSetGCForeground( Clocks_gc, (mask & 0x04)?bright:dark ); /* E */
	GrLine( Clocks_bufwid, Clocks_gc, x,   y+h2+2, x,   y+h2+h2-2);
	GrLine( Clocks_bufwid, Clocks_gc, x+1, y+h2+3, x+1, y+h2+h2-3);
	GrLine( Clocks_bufwid, Clocks_gc, x+2, y+h2+4, x+2, y+h2+h2-4);

        GrSetGCForeground( Clocks_gc, (mask & 0x02)?bright:dark ); /* F */
	GrLine( Clocks_bufwid, Clocks_gc, x,   y+2, x,   y+h2-2);
	GrLine( Clocks_bufwid, Clocks_gc, x+1, y+3, x+1, y+h2-3);
	GrLine( Clocks_bufwid, Clocks_gc, x+2, y+4, x+2, y+h2-4);

        GrSetGCForeground( Clocks_gc, (mask & 0x01)?bright:dark ); /* G */
	GrLine( Clocks_bufwid, Clocks_gc, x+4, y+h2-1, x+w-4, y+h2-1);
	GrLine( Clocks_bufwid, Clocks_gc, x+3, y+h2,   x+w-3, y+h2);
	GrLine( Clocks_bufwid, Clocks_gc, x+4, y+h2+1, x+w-4, y+h2+1);
}

/* draw a Digital clock using the above 7-segment numbers */
static void Clocks_draw_digital_clock( struct tm * dispTime )
{
	GR_COLOR dark  = (Clocks_screen_info.bpp == 16)?
			GR_RGB( 60, 0, 0) : GRAY;
	GR_COLOR light = (Clocks_screen_info.bpp == 16)?
			GR_RGB( 255, 0, 0 ) : WHITE;
	char buf[8];
	int w2 = Clocks_screen_info.cols/2;
	int w3 = Clocks_screen_info.cols/3;
	int w5 = Clocks_screen_info.cols/5;
	int w6 = Clocks_screen_info.cols/6;
	int wA = Clocks_screen_info.cols/10;
	int wC = Clocks_screen_info.cols/12;

	int h2 = Clocks_height>>1;
	int h2p = h2 - w6;

	int h = Clocks_convert_to_12( dispTime->tm_hour );

	if( Clocks_screen_info.bpp != 16 ) dark = BLACK;

	snprintf( buf, 8, "%2d", h );
	Clocks_draw_7segment( (w5*1)-wA-8, h2p, w5-3, w3,
				buf[0], dark, light );
	Clocks_draw_7segment( (w5*2)-wA-4, h2p, w5-3, w3, 
				buf[1], dark, light );

	snprintf( buf, 8, "%02d", dispTime->tm_min );
	Clocks_draw_7segment( (w5*3)-wA+8, h2p, w5-3, w3, 
				buf[0], dark, light );
	Clocks_draw_7segment( (w5*4)-wA+12, h2p, w5-3, w3, 
				buf[1], dark, light );

	GrSetGCForeground( Clocks_gc, light );
	if(    (dispTime->tm_sec&0x01)
#ifdef CLOCK_ALTERNATE_BLINKING
	    && (dispTime->tm_sec<30) 
#endif
	    )
	    GrSetGCForeground( Clocks_gc, dark );
	GrFillEllipse( Clocks_bufwid, Clocks_gc, w2, h2-wC, 3, 3 );

	GrSetGCForeground( Clocks_gc, light );
	if(    (dispTime->tm_sec&0x01)
#ifdef CLOCK_ALTERNATE_BLINKING
	    && (dispTime->tm_sec>=30)
#endif
	    )
	    GrSetGCForeground( Clocks_gc, dark );
	GrFillEllipse( Clocks_bufwid, Clocks_gc, w2, h2+wC, 3, 3 );
}


/* the main clock draw valve - decides who actually gets called */
static void Clocks_draw( void )
{
	static time_t lastt = 0;
	time_t t;
	struct tm * current_time;

	t = time( NULL );
	if ( t == (time_t) - 1 ) return; /* error */

	t += Clocks_offset; 	/* factor in the offset for the world clock */

        current_time = localtime( &t );

        /* start clear (WHITE) */
	if( Clocks_style >= CLOCKS_STYLE_BCD )
	    GrSetGCForeground( Clocks_gc, BLACK );
	else
	    GrSetGCForeground( Clocks_gc, WHITE );
        GrFillRect( Clocks_bufwid, Clocks_gc, 0, 0,
                    Clocks_screen_info.cols, Clocks_height );

	switch( Clocks_style )
	{
	    case( CLOCKS_STYLE_ANALOG ):	/* traditional analog clock */
	    case( CLOCKS_STYLE_DECO ):		/* art-deco analog clock */
	    case( CLOCKS_STYLE_OVERSIZED ):	/* oversized analog clock */
	    case( CLOCKS_STYLE_WATCH ):		/* oversized analog watch */
		    Clocks_draw_analog_clocks( current_time );
		    break;

	    case( CLOCKS_STYLE_VECTOR ):	/* vectorfont clock */
		    Clocks_draw_vector_clock( current_time );
		    break;

	    case( CLOCKS_STYLE_BCD ):		/* BCD binary clock */
		    Clocks_draw_bcd_clock( current_time );
		    break;

	    case( CLOCKS_STYLE_BINARY ):	/* binary clock */
		    Clocks_draw_binary_clock( current_time );
		    break;

	    case( CLOCKS_STYLE_DIGITAL ):	/* digital clock */
		    Clocks_draw_digital_clock( current_time );
		    break;

	    /* __3__ Insert hooks to draw more faces here. */
	}

	if( lastt != t )
	{
	    lastt = t;
	    if( ipod_get_setting( TIME_TICKER ))
		ipod_beep();
	}

        /* copy the buffer into place */
	Clocks_blit();
}

/* our draw event routine */
static void Clocks_do_draw( void )
{
	pz_draw_header(_("Clock"));
	Clocks_draw();
}


/* for time adjustment */
static void Clocks_dec_time( void )
{
	int m;
	int seconds_based = 0;
	time_t t;
	struct tm * current_time;
	struct timeval tv_s;

	t = time( NULL );
	if ( t == (time_t) - 1 ) return; /* error */

        current_time = localtime( &t );

/* the years and months adjustments here are incorrect.  The time should
   be converted to a  struct tm*  and then incremented by 1 year or one
   month.  For now though, we'll just add 365 days for a year.
   This probably won't work right on leap years.  February will act funny
   in those years, since it's currently hardcoded to be 28 days instead
   of 29 days.  Man, our calendar sucks.
*/

	switch( Clocks_sel ) {
	case( CLOCKS_SEL_YEARS ):
		t -= 60*60*24*365;	/* wrongish */
		seconds_based = 1;
		break;

	case( CLOCKS_SEL_MONTHS ):
		m = current_time->tm_mon-1;
		if( m < 0 ) m=11;
		t -= 60*60*24 * monthlens[ m ];
		seconds_based = 1;
		break;

	case( CLOCKS_SEL_DAYS ):
		t -= 60*60*24;
		seconds_based = 1;
		break;

	case( CLOCKS_SEL_HOURS ):
		t -= 60*60;
		seconds_based = 1;
		break;

	case( CLOCKS_SEL_MINUTES ):
		t -= 60;
		seconds_based = 1;
		break;

	case( CLOCKS_SEL_SECONDS ):
		t -= 1;
		seconds_based = 1;
		break;
	}

	if( seconds_based ) {
		tv_s.tv_sec = t;
		tv_s.tv_usec = 0;
		settimeofday( &tv_s, NULL );
	}
	Clocks_draw();
}

/* for time adjustment */
static void Clocks_inc_time( void )
{
	int seconds_based = 0;
	time_t t;
	struct tm * current_time;
	struct timeval tv_s;

	t = time( NULL );
	if ( t == (time_t) - 1 ) return; /* error */

        current_time = localtime( &t );

	switch( Clocks_sel ) {
	case( CLOCKS_SEL_YEARS ):
		t += 60*60*24*365;	/* wrongish */
		seconds_based = 1;
		break;

	case( CLOCKS_SEL_MONTHS ):
		t += 60*60*24 * monthlens[ current_time->tm_mon ];
		seconds_based = 1;
		break;

	case( CLOCKS_SEL_DAYS ):
		t += 60*60*24;
		seconds_based = 1;
		break;

	case( CLOCKS_SEL_HOURS ):
		t += 60*60;
		seconds_based = 1;
		break;

	case( CLOCKS_SEL_MINUTES ):
		t += 60;
		seconds_based = 1;
		break;

	case( CLOCKS_SEL_SECONDS ):
		t += 1;
		seconds_based = 1;
		break;
	}

	if( seconds_based ) {
		tv_s.tv_sec = t;
		tv_s.tv_usec = 0;
		settimeofday( &tv_s, NULL );
	}
	Clocks_draw();
}


/* shut them down.. shut them all down! */
void Clocks_exit( void ) 
{
	GrDestroyTimer( Clocks_timer );
	GrDestroyGC( Clocks_gc );
	pz_close_window( Clocks_wid );
	GrDestroyWindow( Clocks_bufwid );
	GrDestroyWindow( Clocks_bakwid );
}


/* hold was switched on, grow the window... */
static void Clocks_hold_press( void )
{
	/* bufwid is full size already, no need to change it */
	GrResizeWindow( Clocks_wid, screen_info.cols, screen_info.rows );
	GrMoveWindow( Clocks_wid, 0, 0 );
	Clocks_height = screen_info.rows; /* adjust screen size ref */
}

/* hold was switched off, shrink the window... */
static void Clocks_hold_release( void )
{
	/* we leave bufwid at the full size, no reason to change it */
	GrResizeWindow( Clocks_wid, screen_info.cols, 
			screen_info.rows - (HEADER_TOPLINE +1 ));
	GrMoveWindow( Clocks_wid, 0, HEADER_TOPLINE + 1 );
	Clocks_height = (screen_info.rows - (HEADER_TOPLINE + 1));
}


/* event handler */
static int Clocks_handle_event (GR_EVENT *event)
{
	switch( event->type )
	{
	    case GR_EVENT_TYPE_TIMER:
		Clocks_draw();
		break;

	    case GR_EVENT_TYPE_KEY_UP:
		switch (event->keystroke.ch)
		{
		case 'h': // hold release
			Clocks_hold_release();
			break;
		}
		break;

	    case GR_EVENT_TYPE_KEY_DOWN:
		switch (event->keystroke.ch)
		{
		case IPOD_SWITCH_HOLD: // hold press
		    Clocks_hold_press();
		    break;


		case IPOD_BUTTON_ACTION: // Wheel button
		    if( Clocks_set )
		    {
			    int max = CLOCKS_SEL_MAX;

			    if( Clocks_style == CLOCKS_STYLE_ANALOG)
				    max = CLOCKS_SEL_SECONDS;
			    Clocks_sel++;
			    if( Clocks_sel > max )
				    Clocks_sel = CLOCKS_SEL_HOURS;
		    } else {
			    Clocks_style++;
			    if( Clocks_style > CLOCKS_STYLE_MAX )
				    Clocks_style = 0;
		    }
		    Clocks_draw();
		    break;

		case IPOD_WHEEL_ANTICLOCKWISE: // Wheel left
		    if( Clocks_set ) {
			    Clocks_dec_time();
			    Clocks_draw();
			    /* since we screwed up the time, reset the timer */
			    GrDestroyTimer( Clocks_timer );
			    Clocks_timer = GrCreateTimer( Clocks_wid, 1000 ); 
		    }
		    break;

		case IPOD_WHEEL_CLOCKWISE: // Wheel right
		    if( Clocks_set ) {
			    Clocks_inc_time();
			    Clocks_draw();
			    /* since we screwed up the time, reset the timer */
			    GrDestroyTimer( Clocks_timer );
			    Clocks_timer = GrCreateTimer( Clocks_wid, 1000 ); 
		    }
		    break;

		case IPOD_BUTTON_MENU: // Menu button
		case 'q': // (quit)
		    Clocks_exit();
		    break;

		case IPOD_BUTTON_PLAY: // Play/pause button
		case IPOD_BUTTON_REWIND: // Rewind button
		case IPOD_BUTTON_FORWARD: // Fast forward button
		default:
		    return EVENT_UNUSED;
		    break;
		} // keystroke
		break;   // key down

	} // event type

	return 1;
}


/* the main entry point */
static void new_Clocks_window_common(void)
{
	GrGetScreenInfo(&Clocks_screen_info);

	Clocks_gc = GrNewGC();
        GrSetGCUseBackground(Clocks_gc, GR_FALSE);
        GrSetGCForeground(Clocks_gc, BLACK);

	Clocks_height = (screen_info.rows - (HEADER_TOPLINE + 1));

	Clocks_wid = pz_new_window( 0, HEADER_TOPLINE + 1,
		    screen_info.cols, Clocks_height,
		    Clocks_do_draw, Clocks_handle_event );

	Clocks_bufwid = GrNewPixmap( screen_info.cols, screen_info.rows, NULL );
	Clocks_bakwid = GrNewPixmap( screen_info.cols, screen_info.rows, NULL );

        GrSelectEvents( Clocks_wid, GR_EVENT_MASK_TIMER
					| GR_EVENT_MASK_EXPOSURE
					| GR_EVENT_MASK_KEY_UP
					| GR_EVENT_MASK_KEY_DOWN );

	Clocks_timer = GrCreateTimer( Clocks_wid, 1000 ); /* 1 sec. */

	GrMapWindow( Clocks_wid );
}


/* display the clock */
void new_clock_window(void)
{
	Clocks_style = Clocks_bak;
	Clocks_set = 0;
	Clocks_sel = CLOCKS_SEL_DISPLAY;
	Clocks_offset = 0;

	new_Clocks_window_common();
}


/* display the world clock */
void new_world_clock_window(void)
{
	int locl_z_offs = clocks_tz_offsets[ ipod_get_setting( TIME_ZONE )];
	int targ_z_offs = clocks_tz_offsets[ ipod_get_setting( TIME_WORLDTZ )];

	int locl_d_offs = clocks_dst_offsets[ ipod_get_setting( TIME_DST )];
	int targ_d_offs = clocks_dst_offsets[ ipod_get_setting( TIME_WORLDDST )];

	Clocks_style = Clocks_bak;
	Clocks_set = 0;
	Clocks_sel = CLOCKS_SEL_DISPLAY;

	/*  the way this works is that we look at the difference between
	    the target zone, and the local zone.  That difference is the
	    number of minutes of delta we need to deal with.  
	    Once we have that, we multiply it by 60, and use it when we 
	    display the time.
	    We do this for the timezone first, then the DST second.
	*/
	Clocks_offset = ( targ_z_offs - locl_z_offs )*60;
	Clocks_offset += ( targ_d_offs - locl_d_offs )*60;

	new_Clocks_window_common();
}


/* let the user change the time */
void new_Set_Time_window( void )
{
	if( !Clocks_set )
		Clocks_bak = Clocks_style;
	Clocks_style = CLOCKS_STYLE_ANALOG;
	Clocks_set = 1;
	Clocks_sel = CLOCKS_SEL_HOURS;

	new_Clocks_window_common();
}

/* let the user change the time and date */
void new_Set_DateTime_window( void )
{
	if( !Clocks_set )
		Clocks_bak = Clocks_style;
	Clocks_style = CLOCKS_STYLE_VECTOR;
	Clocks_set = 1;
	Clocks_sel = CLOCKS_SEL_HOURS;

	new_Clocks_window_common();
}
