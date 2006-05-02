/*
 * Copyright (C) 2004 Bernard Leach, Scott Lawrence, Joshua Oreman, et al.
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

static int initted = 0;

extern int ipod_read_apm(int *battery, int *charging);

static int make_dirty (TWidget *this) { this->dirty++; ttk_dirty |= TTK_DIRTY_HEADER; return 0; }

extern int pz_hold_is_on;

static TWidget *headerBar = 0;


/* current issues:
	Header positioning for widgets seems to have off-by-1 errors
	Selection of active widgets per side
	Selection of decoratio:
*/


/* ********************************************************************** */ 
/* List manipulations */


/* the list of all available header widgets */
static header_info * headerWidgets = NULL;

/* the list of all available decorations */
static header_info * headerDecorations = NULL;

/* this gets inc'ed - for cycling, enumerating, z-sorting widgets */
static long zvalue = 0L;


/* pz_add_header_widget
**
**	registers a widget to be available for the left or right side 
*/
void pz_add_header_widget( char * widgetDisplayName,
			    update_fcn update_function,
			    draw_fcn draw_function,
			    void * data )
{
	header_info * new = (header_info *)malloc( sizeof( header_info ));
	if( !new ) {
		printf( "Malloc error on %s\n", widgetDisplayName );
		return;
	}
	new->side = 0;
	new->LZorder = zvalue++;
	new->RZorder = zvalue++;
	new->updfcn = update_function;
	new->drawfcn = draw_function;
	new->data = data;
	new->name = widgetDisplayName;
	new->next = NULL;

	/* now, setup the internal wid */
	new->widg = ttk_new_widget (0, 0);
	new->widg->h = ttk_screen->wy;
	new->widg->w = ttk_screen->wy;	/* square for now */
	new->widg->draw = NULL;	/* should use this */
	new->widg->timer = NULL;	/* should use this */
	/* ttk_widget_set_timer (new->widg, 1000); */

	/* tack it onto the list... */
	if( !headerWidgets ) {
		headerWidgets = new;
	} else {
		header_info * h = headerWidgets;
		while( h->next ) {
			h = h->next;
		}
		h->next = new;
	}

/* XXXXX
	printf( "Added Header Widget %s\n", widgetDisplayName );
*/
}


void pz_add_header_decoration( char * decorationDisplayName,
				update_fcn update_function,
				draw_fcn draw_function,
				void * data )
{
	header_info * new = (header_info *)malloc( sizeof( header_info ));
	if( !new ) {
		printf( "Malloc error on %s\n", decorationDisplayName );
		return;
	}
	new->widg = NULL;
	new->side = 0;
	new->LZorder = -1;
	new->RZorder = -1;
	new->updfcn = update_function;
	new->drawfcn = draw_function;
	new->data = data;
	new->name = decorationDisplayName;
	new->next = NULL;

	/* now, setup the internal wid */
	new->widg = ttk_new_widget (0, 0);
	new->widg->h = ttk_screen->wy;
	new->widg->w = ttk_screen->w;	
	new->widg->draw = NULL;		/* should use this */
	new->widg->timer = NULL;	/* should use this */
	/* ttk_widget_set_timer (new->widg, 1000); */

	/* tack it onto the list... */
	if( !headerDecorations ) {
		headerDecorations = new;
	} else {
		header_info * h = headerDecorations;
		while( h->next ) {
			h = h->next;
		}
		h->next = new;
	}

/* XXXXX
	printf( "Added Header Decoration %s\n", decorationDisplayName );
*/
}

header_info * find_header_item( header_info * list, char * name )
{
	header_info * curr = list;

	if( !list || !name ) return( NULL );

	while( curr ) {
		if( curr->name != NULL &&  !strcmp( name, curr->name )) {
			return( curr );
		}
		curr = curr->next;
	}
	return( NULL );
}

void pz_enable_widget_on_side( int side, char * name )
{
	header_info * item = find_header_item( headerWidgets, name );

	if( item != NULL ) {
		item->side |= side;
	}
}

void pz_enable_header_decorations( char * name )
{
	header_info * d = headerDecorations;

	while( d ) {
		if( d->name != NULL && !strcmp( name, d->name )) {
			/* found it!  Set the bit! */
			d->side |= HEADER_SIDE_DECORATION;
		} else {
			/* not it... clear the bit */
			d->side &= ~HEADER_SIDE_DECORATION;
		}
		d = d->next;
	}
}


static void cycle_widgets_on_side( int side );

void force_update_of_widget( char * name )
{
	header_info * item = find_header_item( headerWidgets, name );

	if( item != NULL ) {
		/* update it! */
		if( item->updfcn ) item->updfcn( item );

		/* in case the widget disappears, force a cycle */
		if( item->widg->w == 0 ) {
			if( item->side & HEADER_SIDE_LEFT )
				cycle_widgets_on_side( HEADER_SIDE_LEFT );
			if( item->side & HEADER_SIDE_RIGHT )
				cycle_widgets_on_side( HEADER_SIDE_RIGHT );
		}
	}
}

void pz_clear_header_lists( void ) 
{
	header_info * t = headerDecorations;
	header_info * n = NULL;
	while( t ) {
		n = t->next;
		free( t );
		t = n;
	}
	t = headerWidgets;
	while( t ) {
		n = t->next;
		free( t );
		t = n;
	}
}


/* ********************************************************************** */ 
/* Internal widgets */

