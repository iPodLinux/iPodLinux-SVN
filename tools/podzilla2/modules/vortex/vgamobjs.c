/*
 * vgameobjs
 *
 *   contains all of the vortex game objects and rendering
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
  All of these each follow the same sort of overall design:
	- there is an array that contains VGO_XXXX_NUM elements of a struct
	- each struct has an 'active' variable; state/enum or int.
	- the add() routine rolls through the array, looking for an open slot
	  - if there's an open slot, it initializes and returns
	  - if there's no open slot, it just returns without doing anything
	- the draw() routine draws the entire array appropriately to the surface
	- the clear() routine wipes all elements of the struct to inactive state
	- the poll() routine gets called from a timer
	  - it handles moving items along their trajectories
	  - it handles all of the state changes for that kind of object
*/

#include "console.h"
#include "levels.h"
#include "vglobals.h"
#include "vgamobjs.h"


#define VGO_ENEMIES_NUM 	(16)	/* max number of enemies */
#define VGO_BOLTS_NUM		(16)	/* max number of shots fired */
#define VGO_POWERUPS_NUM	(20)	/* max number of powerups */

static enemy enemies[ VGO_ENEMIES_NUM ];
static bolt bolts[ VGO_BOLTS_NUM ];
static powerup powerups[ VGO_POWERUPS_NUM ];

/* ********************************************************************** */

void Vortex_CollisionDetection( void )
{
	int b,d,e,p;
	int r;
	char * buf;

	/* check for bolt-enemy collisions */
	for( e=0 ; e<VGO_ENEMIES_NUM ; e++ )
	{
	    if( (enemies[e].state == VORTEX_ENS_ACTIVE)
		|| (  (enemies[e].state == VORTEX_ENS_ZOOMY)
		    &&
		      (enemies[e].type == VORTEX_ENT_SPIKER) 
		    &&
		      (enemies[e].v < 0.0)
		   ) ){

		/* check for bolts hitting enemies */
		for( b=0 ; b<VGO_BOLTS_NUM ; b++ ) {
		    if( bolts[b].active == 1 ) {
			if( bolts[b].web == enemies[e].web ) {

			    /* check for bolts hitting ships */
			    d = (int) (bolts[b].z - enemies[e].z);
			    if( d<1 ) {
				/* HIT! */
				enemies[e].state = VORTEX_ENS_DEATH;
				bolts[b].active = 0;
				vglob.score += 100;

				/* create a new powerup here */
				if( (rand() & 0x0ff) > 128 )
					Vortex_Powerup_create(
						enemies[e].web, enemies[e].z );
			    }

			    /* check for bolts hitting spikes */
			    if( bolts[b].z <= enemies[e].spikeTop ) {
				enemies[e].spikeTop--;
				if( enemies[e].spikeTop < 10 ) {
					enemies[e].state = VORTEX_ENS_DEATH;
					enemies[e].v = 0;
					enemies[e].z = 10;
				}
				bolts[b].active = 0;
				vglob.score += 5;
			    }
			}
		    }
		}
	    }
	}

	/* check for vaus - powerup collisions */
	for( p=0 ; p<VGO_POWERUPS_NUM ; p++ )
	{
		if( (powerups[p].state == VORTEX_PU_EDGE)
		    && ( vglob.wPosMajor == powerups[p].web ))
		{
			switch( powerups[p].type ){
			case( VORTEX_PU_PART ):
				vglob.score += 42;
				vglob.hasParticleLaser = 1;
				Vortex_Console_AddItemAt(
					"Particle Laser!", 0, 0,
					vglob.wxC, vglob.wyC,
					VORTEX_STYLE_NORMAL,
					vglob.color.bonus );
				break;

			case( VORTEX_PU_2000 ):
				vglob.score += 2013;
				r = rand() & 0x0ff;

				if( r > 0xc0 )		buf = "ZAPPO 2000!";
				else if ( r > 0x80 )	buf = "AWESOME!";
				else if ( r > 0x40 )	buf = "EXCELLENT!";
				else			buf = "+2000!";
				Vortex_Console_AddItemAt(
					buf, 0, 0,
					vglob.wxC, vglob.wyC,
					VORTEX_STYLE_NORMAL,
					vglob.color.bonus );
				break;

			case( VORTEX_PU_AF ):
				vglob.score += 333;
				vglob.hasAutoFire = 1;
				Vortex_Console_AddItemAt(
					"AUTO FIRE", 0, 0,
					vglob.wxC, vglob.wyC,
					VORTEX_STYLE_NORMAL,
					vglob.color.bonus );
				break;

			case( VORTEX_PU_BUD ):
				vglob.score += 333;
				Vortex_Console_AddItemAt(
					"LITTLE BUDDY!", 0, 0,
					vglob.wxC, vglob.wyC,
					VORTEX_STYLE_NORMAL,
					vglob.color.bonus );
				break;

			case( VORTEX_PU_LIFE ):
				vglob.lives++;
				vglob.score += 151;
				Vortex_Console_AddItemAt(
					"1UP!", 0, 0,
					vglob.wxC, vglob.wyC,
					VORTEX_STYLE_NORMAL,
					vglob.color.bonus );
				break;

			case( VORTEX_PU_OUTTA ):
				Vortex_Console_AddItemAt(
					"OUTTA HERE!", 0, 0,
					vglob.wxC, vglob.wyC,
					VORTEX_STYLE_NORMAL,
					vglob.color.bonus );
				break;
			}

			powerups[p].state = VORTEX_PU_INACTIVE;
		}
	}
}


