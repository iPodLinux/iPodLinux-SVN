/*
 * Vortex - A Tempest-like game
 * Copyright (C) 2005 Scott Lawrence
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
 *  $Id: globals.c,v 1.2 2005/07/19 02:40:34 yorgle Exp $
 *
 *  $Log: globals.c,v $
 *  Revision 1.2  2005/07/19 02:40:34  yorgle
 *  Created the console system (zooms text from the center, then logs it to the bottom left for a little while)
 *  Web rendering rewrite in progress
 *
 *  Revision 1.1  2005/05/16 02:53:00  yorgle
 *  Initial checkin of Vortex.  It's just a demo right now, soon to be fleshed out.
 *  Remove the "Vortex Demo" line(92) from menu.c to disable the hook.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "../pz.h"
#include "globals.h"

VORTEX_GLOBALS Vortex_globals;

void Vortex_resetGlobals( void )
{
	/* window stuff */
	Vortex_globals.timer_id = 0;
	Vortex_globals.wid      = 0;
	Vortex_globals.gc       = 0;

	/* helpers */
	Vortex_globals.width  = 0;
	Vortex_globals.height = 0;
	
	/* game control stuff */
	Vortex_globals.gameState    = VORTEX_STATE_PLAY;
	Vortex_globals.currentLevel = 0;
	Vortex_globals.lives        = 3;
	Vortex_globals.score        = 54320;
	Vortex_globals.superzapper  = 0;

	/* other stuff here */
	Vortex_globals.ld = NULL;
}