#ifdef NEVER
/** Decorations: **/
static void draw_decorations (TWidget *this, ttk_surface srf)
{
	enum ttk_justification just = TTK_TEXT_CENTER;
	int xp = 0;
	int xw = 0;
	int width = ttk_text_width (ttk_menufont, ttk_windows->w->title);
	int boff = 0;
	ttk_color c;

	if( pz_get_int_setting( pz_global_config, BATTERY_UPDATE ) 
	    == BATTERY_UPDATE_OFF ) {
		boff++;
	}

	/* set up text positioning */
	just = (int) pz_get_int_setting (pz_global_config, TITLE_JUSTIFY);
	ttk_header_set_text_justification( just );
	ttk_header_set_text_position( -1 );	/* default */

	xw = width + 8;
	switch( just ) {
	case( TTK_TEXT_LEFT ):
		/* XX probably need to adjust this based on left widgets */
		if( pz_hold_is_on) {
			ttk_header_set_text_position( ttk_screen->wy + 4 );
			xp = ttk_screen->wy + 4;
		} else {
			ttk_header_set_text_position( 4 );
			xp = 0;
		}
		break;
	case( TTK_TEXT_RIGHT ):
		if( boff ) {
			ttk_header_set_text_position( ttk_screen->w - 2 );
			xp = (ttk_screen->w - width) -4 -3;
		} else {
			ttk_header_set_text_position( hwid_right[hwid_rnext-1]-9 );
			xp = (hwid_right[hwid_rnext-1] - width) - 14;
		}
		break;
	case( TTK_TEXT_CENTER ):
	default:
		ttk_header_set_text_position( ttk_screen->w >>1 );
		xp = ((ttk_screen->w - width) >> 1) - 4;
		xw += 3; /* account for the other side */
		break;
	}

	/* draw the decorations */
	if (decorations == PZ_DEC_AMIGA13 || decorations == PZ_DEC_AMIGA11) {
		/* drag bars */
		if (decorations == PZ_DEC_AMIGA13) {
			/* 1.2/1.3 look */
			int o = ttk_screen->wy / 4;
			ttk_ap_fillrect (srf, ttk_ap_get ("header.fg"),
					ttk_screen->wy + o,
					o,
					(boff)?ttk_screen->w-4:
					hwid_right[hwid_rnext-1]-6 - o + 1,
					o*2-1 );
			ttk_ap_fillrect (srf, ttk_ap_get ("header.fg"), 
					ttk_screen->wy + o,
					ttk_screen->wy - o*2 + 1,
					(boff)?ttk_screen->w-4:
					hwid_right[hwid_rnext-1]-6 - o + 1,
					ttk_screen->wy - o);
	
		} else {
			/* 1.1/1.0 look */
			int i;
			for (i = 1; i < ttk_screen->wy; i += 2) {
				ttk_line (srf, 
					ttk_screen->wy, i,
					(boff)?ttk_screen->w:
					hwid_right[hwid_rnext-1]-6, i,
					ttk_ap_getx ("header.fg") -> color);
			}
		}

		/* faux close widget */
		if ( !pz_get_int_setting (pz_global_config, DISPLAY_LOAD) 
			&& !pz_hold_is_on) {
			double xo = ((float)ttk_screen->wy) / 8.0;
			double yo = ((float)ttk_screen->wy) / 8.0;
			/* this should be tweaked to look better on mini */
			/* i think that the proportions are wrong also */

			ttk_ap_fillrect( srf, ttk_ap_get ("header.fg"), 
					(int) (xo*1),  (int) (yo*1),
					(int) (xo*7),  (int) (yo*7) );
			ttk_ap_fillrect( srf, ttk_ap_get ("header.bg"),
					(int) (xo*1.6), (int) (yo*1.6),
					(int) (xo*6.4), (int) (yo*6.4) );
			ttk_ap_fillrect (srf, ttk_ap_get ("header.accent"), 
					(int) (xo*3.1), (int) (yo*3.1),
					(int) (xo*4.8), (int) (yo*4.8) );
		}


		/* clear text area */
		if (ttk_ap_getx ("header.bg") -> type & TTK_AP_COLOR) {
			switch( just ) {
			case( TTK_TEXT_LEFT ):
				xp = ttk_screen->wy+1;
				ttk_header_set_text_position( 11 + hwid_left[hwid_lnext-1] );
				break;

			case( TTK_TEXT_RIGHT ):
				xp = hwid_right[hwid_rnext-1] - width - 12;
				ttk_header_set_text_position( hwid_right[hwid_rnext-1] - 8 );

				if( boff ) {
					xp = ttk_screen->w - width - 6;
					ttk_header_set_text_position( ttk_screen->w -2 );
				}
				break;

			case( TTK_TEXT_CENTER ):
			default:
				xp = ((ttk_screen->w - width)>>1) - 4;
				ttk_header_set_text_position( ttk_screen->w >>1 );
			}

			ttk_ap_fillrect( srf, ttk_ap_get ("header.bg"),
				xp, 0, xp + width + 8,
				ttk_screen->wy - 2 );
		}

		/* vertical widget separators */
		ttk_ap_fillrect( srf, ttk_ap_get ("header.fg"), 
			    ttk_screen->wy-1, 0,
			    ttk_screen->wy+1, ttk_screen->wy - 1 );
		if( !boff )
			ttk_ap_fillrect( srf, ttk_ap_get ("header.fg"), 
			    hwid_right[hwid_rnext-1] - 4, 0,
			    hwid_right[hwid_rnext-1] - 6, ttk_screen->wy - 1 );

	} else if (decorations == PZ_DEC_AMIGA20) {
		/* top */
		ttk_line( srf, 0, 0, this->w, 0,
				ttk_ap_getx( "header.shine" )->color );
		/* left */
		ttk_line( srf, 0, 0, 0, ttk_screen->wy-2,
				ttk_ap_getx( "header.shine" )->color );
		/* bottom - handled by header.line */
		/* right */
		ttk_line( srf, ttk_screen->w-1, 1,
				ttk_screen->w-1, ttk_screen->wy-2,
				ttk_ap_getx( "header.shadow" )->color );

		/* right widget vertical border bar */
		if( !boff ) {
			ttk_line( srf, hwid_right[hwid_rnext-1]-5, 1,
			       hwid_right[hwid_rnext-1]-5, ttk_screen->wy - 1,
			       ttk_ap_getx( "header.shadow" )->color );
			ttk_line( srf, hwid_right[hwid_rnext-1]-4, 1,
			       hwid_right[hwid_rnext-1]-4, ttk_screen->wy - 1,
			       ttk_ap_getx( "header.shine" )->color );
		}

		/* left widget vertical border bar */
		ttk_line( srf, ttk_screen->wy-2, 0,
				ttk_screen->wy-2, ttk_screen->wy - 1,
				ttk_ap_getx( "header.shadow" )->color );
		ttk_line( srf, ttk_screen->wy-1, 0,
				ttk_screen->wy-1, ttk_screen->wy - 1,
				ttk_ap_getx( "header.shine" )->color );

		/* left widget - faux close button */
		if ( !pz_get_int_setting (pz_global_config, DISPLAY_LOAD) 
			&& !pz_hold_is_on) {
			double xo = ((float)ttk_screen->wy) / 7.0;
			double yo = ((float)ttk_screen->wy) / 7.0;
			ttk_fillrect( srf, xo*2.6, yo*2, xo*4.4, yo*5,
				ttk_ap_getx( "header.shine" )->color );
			ttk_rect( srf, xo*2.6, yo*2, xo*4.4, yo*5,
				ttk_ap_getx( "header.shadow" )->color );
		}

		/* tweak the text for the closebox */
		if( just == TTK_TEXT_LEFT ) 
			ttk_header_set_text_position( ttk_screen->wy + 4 );


	} else if (decorations == PZ_DEC_BEOS) {
		int www = ttk_screen->wy + width + 10;

		/* always force left justification */
		ttk_header_set_text_justification( TTK_TEXT_LEFT );
		ttk_header_set_text_position( ttk_screen->wy + 4 );

		/* faux close widget */
		if ( !pz_get_int_setting (pz_global_config, DISPLAY_LOAD) 
			&& !pz_hold_is_on) {

			/* fill gradient */
			/* unfortunately, we have no diagonal gradient... */
			ttk_vgradient( srf, 4, 4, 
				    ttk_screen->wy-4, ttk_screen->wy-4,
				    ttk_ap_getx( "header.shine" )->color,
				    ttk_ap_getx( "header.accent" )->color );

			/* draw these to get the NE/SW corners */
			ttk_ap_hline( srf, ttk_ap_get( "header.shadow" ),
			    4, ttk_screen->wy-4, 4 );
			ttk_ap_vline( srf, ttk_ap_get( "header.shadow" ),
			    4, 4, ttk_screen->wy-4 );

			/* and the main container boxes... */
			ttk_rect( srf, 4, 4, 
				    ttk_screen->wy-4, ttk_screen->wy-4,
				    ttk_ap_getx( "header.shadow" )->color );
			ttk_rect( srf, 5, 5, 
				    ttk_screen->wy-3, ttk_screen->wy-3,
				    ttk_ap_getx( "header.shine" )->color );
		}

		/* 3d effect */
		/* top */
		ttk_line( srf, 0, 0, www, 0,
				ttk_ap_getx( "header.shine" )->color );
		/* left */
		ttk_line( srf, 0, 0, 0, ttk_screen->wy,
				ttk_ap_getx( "header.shine" )->color );
		/* bottom - handled by header.line */
		/* right */
		ttk_line( srf, www-1, 1,
				www-1, ttk_screen->wy,
				ttk_ap_getx( "header.shadow" )->color );
		

		/* tabify! */
		ttk_fillrect( srf, www, 0,
				    ttk_screen->w, ttk_screen->wy, 
				    ttk_makecol( 0, 0, 0 ) );


	} else if (decorations == PZ_DEC_STTOS) {
		// X X . . X X . .
		// X X . . X X . . 
		unsigned short dots[] = {
			0xcccc, 0xcccc, 0x0000, 0x0000
		};

		int x,y;
		ttk_gc gc = ttk_new_gc();
		ttk_gc_set_foreground (gc, ttk_ap_getx ("header.accent") -> color);
		TWindow *pixmap = malloc (sizeof(TWindow)); pixmap->srf = srf;

		for( x=ttk_screen->wy+4 ; x<ttk_screen->w ; x += 8 ) {
			for( y=1 ; y<ttk_screen->wy ; y ++ )
			    if( y&0x02 ) t_GrBitmap( pixmap, gc, x, y, 8, 1, dots );
		}

		ttk_free_gc (gc);
		free (pixmap);

		/* draw the closebox */
		if ( !pz_get_int_setting (pz_global_config, DISPLAY_LOAD) 
			&& !pz_hold_is_on) {
			int v = ttk_screen->wy-1;
			int offs = v>>2;

			c = ttk_ap_getx( "header.bg" )->color;
			ttk_fillrect( srf, 0, 0, v, v, c );
			ttk_fillrect( srf, offs, offs, 
				( (offs&1)?1:0 ) + v-offs,
				( (offs&1)?1:0 ) + v-offs,
				ttk_ap_getx( "header.fg" )->color );

			/* TL-BR */
			ttk_line( srf,  0, 0,  v,   v,  c );
			ttk_line( srf,  0, 1,  v-1, v,  c );
			ttk_line( srf,  1, 0,  v, v-1,  c );

			/* BL-TR */
			ttk_line( srf,  0, v,    v, 0,   c );
			ttk_line( srf,  0, v-1,  v-1, 0, c );
			ttk_line( srf,  1, v,    v, 1,   c );
		}

		/* draw the separator */
		ttk_fillrect( srf, ttk_screen->wy, 0, 
				   ttk_screen->wy+2, ttk_screen->wy-1,
				ttk_ap_getx( "header.fg" )->color );
				

		/* tweak the text for the closebox */
		if( just == TTK_TEXT_LEFT ) {
			ttk_header_set_text_position( ttk_screen->wy + 6 );
			xp = ttk_screen->wy +2;
		}

		ttk_fillrect( srf, xp, 0, xp+xw, ttk_screen->wy,
				ttk_ap_getx( "header.bg" )->color );


	} else if (decorations == PZ_DEC_LISA) {
		int xL, xR;
		int v = ttk_screen->wy -1;

		ttk_header_set_text_justification( TTK_TEXT_CENTER );
		xp = ((ttk_screen->w - width)>>1) - 5;
		ttk_header_set_text_position( ttk_screen->w >>1 );

		/* center */
		c = ttk_ap_getx( "header.accent" )->color;
		ttk_fillrect( srf, xp, 0, xp+xw, v+1, c);

		xL = xp;  xR = xp + xw;

		/* four-bars */
		ttk_fillrect( srf, xL-5, 0, xL-1, v+1, c );
		ttk_fillrect( srf, xR+1, 0, xR+5, v+1, c );

		/* three-bars */
		ttk_fillrect( srf, xL-9, 0, xL-6, v+1, c );
		ttk_fillrect( srf, xR+6, 0, xR+9, v+1, c );

		/* two-bars */
		ttk_fillrect( srf, xL-12, 0, xL-10, v+1, c );
		ttk_fillrect( srf, xR+10, 0, xR+12, v+1, c );

		/* lines */
		ttk_line( srf, xL-14, 0, xL-14, v, c );
		ttk_line( srf, xL-16, 0, xL-16, v, c );
		ttk_line( srf, xL-18, 0, xL-18, v, c );

		ttk_line( srf, xR+13, 0, xR+13, v, c );
		ttk_line( srf, xR+15, 0, xR+15, v, c );
		ttk_line( srf, xR+17, 0, xR+17, v, c );

	} else if (decorations == PZ_DEC_MACOS7) {
		int yb;
		int r,g,b;

		ttk_header_set_text_justification( TTK_TEXT_CENTER );
		xp = ((ttk_screen->w - width)>>1) - 4;
		ttk_header_set_text_position( ttk_screen->w >>1 );

		ttk_unmakecol_ex( ttk_ap_getx( "header.accent" )->color,
						&r, &g, &b, srf);

		for( yb = 5 ; yb < ttk_screen->wy-3 ; yb += 2 ) {
			ttk_ap_hline( srf, ttk_ap_getx( "header.shadow" ),
					1, ttk_screen->w-2, yb );
		}

		/* draw a lighter box around it, accent color */
		c = ttk_makecol( (r+255)>>1, (g+255)>>1, (b+255)>>1 );
		ttk_fillrect( srf, xp, 0, xp+xw, ttk_screen->wy, 
				ttk_ap_getx( "header.bg" )->color);
		ttk_rect( srf, 0, 0, ttk_screen->w, ttk_screen->wy, c );

		/* draw the closebox */
		if ( !pz_get_int_setting (pz_global_config, DISPLAY_LOAD) 
			&& !pz_hold_is_on) {

			ttk_fillrect( srf, 6, 2, 
				    ttk_screen->wy-1, ttk_screen->wy-2,
				    ttk_ap_getx( "header.bg" )->color );

			/* draw these to get the NE/SW corners */
			ttk_ap_hline( srf, ttk_ap_get( "header.accent" ),
			    7, ttk_screen->wy-3, 5 );
			ttk_ap_vline( srf, ttk_ap_get( "header.accent" ),
			    7, 5, ttk_screen->wy-5 );

			/* and the main container boxes... */
			ttk_rect( srf, 7, 5, 
				    ttk_screen->wy-3, ttk_screen->wy-5,
				    ttk_ap_getx( "header.accent" )->color );
			ttk_rect( srf, 8, 6, 
				    ttk_screen->wy-2, ttk_screen->wy-4, c );
		}


	} else if (decorations == PZ_DEC_MACOS8) {
		int yb, v;
		ttk_header_set_text_justification( TTK_TEXT_CENTER );
		xp = ((ttk_screen->w - width)>>1) - 4;
		ttk_header_set_text_position( ttk_screen->w >>1 );

		/* outer box */
		ttk_rect( srf, 0, 0, ttk_screen->w, ttk_screen->wy, 
				ttk_ap_getx( "header.shine" )->color);

		v = ttk_screen->wy;

		/* dragbars */
		for ( yb=4; yb<ttk_screen->wy-6 ; yb +=2 ) {
			/* left side */
			ttk_ap_hline( srf, ttk_ap_get( "header.shine" ), v, xp-3, yb );
			ttk_ap_hline( srf, ttk_ap_get( "header.shadow" ), v+1, xp-2, yb+1 );

			/* right side */
			ttk_ap_hline( srf, ttk_ap_get( "header.shine" ), 
					width+xp+10,
					(boff)? ttk_screen->w-8 : ttk_screen->w-v-9, 
					yb );
			ttk_ap_hline( srf, ttk_ap_get( "header.shadow" ), 
					width+xp+11,
					(boff)? ttk_screen->w-7 : ttk_screen->w-v-8,
					yb+1 );
			
		}

		/* draw the closebox */
		if ( !pz_get_int_setting (pz_global_config, DISPLAY_LOAD) 
			&& !pz_hold_is_on) {

			ttk_vgradient( srf, 4, 4, 
				    ttk_screen->wy-4, ttk_screen->wy-4,
				    ttk_ap_getx( "header.shadow" )->color,
				    ttk_ap_getx( "header.shine" )->color );

			/* outer recess */
			ttk_rect( srf, 4, 4, v-5, v-5, ttk_ap_get( "header.shadow" )->color);
			ttk_rect( srf, 5, 5, v-4, v-4, ttk_ap_get( "header.shine" )->color);
			ttk_rect( srf, 5, 5, v-5, v-5, 0 );

			/* inner button */
			ttk_rect( srf, 6, 6, v-6, v-6, ttk_ap_get( "header.shine" )->color);
			ttk_line( srf, 7, v-7, v-7, v-7, ttk_ap_get( "header.shadow" )->color);
			ttk_line( srf, v-7, 7, v-7, v-7, ttk_ap_get( "header.shadow" )->color);
		}


	} else if (decorations == PZ_DEC_DOTS) {
		// . X X X .
		// X X X X X
		// X X X X X
		// X X X X X
		// . X X X .
		unsigned short circle[] = { 
			0x7000, 0xf800, 0xf800, 0xf800, 0x7000 };
		// Ugh. TTK doesn't export a proper bitmap function. Yet.
		ttk_gc gc = ttk_new_gc();
		ttk_gc_set_foreground (gc, ttk_ap_getx ("header.accent") -> color);
		TWindow *pixmap = malloc (sizeof(TWindow)); pixmap->srf = srf;
		int pL, pR;
		int xL, xR;
		int sxL, sxR;
		
		/* starting values */
		xL = 0;
		xR = ttk_screen->w;

		sxL = (ttk_screen->w>>1)+2; /* account for radius */
		sxR = (ttk_screen->w>>1)-2;

		/* account for visible widgets */
		if( pz_hold_is_on ) xL = ttk_screen->wy;
		if( !boff ) xR = hwid_right[hwid_rnext-1] - 4;

		/* account for text */
		if( just == TTK_TEXT_LEFT )
			xL += (width + 6);
		else if( just == TTK_TEXT_RIGHT )
			xR -= (width + 6);
		else {
			sxL -= ((width>>1) + 6);
			sxR += ((width>>1) + 6);
		}

		/* draw the dots, from the center out */
		for( pL = (ttk_screen->w)>>1, pR = (ttk_screen->w)>>1;
		     pL > 1;
		     pL -= 11, pR += 11 ){

			if( pL>xL && pL <= sxL ) {
				t_GrBitmap (pixmap, gc, pL - 2, 
					(ttk_screen->wy >> 1) - 2, 5, 5, circle);
			}
			if( pR<xR && pR >= sxR ) {
				t_GrBitmap (pixmap, gc, pR - 2, 
					(ttk_screen->wy >> 1) - 2, 5, 5, circle);
			}
		}

		ttk_free_gc (gc);
		free (pixmap);

	} else if(  (decorations == PZ_DEC_BIGRAD) ||
		(decorations == PZ_DEC_TRIGRAD) ||
		(decorations == PZ_DEC_BIGRADBAR) ||
		(decorations == PZ_DEC_TRIGRADBAR) ){
	int y;

	if( decoration_colors_dirty )
	{
		int rt, gt, bt;
		int rm, gm, bm;
		int rb, gb, bb;
		int h2 = this->h/2;

		ttk_unmakecol_ex( ttk_ap_getx( "header.gradient.top" )->color,
						&rt, &gt, &bt, srf);
		ttk_unmakecol_ex( ttk_ap_getx( "header.gradient.bottom" )->color,
						&rb, &gb, &bb, srf);

		if( (decorations == PZ_DEC_TRIGRAD)
		    || (decorations == PZ_DEC_TRIGRADBAR ))
		{
		    ttk_unmakecol_ex( ttk_ap_getx( "header.gradient.middle" )->color,
						&rm, &gm, &bm, srf);
		} else {
		    rm = ( rb + rt ) >> 1;
		    gm = ( gb + gt ) >> 1;
		    bm = ( bb + bt ) >> 1;
		}

		ttk_unmakecol_ex( ttk_ap_getx( "header.gradient.top" )->color,
						&rt, &gt, &bt, srf);

		/* that is; when the color scheme is changed, compute an array
		    of 22 or however many lines it is... */
		for( y=0 ; y<this->h ; y++ )
		{
			if( y<h2 ) {
			    int h2my = h2-y;
			    gradcol[y] = ttk_makecol_ex( 
					(rm * y)/h2 + (rt * (h2my)/h2),
					(gm * y)/h2 + (gt * (h2my)/h2),
					(bm * y)/h2 + (bt * (h2my)/h2),
					srf );
			} else {
			    int yy = y-h2;
			    int h2my = h2-yy;
			    gradcol[y] = ttk_makecol_ex( 
					(rb * yy)/h2 + (rm * (h2my)/h2),
					(gb * yy)/h2 + (gm * (h2my)/h2),
					(bb * yy)/h2 + (bm * (h2my)/h2),
					srf );
			}
		}

		/* and clear the flag... */
		decoration_colors_dirty = 0;
	}

	/* draw the gradient to the header... */
	for( y=0 ; y<this->h ; y++ )
	{
	    ttk_line( srf, 0, y, this->w, y, gradcol[y] );
	}

	/* fill the top if it's a half-gradient */
	if( (    (decorations == PZ_DEC_BIGRADBAR)
	      || (decorations == PZ_DEC_TRIGRADBAR)
	    ) && ttk_ap_get( "header.gradient.bar" ) )
	{
	    int spacing =  ttk_ap_get( "header.gradient.bar" )->spacing;
	    int h2 = this->h>>1;

	    /* spacing seems like it gets corrupted? this patches it. */
	    if( ( spacing < (h2*-1)) || ( spacing > h2) ) spacing = 0;

	    if( spacing > 0 )
		    ttk_fillrect( srf, 
			    0, spacing,
			    this->w, this->h>>1, 
			    ttk_ap_get ("header.gradient.bar")->color );
	    if( spacing < 0 ) {
		    spacing *= -1;
		    ttk_fillrect( srf, 
			    0, (this->h>>1) - spacing,
			    this->w, (this->h>>1) + spacing, 
			    ttk_ap_get ("header.gradient.bar")->color );
	    }
	}
    }    
}
#endif


