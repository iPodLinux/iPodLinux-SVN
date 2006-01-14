/*
 * Copyright (c) 2006 Courtney Cavin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
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

#include "pz.h"

static PzModule *module;

static int idle_handler(void *d)
{
	PzWindow *(* local_mpd_currently_playing)(void);
	char (* local_mpdc_is_playing)(void);
	char (* local_mcp_active)(void);
	
	local_mpdc_is_playing = pz_module_softdep("mpdc", "mpdc_is_playing");
	if (local_mpdc_is_playing && local_mpdc_is_playing()) {
		local_mcp_active = pz_module_softdep("mpdc", "mcp_active");
		if (!local_mcp_active || local_mcp_active())
			return 0;
		if ((local_mpd_currently_playing = pz_module_softdep("mpdc",
				"mpd_currently_playing")))
			ttk_show_window(local_mpd_currently_playing());
		
	}
	else {/* this is where the actual sleeping stuff would go */}
	return 0;
}

static void init_sleep()
{
	module = pz_register_module("sleep", NULL);
	pz_register_idle(idle_handler, NULL);
}

PZ_MOD_INIT(init_sleep)
