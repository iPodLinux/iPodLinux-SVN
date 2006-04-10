/*
 * Copyright (C) 2004-2006 Jonathynne Bett'ncort
 * http://www.kreativekorp.com
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

#include "ipod.h"
#include <string.h>

#define FALSE 0
#define TRUE !FALSE

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		ipod_set_backlight(!ipod_get_backlight());
	} else {
		// do specific whatever thing
		if (strcasecmp(argv[1], "on") == 0) { ipod_set_backlight(TRUE); }
		if (strcasecmp(argv[1], "off") == 0) { ipod_set_backlight(FALSE); }
		if (strcasecmp(argv[1], "toggle") == 0) { ipod_set_backlight(!ipod_get_backlight()); }
	}
	return 0;
}
