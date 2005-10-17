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
 * $Log: appearance.c,v $
 * Revision 1.10  2005/07/14 18:03:59  yorgle
 * Added in load average display (settable via settings menu)
 * (tested on OS X and on ipod, assumed to work on linux in general as well)
 * Added in two colors for the load average display too.
 *
 * Revision 1.9  2005/07/14 14:38:51  yorgle
 * charge color icon
 * it was hard to see light gray
 * changed it to dark gray
 *
 * Revision 1.8  2005/07/13 03:27:20  yorgle
 * Various color scheme tweaks
 *
 * Revision 1.7  2005/07/12 05:27:02  yorgle
 * Decorations added:  Plain (default), Amiga 1.1, Amiga 1.3, m:robe
 * Moved the lock/hold widget to the left by a touch to center it better
 * Added another color to the color scheme system: CS_TITLEACC  title accent color
 *
 * Revision 1.6  2005/07/12 03:51:38  yorgle
 * Added the m:robe color scheme by Stuart Clark (Decipher)
 * Slight tweak to the Amiga 1.x scheme
 * Always sets the number of color schemes in the menu, rather than just for mono
 *
 * Revision 1.5  2005/07/11 00:18:10  yorgle
 * Added the "gameboy" pea-green color scheme (although it's not very accurate)
 * Tweak in amiga 1.x color scheme to make the battery meter look better
 *
 * Revision 1.4  2005/07/10 23:18:28  yorgle
 * Tweaked the Amiga2 color scheme to be more readable (black-on-blue for title)
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

#include <stdio.h>	/* NULL */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include "pz.h"
#include "ipod.h"	/* ipod_get_setting */
#include "settings.h"  /* get_string_setting */


#ifdef IPOD
#define SCHEMESDIR "/usr/share/schemes/"
#else
#define SCHEMESDIR "schemes/"
#endif

/******************************************************************************
** color scheme stuff
*/

/* this sets a new scheme.  It constrains the input value to something valid */
void appearance_set_color_scheme (const char *file, int save)
{
    if (save) {
	unlink (SCHEMESDIR "default.cs");
	symlink (file, SCHEMESDIR "default.cs");
    }

    ttk_epoch++;

    int odfd = open (".", O_RDONLY);
    chdir (SCHEMESDIR);
    ttk_ap_load (file);
    fchdir (odfd);
    close (odfd);
}

/* this resets the color scheme to the last actually-set value */
void appearance_reset_color_scheme() 
{
    ttk_ap_load (SCHEMESDIR "default.cs");
}

/* this gets the current color scheme value */
const char *appearance_get_color_scheme (void)
{
    static char link[256];
    readlink (SCHEMESDIR "default.cs", link, 256);
    return link;
}

/* this gets a color from the current color scheme */
GR_COLOR appearance_get_color (const char *prop)
{
    int r, g, b;
    ttk_unmakecol (ttk_ap_getx (prop) -> color, &r, &g, &b);
    return GR_RGB (r, g, b);
}

/* color scheme selector */
int apsc_scroll (TWidget *this, int dir) 
{
    int ret = ttk_menu_scroll (this, dir);
    appearance_set_color_scheme (ttk_menu_get_selected_item (this) -> data, 0);
    ttk_epoch++;
    return ret;
}
int apsc_down (TWidget *this, int button)
{
    if (button == TTK_BUTTON_ACTION) {
	appearance_set_color_scheme (ttk_menu_get_selected_item (this) -> data, 1);
	return ttk_menu_down (this, TTK_BUTTON_MENU);
    }
    return ttk_menu_down (this, button);
}
static ttk_menu_item empty_menu[] = {{ 0 }};
TWindow *appearance_select_color_scheme (ttk_menu_item *item) 
{
    TWindow *ret = ttk_new_window();
    TWidget *menu = ttk_new_menu_widget (empty_menu, ttk_menufont, item->menuwidth, item->menuheight);
    menu->scroll = apsc_scroll;
    menu->down = apsc_down;

    int odfd = open (".", O_RDONLY);
    chdir (SCHEMESDIR);
    DIR *dp = opendir (".");
    if (!dp) {
	pz_perror (SCHEMESDIR);
	return TTK_MENU_DONOTHING;
    }
    struct dirent *d;
    while ((d = readdir (dp)) != 0) {
	if (strchr (d->d_name, '.') && !strcmp (strrchr (d->d_name, '.'), ".cs") && !!strcmp (d->d_name, "default.cs")) {
	    ttk_menu_item *item = calloc (1, sizeof(struct ttk_menu_item));
	    FILE *f = fopen (d->d_name, "r");
	    if (!f) continue;
	    char buf[100];

	    item->data = malloc (strlen (d->d_name) + 1);
	    strcpy (item->data, d->d_name);

	    fgets (buf, 100, f);
	    if (strchr (buf, ' '))
		item->name = strdup (strchr (buf, ' ') + 1);
	    else
		item->name = strdup (d->d_name);
	    fclose (f);

	    ttk_menu_append (menu, item);
	}
    }
    closedir (dp);
    fchdir (odfd);
    close (odfd);

    ttk_add_widget (ret, menu);
    ttk_window_set_title (ret, _("Color Scheme"));
    return ret;
}

/******************************************************************************
** Decoration stuff
*/

const char * appearance_decorations[] = { "Plain",
		"Amiga 1.1", "Amiga 1.3",
		"m:robe", 0 };

static int decoration_current_idx = 0;

void appearance_set_decorations( int index )
{
	if( index < 0 ) index = 0;
	if( index >= NDECORATIONS ) index = 0;
	decoration_current_idx = index;
}

int appearance_get_decorations( void )
{
	return( decoration_current_idx );
}

/* rendering code is over in pz.c for now. */


/******************************************************************************
** General stuff
*/

/* this sets up the tables and such.  Eventually this might load in files */
void appearance_init( void )
{
	appearance_set_decorations( ipod_get_setting( DECORATIONS ));
}