/* ********************************************************************** */




void Vortex_Bolt_draw( ttk_surface srf )
{
        int b,w,z;
	ttk_color c = 0;

	for( b=0 ; b<VGO_BOLTS_NUM ; b++ )
	{
		if( bolts[b].active == 1 )
		{
			w = bolts[b].web;
			z = (int) bolts[b].z;

			if( bolts[b].type == VORTEX_BOLT_PARTICLE )
			    	c = vglob.color.plaser;
			else if( bolts[b].type == VORTEX_BOLT_SUPERZAPPER )
				c = (vglob.timer&1)? vglob.color.bg :
				    (vglob.timer&2)? vglob.color.super 
						   : vglob.color.plaser;
			else if( bolts[b].type == VORTEX_BOLT_FRIENDLY )
				c = vglob.color.bolts;
		
			if( bolts[b].type == VORTEX_BOLT_PARTICLE )
			{
				/* angle 1 */
				ttk_line( srf,  
					vglob.ptsX[w][z][0],
					vglob.ptsY[w][z][0],
					vglob.ptsX[w][z-2][1],
					vglob.ptsY[w][z-2][1],
					c );

				/* angle 2 */
				ttk_line( srf,  
					vglob.ptsX[w][z-2][1],
					vglob.ptsY[w][z-2][1],
					vglob.ptsX[w+1][z][0],
					vglob.ptsY[w+1][z][0],
					c );

				/* crossbar */
				ttk_line( srf,  
					vglob.ptsX[w+1][z][0],
					vglob.ptsY[w+1][z][0],
					vglob.ptsX[w][z][0],
					vglob.ptsY[w][z][0],
					c );
			} else if (bolts[b].type == VORTEX_BOLT_SUPERZAPPER ) {
				ttk_line( srf,  
					vglob.ptsX[w+1][z][0],
					vglob.ptsY[w+1][z][0],
					vglob.ptsX[w][z][0],
					vglob.ptsY[w][z][0],
					c );
			} else {
				/* vert line */
				ttk_line( srf,  
					vglob.ptsX[w][z][1],
					vglob.ptsY[w][z][1],
					vglob.ptsX[w][z-2][1],
					vglob.ptsY[w][z-2][1],
					c );
			}
		}
	}
}

void Vortex_Bolt_poll( void )
{
	int b;
	for( b=0 ; b<VGO_BOLTS_NUM ; b++ )
	{
		if( bolts[b].active == 1 )
		{
			bolts[b].z -= bolts[b].v;

			if( bolts[b].z < 9.0 )
				bolts[b].active = 0;
		}
	}
}

