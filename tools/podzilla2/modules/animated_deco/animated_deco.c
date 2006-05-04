/*
 * Animated Decoration - A simple header decoration that's somewhat animated
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

#include "pz.h"

PzModule * module;

extern ttk_color pz_dec_ap_get_solid( char * name );

static int cv = 0;

/* this gets called at the user specified interval */
void deco_update( struct header_info * hdr )
{
	if( !hdr || !hdr->widg ) return;
	cv++;
}

/* this gets called whenever we need to be redrawn */
void deco_draw( struct header_info * hdr, ttk_surface srf )
{
	int v128 = (cv & 0x63)*2;
	if( !hdr || !hdr->widg ) return;

	/* this doesn't do what i wanted it to do, but it's good enough */

	/* fill the whole thing, to get widget spaces */
	ttk_fillrect( srf, 0, 0, ttk_screen->w, hdr->widg->h,
		    ttk_makecol( 64 + v128, 64 + v128, 64 + v128 ));

	/* now just the title area */
	ttk_fillrect( srf, hdr->widg->x, 0,
			hdr->widg->x+hdr->widg->w-1, hdr->widg->h,
			ttk_makecol( 128 + v128, 128 + v128, 128 + v128 ));
}



void cleanup_decoration( void ) 
{
}


void init_decoration() 
{
	/* internal module name */
	module = pz_register_module( "animated_deco", cleanup_decoration );

	/* register our callback function */
	pz_add_header_decoration( "Animated", deco_update, deco_draw, &cv );
}

PZ_MOD_INIT (init_decoration)
