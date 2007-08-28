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
#define VORTEX_BOLT_PARTICLE	(1)	/* player fired particle laser */
#define VORTEX_BOLT_ENEMY	(2)	/* enemy fired bolt */
#define VORTEX_BOLT_SUPERZAPPER (3)	/* player fired superzapper */

typedef struct bolt {
	int active;		/* 1 = active, 0 = free */
	int type;		/* bolt type */
	int web;		/* where it is, rotationally */
	double z;		/* where it is, depth */
	double v;		/* velocity */
} bolt;


/* enemy type */
#define VORTEX_ENT_FLIPPER	(0)	/* standard flipper */
#define VORTEX_ENT_SPIKER	(1)	/* spike generator */
#define VORTEX_ENT_FUSEBALL	(2)	/* Zzzzzt */
#define VORTEX_ENT_PULSAR	(3)	/* shock! */
#define VORTEX_ENT_TANKER	(4)	/* 2-for-1 special */

/* enemy state */
#define VORTEX_ENS_INACTIVE	(0)	/* enemy slot is inactive */
#define VORTEX_ENS_ZOOMY	(1)	/* enemy is approaching the web */
#define VORTEX_ENS_ACTIVE	(2)	/* enemy is on the web and vulnerable */
#define VORTEX_ENS_FLIPPING	(3)	/* enemy is flipping */
#define VORTEX_ENS_DEATH	(4)	/* enemy is dying */

typedef struct enemy {
	int state;		/* what life state it is in */
	int type;		/* what kind of enemy it is */
	int web;		/* where it is, rotationally */
	double z;		/* where it is, depth */
	double v;		/* velocity */
	/* for flippers */
	int timeToFlip;		/* countdown time until it flips again */
	int timeToFire;		/* countdown time until it fires again */
	/* for spikers */
	int spikeTop;		/* spike size (from back to front ) */
	int finalTop;		/* how far to go in before reversing */
} enemy;


#define VORTEX_PU_PART	(0)	/* Particle laser */
#define VORTEX_PU_2000	(1)	/* Zappo 2000 - even numbered powerups */
#define VORTEX_PU_BUD	(2)	/* little buddy */
#define VORTEX_PU_LIFE	(3)	/* extra life */
#define VORTEX_PU_OUTTA (4)	/* outta here.  5k points, + next level jump */
#define VORTEX_PU_AF	(5)	/* autofire */

#define VORTEX_PU_INACTIVE	(0)	/* powerup slot is empty */
#define VORTEX_PU_ZOOM		(1)	/* powerup is sliding up the side */
#define VORTEX_PU_EDGE		(2)	/* powerup is sitting on the edge */

typedef struct powerup {
	int type;		/* what kind of enemy it is */
	int state;		/* what state is it in? */
	int web;		/* where it is, rotationally */
	int zStart;		/* starting z position */
	double z;		/* where it is, depth */
	double v;		/* velocity */
	int edgetime;		/* how long it should wait at the edge */
} powerup;


/* gameplay interactions */
void Vortex_CollisionDetection( void );

/* bolt manipulation functions */
void Vortex_Bolt_draw( ttk_surface srf );
void Vortex_Bolt_add( void );
void Vortex_Bolt_clear( void );
void Vortex_Bolt_poll( void );
void Vortex_Bolt_Superzapper( void );

/* enemy functions */
void Vortex_Enemy_draw( ttk_surface srf );
void Vortex_Enemy_add( int type );
void Vortex_Enemy_clear( void );
void Vortex_Enemy_poll( void );

/* powerup functions */
void Vortex_Powerup_draw( ttk_surface srf );
void Vortex_Powerup_add( int web, int z, int type );
void Vortex_Powerup_create( int web, int z );
void Vortex_Powerup_clear( void );
void Vortex_Powerup_poll( void );

#endif


