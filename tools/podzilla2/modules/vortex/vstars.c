/*
 * vstars
 *
 *   contains all of the starfield stuff
 *
 *   for now, it just draws a static starfield, but we might do more later
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

#include "vstars.h"

vstar stars[32];

void Star_GenerateStars( void );
void Star_DrawStars( ttk_surface srf );

void Star_GenerateStars( void )
{       
        int p;
        for( p=0 ; p<32 ; p++ )
        {       
                stars[p].x = Vortex_Rand( ttk_screen->w );
                stars[p].y = Vortex_Rand( ttk_screen->h-ttk_screen->wy );
                stars[p].c = ttk_makecol( 120 + Vortex_Rand( 128 ),
					  120 + Vortex_Rand( 128 ),
					  120 + Vortex_Rand( 128 ) );
		stars[p].s = Vortex_Rand( 255 );
    }
}

void Star_DrawStars( ttk_surface srf )
{       
        int p;
        for( p=0 ; p<32 ; p++ )
        {       
                ttk_pixel( srf, stars[p].x, stars[p].y, stars[p].c );
		if( stars[p].s < 32 ) {
		    /* 1/8 chance it'll be a larger square star */
		    ttk_pixel( srf, stars[p].x+1, stars[p].y+0, stars[p].c );
		    ttk_pixel( srf, stars[p].x+1, stars[p].y+1, stars[p].c );
		    ttk_pixel( srf, stars[p].x+0, stars[p].y+1, stars[p].c );
		} else if (stars[p].s < 48)  {
		    /* 1/16 chance it'll be a large t-shaped star */
		    ttk_pixel( srf, stars[p].x+1, stars[p].y+0, stars[p].c );
		    ttk_pixel( srf, stars[p].x-1, stars[p].y-0, stars[p].c );
		    ttk_pixel( srf, stars[p].x+0, stars[p].y+1, stars[p].c );
		    ttk_pixel( srf, stars[p].x-0, stars[p].y-1, stars[p].c );
		}
        }
}
