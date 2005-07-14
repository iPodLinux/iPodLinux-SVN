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
 * Revision 1.8  2005/07/13 03:27:20  yorgle
 * Various color scheme tweaks
 *
 * Revision 1.7  2005/07/12 05:27:02  yorgle
 * Decorations added:  Plain (default), Amiga 1.1, Amiga 1.3, m:robe
 * Moved the lock/hold widget to the left by a touch to center it better
 * Added another color to the color scheme system: CS_TITLEACC  title accent color
 *
 * Revision 1.6  2005/07/12 03:51:38  yorgle
 * Added the m:robe color scheme by Stuart Clark (Decipher)
 * Slight tweak to the Amiga 1.x scheme
 * Always sets the number of color schemes in the menu, rather than just for mono
 *
 * Revision 1.5  2005/07/11 00:18:10  yorgle
 * Added the "gameboy" pea-green color scheme (although it's not very accurate)
 * Tweak in amiga 1.x color scheme to make the battery meter look better
 *
 * Revision 1.4  2005/07/10 23:18:28  yorgle
 * Tweaked the Amiga2 color scheme to be more readable (black-on-blue for title)
 *
 * Revision 1.3  2005/07/10 22:58:49  yorgle
 * Added in more color schemes: Monocrhome-inverted, Amigados 1, amigados 2
 * Added a hook to menu.c to limit the choices on monochrome ipods
 * Added patches to mlist to draw menu items in color
 * - needs a full screen redraw when color schemes change, or a forced-restart or somesuch.
 *
 * Revision 1.2  2005/07/10 01:36:28  yorgle
 * Added color scheming to message and pz_error
 *
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
#include "mlist.h"	/* for the menu twiddling */


/******************************************************************************
** color scheme stuff
*/

/* same as the old style */
static GR_COLOR colorscheme_mono[] = {
	GR_RGB( 255, 255, 255 ),	/* bg */
	GR_RGB(   0,   0,   0 ),	/* fg */

	GR_RGB(   0,   0,   0 ),	/* sel bg */
	DKGRAY,				/* sel bg border */
	GR_RGB( 255, 255, 255 ),	/* sel fg */

	WHITE, GRAY, DKGRAY, BLACK,	/* arrow animation */
	GR_RGB( 255, 255, 255 ),	/* title bg */
	GR_RGB(   0,   0,   0 ),	/* title fg */
	GRAY,				/* title accent */
	BLACK,				/* title line */

	BLACK, WHITE, GRAY,		/* scrollbar */
	BLACK, WHITE, GRAY,		/* slider */
	BLACK, WHITE, DKGRAY, BLACK, DKGRAY,	/* battery */
	BLACK, BLACK,			/* lock */
	BLACK, GRAY, WHITE, WHITE,	/* message window */
};

/* inverted of the above */
static GR_COLOR colorscheme_monoinv[] = {
	BLACK, WHITE,
	WHITE, GRAY, BLACK,
	BLACK, DKGRAY, GRAY, WHITE,
	BLACK, WHITE, GRAY, WHITE,

	WHITE, BLACK, GRAY,
	WHITE, BLACK, GRAY,
	WHITE, BLACK, WHITE, WHITE, WHITE,
	WHITE, WHITE,
	WHITE, DKGRAY, BLACK, BLACK
};

/* the above monochrome, but with classic gameboy pea-green hues */
#define GB_WHITE	GR_RGB( 168, 168, 125)
#define GB_GRAY		GR_RGB( 125, 125,  82)
#define GB_DKGRAY	GR_RGB(  82,  82,  38)
#define GB_BLACK	GR_RGB(   0,   0,   0)

static GR_COLOR colorscheme_gameboy[] = {
	GB_WHITE, GB_BLACK,
	GB_BLACK, GB_DKGRAY, GB_WHITE,
	GB_WHITE, GB_GRAY, GB_DKGRAY, GB_BLACK,
	GB_WHITE, GB_BLACK, GB_GRAY, GB_BLACK,
	
	GB_BLACK, GB_WHITE, GB_DKGRAY,
	GB_BLACK, GB_WHITE, GB_DKGRAY,
	GB_BLACK, GB_WHITE, GB_DKGRAY, GB_BLACK, GB_GRAY,
	GB_BLACK, GB_BLACK,
	GB_BLACK, GB_GRAY, GB_WHITE, GB_WHITE,
};

/* a basic blueish scheme.  it looks ok, but nothing to write home about */
static GR_COLOR colorscheme_cyans[] = {
	/* menu colors */
	GR_RGB( 184, 241, 255 ),	/* light blue */
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
	    GR_RGB(   0, 192,   0 ),	   /* green */
	    GR_RGB( 255, 165,   0 ),	   /* orange */
	    GR_RGB(   0,   0, 255 ),       /* blue */
	GR_RGB( 139,  0,   0 ),		/* lock */
	    GR_RGB( 255,   0,   0 ),
	BLACK, GREEN,			/* messages */
	    GR_RGB( 200, 232, 253 ),	   /* lt blue */
	    GR_RGB( 255, 255,   0 ),	   /* yellow */
};

#define A1_BLUE	  GR_RGB(   0,  85, 170 )
#define A1_BLACK  GR_RGB(   0,   0,  34 )
#define A1_WHITE  GR_RGB( 255, 255, 255 )
#define A1_ORANGE GR_RGB( 255, 136,   0 )

static GR_COLOR colorscheme_amiga1[] = {
	A1_BLUE, A1_WHITE,		/* menu bg/fg */
	A1_WHITE, A1_BLUE, A1_ORANGE,	/* selected items */
	A1_ORANGE, A1_BLACK, A1_ORANGE, A1_BLACK,	/* anim */
	A1_WHITE, A1_BLACK, A1_BLUE, A1_ORANGE,	/* titlebar */
	A1_WHITE, A1_BLACK, A1_ORANGE,	/* scrollbar */
	A1_WHITE, A1_BLACK, A1_ORANGE,	/* slider */
	A1_BLACK, A1_WHITE, A1_BLUE, A1_ORANGE, A1_BLUE, 	/* battery */
	A1_ORANGE, A1_ORANGE,		/* hold */
	A1_BLACK, A1_BLUE, A1_WHITE, A1_ORANGE,	/* error/warning */
};

#define A2_GRAY  GR_RGB( 170, 170, 170 )
#define A2_WHITE GR_RGB( 255, 255, 255 )
#define A2_BLACK GR_RGB(   0,   0,   0 )
#define A2_BLUE  GR_RGB( 131, 172, 214 )

static GR_COLOR colorscheme_amiga2[] = {
	A2_GRAY, A2_BLACK,		/* menu items */
	A2_BLACK, A2_GRAY, A2_BLUE,	/* selected items */
	A2_BLACK, A2_WHITE, A2_WHITE, A2_BLUE, 	/* anim */
	A2_BLUE, A2_BLACK, A2_BLACK, A2_BLACK,	/* title */
	A2_BLUE, A2_BLACK, A2_WHITE,	/* scrollbar */
	A2_WHITE, A2_BLACK, A2_BLUE,	/* slider */
	A2_WHITE, A2_BLACK, A2_BLUE, A2_BLUE, A2_BLUE,	/* battery */
	A2_WHITE, A2_WHITE,		/* hold */
	A2_BLACK, A2_BLUE, A2_WHITE, A2_BLUE,	/* error */
};


/* m:robe scheme by Stuart Clark (Decipher) */
#define MR_RED  GR_RGB( 255, 0, 0 )
static GR_COLOR colorscheme_mrobe[] = {
	BLACK, MR_RED,				/* menu items */
	MR_RED, MR_RED, BLACK,			/* selected items */
	BLACK, BLACK, BLACK, BLACK,		/* anim */
	BLACK, MR_RED, MR_RED, MR_RED,		/* title */
	MR_RED, BLACK, MR_RED,			/* scrollbar */
	MR_RED, BLACK, MR_RED,			/* slider */
	MR_RED, BLACK, MR_RED, MR_RED, MR_RED,	/* battery */
	MR_RED, MR_RED,				/* hold */
	MR_RED, MR_RED, BLACK, BLACK		/* error */
};


int colorscheme_max = CS_NSCHEMES;

static GR_COLOR * schemes[] = {
	colorscheme_mono,
	colorscheme_monoinv,
	colorscheme_gameboy,
	colorscheme_cyans,
	colorscheme_amiga1,
	colorscheme_amiga2,
	colorscheme_mrobe
};

/* these are separate, since they are also used in the menu system. 
 *  these need to be in sync with the above schemes[] table. 
 */
char * colorscheme_names[] = {
	"Mono",
	"Mono Inv",
	"Gameboy",
	"Cyan",
	"Amiga 1.x",
	"Amiga 2.x",
	"m:robe"
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


/******************************************************************************
** Decoration stuff
*/

char * appearance_decorations[] = { "Plain",
		"Amiga 1.1", "Amiga 1.3",
		"m:robe" };

static int decoration_current_idx = 0;

void appearance_set_decorations( int index )
{
	if( index < 0 ) index = 0;
	if( index >= NDECORATIONS ) index = 0;
	decoration_current_idx = index;
}

int appearance_get_decorations( void )
{
	return( decoration_current_idx );
}

/* rendering code is over in pz.c for now. */


/******************************************************************************
** General stuff
*/

/* for tweaking the number of schemes - limiting for monochrome ipods */
extern void menu_adjust_nschemes( int val );

/* this sets up the tables and such.  Eventually this might load in files */
void appearance_init( void )
{
	int reqscheme = ipod_get_setting( COLORSCHEME );

	if( screen_info.bpp < 16 ) {
	   	colorscheme_max = CS_MONO_LAST;
	}

	if( reqscheme > colorscheme_max ) reqscheme = 0;
	appearance_set_color_scheme( reqscheme );
	appearance_set_decorations( ipod_get_setting( DECORATIONS ));

	/* and now some magic twiddling to tweak the menus... */
	menu_adjust_nschemes( colorscheme_max+1 );
}
