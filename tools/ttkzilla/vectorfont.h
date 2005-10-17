/*
 * vectorfont - vector polyline font
 *
 * Copyright (C) 2005 Scott Lawrence
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
 */


typedef struct vector_point {
        char x;
        char y;
} VECTOR_POINT;

extern VECTOR_POINT V_points[];	/* core points */

/* The fonts are designed as follows:
        - using the V_points array above as all of the available points for
          a character, we select points "p" to use for the p[] array.
        - the flags byte specifies things about this point:
            VFONT_OPEN - we are to skip drawing any line from this point
            VFONT_CLOSED - we are to connect a line from this point to the next
*/
typedef struct vector_poly_struct {
        char p[9];
        char flags[8];  /* this could be bits of a single char, but this is quicker. */
} VECTOR_POLY_STRUCT;

/* flags for each of the connectors */
#define VFONT_OPEN   ((char)0)
#define VFONT_CLOSED ((char)1)
#define VFONT_LOOP   ((char)2)

/* special characters */
#define VECTORFONT_SPECIAL_UP		(250)
#define VECTORFONT_SPECIAL_LEFT		(251)
#define VECTORFONT_SPECIAL_DOWN		(252)
#define VECTORFONT_SPECIAL_RIGHT	(253)


/* vector_render_polystruct
 *
 *  draw a generic polyline based on passed-in parameters
 *
 *  - use this for rendering custom polystructs with the built-in calls.
 *  - not necessary for standard use.  use vector_render_string below.
 */
void vector_render_polystruct( GR_WINDOW_ID wid, GR_GC_ID gc,
                                int index, VECTOR_POLY_STRUCT *vfs,
                                VECTOR_POINT * points,
                                int scale, int x, int y );


/* vector_render_char
 *
 *  (see renderString for parameters)
 */
void vector_render_char( GR_WINDOW_ID wid, GR_GC_ID gc,
			char c, int scale, int x, int y );


/* vector_string_pixel_width
 *
 *  return the width of the string in pixels 
 */
int vector_string_pixel_width( char * string, int kern, int scale );


/* vector_pixel_height
 *
 *  return the height of the string in pixels
 */
#define vector_pixel_height( scale ) \
	( (scale) * 8 )


/* vector_render_string
 *
 *  string: input string
 *    kern: inter-character kern (original font points)
 *   scale: scaling of the points
 *       x: start x position
 *       y: start y position
 */
void vector_render_string( GR_WINDOW_ID wid, GR_GC_ID gc, 
			    char * string, int kern, int scale, int x, int y );
void vector_render_string_center( GR_WINDOW_ID wid, GR_GC_ID gc, 
			    char * string, int kern, int scale, int x, int y );
void vector_render_string_right( GR_WINDOW_ID wid, GR_GC_ID gc, 
			    char * string, int kern, int scale, int x, int y );
