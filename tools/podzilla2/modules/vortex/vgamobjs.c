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

#include "console.h"
#include "levels.h"
#include "vglobals.h"
#include "vgamobjs.h"

#define VGO_BOLTS_NUM	(3)

static bolt bolts[ VGO_BOLTS_NUM ];

void Vortex_Bolt_draw( ttk_surface srf )
{
        int b,w,z;
	ttk_color c;

	for( b=0 ; b<VGO_BOLTS_NUM ; b++ )
	{
		if( bolts[b].active == 1 )
		{
			w = bolts[b].web;
			z = (int) bolts[b].z;

			if( bolts[b].type == VORTEX_BOLT_PARTICLE )
			    c = vglob.color.plaser;
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


void Vortex_Bolt_add( void )
{
	int b;
	for( b=0 ; b<VGO_BOLTS_NUM ; b++ )
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
#define VGO_ENEMIES_NUM (16)

enemy enemies[VGO_ENEMIES_NUM];

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
			case( VORTEX_ENT_FLIPPER ):
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

void Vortex_Enemy_add( void )
{
	int e;
	for( e=0 ; e<VGO_ENEMIES_NUM ; e++ ){
		if( enemies[e].state == VORTEX_ENS_INACTIVE ) {
			enemies[e].state      = VORTEX_ENS_ZOOMY;
			enemies[e].type       = VORTEX_ENT_FLIPPER;/* for now */
			enemies[e].web	      = rand() & 0x0f;
			enemies[e].v          = 0.25;
			enemies[e].z          = 0;
			enemies[e].timeToFlip = 20;
			enemies[e].timeToFire = 10;

			/* for now. hack it */
			if( !(vortex_levels[ enemies[e].web ].flags & LF_CLOSEDWEB) )
				enemies[e].web = Vortex_Rand( 13 );
			return;
		}
	}
}

void Vortex_Enemy_clear( void )
{
	int e;
	for( e=0 ; e<VGO_ENEMIES_NUM ; e++ ){
		enemies[e].state = VORTEX_ENS_INACTIVE;
	}

}

void Vortex_Enemy_poll( void )
{
	int e;
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
			if( enemies[e].z > (NUM_Z_POINTS - 6)) {
				enemies[e].state = VORTEX_ENS_INACTIVE;

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
					Vortex_Console_AddItem( "OUCH", 0, 0,
					    VORTEX_STYLE_NORMAL,
					    vglob.color.bonus );
				} else {
				    Vortex_Console_AddItem( "-75", 0, 0,
					VORTEX_STYLE_NORMAL, vglob.color.con );
				}
			}
			break;

		case( VORTEX_ENS_DEATH ):
			enemies[e].state = VORTEX_ENS_INACTIVE;
			break;
		}
	}
}



/* ********************************************************************** */

void Vortex_CollisionDetection( void )
{
	int b,d,e;

	for( e=0 ; e<VGO_ENEMIES_NUM ; e++ )
	{
	    if( enemies[e].state == VORTEX_ENS_ACTIVE ) {
		for( b=0 ; b<VGO_BOLTS_NUM ; b++ ) {
		    if( bolts[b].active == 1 ) {
			if( bolts[b].web == enemies[e].web ) {
			    d = (int) (bolts[b].z - enemies[e].z);
			    if( d<1 ) {
				/* HIT! */
				enemies[e].state = VORTEX_ENS_DEATH;
				bolts[b].active = 0;
				vglob.score += 100;
			    }
			}
		    }
		}
	    }
	}
}
