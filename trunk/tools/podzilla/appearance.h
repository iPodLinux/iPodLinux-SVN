/*
 * appearance - color scheming and such for all of podzilla
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
 */

/* 
 * $Log: wumpus.c,v $
 *
 */

#define CS_BG		(0)	/* standard background */
#define CS_FG		(1)	/* standard foreground (text) */

#define CS_SELBG	(2)	/* selected background */
#define CS_SELBGBDR	(3)	/* selected background border */
#define CS_SELFG	(4)	/* selected foreground (text) */

#define CS_ARROW0	(5)	/* arrow > color 0 */
#define CS_ARROW1	(6)	/* arrow > color 1 */
#define CS_ARROW2	(7)	/* arrow > color 2 */
#define CS_ARROW3	(8)	/* arrow > color 3*/

#define CS_TITLEBG	(9)	/* titlebar background */
#define CS_TITLEFG	(10)	/* titlebar foreground */
#define CS_TITLELINE	(11)	/* titlebar separator line */

/* UI Widgets */
#define CS_SCRLBDR	(12)	/* scrollbar border */
#define CS_SCRLCTNR	(13)	/* scrollbar container */
#define CS_SCRLKNOB	(14)	/* scrollbar knob */

#define CS_SLDRBDR	(15)	/* slider border */
#define CS_SLDRCTNR	(16)	/* slider container */
#define CS_SLDRFILL	(17)	/* slider filled */

/* podzilla customized widgets */
#define CS_BATTBDR	(18)	/* battery icon border */
#define CS_BATTCTNR	(19)	/* battery icon container */
#define CS_BATTFILL	(20)	/* battery icon filled (normal) */
#define CS_BATTLOW	(21)	/* battery icon filled (low) */

#define CS_HOLDBDR	(22)	/* hold icon border */
#define CS_HOLDFILL	(23)	/* hold icon fill */

#define CS_MAX		(23)


/* counts of number of schemes... */
#define CS_NSCHEMES	(3)	/* total number of color schemes */
#define CS_MONO_LAST	(0)	/* last index of mono schemes */

extern char * colorscheme_names[];

void appearance_set_color_scheme( int index );
int appearance_get_color_scheme( void );

GR_COLOR appearance_get_color( int index );

void appearance_init();
