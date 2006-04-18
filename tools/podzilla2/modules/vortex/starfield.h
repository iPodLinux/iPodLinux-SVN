/********************************************************************************************
*
* starfield.h
*
* Copyright (C) 2005 Kevin Ferrare, original starfield for Rockbox (http://www.rockbox.org)
* Copyright (C) 2006 Felix Bruns, ported to iPodlinux/podzilla2/ttk
*
* 2006 Scott Lawrence, Added color tinting, integrated into Vortex
*
* All files in this archive are subject to the GNU General Public License.
* See the file COPYING in the source tree root for full license agreement.
*
* This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
* KIND, either express or implied.
*
********************************************************************************************/

/* this registers the demo module */
void Module_Starfield_init( void );

/* this initializes a session for your module - sets up #stars, etc */
void Module_Starfield_session( int nstars, int velocity );

/* this moves and draws the stars onto the surface */
void Module_Starfield_draw( ttk_surface srf );