/******************************************************************************/

/* these need to correspond directly to their counterparts in menu.c */
static int ratesecs[] = { 1, 2, 5, 10, 15, 30, 60, -1 };

#define DISP_MODE_ALL	(0)
#define DISP_MODE_CYCLE	(1)
#define DISP_MODE_NONE	(2)


/* find the item on the selected side with the lowest Z value */
static header_info * find_lowestZ_for_side( int side )
{
	header_info * curr = headerWidgets;
	header_info * lowesths = NULL;
	int lowvalue = zvalue+1;

	while( curr != NULL ) {
		if( (side == HEADER_SIDE_LEFT) && 
		    (curr->side & HEADER_SIDE_LEFT) ) {
			if( curr->LZorder < lowvalue ) {
				lowesths = curr;
				lowvalue = curr->LZorder;
			}
		} else if( (side == HEADER_SIDE_RIGHT) && 
		           (curr->side & HEADER_SIDE_RIGHT) ) {
			if( curr->RZorder < lowvalue ) {
				lowesths = curr;
				lowvalue = curr->RZorder;
			}
		}
		curr = curr->next;
	}

	return( lowesths );
}

/* find the item on the selected side with the highest Z value */
static header_info * find_highestZ_for_side( int side )
{
	header_info * curr = headerWidgets;
	header_info * highesths = NULL;
	int highvalue = 0;

	while( curr != NULL ) {
		if( (side == HEADER_SIDE_LEFT) && 
		    (curr->side & HEADER_SIDE_LEFT) ) {
			if( curr->LZorder >= highvalue ) {
				highesths = curr;
				highvalue = curr->LZorder;
			}
		} else if( (side == HEADER_SIDE_RIGHT) && 
		           (curr->side & HEADER_SIDE_RIGHT) ) {
			if( curr->RZorder >= highvalue ) {
				highesths = curr;
				highvalue = curr->RZorder;
			}
		}
		curr = curr->next;
	}

	return( highesths );
}

