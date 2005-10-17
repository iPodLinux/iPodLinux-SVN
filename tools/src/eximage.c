#include "ttk.h"
#include "menu.h"
#include "imgview.h"
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

    mainwindow = ttk_init();
    ttk_menufont = ttk_get_font ("Chicago", 12);
    ttk_textfont = ttk_get_font ("Espy Sans", 10);
    
    imgviewer = ttk_new_imgview_widget (mainwindow->w, mainwindow->h,
					ttk_load_image (argv[1]));
    ttk_add_widget (mainwindow, imgviewer);
    ttk_window_title (mainwindow, "Image Viewer");
    ttk_run();
}