void Vortex_Bolt_Superzapper( void ) 
{
    	int b;
	if( !vglob.hasSuperZapper ) return;
	vglob.hasSuperZapper = 0;

	for( b=0 ; b<VGO_BOLTS_NUM ; b++ ) {
		bolts[b].active = 1;
		bolts[b].web = b&0x0f;
		bolts[b].z = NUM_Z_POINTS-2;
		bolts[b].v = 0.5;
		bolts[b].type = VORTEX_BOLT_SUPERZAPPER;
	}
	Vortex_Console_AddItemAt(
		"GO BOOM!", 0, 0,
		vglob.wxC, vglob.wyC,
		VORTEX_STYLE_NORMAL,
		vglob.color.bonus );
}


void Vortex_Bolt_add( void )
{
	int b;
	int max = VGO_BOLTS_NUM;

	if( vglob.hasAutoFire ) max = 6;

	for( b=0 ; b<max ; b++ )
	{
		if( bolts[b].active == 0 )
		{
			bolts[b].active = 1;
			bolts[b].web    = vglob.wPosMajor;
			bolts[b].z 	= NUM_Z_POINTS-2;

			if( vglob.hasParticleLaser == 1 ) {
				bolts[b].v    = 1.7; /* particles go quicker */
				bolts[b].type = VORTEX_BOLT_PARTICLE;
			} else {
				bolts[b].v    = 1;
				bolts[b].type = VORTEX_BOLT_FRIENDLY;
			}
			return;
		}
	}
}

void Vortex_Bolt_clear( void )
{
	int b;
	for( b=0 ; b<VGO_BOLTS_NUM ; b++ )
		bolts[b].active = 0;
}



/* ********************************************************************** */
/* Enemy logic and stuff */


void Vortex_Enemy_draw( ttk_surface srf )
{
	int e,w,z;
	for( e=0 ; e<VGO_ENEMIES_NUM ; e++ ){
		w = enemies[e].web;
		z = (int) enemies[e].z;

		switch( enemies[e].state ) {
		case( VORTEX_ENS_DEATH ):
				ttk_line( srf,  vglob.ptsX[w  ][z  ][0],
						vglob.ptsY[w  ][z  ][0],
						vglob.ptsX[w+1][z+3][0],
						vglob.ptsY[w+1][z+3][0],
						vglob.color.edeath );
				ttk_line( srf,  vglob.ptsX[w  ][z+3][0],
						vglob.ptsY[w  ][z+3][0],
						vglob.ptsX[w+1][z  ][0],
						vglob.ptsY[w+1][z  ][0],
						vglob.color.edeath );

				ttk_ellipse( srf, vglob.ptsX[w  ][z][1],
						  vglob.ptsY[w  ][z][1],
						  10, 10, vglob.color.edeath );
				ttk_ellipse( srf, vglob.ptsX[w  ][z][1],
						  vglob.ptsY[w  ][z][1],
						  5, 5, vglob.color.edeath );
			break;

		case( VORTEX_ENS_ZOOMY ):
		case( VORTEX_ENS_ACTIVE ):
			switch( enemies[e].type ){
			case( VORTEX_ENT_SPIKER ):
				/* first draw the depth line */
				if( enemies[e].spikeTop > 9 ) {
				    ttk_aaline( srf, 
					vglob.ptsX[w][9][1],
					vglob.ptsY[w][9][1],
					vglob.ptsX[w][enemies[e].spikeTop][1],
					vglob.ptsY[w][enemies[e].spikeTop][1],
					vglob.color.spikes );
				}

				/* next draw the spiker */
				if( enemies[e].z > 0 ) {
				    ttk_ellipse( srf, vglob.ptsX[w][z][1],
					    vglob.ptsY[w][z][1],
					    2, 2, vglob.color.spikers );
				    ttk_ellipse( srf, vglob.ptsX[w][z][1],
					    vglob.ptsY[w][z][1],
					    3, 3, vglob.color.spikers );
				    ttk_ellipse( srf, vglob.ptsX[w][z][1],
					    vglob.ptsY[w][z][1],
					    4, 4, vglob.color.spikers );
				}
				break;

			case( VORTEX_ENT_FLIPPER ):
				/* these look better non-antialiased */
				/* aliased? */
				ttk_line( srf,  vglob.ptsX[w  ][z  ][0],
						vglob.ptsY[w  ][z  ][0],
						vglob.ptsX[w+1][z+3][0],
						vglob.ptsY[w+1][z+3][0],
						vglob.color.flippers );
				ttk_line( srf,  vglob.ptsX[w  ][z+3][0],
						vglob.ptsY[w  ][z+3][0],
						vglob.ptsX[w+1][z  ][0],
						vglob.ptsY[w+1][z  ][0],
						vglob.color.flippers );

				ttk_line( srf,  vglob.ptsX[w  ][z+3][0],
						vglob.ptsY[w  ][z+3][0],
						vglob.ptsX[w  ][z  ][0],
						vglob.ptsY[w  ][z  ][0],
						vglob.color.flippers );
				ttk_line( srf,  vglob.ptsX[w+1][z+3][0],
						vglob.ptsY[w+1][z+3][0],
						vglob.ptsX[w+1][z  ][0],
						vglob.ptsY[w+1][z  ][0],
						vglob.color.flippers );
			}
			break;

		default:
			break;
		}
	}
}

