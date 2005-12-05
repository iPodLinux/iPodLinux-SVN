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
#define VORTEX_STATE_LEVELSEL	(1)	/* user selecting a stage */
#define VORTEX_STATE_GAME	(2)	/* user playing a stage */
#define VORTEX_STATE_DEATH	(3)	/* user is dying. */
#define VORTEX_STATE_DEAD	(4)	/* user has died.  bummer */


/* colors for rendering */
typedef struct _vortex_colors {
	ttk_color bg;		// background
	ttk_color title;	// VORTEX
	ttk_color select;	// SELECT START LEVEL
	ttk_color level;	// selected level
	ttk_color credits;	// THANKS TO...
	ttk_color con;		// console text 
	ttk_color bonus;	// console bonus text
	ttk_color web_top;	// closest portion of web
	ttk_color web_mid;	// arms of the web
	ttk_color web_bot;	// furthest portion of the web
	ttk_color baseind;	// base icon indicators (lives left)
	ttk_color score;	// current score
	ttk_color player;	// player character
	ttk_color bolts;	// player's bolts
	ttk_color super;	// superzapper color
	ttk_color flippers;	// enemy characters
	ttk_color stars;	// background starfield color
} vortex_colors;


typedef struct vortex_globals {
	/* pz meta stuff */
	PzModule * module;
	PzWindow * window;
	PzWidget * widget;

	/* gameplay stuff */
	int state;
	int level;
	int startLevel;
	int timer;

	int score;
	int lives;

	vortex_colors color;

} vortex_globals;

#endif
