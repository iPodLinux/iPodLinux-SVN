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

#ifndef _TTK_GRADIENT_H_
#define _TTK_GRADIENT_H_

#include "ttk.h"

typedef struct _gradient_node
{
	ttk_color start;
	ttk_color end;
	ttk_color gradient[256];
	struct _gradient_node * next;
} gradient_node;


/* erase the entire list */
void ttk_gradient_clear( void );

/* find a gradient from the list, return NULL on failure, or the node */
gradient_node * ttk_gradient_find( ttk_color start, ttk_color end );

/* find a gradient from the list, create, return NULL on failure, or the node */
gradient_node * ttk_gradient_find_or_add( ttk_color start, ttk_color end );

#endif