/* draw all of the widgets on the selected side in the proper mode */
static int draw_widgets_for_side( int side, int mode, ttk_surface srf )
{
	header_info * c;
	int xp = 0;

	if( side == HEADER_SIDE_RIGHT ) {
		xp = ttk_screen->w;
	}

	if( mode == DISP_MODE_CYCLE ) {
		/* just draw the topmost one */
		c = find_highestZ_for_side( side );
		if( c && c->drawfcn ) {
			if( side == HEADER_SIDE_RIGHT )	xp -= c->widg->w;
			c->widg->x = xp;
			if( srf != NULL ) c->drawfcn( c, srf );
			if( side == HEADER_SIDE_LEFT )	xp += c->widg->w;
		}

	} else if ( mode == DISP_MODE_ALL ) {
		/* draw them all */
		c = headerWidgets;
		while( c != NULL ) {
			if( c->side & side ) {
				if( c && c->drawfcn ) {
					if( side == HEADER_SIDE_RIGHT ) {
						xp -= c->widg->w;
					}
					c->widg->x = xp;
					if( srf != NULL ) c->drawfcn( c, srf );
					if( side == HEADER_SIDE_LEFT ) {
						xp += c->widg->w;
					}
				}
			}
			c = c->next;
		}
	}
	return( xp );
}

/* and here's another version for computing offsets for the decorations */
#define compute_offset_for_side( side, mode ) \
	draw_widgets_for_side( (side), (mode), NULL )


/* update all of the widgets on the selected side */
static void update_widget_list_for_side( int side, TWidget *this, int Mode )
{
	header_info * c = headerWidgets;

	while( c != NULL ) {
		if( c->side & side ) {
			if( c->updfcn ) {
				c->updfcn( c );
				make_dirty( this );
			}
		}
		c = c->next;
	}
}


/* to cycle, we simply find the lowest z-order item, and make it the highest */
/* some extra heuristics exist in there to tweak for 0-width (hidden) items */
static void cycle_widgets_on_side( int side )
{
	header_info * lowest = find_lowestZ_for_side( side );
	header_info * first = lowest;
	int loops = 0;

	while( loops < 4 ) {
		if( lowest ) {
			if( side == HEADER_SIDE_LEFT )  
				lowest->LZorder = zvalue++;
			else
				lowest->RZorder = zvalue++;
		}

		if( lowest->widg->w > 0 ) return;

		if( lowest == first ) {
			if( loops > 0 ) {
				return;
			}
			loops++;
		} 
		lowest = find_lowestZ_for_side( side );
	}
}

/* iterate over the list, find the active one, call its update function */
static void update_decorations( void )
{
	header_info * d = headerDecorations;
	while( d ){
		if( d->side & HEADER_SIDE_DECORATION ) {
			if( d->updfcn ) {
				d->updfcn( d );
				return; /* short circuit! */
			}
		}
		d = d->next;
	}
}

static int handle_header_updates( TWidget *this )
{
	static int d_upd_countdown = 0;
	static int l_upd_countdown = 0;
	static int r_upd_countdown = 0;
	static int l_cyc_countdown = 0;
	static int r_cyc_countdown = 0;

	int d_upd_rate = ratesecs[ pz_get_int_setting( 
					pz_global_config, DECORATION_RATE ) ];

	int l_cyc_mode = pz_get_int_setting( pz_global_config, HEADER_METHOD_L );
	int l_upd_rate = ratesecs[ pz_get_int_setting( 
					pz_global_config, HEADER_UPD_RATE_L ) ];
	int l_cyc_rate = ratesecs[ pz_get_int_setting( 
					pz_global_config, HEADER_CYC_RATE_L ) ];

	int r_cyc_mode = pz_get_int_setting( pz_global_config, HEADER_METHOD_R );
	int r_upd_rate = ratesecs[ pz_get_int_setting( 
					pz_global_config, HEADER_UPD_RATE_R ) ];
	int r_cyc_rate = ratesecs[ pz_get_int_setting( 
					pz_global_config, HEADER_CYC_RATE_R ) ];

	/* checks to make sure the user didn't go from 60s to 1s and such */
	if( l_cyc_countdown > l_cyc_rate ) l_cyc_countdown = l_cyc_rate;
	if( r_cyc_countdown > r_cyc_rate ) r_cyc_countdown = r_cyc_rate;
	if( l_upd_countdown > l_upd_rate ) l_upd_countdown = l_upd_rate;
	if( r_upd_countdown > r_upd_rate ) r_upd_countdown = r_upd_rate;
	if( d_upd_countdown > d_upd_rate ) d_upd_countdown = d_upd_rate;

	/* first, update all of the widgets */
	if( (l_cyc_mode != DISP_MODE_NONE) && (--l_upd_countdown <= 0)) {
	    	l_upd_countdown = l_upd_rate;
		update_widget_list_for_side( HEADER_SIDE_LEFT, this, l_cyc_mode );
	}
	if( (r_cyc_mode != DISP_MODE_NONE) && (--r_upd_countdown <= 0)) {
	    	r_upd_countdown = r_upd_rate;
		update_widget_list_for_side( HEADER_SIDE_RIGHT, this, r_cyc_mode );
	}

	/* next, cycle them, if applicable */
	if( (l_cyc_mode == DISP_MODE_CYCLE) && (--l_cyc_countdown < 0) ) {
		l_cyc_countdown = l_cyc_rate;
		cycle_widgets_on_side( HEADER_SIDE_LEFT );
		make_dirty( this ); /* force a redraw */
	}
	if( (r_cyc_mode == DISP_MODE_CYCLE) && (--r_cyc_countdown < 0) ) {
		r_cyc_countdown = r_cyc_rate;
		cycle_widgets_on_side( HEADER_SIDE_RIGHT );
		make_dirty( this ); /* force a redraw */
	}

/* XXXX
	printf( "(time: %d %d   %d %d)\n",
		l_upd_countdown, l_upd_rate, l_cyc_countdown, l_cyc_rate );
	fflush( stdout );
*/
	/* finally... update the header decorations */
	if( (--d_upd_countdown <= 0) ) {
	    	d_upd_countdown = d_upd_rate;
		update_decorations();
	}

	return( 0 );
}


/* adjust TTK to set up the header justification properly */
void pz_header_justification_helper( int lx, int rx )
{
	enum ttk_justification just = TTK_TEXT_CENTER;
		ttk_header_set_text_justification( TTK_TEXT_CENTER );

	/* get the setting */
	just = (int) pz_get_int_setting (pz_global_config, TITLE_JUSTIFY);

	/* tell ttk what to do */
	ttk_header_set_text_justification( just );
	ttk_header_set_text_position( -1 );	/* default */


	switch( just ) {
	case( TTK_TEXT_LEFT ):
		ttk_header_set_text_position( lx );
		break;

	case( TTK_TEXT_RIGHT ):
		ttk_header_set_text_position( rx );
		break;

	case( TTK_TEXT_CENTER ):
	default:
		ttk_header_set_text_position( ttk_screen->w >>1 );
		break;
	}
}


