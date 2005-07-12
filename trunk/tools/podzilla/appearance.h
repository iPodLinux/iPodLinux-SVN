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
 * $Log: appearance.h,v $
 * Revision 1.4  2005/07/11 00:18:10  yorgle
 * Added the "gameboy" pea-green color scheme (although it's not very accurate)
 * Tweak in amiga 1.x color scheme to make the battery meter look better
 *
 * Revision 1.3  2005/07/10 22:58:49  yorgle
 * Added in more color schemes: Monocrhome-inverted, Amigados 1, amigados 2
 * Added a hook to menu.c to limit the choices on monochrome ipods
 * Added patches to mlist to draw menu items in color
 * - needs a full screen redraw when color schemes change, or a forced-restart or somesuch.
 *
 * Revision 1.2  2005/07/10 01:36:28  yorgle
 * Added color scheming to message and pz_error
 *
 * Revision 1.1  2005/07/09 20:49:16  yorgle
 * Added in appearance.[ch].  Currently it only supports color schemes
 * Color scheme selection in menu.c
 * color-scheme support in mlist.c, slider.c, pz.c (menu scrollbar, pz header, sclider)
 *
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
#define CS_BATTCHRG	(22)	/* battery is charging */

#define CS_HOLDBDR	(23)	/* hold icon border */
#define CS_HOLDFILL	(24)	/* hold icon fill */

/* error/warning messages */
#define CS_MESSAGEFG	(25)	/* pz_message forground text */
#define CS_MESSAGELINE	(26)	/* highlight line */
#define CS_MESSAGEBG	(27)	/* pz_message background */
#define CS_ERRORBG	(28)	/* pz_error background */

#define CS_MAX		(28)	/* total number of colors per scheme */


/* counts of number of schemes... */
#define CS_NSCHEMES	(7)	/* total number of color schemes */
#define CS_MONO_LAST	(1)	/* last index of mono schemes */

/* the array of names for the menu system */
extern char * colorscheme_names[];

/* initialize the color scheme system */
void appearance_init();

/* get and set the current scheme number (index into the above) */
void appearance_set_color_scheme( int index );
int appearance_get_color_scheme( void );

/* gets a color from the currently selected scheme.
   use the above #defines to select which color to retrieve */
GR_COLOR appearance_get_color( int index );