void Vortex_Enemy_add( int type )
{
	int e;
	for( e=0 ; e<VGO_ENEMIES_NUM ; e++ ){
		if( enemies[e].state == VORTEX_ENS_INACTIVE ) {

			enemies[e].state      = VORTEX_ENS_ZOOMY;
			enemies[e].type       = type;
			enemies[e].web	      = rand() & 0x0f;
			enemies[e].v          = 0.25;
			enemies[e].z          = 0;
			enemies[e].timeToFlip = 20;
			enemies[e].timeToFire = 10;
			enemies[e].spikeTop   = 0;
			enemies[e].finalTop   = 8 + Vortex_Rand( 22 );

			/* for now. hack it */
			if( !(vortex_levels[ enemies[e].web ].flags & LF_CLOSEDWEB) )
				enemies[e].web = Vortex_Rand( 13 );
			return;
		}
	}
}

static int enemy_flipper_countup;

void Vortex_Enemy_clear( void )
{
	int e;
	for( e=0 ; e<VGO_ENEMIES_NUM ; e++ ){
		enemies[e].state = VORTEX_ENS_INACTIVE;
	}

	enemy_flipper_countup = -99;
}


void Vortex_Enemy_poll( void )
{
	int e;
	static int t = 0;
	static int timings_flipper[16] = { 70, 65, 60, 55, 50, 47, 45,
			 		   40, 37, 33, 30, 28, 26, 24 };
        int idx = vglob.currentLevel;
	if( idx > 15 ) idx = 15;
    
	/* trigger a new enemy if necessary */
	enemy_flipper_countup++;
	if( (enemy_flipper_countup >= timings_flipper[idx]-20)
	    || (enemy_flipper_countup<0) )
	{
		t ^= 1;		/* for now, every other one is each */
		if( t )
			Vortex_Enemy_add( VORTEX_ENT_FLIPPER );
		else
			Vortex_Enemy_add( VORTEX_ENT_SPIKER );
		if( t>=4 ) t=0;
		enemy_flipper_countup = 0;
	}


	/* check the state stuff */
	for( e=0 ; e<VGO_ENEMIES_NUM ; e++ )
	{
		switch( enemies[e].state ) {
		case( VORTEX_ENS_INACTIVE ):
			break;

		case( VORTEX_ENS_ZOOMY ):
			enemies[e].z += enemies[e].v;
			if( enemies[e].z > 5 ) {
				enemies[e].state = VORTEX_ENS_ACTIVE;
			}
			break;

		case( VORTEX_ENS_ACTIVE ):
			enemies[e].z += enemies[e].v;

			switch( enemies[e].type ){
			case( VORTEX_ENT_SPIKER ):
				/* check for movement */
				if( enemies[e].z > enemies[e].finalTop )
				{
					/* bounce it back! */
					enemies[e].v *= -1;
					enemies[e].z += enemies[e].v;
				}
				if( enemies[e].z > enemies[e].spikeTop )
					enemies[e].spikeTop = (int)enemies[e].z;
				break;

			case( VORTEX_ENT_FLIPPER ):
				if( enemies[e].z > (NUM_Z_POINTS - 6)) {
					enemies[e].state = VORTEX_ENS_INACTIVE;

					/* enemy made it out to edge! */

					/* for now... */
					vglob.score -= 75;
					if( vglob.score < 0 )
					{
						vglob.score = 0;
						Vortex_Enemy_clear();
						vglob.lives--;
						if( vglob.lives <= 0 ){
							vglob.lives = 0;
						}
						Vortex_Console_AddItemAt(
							"OUCH", 0, 0,
							vglob.wxC, vglob.wyC,
							VORTEX_STYLE_NORMAL,
							vglob.color.bonus );
					}
				}
				break;
			}
			break;

		case( VORTEX_ENS_DEATH ):
			enemies[e].state = VORTEX_ENS_INACTIVE;
			break;
		}
	}
}