/* iterate over the list, find the active one, adjust x&w, draw it! */
static void draw_header_decorations( int x1, int x2, ttk_surface srf )
{
	header_info * d = headerDecorations;
	while( d ){
		if( d->side & HEADER_SIDE_DECORATION ) {
			if( d->drawfcn ) {
				d->widg->x = x1;
				d->widg->w = x2-x1;
				d->drawfcn( d, srf );
				return; /* short circuit! */
			}
		}
		d = d->next;
	}
}




/* draw the left and right widgets over the bbacking decoration */
static void draw_headerBar( TWidget *this, ttk_surface srf )
{
	int xl, xr;
	int l_cyc_mode = pz_get_int_setting( pz_global_config, HEADER_METHOD_L);
	int r_cyc_mode = pz_get_int_setting( pz_global_config, HEADER_METHOD_R);

	/* 1. compute space that header widgets take up */
	xl = compute_offset_for_side( HEADER_SIDE_LEFT, l_cyc_mode );
	xr = compute_offset_for_side( HEADER_SIDE_RIGHT, r_cyc_mode );

	/* 2. draw header decorations */
	pz_header_justification_helper( xl, xr );
	draw_header_decorations( xl, xr, srf );

	/* 3. draw widgets */
	xl = draw_widgets_for_side( HEADER_SIDE_LEFT, l_cyc_mode, srf );
	xr = draw_widgets_for_side( HEADER_SIDE_RIGHT, r_cyc_mode, srf );
}


static TWidget *new_headerBar_widget() 
{
	TWidget *ret = ttk_new_widget (0, 0);
	ret->w = ttk_screen->w;
	ret->h = ttk_screen->wy;
	ret->draw = draw_headerBar;
	ret->timer = handle_header_updates;
	ttk_widget_set_timer (ret, 1000);
	return ret;
}



/* ********************************************************************** */ 
/* Header Decorations */ 

static int hcolor;

/* one for testing... */
void test_draw_decorations( struct header_info * hdr, ttk_surface srf )
{
	ttk_color c;
	char * data;
	if( !hdr ) return;
	data = (char *) hdr->data;

	/* fill the whole back */
	ttk_fillrect( srf,
			0, 0,
			ttk_screen->w, ttk_screen->wy,
			ttk_makecol( 255, 0, 0 ) );

	/* now fill just the center bit */
	if( (hcolor & 7) == 6) hcolor = 0;
	if( (hcolor & 7) == 0) c = ttk_makecol( 255,   0,   0 );
	if( (hcolor & 7) == 1) c = ttk_makecol( 255, 255,   0 );
	if( (hcolor & 7) == 2) c = ttk_makecol(   0, 255,   0 );
	if( (hcolor & 7) == 3) c = ttk_makecol(   0, 255, 255 );
	if( (hcolor & 7) == 4) c = ttk_makecol(   0,   0, 255 );
	if( (hcolor & 7) == 5) c = ttk_makecol( 255,   0, 255 );
	ttk_fillrect( srf,
			hdr->widg->x, 0,
			hdr->widg->x + hdr->widg->w, ttk_screen->wy,
			c );

	/* the left and right sides - find any holes */
	if( hdr->widg->x != 0 )
		ttk_fillrect( srf,
			0, 0,
			hdr->widg->x, ttk_screen->wy,
			ttk_makecol( 0, 255, 0 ) );
	if( hdr->widg->x+hdr->widg->w != ttk_screen->w )
		ttk_fillrect( srf,
			hdr->widg->w + hdr->widg->x, 0,
			ttk_screen->w,  ttk_screen->wy,
			ttk_makecol( 255, 255, 0 ) );
/*
	printf( "Decoration \"%s\"  %d %d\n", data, hdr->widg->x, hdr->widg->w);
*/
}

void test_update_decorations( struct header_info * hdr )
{
/*
	printf( "Update Header Timeout\n" );
*/
	ttk_dirty |= TTK_DIRTY_HEADER;
	hcolor++;
}


/* this is helpful for getting a real solid color when there's a gradient
   or if there's a regular color */
ttk_color pz_dec_ap_get_solid( char * name )
{
	ttk_color c;
	TApItem *ap = ttk_ap_get( name );
	if( ap ) {
		if( ap->type & TTK_AP_COLOR ) {
			c = ap->color;
		} else if( ap->type & TTK_AP_GRADIENT ) {
			c = ap->gradstart;
		}
	} else {
		c = ttk_makecol( WHITE );
	}
	return( c );
}

/* plain - do nothing */
void dec_plain( struct header_info * hdr, ttk_surface srf )
{
	ttk_color c = pz_dec_ap_get_solid( "header.bg" );
	ttk_fillrect( srf, 0, 0, ttk_screen->w, hdr->widg->h, c );
}

/* do it as it's defined in the CS file */
void dec_csdef( struct header_info * hdr, ttk_surface srf )
{
	ttk_ap_fillrect( srf, ttk_ap_get ("header.bg"),
			0, 0, ttk_screen->w, hdr->widg->h ); 
}

/* m-robe style dots */
void dec_dots( struct header_info * hdr, ttk_surface srf )
{
	enum ttk_justification just = pz_get_int_setting( 
					pz_global_config, TITLE_JUSTIFY);
	int tw = ttk_text_width (ttk_menufont, ttk_windows->w->title);
	int pL, pR;
	int xL, xR;
	int sxL, sxR;

	/* starting values */
	xL = hdr->widg->x;
	xR = hdr->widg->x + hdr->widg->w;

	sxL = (ttk_screen->w>>1)+2; /* account for radius */
	sxR = (ttk_screen->w>>1)-2;

	/* account for text */
	if( just == TTK_TEXT_LEFT )
		xL += (tw + 6);
	else if( just == TTK_TEXT_RIGHT )
		xR -= (tw + 6);
	else {
		sxL -= ((tw>>1) + 6);
		sxR += ((tw>>1) + 6);
	}

	/* draw the dots, from the center out */
	for( pL = (ttk_screen->w)>>1, pR = (ttk_screen->w)>>1;
	     pL > 1;
	     pL -= 11, pR += 11 ){

		if( pL>xL && pL <= sxL ) {
			ttk_draw_icon( pz_icon_dot, srf, 
				pL - 3, (ttk_screen->wy >> 1) - 2, 
				ttk_ap_getx( "header.accent" ),
				ttk_ap_getx( "header.accent" )->color );
		}
		if( pR<xR && pR >= sxR ) {
			ttk_draw_icon( pz_icon_dot, srf, 
				pR - 3, (ttk_screen->wy >> 1) - 2, 
				ttk_ap_getx( "header.accent" ),
				ttk_ap_getx( "header.accent" )->color );
		}
	}
}


/* dots over the background solid color */
void dec_plaindots( struct header_info * hdr, ttk_surface srf )
{
	dec_plain( hdr, srf );
	dec_dots( hdr, srf );
}


#define RAISED  (0)
#define LOWERED (1)
void dec_draw_3d( ttk_surface srf, int x1, int y1, int x2, int y2, int updown )
{
	ttk_color nw, se;

	if( updown == LOWERED ) {
		se = ttk_ap_get( "header.shine" )->color;
		nw = ttk_ap_get( "header.shadow" )->color;
	} else {
		nw = ttk_ap_get( "header.shine" )->color;
		se = ttk_ap_get( "header.shadow" )->color;
	}

	ttk_line( srf, x2, y1, x2, y2, se );
	ttk_line( srf, x1, y2, x2, y2, se );

	ttk_line( srf, x1, y1, x2, y1, nw );
	ttk_line( srf, x1, y1, x1, y2, nw );
}


