/*
 * levels
 *
 *   contains all of the vector information for all of the levels.
 *
 *   01-2005 Scott Lawrence
 *
 *   Generated via some code from the Tempest, Tempest Tubes, and TempED roms
 *
 *	these are arranged strangely, but it's how the 
 *	original game stored the data.
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

#include "levels.h"

LEVELDATA vortex_levels[NLEVELS] = {
    {
	32, /* order */
	{ 199, 175, 151, 128, 104, 80, 56, 32, 
          56, 80, 103, 128, 152, 176, 199, 223 }, /* x */
	{ 176, 199, 176, 152, 176, 199, 176, 151, 
          128, 104, 80, 56, 80, 104, 128, 151 }, /* y */
	{ 6, 10, 10, 6, 6, 10, 10, 14, 
          14, 14, 14, 2, 2, 2, 2, 6 }, /* angles */
	10, /* scale */
	592, /* fscale */
	94, /* y3d */
	-238, /* y2d */
	LF_CLOSEDWEB /* flags */
    }
};
