/*
 * polyfont - vector polyline font
 *
 *  Copyright (C) 2005 Scott Lawrence
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


#include <stdio.h>	/* for NULL */
#include <string.h>	/* for strlen */
#include "pz.h"
#include "vectorfont.h"


/* just some of the points that we need.. with these, we can do all alphanums */
VECTOR_POINT V_points[] = {
	{ 0, 0 },	{ 2, 0 },	{ 4, 0 },	/* 00 -- 01 -- 02   */
							/* -- -- -- -- -- */
	{ 0, 2 },	{ 2, 2 },	{ 4, 2 },	/* 03 -- 04 -- 05 */
	{ 0, 4 },	    { 3, 4 },	{ 4, 4 },	/* 06 -- -- 07 08   */
			{ 2, 5 },	{ 4, 5 },	/* -- -- 09 -- 10 */
	{ 0, 6 },	{ 2, 6 },	{ 4, 6 },	/* 11 -- 12 -- 13 */
	{ 0, 8 },	{ 2, 8 },	{ 4, 8 },	/* 14 -- 15 -- 16   */

	/* the following are for objects (17) */
	{ 6, 5 }, { 6, 8 }, { 8, 0 }, { 9,4 }, { 12, 4 }, /* extras for CLAW */

	/* and some for punctuation (22) */
	{ 3, 3 }, { 1, 3 }, { 3, 6 }, { 1, 6 }, { 2, 7 },
	/* (27) */
	{ 1, 0 }, { 3, 0 }, { 1, 8 }, { 3, 8 }, { 3, 7 }, { 1, 7 },
	/* (33) */
	{ 5, 8 }, { 2, 4 }
};

/* possible upgrade for this...
	instead of having a lookup table of points, instead have each 
	integer below be a hex byte:
	    0x00 -> $00  -> $xy
	upper nibble is the x position
	lower nibble is the y position

	flags array is still the same.  so '0' below would translate to:
	    { 0x00, 0x40, 0x48, 0x08, 0x00 }
	with no VECTOR_POINT lookup structure.

	this allows up to 16x16 structures, which isn't good for 
	the ships.  Perhaps for those, it should be int arrays instead of
	char arrays?
*/



/* digits for text rendering */
VECTOR_POLY_STRUCT V_fontDigits[] =
{
	/* 0 */ {{ 0, 2, 16, 14, 0 }, { 1, 1, 1, 1,  1, 0, 0, 0 }},
	/* 1 */ {{ 1, 15 }, { 1, 0, 0, 0,  0, 0, 0, 0 }},
	/* 2 */ {{ 0, 2, 8, 6, 14, 16 }, { 1, 1, 1, 1,  1, 0, 0, 0 }},
	/* 3 */ {{ 0, 2, 16, 14, 6, 8 }, { 1, 1, 1, 0,  1, 0, 0, 0 }},
	/* 4 */ {{ 0, 6, 8, 2, 16 }, { 1, 1, 0, 1,  0, 0, 0, 0 }},
	/* 5 */ {{ 2, 0, 6, 8, 16, 14 }, { 1, 1, 1, 1,  1, 0, 0, 0 }},
	/* 6 */ {{ 0, 14, 16, 8, 6 }, { 1, 1, 1, 1,  0, 0, 0, 0 }},
	/* 7 */ {{ 0, 2, 16 }, { 1, 1, 0, 0,  0, 0, 0, 0 }},
	/* 8 */ {{ 0, 2, 16, 14, 0, 6, 8 }, { 1, 1, 1, 1,  0, 1, 0, 0 }},
	/* 9 */ {{ 8, 6, 0, 2, 16 }, { 1, 1, 1, 1,  0, 0, 0, 0 }},
};


