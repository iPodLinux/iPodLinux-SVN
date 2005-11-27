/*
 * Copyright (c) 2005 Joshua Oreman
 *
 * This file is a part of TTK.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "ttk.h"

// Speaker icon (playing song)
// . . . X . . X .
// . . X X . . . X
// X X X X . X . X
// X X X X . X . X
// X X X X . X . X
// . . X X . . . X
// . . . X . . X .

unsigned char ttk_icon_spkr[] = { 8, 13,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 3, 0, 0, 3, 0,
    0, 0, 3, 3, 0, 0, 0, 3,
    3, 3, 3, 3, 0, 3, 0, 3,
    3, 3, 3, 3, 0, 3, 0, 3,
    3, 3, 3, 3, 0, 3, 0, 3,
    0, 0, 3, 3, 0, 0, 0, 3,
    0, 0, 0, 3, 0, 0, 3, 0,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

// Play icon:
// X X . . . . . .
// X X X . . . . .
// X X X X . . . .
// X X X X X . . .
// X X X X X X . .
// X X X X X X X .
// X X X X X X X X
// X X X X X X X .
// X X X X X X . .
// X X X X X . . .
// X X X X . . . .
// X X X . . . . .
// X X . . . . . .

unsigned char ttk_icon_play[] = { 8, 13,
    3, 3, 0, 0, 0, 0, 0, 0,
    3, 3, 3, 0, 0, 0, 0, 0,
    3, 3, 3, 3, 0, 0, 0, 0,
    3, 3, 3, 3, 3, 0, 0, 0,
    3, 3, 3, 3, 3, 3, 0, 0,
    3, 3, 3, 3, 3, 3, 3, 0,
    3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 0,
    3, 3, 3, 3, 3, 3, 0, 0,
    3, 3, 3, 3, 3, 0, 0, 0,
    3, 3, 3, 3, 0, 0, 0, 0,
    3, 3, 3, 0, 0, 0, 0, 0,
    3, 3, 0, 0, 0, 0, 0, 0
};


// Executable icon:
// X X . . . . X X
// X X X . . X X X
// . X X X X X X .
// . . X X X X . .
// . . . X X . . .
// . . X X X X . .
// . X X X X X X .
// X X X . . X X X
// X X . . . . X X

unsigned char ttk_icon_exe[] = { 8, 13,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    3, 3, 0, 0, 0, 0, 3, 3,
    3, 3, 3, 0, 0, 3, 3, 3,
    0, 3, 3, 3, 3, 3, 3, 0,
    0, 0, 3, 3, 3, 3, 0, 0,
    0, 0, 0, 3, 3, 0, 0, 0,
    0, 0, 3, 3, 3, 3, 0, 0,
    0, 3, 3, 3, 3, 3, 3, 0,
    3, 3, 3, 0, 0, 3, 3, 3,
    3, 3, 0, 0, 0, 0, 3, 3,
    
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};


// Pause icon:
// X X X . . X X X
// X X X . . X X X
// X X X . . X X X
// X X X . . X X X
// X X X . . X X X
// X X X . . X X X
// X X X . . X X X
// X X X . . X X X
// X X X . . X X X
// X X X . . X X X
// X X X . . X X X
// X X X . . X X X
// X X X . . X X X

unsigned char ttk_icon_pause[] = { 8, 13,
    3, 3, 3, 0, 0, 3, 3, 3,
    3, 3, 3, 0, 0, 3, 3, 3,
    3, 3, 3, 0, 0, 3, 3, 3,
    3, 3, 3, 0, 0, 3, 3, 3,
    3, 3, 3, 0, 0, 3, 3, 3,
    3, 3, 3, 0, 0, 3, 3, 3,
    3, 3, 3, 0, 0, 3, 3, 3,
    3, 3, 3, 0, 0, 3, 3, 3,
    3, 3, 3, 0, 0, 3, 3, 3,
    3, 3, 3, 0, 0, 3, 3, 3,
    3, 3, 3, 0, 0, 3, 3, 3,
    3, 3, 3, 0, 0, 3, 3, 3,
    3, 3, 3, 0, 0, 3, 3, 1
};


// Submenu icon.
// . . . . . . . .
// . . X . . . . .
// . X X X . . . .
// . . X X X . . .
// . . . X X X . .
// . . . . X X X .
// . . . . . X X X
// . . . . X X X .
// . . . X X X . .
// . . X X X . . .
// . X X X . . . .
// . . X . . . . .
// . . . . . . . .

unsigned char ttk_icon_sub[] = { 8, 13,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 3, 0, 0, 0, 0, 0, 0,
    3, 3, 3, 0, 0, 0, 0, 0,
    0, 3, 3, 3, 0, 0, 0, 0,
    0, 0, 3, 3, 3, 0, 0, 0,
    0, 0, 0, 3, 3, 3, 0, 0,
    0, 0, 0, 0, 3, 3, 3, 0,
    0, 0, 0, 3, 3, 3, 0, 0,
    0, 0, 3, 3, 3, 0, 0, 0,
    0, 3, 3, 3, 0, 0, 0, 0,
    3, 3, 3, 0, 0, 0, 0, 0,
    0, 3, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};


// Hold icon.
// . . X X X X . .
// . X . . . . X .
// . X . . . . X .
// . X . . . . X .
// X X X X X X X X
// X X X X X X X X
// X X X X X X X X
// X X X X X X X X
// X X X X X X X X
// X X X X X X X X
// X X X X X X X X

unsigned char ttk_icon_hold[] = { 8, 10,
    0, 0, 3, 3, 3, 3, 0, 0,
    0, 3, 0, 0, 0, 0, 3, 0,
    0, 3, 0, 0, 0, 0, 3, 0,
    0, 3, 0, 0, 0, 0, 3, 0,
    3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3
};


// Normal battery. Draw fill from (x+5, y+2) to (x+20, y+2)
// . . . X X X X X X X X X X X X X X X X X X X . . .
// . . . X . . . . . . . . . . . . . . . . . X . . .
// . . . X . . . . . . . . . . . . . . . . . X . . .
// . . . X . . . . . . . . . . . . . . . . . X X . .
// . . . X . . . . . . . . . . . . . . . . . . X . .
// . . . X . . . . . . . . . . . . . . . . . . X . .
// . . . X . . . . . . . . . . . . . . . . . X X . .
// . . . X . . . . . . . . . . . . . . . . . X . . .
// . . . X . . . . . . . . . . . . . . . . . X . . .
// . . . X X X X X X X X X X X X X X X X X X X . . .

unsigned char ttk_icon_battery[] = { 25, 10,
    0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0,
    0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0,
    0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0,
    0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0,
    0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0,
    0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0,
    0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0,
    0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0,
    0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0,
    0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0
};

// Charging bolt. Draw at same (x, y) as battery.
// . . * X X X X *
// . . @ X X X . .
// . * X X X * . .
// . @ X X . * * *
// * X X X X X X X
// @ X X X X X X *
// . . . * X X * .
// . . . X X * . .
// . . * X . . . .
// . . X * . . . .

unsigned char ttk_icon_charging[] = { 8, 10,
    0, 0, 1, 3, 3, 3, 3, 1,
    0, 0, 2, 3, 3, 3, 0, 0,
    0, 1, 3, 3, 3, 1, 0, 0,
    0, 2, 3, 3, 0, 1, 1, 1,
    1, 3, 3, 3, 3, 3, 3, 3,
    2, 3, 3, 3, 3, 3, 3, 1,
    0, 0, 0, 1, 3, 3, 1, 0,
    0, 0, 0, 3, 3, 1, 0, 0,
    0, 0, 1, 3, 0, 0, 0, 0,
    0, 0, 3, 1, 0, 0, 0, 0
};

void ttk_draw_icon (unsigned char *icon, ttk_surface srf, int sx, int sy, ttk_color col, int inv) 
{
    int x, y;
    unsigned char *p = icon + 2;
    int r, g, b;
    ttk_unmakecol_ex (col, &r, &g, &b, srf);
    int c[4] = { ttk_makecol_ex (255 - r, 255 - g, 255 - b, srf),
		 ttk_makecol_ex ((255-r)*2/3, (255-g)*2/3, (255-b)*2/3, srf),
		 ttk_makecol_ex ((255-r)/3, (255-g)/3, (255-b)/3, srf),
		 ttk_makecol_ex (r, g, b, srf) };
    
    if (inv) {
	int tmp;
	tmp = c[3], c[3] = c[0], c[0] = tmp;
	tmp = c[2], c[2] = c[1], c[1] = tmp;
    }

    for (y = 0; y < icon[1]; y++) {
	for (x = 0; x < icon[0]; x++) {
	    if (*p)
		ttk_pixel (srf, sx + x, sy + y, c[*p]);
	    p++;
	}
    }
}
