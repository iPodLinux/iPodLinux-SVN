/*
 * globals
 *
 *   contains all of the globals for vortex
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

#ifndef __VORTEX_GLOBALS_H__
#define __VORTEX_GLOBALS_H__

#define VORTEX_STATE_UNDEFINED	(-1)	/* nothing. */
#define VORTEX_STATE_STARTUP	(0)	/* intro animation */
#define VORTEX_STATE_STYLESEL	(1)	/* user selecting 2k5 or classic */
#define VORTEX_STATE_LEVELSEL	(2)	/* user selecting a stage */
#define VORTEX_STATE_GAME	(3)	/* user playing a stage */
#define VORTEX_STATE_DEATH	(4)	/* user is dying. */
#define VORTEX_STATE_DEAD	(5)	/* user has died.  bummer */


/* colors for rendering */
typedef struct _vortex_colors {
	ttk_color bg;		// background
	ttk_color title;	// VORTEX
	ttk_color select;	// SELECT START LEVEL
	ttk_color sellevel;	// selected level
	ttk_color credits;	// THANKS TO...
	ttk_color version;	// version #..
	ttk_color con;		// console text 
	ttk_color bonus;	// console bonus text
	ttk_color web_top;	// closest portion of web
	ttk_color web_mid;	// arms of the web
	ttk_color web_bot;	// furthest portion of the web
	ttk_color web_top_sel;	// current PC position
	ttk_color web_top_dot;	// top vector simulation dots
	ttk_color web_bot_dot;	// bottom vector simulation dots
	ttk_color web_fill;	// fill color for the web sides 
	ttk_color baseind;	// base icon indicators (lives left)
	ttk_color baseindo;	// base icon indicators (outline)
	ttk_color score;	// current score
	ttk_color level;	// current level
	ttk_color player;	// player character
	ttk_color player_fill;	// player character (fill color)
	ttk_color bolts;	// player's bolts
	ttk_color plaser;	// player's particle laser bolts
	ttk_color super;	// superzapper color
	ttk_color flippers;	// enemy characters
	ttk_color edeath;	// enemy death
} vortex_colors;


#define NUM_SEGMENTS	(16)	/* number of panels per web */
#define NUM_Z_POINTS	(32)	/* 0=center, 32=top */
#define NUM_MIDS	(2)	/* 0=side, 1=center */

typedef struct vortex_globals {
	/* pz meta stuff */
	PzModule * module;
	PzWindow * window;
	PzWidget * widget;

	/* gameplay stuff */
	int state;
	int level;
	int startLevel;
	int currentLevel;
	int timer;

	int hasParticleLaser;
	int hasSuperZapper;

	int score;
	int lives;

	int wPosMajor;
	int wPosMinor;

	/* center of the current panel */
	int pcxC, pcyC; /* this will become vglob.ptsX[current][32][1] */

	/* center of the web on the screen, adjusted */
	int wxC, wyC;

	/* intermediary points on the current web */
	int   ptsX[NUM_SEGMENTS+1][NUM_Z_POINTS][NUM_MIDS];
	int   ptsY[NUM_SEGMENTS+1][NUM_Z_POINTS][NUM_MIDS];

	/* player ship outline */
	short px[5];
	short py[5];

	vortex_colors color;

	int classicMode;
} vortex_globals;

/* so that we can all use it... */
extern vortex_globals vglob;

/* now some useful generic and utility stuff... */

#define MIN(A,B) (((A)>(B))?(B):(A))
#define MAX(A,B) (((A)<(B))?(B):(A))

int Vortex_Rand( int max );

#endif