/* letters for alphabetical (English latin font) */
VECTOR_POLY_STRUCT V_fontAlpha[] =
{
	/* A */ {{ 14, 3, 1, 5, 16, 6, 8 }, { 1, 1, 1, 1,  0, 1, 0, 0 }},
	/* B */ {{ 14, 0, 5, 6, 13, 14 }, { 1, 1, 1, 1,  1, 0, 0, 0 }},
	/* C */ {{ 2, 0, 14, 16 }, { 1, 1, 1, 0,  0, 0, 0, 0 }},
	/* D */ {{ 14, 0, 1, 5, 13, 15, 14 }, { 1, 1, 1, 1,  1, 1, 0, 0 }},
	/* E */ {{ 2, 0, 14, 16, 6, 7 }, { 1, 1, 1, 0,  1, 0, 0, 0 }},
	/* F */ {{ 2, 0, 14, 6, 7 }, { 1, 1, 0, 1,  0, 0, 0, 0 }},
	/* G */ {{ 5, 2, 0, 14, 16, 10, 9 }, { 1, 1, 1, 1,  1, 1, 0, 0 }},
	/* H */ {{ 0, 14, 6, 8, 2, 16 }, { 1, 0, 1, 0,  1, 0, 0, 0 }},
	/* I */ {{ 0, 2, 1, 15, 14, 16 }, { 1, 0, 1, 0,  1, 0, 0, 0 }},
	/* J */ {{ 0, 2, 28, 30, 14, 11 }, { 1, 0, 1, 1,  1, 0, 0, 0 }},
	/* K */ {{ 0, 14, 2, 6, 16 }, { 1, 0, 1, 1,  0, 0, 0, 0 }},
	/* L */ {{ 0, 14, 16 }, { 1, 1, 0, 0,  0, 0, 0, 0 }},
	/* M */ {{ 14, 0, 4, 2, 16 }, { 1, 1, 1, 1,  0, 0, 0, 0 }},
	/* N */ {{ 14, 0, 16, 2 }, { 1, 1, 1, 0,  0, 0, 0, 0 }},
	/* O */ {{ 0, 2, 16, 14, 0 }, { 1, 1, 1, 1,  1, 0, 0, 0 }},
	/* P */ {{ 14, 0, 2, 8, 6 }, { 1, 1, 1, 1,  0, 0, 0, 0 }},
	/* Q */ {{ 0, 2, 13, 15, 14, 0, 12, 16 }, { 1, 1, 1, 1,  1, 0, 1, 0 }},
	/* R */ {{ 14, 0, 2, 8, 6, 16 }, { 1, 1, 1, 1,  1, 0, 0, 0 }},
	/* S */ {{ 2, 0, 6, 8, 16, 14 }, { 1, 1, 1, 1,  1, 0, 0, 0 }},
	/* T */ {{ 0, 2, 1, 15 }, { 1, 0, 1, 0,  0, 0, 0, 0 }},
	/* U */ {{ 0, 14, 16, 2 }, { 1, 1, 1, 0,  0, 0, 0, 0 }},
	/* V */ {{ 0, 15, 2 }, { 1, 1, 0, 0,  0, 0, 0, 0 }},
	/* W */ {{ 0, 14, 12, 16, 2 }, { 1, 1, 1, 1,  0, 0, 0, 0 }},
	/* X */ {{ 0, 16, 2, 14 }, { 1, 0, 1, 0,  0, 0, 0, 0 }},
	/* Y */ {{ 0, 4, 2, 4, 15 }, { 1, 1, 0, 1,  0, 0, 0, 0 }},
	/* Z */ {{ 0, 2, 14, 16 }, { 1, 1, 1, 0,  0, 0, 0, 0 }}
};

/* punctuation */
VECTOR_POLY_STRUCT V_punctuation[] =
{
	/* ! */ {{ 9, 1, 2, 9,  26, 29, 30, 26}, { 1, 1, 1, 0, 1, 1, 1, 0 }},
	/* : */ {{ 4, 22, 23, 4,  9, 24, 25, 9}, { 1, 1, 1, 0, 1, 1, 1, 0 }},
	/* / */ {{ 14, 2 }, { 1, 0, 0, 0,  0, 0, 0, 0}},
	/* \ */ {{ 0, 16 }, { 1, 0, 0, 0,  0, 0, 0, 0}},
	/* _ */ {{ 14, 16 }, { 1, 0, 0, 0,  0, 0, 0, 0}},
	/* . */ {{ 12, 31, 15, 32, 12 }, { 1, 1, 1, 1,  0, 0, 0, 0}},
	/* , */ {{ 12, 31, 15, 29, 32, 12 }, { 1, 1, 1, 1,  1, 0, 0, 0}},
	/* [ */ {{ 28, 27, 29, 30 }, { 1, 1, 1, 0,  0, 0, 0, 0}},
	/* ] */ {{ 27, 28, 30, 29 }, { 1, 1, 1, 0,  0, 0, 0, 0}},
	/* ' */ {{ 0, 1, 3, 0 }, { 1, 1, 1, 0,  0, 0, 0, 0}},
	/* - */ {{ 6, 8 }, { 1, 0, 0, 0,  0, 0, 0, 0}},
	/* & */ {{ 33, 3, 1, 5, 11, 15, 31}, { 1, 1, 1, 1,  1, 1, 0, 0 }}
};