/* Amiga Decorations */
void dec_draw_Amiga1x( struct header_info * hdr, ttk_surface srf, int WhichAmigaDOS )
{
	int tw = ttk_text_width (ttk_menufont, ttk_windows->w->title);
	enum ttk_justification just = pz_get_int_setting( 
					pz_global_config, TITLE_JUSTIFY);
	int i;
	int xp1 = hdr->widg->x;
	int xp2 = hdr->widg->x + hdr->widg->w;
	int tx1 = 0, tx2 = 0;
	double yo, xo;

	dec_plain( hdr, srf );

	/* draw the faux close box, if applicable. */
	if( hdr->widg->x == 0 ) {
		xp1 = hdr->widg->h;	/* square gadget */
		xo = ((float)hdr->widg->h) / 8.0;
		yo = ((float)hdr->widg->h) / 8.0;

		if( WhichAmigaDOS < 20 ) {
			ttk_ap_fillrect( srf, ttk_ap_get ("header.fg"),
					(int) (xo*1),  (int) (yo*1),
					(int) (xo*7),  (int) (yo*7) );
			ttk_ap_fillrect( srf, ttk_ap_get ("header.bg"),
					(int) (xo*1.6), (int) (yo*1.6),
					(int) (xo*6.4), (int) (yo*6.4) );
			ttk_ap_fillrect (srf, ttk_ap_get ("header.accent"),
					(int) (xo*3.1), (int) (yo*3.1),
					(int) (xo*4.8), (int) (yo*4.8) );
		} else {
			dec_draw_3d( srf, 0, 0, hdr->widg->h - 1, 
				hdr->widg->h-1, RAISED );

			ttk_fillrect( srf, xo*3, xo*2, xo*5, xo*6, 
				ttk_ap_getx( "header.shine" )->color );
			ttk_rect( srf, xo*3, xo*2, xo*5, xo*6, 
				ttk_ap_getx( "header.shadow" )->color );
		}

	} else if( WhichAmigaDOS == 20 ) {
		xo = ((float)hdr->widg->h) / 8.0;
		dec_draw_3d( srf, 0, 0, hdr->widg->x - 1, 
				hdr->widg->h-2, RAISED );
	}

	/* draw drag bars */
	if( WhichAmigaDOS == 11 ) {
		/* 1.1 dragbars */
		for (i = 1; i < ttk_screen->wy; i += 2) {
			ttk_line (srf,
				xp1, i, xp2, i,
				ttk_ap_getx ("header.fg") -> color);
		}
	} else if( WhichAmigaDOS == 13 ) {
		/* 1.3 dragbars */
		int o = ttk_screen->wy / 4;
		ttk_ap_fillrect (srf, ttk_ap_get ("header.fg"),
			xp1 + o,  o,
			xp2 - o,  o*2 - 1 );
		ttk_ap_fillrect (srf, ttk_ap_get ("header.fg"),
			xp1 + o,  hdr->widg->h - o*2 + 1,
			xp2 - o,  hdr->widg->h - o );
	} else {
		dec_draw_3d( srf, xp1, 0, xp2-1, 
			hdr->widg->h-1, RAISED );
	}


	if( WhichAmigaDOS == 20 ) {
		if( hdr->widg->x + hdr->widg->w != ttk_screen->w ) {
		    dec_draw_3d( srf, hdr->widg->x + hdr->widg->w, 0,
				      ttk_screen->w-1, hdr->widg->h-1,
				      RAISED );
		}
	} else {
		/* draw vertical separators */
		ttk_ap_fillrect( srf, ttk_ap_get( "header.fg" ),
				xp1-1, 0, xp1+1, hdr->widg->h );
		if( xp2 != ttk_screen->w ) 
			ttk_ap_fillrect( srf, ttk_ap_get( "header.fg" ),
				xp2 - 1, 0, xp2 + 1, hdr->widg->h );
	}

	/* blot out the backing for the text */
	pz_header_justification_helper( xp1+3, xp2-3 );

	if( WhichAmigaDOS < 20 ) {
		switch( just ) {
		case( TTK_TEXT_LEFT ):
			tx1 = xp1+1;
			tx2 = xp1+tw+5;
			break;
		case( TTK_TEXT_RIGHT ):
			tx1 = xp2-tw-5;
			tx2 = xp2-1;
			break;
		default: /* center */
			tx1 = (ttk_screen->w - tw - 5) >> 1;
			tx2 = tx1 + tw + 5;
		}
		ttk_ap_fillrect( srf, ttk_ap_get( "header.bg" ),
			tx1, 0, tx2, hdr->widg->h );
	}
}

/* thse just call the above appropriately */
void dec_draw_Amiga13( struct header_info * hdr, ttk_surface srf )
{
	dec_draw_Amiga1x( hdr, srf, 13 ); /* AmigaDOS 1.3 */
}

void dec_draw_Amiga11( struct header_info * hdr, ttk_surface srf )
{
	dec_draw_Amiga1x( hdr, srf, 11 ); /* AmigaDOS 1.1 */
}

void dec_draw_Amiga20( struct header_info * hdr, ttk_surface srf )
{
	dec_draw_Amiga1x( hdr, srf, 20 ); /* AmigaDOS 2.0 */
}


/* BeOS yellow tab */
void dec_draw_BeOS( struct header_info * hdr, ttk_surface srf )
{
	int tw = ttk_text_width (ttk_menufont, ttk_windows->w->title);
	int www = tw + 10 + hdr->widg->x;

	if( hdr->widg->x == 0 ) {
		www += ttk_screen->wy;	/* add the width of faux close */
	}

	/* always force left justification */
	ttk_header_set_text_justification( TTK_TEXT_LEFT );
	if( hdr->widg->x == 0 ) {
		ttk_header_set_text_position( hdr->widg->x + hdr->widg->h + 4 );
	} else {
		ttk_header_set_text_position( hdr->widg->x + 4 );
	}

	/* backing */
	ttk_fillrect( srf, 0, 0, ttk_screen->w, ttk_screen->wy, 
			ttk_makecol( BLACK ));

	/* yellow tab */
	ttk_fillrect( srf, 0, 0, www, ttk_screen->wy, 
			ttk_ap_get( "header.bg" )->color ); /* xxx color */

	/* faux close widget */
	/* fill gradient -- unfortunately, we have no diagonal gradient... */
	ttk_vgradient( srf, 4, 4, 
		    ttk_screen->wy-4, ttk_screen->wy-4,
		    ttk_ap_getx( "header.shine" )->color,
		    ttk_ap_getx( "header.accent" )->color );
	       
	/* draw these to get the NE/SW corners */
	ttk_ap_hline( srf, ttk_ap_get( "header.shadow" ), 
	    4, ttk_screen->wy-4, 4 );
	ttk_ap_vline( srf, ttk_ap_get( "header.shadow" ),
	    4, 4, ttk_screen->wy-4 );

	/* and the main container boxes... */
	ttk_rect( srf, 4, 4, 
		    ttk_screen->wy-4, ttk_screen->wy-4,
		    ttk_ap_getx( "header.shadow" )->color );
	ttk_rect( srf, 5, 5, 
		    ttk_screen->wy-3, ttk_screen->wy-3,
		    ttk_ap_getx( "header.shine" )->color );
	
	/* 3d effect */
	/* top */
	ttk_line( srf, 0, 0, www, 0,
			ttk_ap_getx( "header.shine" )->color );
	/* left */      
	ttk_line( srf, 0, 0, 0, ttk_screen->wy,
			ttk_ap_getx( "header.shine" )->color );
	/* bottom - handled by header.line */
	/* right */
	ttk_line( srf, www-1, 1,
			www-1, ttk_screen->wy,
			ttk_ap_getx( "header.shadow" )->color ); 
}

/* Atari ST's horrible looking TOS interface */

/* TOS hashmarks
**   x x . . x x . . 
**   x x . . x x . . 
*/
static unsigned char dec_tos_hash[] = { 4, 1,
	3, 3, 0, 0, 3, 3, 0, 0,
	3, 3, 0, 0, 3, 3, 0, 0
};
void dec_draw_STTOS( struct header_info * hdr, ttk_surface srf )
{
	int x,y, xp;
	int startx = hdr->widg->x;
	ttk_color bg_c = pz_dec_ap_get_solid( "header.bg" );
	ttk_color fg_c = pz_dec_ap_get_solid( "header.fg" );
	int tw = ttk_text_width (ttk_menufont, ttk_windows->w->title);
	enum ttk_justification just = 
		(int) pz_get_int_setting (pz_global_config, TITLE_JUSTIFY);

	if( startx == 0 ) {
		startx += hdr->widg->h;
	}

	/* draw the horrid looking texture */
	for( x=startx+2 ; x<hdr->widg->x+hdr->widg->w ; x += 4 ) {
		for( y=0 ; y<hdr->widg->h ; y ++ ) {
			if( (y&0x02) == 0 ) {
				ttk_draw_icon( dec_tos_hash, srf, x, y, 
				    ttk_ap_getx( "header.fg" ), fg_c );
			}
		}
	}

	/* i don't know what's worse... that people thinks this looks good
	   or that i'm wasting so much time to make it available for them  */

	/* draw the crummy close box */
	if( hdr->widg->x == 0 ) {
		int v = hdr->widg->h-1;
		int offs = v>>3;

		ttk_fillrect( srf, 0, 0, v, v, bg_c );
		ttk_fillrect( srf, offs, offs,
			( (offs&1)?1:0 ) + v-offs,
			( (offs&1)?1:0 ) + v-offs,
			fg_c );

		/* TL-BR */
		ttk_line( srf,  0, 0,  v,   v,  bg_c );
		ttk_line( srf,  0, 1,  v-1, v,  bg_c );
		ttk_line( srf,  1, 0,  v, v-1,  bg_c );

		/* BL-TR */
		ttk_line( srf,  0, v,    v, 0,   bg_c );
		ttk_line( srf,  0, v-1,  v-1, 0, bg_c );
		ttk_line( srf,  1, v,    v, 1,   bg_c );
	}

	/* tweak the text for the closebox */
	pz_header_justification_helper( startx + 6, 
					hdr->widg->x + hdr->widg->w - 4 );
	switch( just ) {
	case( TTK_TEXT_LEFT ):
		xp = startx + 2;
		break;
	case( TTK_TEXT_RIGHT ):
		xp = hdr->widg->x + hdr->widg->w - tw - 6;
		break;
	default:
		xp = ((ttk_screen->w - tw ) >> 1) -3;
		break;
	}
	/* backwash..er.. backfill */
	ttk_fillrect( srf, xp, 0, xp+tw+6, hdr->widg->h, bg_c );

	/* draw the separators */
	ttk_fillrect( srf, startx, 0, startx+2, hdr->widg->h-1, fg_c );
	ttk_fillrect( srf, hdr->widg->x+hdr->widg->w-1, 0, 
			   hdr->widg->x+hdr->widg->w+1, hdr->widg->h-1, fg_c );
	
}

