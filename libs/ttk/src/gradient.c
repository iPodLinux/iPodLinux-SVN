/*
 * Copyright (c) 2005 Scott Lawrence
 *
 * This file is a part of TTK.
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

#include "ttk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* our list... */
static gradient_node * gradient_list = NULL;


/* erase the entire list */
void ttk_gradient_clear( void )
{
	gradient_node * t = gradient_list;
	gradient_node * n = NULL;

	/* just walk the list, erasing as we go... I am a leaf on the wind */
	while( t ) {
		n = t->next;
		free( t );
		t = n;
	}
	gradient_list = NULL;
}

/* find a gradient from the list, return NULL on failure, or the node */
gradient_node * ttk_gradient_find( ttk_color start, ttk_color end )
{
	gradient_node * t = gradient_list;

	/* just iterate over the list to find the requested node */
	while( t != NULL )
	{
		if( t->start == start  &&  t->end == end )
			return( t );
		t = t->next;
	}

	/* dude.  bummer.  it wasn't in the list! */
	return( NULL );
}

/* find a gradient from the list, create, return NULL on failure, or the node */
gradient_node * ttk_gradient_find_or_add( ttk_color start, ttk_color end )
{
	int x,s;
	int rA,gA,bA,rZ,gZ,bZ;

	/* attempt to find it first, return on success */
	gradient_node * t = ttk_gradient_find( start, end );
	if( t )  return( t );

	/* not found.. allocate a new one... */
	t = (gradient_node *)malloc( sizeof( gradient_node ));
	if( !t ) return( NULL );

	/* prepend it to the global list */
	t->next = gradient_list;
	gradient_list = t;

	/* populate it */
	t->start = start;
	t->end = end;

        ttk_unmakecol( start, &rA, &gA, &bA );
        ttk_unmakecol( end,   &rZ, &gZ, &bZ );

	for( x=0; x<256 ; x++ ){
		s=255-x;
		t->gradient[x] = ttk_makecol( 
				((rA*s)>>8) + ((rZ*x)>>8),
				((gA*s)>>8) + ((gZ*x)>>8),
				((bA*s)>>8) + ((bZ*x)>>8) );
	}

	/* fling it back, like a monkey flings poo. */
	return( t );
}
