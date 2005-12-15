/*
 * Copyright (c) 2005 Joshua Oreman
 *
 * This file is a part of TTK.
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

#include "ttk.h"
#include <stdlib.h>
#include <stdio.h>

int main (int argc, char **argv) 
{
    if (argc <= 1) {
	fprintf (stderr, "Usage: ./eximage somefile.png\n");
	return 1;
    }
    
    TWindow *mainwindow;
    TWidget *imgviewer;
    ttk_surface img=ttk_load_image(argv[1]);

    mainwindow = ttk_init();
    ttk_menufont = ttk_get_font ("Chicago", 12);
    ttk_textfont = ttk_get_font ("Espy Sans", 10);
    
    imgviewer = ttk_new_imgview_widget (mainwindow->w, mainwindow->h,
					img);
    ttk_add_widget (mainwindow, imgviewer);
    ttk_window_title (mainwindow, "Image Viewer");
    ttk_run();
    ttk_free_image(img);
    ttk_free_window(mainwindow);
    ttk_quit();
}
