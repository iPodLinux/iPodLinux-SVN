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
 * Revision 1.7  2005/07/14 18:03:59  yorgle
 * Added in load average display (settable via settings menu)
 * (tested on OS X and on ipod, assumed to work on linux in general as well)
 * Added in two colors for the load average display too.
 *
 * Revision 1.6  2005/07/12 05:27:02  yorgle
 * Decorations added:  Plain (default), Amiga 1.1, Amiga 1.3, m:robe
 * Moved the lock/hold widget to the left by a touch to center it better
 * Added another color to the color scheme system: CS_TITLEACC  title accent color
 *
 * Revision 1.5  2005/07/12 03:51:39  yorgle
 * Added the m:robe color scheme by Stuart Clark (Decpher)
 * Slight tweak to the Amiga 1.x scheme
 * Always sets the number of color schemes in the menu, rather than just for mono
 *
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

#define CS_BG		"window.bg"	/* standard background */
#define CS_FG		"window.fg"	/* standard foreground (text) */

#define CS_SELBG	"menu.selbg"	/* selected background */
#define CS_SELBGBDR	"menu.selborder" /* selected background border */
#define CS_SELFG	"menu.selfg"	/* selected foreground (text) */

#define CS_ARROW0	"menu.selfg"	/* arrow > color 0 */
#define CS_ARROW1	"menu.selbg"	/* arrow > color 1 */
#define CS_ARROW2	"menu.selfg"	/* arrow > color 2 */
#define CS_ARROW3	"menu.selbg"	/* arrow > color 3*/

#define CS_TITLEBG	"header.bg"	/* titlebar background */
#define CS_TITLEFG	"header.fg"	/* titlebar foreground */
#define CS_TITLEACC	"notanyyet"	/* titlebar accent */
#define CS_TITLELINE	"header.line"	/* titlebar separator line */

/* UI Widgets */
#define CS_SCRLBDR	"scroll.border"	/* scrollbar border */
#define CS_SCRLCTNR	"scroll.bg"	/* scrollbar container */
#define CS_SCRLKNOB	"scroll.bar"	/* scrollbar knob */

#define CS_SLDRBDR	"slider.border"	/* slider border */
#define CS_SLDRCTNR	"slider.bg"	/* slider container */
#define CS_SLDRFILL	"slider.full"	/* slider filled */

/* podzilla customized widgets */
#define CS_BATTBDR	"battery.border"	/* battery icon border */
#define CS_BATTCTNR	"battery.bg"	/* battery icon container */
#define CS_BATTFILL	"battery.fill.normal"	/* battery icon filled (normal) */
#define CS_BATTLOW	"battery.fill.low"	/* battery icon filled (low) */
#define CS_BATTCHRG	"battery.fill.charge"	/* battery is charging */

#define CS_HOLDBDR	"lock.border"	/* hold icon border */
#define CS_HOLDFILL	"lock.fill"	/* hold icon fill */

/* error/warning messages */
#define CS_MESSAGEFG	"dialog.fg"	/* pz_message forground text */
#define CS_MESSAGELINE	"dialog.line"	/* highlight line */
#define CS_MESSAGEBG	"dialog.bg"	/* pz_message background */
#define CS_ERRORBG	"error.bg"	/* pz_error background */

/* load average display */
#define CS_LOADBG	"loadavg.bg"	/* load average container */
#define CS_LOADFG	"loadavg.fg"	/* load average meter */


/* initialize the color scheme system */
void appearance_init();

/* get and set the current scheme number (index into the above) */
void appearance_set_color_scheme (const char *name, int save);
void appearance_reset_color_scheme();
const char *appearance_get_color_scheme();

/* gets a color from the currently selected scheme.
   use the above #defines to select which color to retrieve */
t_GR_COLOR appearance_get_color (const char *prop);

/* opens a window to select colorscheme, inc. preview */
TWindow *appearance_select_color_scheme (ttk_menu_item *item);

/* Decorations stuff */
#define NDECORATIONS	(4)	/* total number of decorations */
extern const char * appearance_decorations[];

void appearance_set_decorations( int index );
int appearance_get_decorations( void );


/* make sure these are in sync with the code in appearance.c */
#define DEC_PLAIN	(0)
#define DEC_AMIGA11	(1)
#define DEC_AMIGA13	(2)
#define DEC_MROBE	(3)

