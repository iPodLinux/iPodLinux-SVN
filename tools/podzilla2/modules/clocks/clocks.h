/*
 * Clocks 2.0 - Clock engine with expandable clock faces for podzilla 2
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

#include <stdio.h>
#include <string.h>
#include "pz.h"

/* this is the total number of registerable clock faces */
#define NCLOCK_FACES (32)

/* the clockface structure - one for each face - internal structure */
typedef struct _clockface {
        void * routine;		/* fcn pointer to a draw routine (see below) */
        char name[32];		/* not used yet */
} clockface;

/* the 'editing' int for the globals can be one of these: */
#define CLOCKS_SEL_NOEDIT       (0)     /* not editing */
#define CLOCKS_SEL_HOURS        (1)     /* editing hours */
#define CLOCKS_SEL_MINUTES      (2)     /* editing minutes */
#define CLOCKS_SEL_SECONDS      (3)     /* editing seconds */
#define CLOCKS_SEL_YEARS        (4)     /* editing year */
#define CLOCKS_SEL_MONTHS       (5)     /* editing month */
#define CLOCKS_SEL_DAYS         (6)     /* editing day */
#define CLOCKS_SEL_MAX          (6)

/* the global struct */
typedef struct _clocks_globals {
        /* pz meta stuff */
        PzModule * module;
        PzWindow * window;
        PzWidget * widget;

        clockface faces[NCLOCK_FACES];
        int nFaces;	/* number of registered faces */
	int cFace;	/* current face */
	int editing;	/* are we setting the time? */
	int timer;	/* internal timer */

	/* for drawing */
        int w, h;
        int fullscreen;
	ttk_color fg;
	ttk_color bg;

	/* for displaying the time */
	int offset;		/* for world clock offset */
	struct tm *dispTime;	/* time to display */

} clocks_globals;


/* pointer to a draw routine */
/* each function should only reference things via the globals structure above,
** which includes width, height, time to use, etc.
*/
typedef void (*draw_face)( ttk_surface srf, clocks_globals * glob );

/* use this function to register more clock faces from new modules */
void clocks_register_face( draw_face fcn, char * name );

