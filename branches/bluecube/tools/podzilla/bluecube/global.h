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

#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#define HEADLINE "BlueCube v0.9"

/* Length of one frame */
#define TICK_INTERVAL  30 /* --> 33,33... fps */

/* Cluster size */
#define CLUSTER_X  4
#define CLUSTER_Y  4

/* Box size (in bricks) */
#define BOX_BRICKS_X 10      
#define BOX_BRICKS_Y 16

/* State definitions */
#define STATE_MENU     0
#define STATE_PLAY     1
#define STATE_GAMEOVER 2
#define STATE_EXIT     3
#define STATE_CREDITS  4

#endif
