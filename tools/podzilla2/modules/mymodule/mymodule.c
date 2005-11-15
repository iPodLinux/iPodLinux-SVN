#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "pz.h"

static PzModule *module;
static ttk_surface image;
static int imgw, imgh;
static char *text;

void draw_mymodule (PzWidget *wid, ttk_surface srf) 
{
    // Draw the message
    char *p = text;
    int y = wid->y + 5;
    while (*p) {
	char svch;
	if (strchr (p, '\n')) {
	    svch = *strchr (p, '\n');
	    *strchr (p, '\n') = 0;
	} else {
	    svch = 0;
	}
	ttk_text (srf, ttk_textfont, wid->x + 5, y, ttk_makecol (BLACK), p);
	p += strlen (p);
	y += ttk_text_height (ttk_textfont) + 1;
	*p = svch;
	if (*p) p++;
    }
    y += 3;
    // Dividing line
    ttk_line (srf, wid->x + 5, y, wid->x + wid->w - 5, y, ttk_makecol (DKGREY));
    
    y += 3;
    // The image
    ttk_blit_image (image, srf, wid->x + (wid->w - imgw) / 2, wid->y + (wid->h - imgh) / 2);
    y += imgh;
    
    // The message
#define MSG "Press a button to quit."
    ttk_text (srf, ttk_menufont, wid->x + (wid->w - ttk_text_width (ttk_menufont, MSG)) / 2,
	      wid->y + wid->h - ttk_text_height (ttk_menufont) - 5, ttk_makecol (BLACK), MSG);
}

int event_mymodule (PzEvent *ev) 
{
    switch (ev->type) {
    case PZ_EVENT_BUTTON_UP:
	pz_close_window (ev->wid->win);
	break;
    case PZ_EVENT_DESTROY:
	ttk_free_surface (image);
	free (text);
	break;
    }
    return 0;
}

PzWindow *new_mymodule_window()
{
    static int calledyet = 0;
    PzWindow *ret;
    FILE *fp;
    
    if (!calledyet) {
	calledyet++;
	pz_message ("Select again to see the fun stuff.");
	return TTK_MENU_DONOTHING;
    }

    image = ttk_load_image (pz_module_get_datapath (module, "image.png"));
    if (!image) {
	pz_error ("Could not load %s: %s", pz_module_get_datapath (module, "image.png"),
		  strerror (errno));
	return TTK_MENU_DONOTHING;
    }
    ttk_surface_get_dimen (image, &imgw, &imgh);

    fp = fopen (pz_module_get_datapath (module, "message.txt"), "r");
    if (!fp) {
	pz_warning ("Could not read from %s: %s.", pz_module_get_datapath (module, "message.txt"),
		    strerror (errno));
	text = (char *)strdup ("Hi! I forgot to supply a message!");
    } else {
	long len;
	fseek (fp, 0, SEEK_END);
	len = ftell (fp);
	fseek (fp, 0, SEEK_SET);
	text = malloc (len + 1);
	fread (text, 1, len, fp);
	text[len] = 0;
    }

    ret = pz_new_window ("MyModule", PZ_WINDOW_NORMAL);
    pz_add_widget (ret, draw_mymodule, event_mymodule);
    return pz_finish_window (ret);
}

void cleanup_mymodule() 
{
    printf ("Bye-bye now...\n");
}

void init_mymodule() 
{
    module = pz_register_module ("mymodule", cleanup_mymodule);
    pz_menu_add_action ("/Extras/Stuff/MyModule", new_mymodule_window);
    printf ("Hi! MyModule loaded, action set.\n");
}

PZ_MOD_INIT (init_mymodule)