/* ********************************************************************** */

void Vortex_Powerup_draw( ttk_surface srf )
{
	int p,w,z,d;

	for( p=0 ; p<VGO_POWERUPS_NUM ; p++ )
	{
		w = powerups[p].web;
		z = (int) powerups[p].z;

		
		if( (z&1) && (powerups[p].state != VORTEX_PU_INACTIVE) ) {
			ttk_aaline( srf,  
					vglob.ptsX[w][powerups[p].zStart][0],
					vglob.ptsY[w][powerups[p].zStart][0],
					vglob.ptsX[w][z][1],
					vglob.ptsY[w][z][1],
					vglob.color.powerup );
			ttk_aaline( srf,  
					vglob.ptsX[w][z][1],
					vglob.ptsY[w][z][1],
					vglob.ptsX[w+1][powerups[p].zStart][0],
					vglob.ptsY[w+1][powerups[p].zStart][0],
					vglob.color.powerup );
			
			for( d=3; d<15 ; d+=5 ) {
				ttk_ellipse( srf, vglob.ptsX[w  ][z][1],
					vglob.ptsY[w  ][z][1],
					d, d, vglob.color.powerup );
			}
		}
	}
}

static int powerup_counter = 0;

void Vortex_Powerup_add( int web, int z, int type )
{
	int p;

	if( powerup_counter == 0 ) {
		Vortex_Console_AddItemAt(
			"COLLECT POWERUPS", 0, 0,
			vglob.wxC, vglob.wyC,
			VORTEX_STYLE_BOLD,
			vglob.color.con );
	}
	powerup_counter++;

	for( p=0 ; p<VGO_POWERUPS_NUM ; p++ )
	{
		if( powerups[p].state == VORTEX_PU_INACTIVE ){
			powerups[p].type     = type;
			powerups[p].state    = VORTEX_PU_ZOOM;
			powerups[p].web	     = web;
			powerups[p].zStart   = z;
			powerups[p].z        = (double) z;
			powerups[p].v	     = 0.5;
			powerups[p].edgetime = 10;
			return;
		}
	}
}


void Vortex_Powerup_create( int web, int z )
{
	/* this needs *EXTENSIVE* work */
	if( (powerup_counter & 0x01) == 0 ) {
		Vortex_Powerup_add( web, z, VORTEX_PU_2000 );
	} else if( vglob.hasParticleLaser == 0 ) {
		Vortex_Powerup_add( web, z, VORTEX_PU_PART );
	} else if( vglob.hasAutoFire == 0 ) {
		Vortex_Powerup_add( web, z, VORTEX_PU_AF );
	} else 
		Vortex_Powerup_add( web, z, VORTEX_PU_2000 );
}

void Vortex_Powerup_clear( void )
{
	int p;
	for( p=0 ; p<VGO_POWERUPS_NUM ; p++ )
		powerups[p].state = VORTEX_PU_INACTIVE;
	powerup_counter = 0;
}

void Vortex_Powerup_poll( void )
{
	int p;
	for( p=0 ; p<VGO_POWERUPS_NUM ; p++ )
	{
		if( powerups[p].state == VORTEX_PU_ZOOM )
		{
			powerups[p].z += powerups[p].v;

			if( powerups[p].z >= (NUM_Z_POINTS-1) ) {
				/* powerup made it out to the edge */
				powerups[p].state = VORTEX_PU_EDGE;
				powerups[p].z = NUM_Z_POINTS-1;
			}
		} else if( powerups[p].state == VORTEX_PU_EDGE ){
			/* powerup hangs out at the edge for a moment */
			powerups[p].edgetime--;
			if( powerups[p].edgetime <= 0 )
				powerups[p].state = VORTEX_PU_INACTIVE;
		}
	}
}


