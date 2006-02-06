/*
 * vstars
 *
 *   contains all of the starfield stuff
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

#ifndef __VORTEX_STARS_H__
#define __VORTEX_STARS_H__

#include "ttk.h"
#include "vglobals.h"

#define STAR_MOTION_STATIC	(0)
#define STAR_MOTION_RANDOM	(1)

typedef struct vstar {
	double x;	/* horizontal */
	double y;	/* vertical */

	double xv;	/* x velocity */
	double yv;	/* x velocity */
	double xa;	/* x acceleration */
	double ya;	/* y acceleration */

	int s;		/* size */
	ttk_color c;	/* color.  duh. */
} vstar;

void Star_GenerateStars( void );	/* will generate a new starfield */
void Star_DrawStars( ttk_surface srf );	/* will draw the stars to srf */
void Star_Poll( void );			/* update the starfield */
void Star_SetStyle( int kind );		/* set the kind of star motion */

PzWindow *StarsDemo_NewWindow( void );	/* for the demo */


#endif
