/*
 * appearance - color scheming and such for all of podzilla
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
 *
 */

/* 
 * $Log: appearance.c,v $
 * Revision 1.1  2005/07/09 20:49:16  yorgle
 * Added in appearance.[ch].  Currently it only supports color schemes
 * Color scheme selection in menu.c
 * color-scheme support in mlist.c, slider.c, pz.c (menu scrollbar, pz header, sclider)
 *
 *
 */

#include <stdio.h>	/* NULL */
#include "pz.h"
#include "ipod.h"	/* ipod_get_setting */


static GR_COLOR colorscheme_mono[] = {
	GR_RGB( 255, 255, 255 ),	/* bg */
	GR_RGB(   0,   0,   0 ),	/* fg */
	GR_RGB(   0,   0,   0 ),	/* sel bg */
	DKGRAY,				/* sel bg border */
	GR_RGB( 255, 255, 255 ),	/* sel fg */
	WHITE, GRAY, DKGRAY, BLACK,	/* arrow animation */
	GR_RGB( 255, 255, 255 ),	/* title bg */
	GR_RGB(   0,   0,   0 ),	/* title fg */
	BLACK,				/* title line */
	BLACK, WHITE, GRAY,		/* scrollbar */
	BLACK, WHITE, GRAY,		/* slider */
	BLACK, WHITE, DKGRAY, BLACK, GRAY,	/* battery */
	BLACK, BLACK,			/* lock */
	BLACK, GRAY, WHITE, WHITE,	/* message window */
};

static GR_COLOR colorscheme_color[] = {
	/* menu colors */
	GR_RGB( 245, 222, 179 ),	/* sandy brown */
	GR_RGB(   0,   0,   0 ),	/* black */

	GR_RGB(  70, 130, 180 ),	/* steel blue */
	GR_RGB(   0,   0, 139 ),	/* dark blue */
	GR_RGB( 255, 255, 255 ),	/* white */

	WHITE, 				/* animated arrow */
	    GR_RGB( 175, 238, 238 ),
	    GR_RGB( 135, 206, 235 ),
	    GR_RGB(   0,   0, 139 ),	

	/* title */
	GR_RGB( 212, 246, 246 ),	/* light blue */
	GR_RGB(   0,   0, 128 ),	/* navy */
	GR_RGB(   0, 128, 128 ),	/* teal */

	BLACK, 				/* scrollbar */
	    GR_RGB( 200, 232, 253 ),	   /* lt blue */
	    GR_RGB(   0, 128, 128 ),	   /* teal */
	BLACK, 				/* slider */
	    GR_RGB( 200, 232, 253 ),	   /* lt blue */
	    GR_RGB(   0, 128, 128 ),	   /* teal */
	BLACK,  			/* battery */
	    GR_RGB( 255, 255, 255 ),	   /* white */
	    GR_RGB(   0, 255,   0 ),	   /* bright green */
	    GR_RGB( 255, 165,   0 ),	   /* orange */
	    GR_RGB(   0,   0, 255 ),       /* blue */
	GR_RGB( 139,  0,   0 ),		/* lock */
	    GR_RGB( 255,   0,   0 ),
	BLACK, GREEN,			/* messages */
	    GR_RGB( 200, 232, 253 ),	   /* lt blue */
	    GR_RGB( 255, 255,   0 ),	   /* yellow */
};

static GR_COLOR colorscheme_color_strange[] = {
	GR_RGB(  10,  33,  76 ),
	GR_RGB( 255, 255, 255 ),
	GR_RGB( 185, 125,  75 ),
	GR_RGB( 255, 255, 116 ),
	GR_RGB(   0,   0,   0 ),
	WHITE, GRAY, DKGRAY, BLACK,	/* animation */
	GR_RGB(   5, 128, 141 ),
	GR_RGB( 209, 116, 168 ),
	GR_RGB( 190, 180, 210 ),
	WHITE, GREEN, BLUE,		/* scrollbar */
	WHITE, BLACK, RED,		/* slider */
	WHITE, BLACK, GREEN, YELLOW, RED,	/* battery */
	WHITE, BLUE,			/* lock */
	BLACK, GREEN,			/* messages */
	    GR_RGB( 200, 232, 253 ),	   /* lt blue */
	    GR_RGB( 255, 255,   0 ),	   /* yellow */
};


int colorscheme_max = CS_NSCHEMES;

static GR_COLOR * schemes[] = {
	colorscheme_mono,
	colorscheme_color,
	colorscheme_color_strange
};

/* these are separate, since they are also used in the menu system. 
 *  these need to be in sync with the above schemes[] table. 
 */
char * colorscheme_names[] = {
	"Mono",
	"Color",
	"Strange"
};


GR_COLOR * colorscheme_current = colorscheme_mono;  /* current color scheme */
static int colorscheme_current_idx = 0;		    /* current index */


/* this sets a new scheme.  It constrains the input value to something valid */
void appearance_set_color_scheme( int index )
{
	if( index < 0 ) index = 0;
	if( index > colorscheme_max) index = colorscheme_max;
	colorscheme_current_idx = index;
	colorscheme_current = schemes[ colorscheme_current_idx ];
}

/* this gets the current color scheme value */
int appearance_get_color_scheme( void )
{
    	return( colorscheme_current_idx );
}

/* this gets a color from the current color scheme */
GR_COLOR appearance_get_color( int index )
{
	if( index < 0 ) index = 0;
	if( index > CS_MAX ) index = CS_MAX;

	return( colorscheme_current[index] );
}

/* this sets up the tables and such.  Eventually this might load in files */
void appearance_init( void )
{
	int reqscheme = ipod_get_setting( COLORSCHEME );

	if( screen_info.bpp < 16 ) {
	   	colorscheme_max = CS_MONO_LAST;
	}

	if( reqscheme > colorscheme_max ) reqscheme = colorscheme_max;
	appearance_set_color_scheme( reqscheme );

	printf( "Using color scheme \"%s\"\n",
			colorscheme_names[ colorscheme_current_idx ] );
}
