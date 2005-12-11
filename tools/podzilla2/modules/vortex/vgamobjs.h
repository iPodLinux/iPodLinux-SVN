/*
 * vgameobjs
 *
 *   contains all of the vortex game objects and structures
 *
 *   2005-12 Scott Lawrence
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
 *  in-game stuff based on Tempest 2000:
	http://www.classicgaming.com/shmups/reviews/tempest/
 */

#ifndef __VORTEX_GAMEOBJECTS_H_
#define __VORTEX_GAMEOBJECTS_H_

/* user (or enemy) fire */
#define VORTEX_BOLT_FRIENDLY	(0)	/* player fired bolt */
#define VORTEX_BOLT_ENEMY	(1)	/* enemy fired bolt */

typedef struct bolt {
	int type;		/* bolt type */
	int web;		/* where it is, rotationally */
	int z;			/* where it is, depth */
	int v;			/* velocity */
} bolt;


/* enemy type */
#define VORTEX_ENT_FLIPPER	(0)	/* standard flipper */
#define VORTEX_ENT_SPIKER	(1)	/* spike generator */
#define VORTEX_ENT_FUSEBALL	(2)	/* Zzzzzt */
#define VORTEX_ENT_PULSAR	(3)	/* shock! */
#define VORTEX_ENT_TANKER	(4)	/* 2-for-1 special */

/* enemy state */
#define VORTEX_ENS_ZOOMY	(0)	/* enemy is approaching the web */
#define VORTEX_ENS_ACTIVE	(1)	/* enemy is on the web and vulnerable */
#define VORTEX_ENS_DEATH	(2)	/* enemy is dying */

typedef struct enemy {
	int state;		/* what life state it is in */
	int type;		/* what kind of enemy it is */
	int web;		/* where it is, rotationally */
	int z;			/* where it is, depth */
	int v;			/* velocity */
	int timeToFlip;		/* countdown time until it flips again */
	int timeToFire;		/* countdown time until it fires again */
} enemy;


#define VORTEX_PU_PART	(0)	/* Particle laser */
#define VORTEX_PU_2000	(1)	/* Zappo 2000*/
#define VORTEX_PU_BUD	(2)	/* little buddy */
#define VORTEX_PU_LIFE	(3)	/* extra life */

typedef struct powerup {
	int type;		/* what kind of enemy it is */
	int web;		/* where it is, rotationally */
	int z;			/* where it is, depth */
	int v;			/* velocity */
} powerup;

#endif