/* specials */
VECTOR_POLY_STRUCT V_specials[] =
{
	/* ^ */ {{ 14, 12, 16, 14 }, { 1, 1, 1, 0,  0, 0, 0, 0 }},
	/* < */ {{ 5, 34, 13, 3 }, { 1, 1, 1, 0,  0, 0, 0, 0 }},
	/* v */ {{ 0, 2, 4, 0 }, { 1, 1, 1, 0,  0, 0, 0, 0 }},
	/* > */ {{ 3, 34, 11, 3 }, { 1, 1, 1, 0,  0, 0, 0, 0 }},
};


/* vector_render_polystruct
 *
 *  draw a generic polyline based on passed-in parameters
 */
void vector_render_polystruct( GR_WINDOW_ID wid, GR_GC_ID gc,
				int index, VECTOR_POLY_STRUCT *vfs,
				VECTOR_POINT * points,
				int scale, int x, int y )
{
	int p;

	if( !vfs ) return;
	if( !points ) points = V_points;

	for( p=0 ; p<8 ; p++ ) {
	    if( vfs[index].flags[p] == VFONT_CLOSED )
		GrLine( wid, gc,
		    x + points[ (int)vfs[index].p[p] ].x * scale,
		    y + points[ (int)vfs[index].p[p] ].y * scale,
		    x + points[ (int)vfs[index].p[p+1] ].x * scale,
		    y + points[ (int)vfs[index].p[p+1] ].y * scale );
	}
}

/* vector_render_char
 *
 *  (see render_string for parameters)
 */
void vector_render_char( GR_WINDOW_ID wid, GR_GC_ID gc,
			char c, int scale, int x, int y )
{
	int index = 0;
	VECTOR_POLY_STRUCT *vfs = NULL;

	if( c == ' ' ) return;		/* don't render anything */

	/* these all fall through to the next item, which increments
           the counter.  This seemed like the best way to do this without
	   searching through some sort of lookup table.... */
	index = 0;
	switch( c ) {
	    case( '&' ):
		index++;
	    case( '-' ):
		index++;
	    case( '\'' ):
	    case( '`' ):
		index++;
	    case( ']' ):  case( ')' ):  case( '}' ):
		index++;
	    case( '[' ):  case( '(' ):  case( '{' ):
		index++;
	    case( ',' ):
		index++;
	    case( '.' ):
		index++;
	    case( '_' ):
		index++;
	    case( '\\' ):
		index++;
	    case( '/' ):
		index++;
	    case( ':' ):
		index++;
	    case( '!' ):
		vfs = V_punctuation;
	    default:
		break;
	}

	if( (c >= '0') && (c <= '9') ) {	 /* numbers */
		vfs = V_fontDigits;
		index = c-'0';
	}
	if( (c >= 'A') && (c <= 'Z') ) {	/* UPPERCASE */
		vfs = V_fontAlpha;
		index = c-'A';
	}
	if( (c >= 'a') && (c <= 'z') ) {	/* lowercase */
		vfs = V_fontAlpha;
		index = c-'a';
	}


	/* check for special characters */
	if( ((unsigned char)c >= 250 ) ) {
		index = (unsigned char)c - 250;
		vfs = V_specials;
	}

	if( !vfs ) return;

	vector_render_polystruct( wid, gc, index, vfs, V_points, scale, x, y );
}


/* vector_string_pixel_width
 *
 *  determine the width of the passed in string, in pixels.
 */
int vector_string_pixel_width( char * string, int kern, int scale )
{
	int sl = strlen( string );
	return( ((sl-1) * scale * kern)+( sl * scale * 5 ) );
}


/* vector_render_string
 *
 *  string: input string
 *    kern: inter-character kern (original font points)
 *   scale: scaling of the points
 *       x: start x position
 *       y: start y position
 */
void vector_render_string( GR_WINDOW_ID wid, GR_GC_ID gc,
			char * string, int kern, int scale, int x, int y )
{
	int c = 0;
	int xp = x;

	if( !string ) return;

	while( string[c] != '\0' ) {
		vector_render_char( wid, gc, string[c], scale, xp, y );
		xp += (5+kern) * scale;
		c++;
	}
}

void vector_render_string_center( GR_WINDOW_ID wid, GR_GC_ID gc,
				char * string, int kern, int scale, 
				int x, int y )
{
	int xp = x-( vector_string_pixel_width( string, kern, scale)/2 );
	int yp = y-(( 8 * scale )/2);

	vector_render_string( wid, gc, string, kern, scale, xp, yp );
}

void vector_render_string_right( GR_WINDOW_ID wid, GR_GC_ID gc,
				char * string, int kern, int scale, 
				int x, int y )
{
	int xp = x - vector_string_pixel_width( string, kern, scale );

	vector_render_string( wid, gc, string, kern, scale, xp, y );
}