/* Apple Lisa */
void dec_draw_Lisa( struct header_info * hdr, ttk_surface srf )
{
	int tw = ttk_text_width (ttk_menufont, ttk_windows->w->title);
	ttk_color c;
	int xw = tw + 8 + 3;
	int xp;
	int xL, xR;
	int v = ttk_screen->wy -1;

	ttk_header_set_text_justification( TTK_TEXT_CENTER );
	xp = ((ttk_screen->w - tw)>>1) - 5;
	ttk_header_set_text_position( ttk_screen->w >>1 );

	/* center */
	c = ttk_ap_getx( "header.accent" )->color;
	ttk_fillrect( srf, xp, 0, xp+xw, v+1, c);

	xL = xp;  xR = xp + xw;

	/* four-bars */
	ttk_fillrect( srf, xL-5, 0, xL-1, v+1, c );
	ttk_fillrect( srf, xR+1, 0, xR+5, v+1, c );

	/* three-bars */
	ttk_fillrect( srf, xL-9, 0, xL-6, v+1, c );
	ttk_fillrect( srf, xR+6, 0, xR+9, v+1, c );

	/* two-bars */
	ttk_fillrect( srf, xL-12, 0, xL-10, v+1, c );
	ttk_fillrect( srf, xR+10, 0, xR+12, v+1, c );

	/* lines */
	ttk_line( srf, xL-14, 0, xL-14, v, c );
	ttk_line( srf, xL-16, 0, xL-16, v, c );
	ttk_line( srf, xL-18, 0, xL-18, v, c );

	ttk_line( srf, xR+13, 0, xR+13, v, c );
	ttk_line( srf, xR+15, 0, xR+15, v, c );
	ttk_line( srf, xR+17, 0, xR+17, v, c );
}

/* Apple Macintosh System 7 */
void dec_draw_MacOS7( struct header_info * hdr, ttk_surface srf )
{
	int yb, xp;
	int r,g,b;
	ttk_color c;
	int tw = ttk_text_width (ttk_menufont, ttk_windows->w->title);
	int xw = tw + 8;

	ttk_header_set_text_justification( TTK_TEXT_CENTER );
	xp = ((ttk_screen->w - tw)>>1) - 4;
	ttk_header_set_text_position( ttk_screen->w >>1 );

	ttk_unmakecol_ex( pz_dec_ap_get_solid( "header.accent" ),
					&r, &g, &b, srf);
	
	for( yb = 5 ; yb < hdr->widg->h-3 ; yb += 2 ) {
		ttk_ap_hline( srf, ttk_ap_getx( "header.shadow" ),
				1, ttk_screen->w-2, yb );
	}

	/* draw a lighter box around it, accent color */
	c = ttk_makecol( (r+255)>>1, (g+255)>>1, (b+255)>>1 );
	ttk_fillrect( srf, xp, 0, xp+xw, hdr->widg->h,
			pz_dec_ap_get_solid( "header.bg" ));
	ttk_rect( srf, 0, 0, ttk_screen->w, hdr->widg->h, c );

	/* draw the closebox */
	if( hdr->widg->x == 0 ) {
		ttk_fillrect( srf, 6, 2,
			hdr->widg->h-1, hdr->widg->h-2,
			pz_dec_ap_get_solid( "header.bg" ) );

		/* draw these to get the NE/SW corners */
		ttk_ap_hline( srf, ttk_ap_get( "header.accent" ),
			7, hdr->widg->h-3, 5 );
		ttk_ap_vline( srf, ttk_ap_get( "header.accent" ),
			7, 5, hdr->widg->h-5 );

		/* and the main container boxes... */
		ttk_rect( srf, 7, 5, 
			hdr->widg->h-3, hdr->widg->h-5,
			pz_dec_ap_get_solid( "header.accent" ) );
		ttk_rect( srf, 8, 6,
			hdr->widg->h-2, hdr->widg->h-4, c );
	}

}

/* Apple Macintosh MacOS 8 */
void dec_draw_MacOS8( struct header_info * hdr, ttk_surface srf )
{
}



/* ********************************************************************** */ 
/* Widgets */ 


void test_update_widget( struct header_info * hdr )
{
	char * data;
	if( !hdr ) return;
	data = (char *) hdr->data;
/*
	printf( "update %s\n", data );
	hdr->widg->w = 5;
*/
}

void test_draw_widget( struct header_info * hdr, ttk_surface srf )
{
	char * data;

	if( !hdr ) return;
	data = (char *) hdr->data;

	ttk_rect( srf,  hdr->widg->x, hdr->widg->y, 
			hdr->widg->x+hdr->widg->w, hdr->widg->y+hdr->widg->h, 
			ttk_ap_get( "header.shadow" )->color);

	pz_vector_string( srf, data, hdr->widg->x+2, hdr->widg->y+2, 5, 9, 1, 
			  ttk_ap_get( "header.shadow" )->color);
/*
	if( data[0] == 'R' )
		printf( "draw %s %d\n", data, hdr->widg->x );
*/
}


/* Hold Widget ****************** */ 

void pz_header_fix_hold() 
{
	force_update_of_widget( "Hold" );
	ttk_dirty |= TTK_DIRTY_HEADER;
}

static void w_hold_update( struct header_info * hdr )
{
	/* If hold is on set the width of our widget to be a little bigger
	   than the size of the icon itself. 
	   If hold is off, set the width to be 0, so it doesn't get drawn
        */
	if( pz_hold_is_on ) {
		hdr->widg->w = pz_icon_hold[0] + 4;
	} else {
		hdr->widg->w = 0;
	}
}

static void w_hold_draw( struct header_info * hdr, ttk_surface srf )
{
	if( pz_hold_is_on ) {
		ttk_draw_icon( pz_icon_hold, srf, hdr->widg->x+3, 
			       hdr->widg->y  + ((hdr->widg->h - pz_icon_hold[1])>>1),
			       ttk_ap_getx ("battery.border"),
			       ttk_ap_getx ("header.bg")->color );
/* XXX 
	there's something going wrong here, since this 
	doesn't draw in the right place.
		ttk_line( srf, hdr->widg->x, 0,
			 	hdr->widg->x+hdr->widg->w, 0, 0 );
*/
				
	}
}


/* Power Widget ***************** */

typedef struct powerstuff {
	int fill;
	int is_charging;
} powerstuff;

static powerstuff the_power_state;

static void w_powericon_update( struct header_info * hdr )
{
	powerstuff * ps = (powerstuff *)hdr->data;

	if( ps != NULL ) {
		ipod_read_apm( &ps->fill, &ps->is_charging );
	}
	hdr->widg->w = pz_icon_battery[0] + 4;
}

static void w_powericon_draw( struct header_info * hdr, ttk_surface srf )
{
	powerstuff * ps = (powerstuff *)hdr->data;
	int ypos;
	int xadd;
	TApItem fill;
	TApItem *ap;

        if (ps->is_charging) {
                memcpy (&fill, ttk_ap_getx ("battery.fill.charge"),
                        sizeof(TApItem));
                ap = ttk_ap_get( "battery.bg.charging" );

        } else if (ps->fill < 64) {
                memcpy (&fill, ttk_ap_getx ("battery.fill.low"),
                        sizeof(TApItem));
                ap = ttk_ap_get( "battery.bg.low" );

        } else {
                memcpy (&fill, ttk_ap_getx ("battery.fill.normal"),
                        sizeof(TApItem));
                ap = ttk_ap_get( "battery.bg" );
        }

	ypos = hdr->widg->y + ((hdr->widg->h - pz_icon_battery[1])>>1),
	xadd = 2;

        /* back fill the container... */
        ttk_ap_fillrect (srf, ap, hdr->widg->x + xadd +4, ypos + 1,
                                hdr->widg->x + xadd +22, ypos + 9);

	/* draw the framework */
	ttk_draw_icon (ttk_icon_battery, srf, hdr->widg->x+xadd, ypos,
			ttk_ap_getx ("battery.border"),
			ttk_ap_getx ("header.bg")->color);

	/* fill the container with stuff */
        if (fill.type & TTK_AP_SPACING) {
                fill.type &= ~TTK_AP_SPACING;
                ttk_ap_fillrect (srf, &fill,
                                xadd + hdr->widg->x + 4 + fill.spacing,
                                ypos + 1 + fill.spacing,
                                xadd + hdr->widg->x + 4 + fill.spacing
                                     + ((ps->fill * (17 - 2*fill.spacing)
                                     + ps->fill / 2) / 512),
                                ypos + 9 - fill.spacing);
        } else {
                ttk_ap_fillrect (srf, &fill,
                                xadd + hdr->widg->x + 4, 
				ypos + 1,
                                xadd + hdr->widg->x + 4 
				     + ((ps->fill * 17 + ps->fill / 2) / 512),
                                ypos + 9);
        }


	/* overlay the charge icon if we're charging */
	if (ps->is_charging)
		ttk_draw_icon( pz_icon_charging, srf, hdr->widg->x+xadd, ypos, 
			ttk_ap_getx ("battery.chargingbolt"),
			ttk_ap_getx ("header.bg")->color);
}

