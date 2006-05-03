/*
 * Header Clock - A header widget that displays the current time
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

#include <time.h>
#include <sys/time.h>
#include "pz.h"

PzModule * module;
static int updatev;

/* some utility stuff */

/* convert 24 hour time to 12 hour time */
static int convert_1224( int hours )
{
        if( pz_get_int_setting( pz_global_config, TIME_1224 ))
                return( hours );

	if( hours == 0 )	hours = 12;
	else if( hours > 12 )	hours -= 12;
	return( hours );
}


#define INSET_X (3)

extern ttk_color pz_dec_ap_get_solid( char * name );

/* this gets called at the user specified interval */
void clock_widget_update( struct header_info * hdr )
{
	if( !hdr || !hdr->widg ) return;

	/* let's adjust our width... it defaults to being square. */
	hdr->widg->w = pz_vector_width( "xxxxx", 5, 9 ,1 ) + (INSET_X*2);

	updatev ^= 1; /* toggle this */
}

/* this gets called whenever we need to be redrawn */
void clock_widget_draw( struct header_info * hdr, ttk_surface srf )
{
	char timestring[8];
	time_t t;
	struct tm * the_time_tm;

	/* make sure things are valid */
	if( !hdr || !hdr->widg || !srf ) return;

	/* now, let's setup the timestring */
	t = time( NULL );
	if( t == (time_t)-1) return;	/* error */
	the_time_tm = localtime( &t );

	snprintf( timestring, 8, "%02d%c%02d",
			convert_1224( the_time_tm->tm_hour ),
			(updatev)? ':' : ' ',
			the_time_tm->tm_min );

	/* for fun, draw a border around ourselves */
	ttk_rect( srf, hdr->widg->x, hdr->widg->y,
			hdr->widg->x + hdr->widg->w,
			hdr->widg->y + hdr->widg->h -1,
			pz_dec_ap_get_solid( "header.accent" ));

	/* now, let's draw out the time string... */
	pz_vector_string( srf, timestring,
			hdr->widg->x + INSET_X,
			hdr->widg->y + ((hdr->widg->h - 9)>>1),
			5, 9, 1, 
			pz_dec_ap_get_solid( "header.fg" ));
}



void cleanup_headerclock( void ) 
{
}


void init_headerclock() 
{
	/* internal module name */
	module = pz_register_module( "headerclock", cleanup_headerclock );

	/* register our callback function */
	pz_add_header_widget( "Clock", 
				clock_widget_update,
				clock_widget_draw,
				"Burrito" );
}

PZ_MOD_INIT (init_headerclock)
