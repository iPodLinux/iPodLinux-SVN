/*
 * BlueCube - just another tetris clone 
 * Copyright (C) 2003  Sebastian Falbesoner
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
 */

/* File: pieces.c === Contains only data (the pieces and the colortable for them) */

#include "global.h"
#include "pieces.h"

/******************************/
/* Color table for the pieces */
/******************************/
int StyleColors[][3] = {
	{128,128,255},
	{255,255,0  },
	{255,128,128},
	{64 ,255,0  },
	{0  ,255,255},
	{255,255,255},
	{16 ,255,128}
};

/********************************************/
/* All the pieces with their rotation forms */
/********************************************/
int Pieces[][4][CLUSTER_X][CLUSTER_Y] = {
{
	{
		{1,1,0,0}, /* o o */
		{1,1,0,0}, /* o o */
		{0,0,0,0}, /*     */
		{0,0,0,0}  /*     */
	},
	{
		{1,1,0,0},
		{1,1,0,0},
		{0,0,0,0},
		{0,0,0,0}
	},
	{
		{1,1,0,0},
		{1,1,0,0},
		{0,0,0,0},
		{0,0,0,0}
	},
	{
		{1,1,0,0},
		{1,1,0,0},
		{0,0,0,0},
		{0,0,0,0}
	}
},



{
	{
		{0,0,2,0}, /*   o o */
		{0,2,2,0}, /* o o   */
		{0,2,0,0}, /*       */
		{0,0,0,0}  /*       */
	},
	{
		{2,2,0,0},
		{0,2,2,0},
		{0,0,0,0},
		{0,0,0,0}
	},
	{
		{0,0,2,0},
		{0,2,2,0},
		{0,2,0,0},
		{0,0,0,0}
	},
	{
		{2,2,0,0},
		{0,2,2,0},
		{0,0,0,0},
		{0,0,0,0}
	}
},



{
	{
		{0,3,0,0}, /* o o   */
		{0,3,3,0}, /*   o o */
		{0,0,3,0}, /*       */
		{0,0,0,0}  /*       */
	},
	{
		{0,3,3,0},
		{3,3,0,0},
		{0,0,0,0},
		{0,0,0,0}
	},
	{
		{0,3,0,0},
		{0,3,3,0},
		{0,0,3,0},
		{0,0,0,0}
	},
	{
		{0,3,3,0},
		{3,3,0,0},
		{0,0,0,0},
		{0,0,0,0}
	}
},



{
	{
		{0,4,0,0}, /* o o o */
		{0,4,4,0}, /*   o   */
		{0,4,0,0}, /*       */
		{0,0,0,0}  /*       */
	},
	{
		{0,4,0,0},
		{4,4,4,0},
		{0,0,0,0},
		{0,0,0,0}
	},
	{
		{0,4,0,0},
		{4,4,0,0},
		{0,4,0,0},
		{0,0,0,0}
	},
	{
		{0,0,0,0},
		{4,4,4,0},
		{0,4,0,0},
		{0,0,0,0}
	}
},


{
	{
		{0,0,0,0}, /* o   */
		{5,5,5,0}, /* o   */
		{0,0,5,0}, /* o o */
		{0,0,0,0}  /*     */
	},
	{
		{0,5,5,0},
		{0,5,0,0},
		{0,5,0,0},
		{0,0,0,0}
	},
	{
		{5,0,0,0},
		{5,5,5,0},
		{0,0,0,0},
		{0,0,0,0}
	},
	{
		{0,5,0,0},
		{0,5,0,0},
		{5,5,0,0},
		{0,0,0,0}
	}
},


{
	{
		{0,0,6,0}, /*   o */
		{6,6,6,0}, /*   o */
		{0,0,0,0}, /* o o */
		{0,0,0,0}, /*     */
	},
	{
		{0,6,6,0},
		{0,0,6,0},
		{0,0,6,0},
		{0,0,0,0}
	},
	{
		{0,0,0,0},
		{6,6,6,0},
		{6,0,0,0},
		{0,0,0,0}
	},
	{
		{0,6,0,0},
		{0,6,0,0},
		{0,6,6,0},
		{0,0,0,0}
	}
},


{
	{
		{0,0,0,0}, /* o */
		{7,7,7,7}, /* o */
		{0,0,0,0}, /* o */
		{0,0,0,0}  /* o */
	},
	{
		{0,0,7,0},
		{0,0,7,0},
		{0,0,7,0},
		{0,0,7,0}
	},
	{
		{0,0,0,0},
		{0,0,0,0},
		{7,7,7,7},
		{0,0,0,0}
	},
	{
		{0,7,0,0},
		{0,7,0,0},
		{0,7,0,0},
		{0,7,0,0}
	}
}
};