static void w_powertext_draw( struct header_info * hdr, ttk_surface srf )
{
	char buf[8];
	powerstuff * ps = (powerstuff *)hdr->data;
	ttk_color c = 0;
	TApItem *ap;

	/* set up the appropriate color */
	if( ps->fill < 100 )
		ap = ttk_ap_get( "battery.fill.low" );
	else
		ap = ttk_ap_get( "battery.fill.normal" );
	if( ap ) {
		if( ap->type & TTK_AP_GRADIENT ) {
			c = ap->gradstart;
		} else {
			c = ap->color;
		}
	} else {
		c = ttk_makecol( GREY );
	}

	if( ps->is_charging >= 1000 )
		strcpy (buf, "Chrg");
	else
		snprintf( buf, 32, "%c%d", 
			    (ps->is_charging)?'+':' ',
			    ps->fill );

	pz_vector_string_center( srf, buf, hdr->widg->x+1 + (hdr->widg->w>>1),
			hdr->widg->y + (hdr->widg->h>>1),
			5, 9, 1, c );
}


/* Load Average ***************** */ 


static double get_load_average( void )
{
	double ret = 0.00;
#ifdef __linux__
	FILE * file;
	float f;

	file = fopen( "/proc/loadavg", "r" );
	if( file ) {
		fscanf( file, "%f", &f );
		ret = f;
		fclose( file );
	} else {
		ret = 0.50;
	}
#else
#ifdef __APPLE__
	double avgs[3];
	if( getloadavg( avgs, 3 ) < 0 ) return( 0.50 );
	ret = avgs[0];
#else 
	ret = (float)(rand()%20) * 0.05;
#endif
#endif
	return( ret );
}


#define N_LAV_ENTRIES	(4)	/* should be a divisor of the width */
typedef struct _lav_data {
	double dv[N_LAV_ENTRIES];
	int iv[N_LAV_ENTRIES];
} _lav_data;

static _lav_data lav_data;

static void w_lav_update( struct header_info * hdr )
{
	_lav_data * ld = (_lav_data *)hdr->data;
	int h;

	for( h=0 ; h<N_LAV_ENTRIES-1 ; h++ ) {
		ld->dv[h] = ld->dv[h+1];
		ld->iv[h] = ld->iv[h+1];
	}
	ld->dv[h] = get_load_average();
	ld->iv[h] = (hdr->widg->h-5) - (ld->dv[h] * hdr->widg->h-5);
	if( ld->iv[h] < 0 ) 
		ld->iv[h] = 0;
	if( ld->iv[h] > (hdr->widg->h-5)) 
		ld->iv[h]= hdr->widg->h-5;
}


static void w_lav_draw( struct header_info * hdr, ttk_surface srf )
{
	int h;
	int sz = hdr->widg->w -4;
	int w4 = sz / N_LAV_ENTRIES;
	_lav_data * ld = (_lav_data *)hdr->data;

	for( h=0 ; h< N_LAV_ENTRIES ; h++ ) {
		/* backing */
		ttk_ap_fillrect (srf, ttk_ap_get ("loadavg.bg"), 
				hdr->widg->x + 2 + (h*w4),
				hdr->widg->y + 2,
				hdr->widg->x + 2 + (h*w4) + w4 + 1,
				hdr->widg->y + 2 + ld->iv[h]);

		/* body */
		ttk_ap_fillrect (srf, ttk_ap_get ("loadavg.fg"), 
				hdr->widg->x + 2 + (h*w4),
				hdr->widg->y + 2 + ld->iv[h],
				hdr->widg->x + 2 + (h*w4) + w4 + 1, 
				hdr->widg->y + hdr->widg->h - 1 -2);

		/* spike line */
		ttk_ap_hline( srf, ttk_ap_get( "loadavg.spike" ),
			    hdr->widg->x + 2 + (h*w4),
			    hdr->widg->x + 2 + (h*w4) + w4,
			    hdr->widg->y+ld->iv[h] + 2 );
	}

}



/* ********************************************************************** */ 
/* Initialization */ 

void pz_header_init() 
{
	if( !initted ) {
		/* register all internal widgets */
		pz_add_header_widget( "Hold", w_hold_update,
					w_hold_draw, (void *)NULL );

		pz_add_header_widget( "Load Average",   /* name */
					w_lav_update,   /* update fcn */
					w_lav_draw,     /* draw fcn */
					&lav_data );	/* data */

		pz_add_header_widget( "Power Icon", w_powericon_update, 
					w_powericon_draw, &the_power_state );
		pz_add_header_widget( "Power Text", w_powericon_update, 
					w_powertext_draw, &the_power_state );

		
		pz_add_header_widget("L1", test_update_widget,
					test_draw_widget, "L2" );
		pz_add_header_widget("L2", test_update_widget, 
					test_draw_widget, "L3" );
		pz_add_header_widget("L3", test_update_widget, 
					test_draw_widget, "L4" );
		pz_add_header_widget("R1", test_update_widget, 
					test_draw_widget, "R1" );
		pz_add_header_widget("R2", test_update_widget, 
					test_draw_widget, "R2" );

		/* register all internal decorations */
		pz_add_header_decoration( "Plain", NULL, dec_plain,
					"BleuLlama" );
		pz_add_header_decoration( "CS Gradient", NULL, dec_csdef,
					"BleuLlama" );

		/* dots! */
		pz_add_header_decoration( "Plain Dots", NULL, dec_plaindots,
					"BleuLlama" );
		pz_add_header_decoration( "CS Dots", NULL, dec_dots,
					"BleuLlama" );

		/* AmigaDOS are first because I say so. */
		pz_add_header_decoration( "Amiga 1.1", NULL, dec_draw_Amiga11,
					"Thanks, =RJ=!" );
		pz_add_header_decoration( "Amiga 1.3", NULL, dec_draw_Amiga13,
					"Thanks, =RJ=!" );
		pz_add_header_decoration( "Amiga 2.0", NULL, dec_draw_Amiga20,
					"BleuLlama" );

		/* other inferior OS lookalikes */
		pz_add_header_decoration( "BeOS", NULL, dec_draw_BeOS, 
					"BleuLlama" );
		pz_add_header_decoration( "Atari ST TOS", NULL, dec_draw_STTOS, 
					"BleuLlama" );
		pz_add_header_decoration( "Lisa", NULL, dec_draw_Lisa, 
					"BleuLlama" );
		pz_add_header_decoration( "Mac System 7", NULL, dec_draw_MacOS7, 
					"BleuLlama" );
/*
		pz_add_header_decoration( "MacOS 8", NULL, dec_draw_MacOS8, 
					"BleuLlama" );
*/

		/* a test one for the hell of it.  move this into a module? */
		pz_add_header_decoration( "Test Header", 
					test_update_decorations, 
					test_draw_decorations,
					"HDR" );

		/* set up "Plain" as the default */
		pz_enable_header_decorations( "Plain" );
	}
	initted++;

	/* for now, hardcode these... */
	pz_enable_widget_on_side( HEADER_SIDE_LEFT, "Load Average" );
	pz_enable_widget_on_side( HEADER_SIDE_LEFT, "Hold" );

	pz_enable_widget_on_side( HEADER_SIDE_RIGHT, "Power Icon" );
	pz_enable_widget_on_side( HEADER_SIDE_RIGHT, "Power Text" );
	
	/* load in the user settings */
	pz_enable_header_decorations( (char *) 
		    pz_get_string_setting( pz_global_config, DECORATIONS ));

	if( headerBar ) ttk_remove_header_widget( headerBar );
	ttk_add_header_widget( headerBar = new_headerBar_widget() );
}


/* ********************************************************************** */ 
/* menu stuff */

static int decorations_scroll( TWidget *this, int dir )
{
	int ret = ttk_menu_scroll( this, dir );
	pz_enable_header_decorations( ttk_menu_get_selected_item(this)->data );
	ttk_epoch++;
	return ret;
}

static int decorations_button( TWidget *this, int button, int time )
{
	if( button == TTK_BUTTON_ACTION ) {
		/* save it out */
		pz_set_string_setting( pz_global_config, DECORATIONS,
			ttk_menu_get_selected_item(this)->name );
		return( ttk_menu_button( this, TTK_BUTTON_MENU, 0 ));

	} else if ( button == TTK_BUTTON_MENU ) {
		pz_enable_header_decorations( (char *)
		    pz_get_string_setting( pz_global_config, DECORATIONS ));
	}
	return( ttk_menu_button( this, button, time ));
}

TWindow * pz_select_decorations( void )
{
	int iidx = 0;
	header_info * d = headerDecorations;

	TWindow * ret = ttk_new_window();
	TWidget * menu = ttk_new_menu_widget( 0, ttk_menufont,
				ttk_screen->w - ttk_screen->wx,
				ttk_screen->h - ttk_screen->wy );

	menu->scroll = decorations_scroll;
	menu->button = decorations_button;


	while( d ) {
		ttk_menu_item * item = calloc( 1, sizeof( struct ttk_menu_item ));
		item->data = malloc( strlen( d->name )+1 );
		item->name = malloc( strlen( d->name )+1 );
		strcpy( item->data, d->name );
		strcpy( (char *)item->name, d->name );

		ttk_menu_append( menu, item );
		
		iidx++;
		if( !strcmp( d->name,
		    pz_get_string_setting( pz_global_config, DECORATIONS ))) {
			ttk_menu_scroll( menu, iidx*5 );
		}

		d = d->next;
	}

	ttk_add_widget( ret, menu );
	ttk_window_set_title( ret, _("Decorations"));
	return ret;
}


TWindow * pz_select_left_widgets( void )
{
	return NULL;
}

TWindow * pz_select_right_widgets( void )
{
	return NULL;
}
