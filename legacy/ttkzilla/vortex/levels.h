/*
 * levels
 *
 *   contains all of the vector information for all of the levels.
 *
 *   01-2005 Scott Lawrence
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
 *  $Id: levels.h,v 1.1 2005/05/16 02:53:00 yorgle Exp $
 *
 *  $Log: levels.h,v $
 *  Revision 1.1  2005/05/16 02:53:00  yorgle
 *  Initial checkin of Vortex.  It's just a demo right now, soon to be fleshed out.
 *  Remove the "Vortex Demo" line(92) from menu.c to disable the hook.
 *
 */

#ifndef __VORTEX_GLOBALS_H__
#define __VORTEX_GLOBALS_H__

#define uchar unsigned char

#define NLEVELS (16*3)

typedef struct {
	uchar order;            /* remap above */

	uchar x[16];            /* all x ordinates */
	uchar y[16];            /* all y ordinates */

	uchar angle[16];        /* all sector angles */

	uchar scale;            /* scale */
	int   fscale;           /* flipper scale */
	uchar y3d;              /* 3d y offset */
	int   y2d;              /* 2d offset */

	char  flags;            /* flags for the level (use LF_* below) */
} LEVELDATA;

/* flags for the above */
#define LF_NONE      (0x00)	/* nothing special */
#define LF_OPENWEB   (0x01)	/* web has start and end points */
#define LF_CLOSEDWEB (0x02)	/* web loops around at the end */

extern LEVELDATA vortex_levels[NLEVELS];

#endif
