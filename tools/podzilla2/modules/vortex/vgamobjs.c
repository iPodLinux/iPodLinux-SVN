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
			if( vglob.hasParticleLaser )
			    c = vglob.color.plaser;
			else
			    c = vglob.color.bolts;
		
			if( vglob.hasParticleLaser )
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
			bolts[b].type   = VORTEX_BOLT_FRIENDLY;
			bolts[b].web    = vglob.wPosMajor;
			bolts[b].z 	= NUM_Z_POINTS-2;

			if( vglob.hasParticleLaser == 1 )
				bolts[b].v = 1.5; /* particles go quicker */
			else 
				bolts[b].v = 1;
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
