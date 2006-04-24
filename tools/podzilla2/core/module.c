/*
 * Copyright (c) 2005 Joshua Oreman
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

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#if defined(linux) || defined(IPOD)
#include <sys/mount.h>
#include <linux/fs.h>
#else
#ifdef MountPods
#error can only mount pods under Linux
#endif
#endif

#ifdef IPOD
#include "ucdl.h"
#else
#define _GNU_SOURCE /* for RTLD_DEFAULT */
#include <dlfcn.h>
#endif

int __pz_builtin_number_of_init_functions;
void (*__pz_builtin_init_functions[256])();
const char *__pz_builtin_names[256];

typedef struct _pz_Module
{
    char *name;
    char *longname;
    char *author;

    struct _pz_Module **deps; // terminated with a 0 entry
    char *depsstr;
    char *sdepsstr;
    char *providesstr; // We don't enforce the "provides" in mod_init, only in the mod select interface.

    char *podpath;
    char *mountpt;
    char *cfgpath;
    int mountnr;
    int extracted;

    void *handle;
    int to_load;       // positive - load from module. negative - initialize builtin. 0 - nothing.
    int ordered;
    void (*init)();
    void (*cleanup)();

    int to_free;

    struct _pz_Module *next;
} PzModule;

#define NODEF_MODULE
#include "pz.h"

static PzModule *module_head;

#ifdef IPOD
#define MountPods
#else
#ifdef MountPods
#warning You decided you want me to MountPods. You need to: have a 2.6 kernel; run podzilla as root; make module-2.6, load podfs.ko; pass max_loop=128 on the kernel command line; and be using devfs. As you can see, we do not recommend this.
#endif
#endif

// sets mod->mountpt = 0 on failure, returns -1. 0 = success.
static int mount_pod (PzModule *mod) 
{
#ifdef MountPods
    char mountline[256];
    static int nextmount = 0;

#ifdef IPOD
#define PODDIR "/tmp/modules/"
#else
#define PODDIR "mpods/"
#endif
    mkdir (PODDIR, 0777);
    mod->mountpt = malloc (strlen (PODDIR) + 10);
    sprintf (mod->mountpt, PODDIR "%d/", nextmount);
    mkdir (mod->mountpt, 0777);

    sprintf (mountline, "mount -t podfs -o loop %s %s", mod->podpath, mod->mountpt);
    mod->mountnr = nextmount++;

    // mount it
    if (system (mountline) != 0) {
    	strcat (mountline, " 2>mountpod.err >mountpod.err");
    	system (mountline);
    	pz_error ("Module %s (#%d): mount: exit %d - some sort of error, check mountpod.err",
                  strrchr (mod->podpath, '/') + 1, mod->mountnr);
    	free (mod->mountpt);
    	mod->mountpt = 0;
    	return -1;
    }
#else
    struct stat st;

    mod->mountpt = strdup (mod->podpath);
    if (strrchr (mod->mountpt, '.'))
        *strrchr (mod->mountpt, '.') = 0;
    
    if (stat (mod->mountpt, &st) < 0 || !S_ISDIR (st.st_mode)) {
	if (stat (mod->mountpt, &st) >= 0)
	    errno = ENOTDIR;
	pz_error ("Couldn't access xpod dir for %s: %s", mod->mountpt, strerror (errno));
	free (mod->mountpt);
	mod->mountpt = 0;
	return -1;
    }
#endif
    return 0;
}

#define MODULE_INF_FILE "Module"
static void load_modinf (PzModule *mod) 
{
    char *buf = malloc (512);
    FILE *fp;

    if (!mod->podpath) { // static mod, no POD
	// mod->name already set
	mod->longname = strdup (mod->name);
	mod->author = strdup ("podzilla Team");
	free (buf);
	return;
    }

    sprintf (buf, "%s/" MODULE_INF_FILE, mod->mountpt);
    fp = fopen (buf, "r");
    if (!fp) {
	pz_perror (buf);
    } else {
        while (fgets (buf, 511, fp)) {
            char *key, *value;
            
            if (buf[strlen (buf) - 1] == '\n')
                buf[strlen (buf) - 1] = 0;
            if (strchr (buf, '#'))
                *strchr (buf, '#') = 0;
            if (strlen (buf) < 1)
                continue;
            if (!strchr (buf, ':')) {
                pz_error (_("Line without colon in Module file for %s, ignored"), strrchr (mod->podpath, '/'));
                continue;
            }
            
            key = buf;
            value = strchr (buf, ':') + 1;
            while (isspace (*value)) value++;
            while (isspace (*(value + strlen (value) - 1))) value[strlen (value) - 1] = 0;
            *strchr (key, ':') = 0;
            
            if (strcmp (key, "Module") == 0) {
                mod->name = malloc (strlen (value) + 1);
                strcpy (mod->name, value);
            } else if (strcmp (key, "Display-name") == 0) {
                mod->longname = malloc (strlen (value) + 1);
                strcpy (mod->longname, value);
            } else if (strcmp (key, "Author") == 0) {
                mod->author = malloc (strlen (value) + 1);
                strcpy (mod->author, value);
            } else if (strcmp (key, "Dependencies") == 0) {
                mod->depsstr = malloc (strlen (value) + 1);
                strcpy (mod->depsstr, value);
	    } else if (strcmp (key, "Soft-Dependencies") == 0) {
                mod->sdepsstr = malloc (strlen (value) + 1);
                strcpy (mod->sdepsstr, value);
            } else if (strcmp (key, "Provides") == 0) {
                mod->providesstr = malloc (strlen (value) + 1);
                strcpy (mod->providesstr, value);
            } else if (strcmp (key, "Contact") == 0) {
                // nothing
            } else if (strcmp (key, "Category") == 0) {
                // nothing
            } else if (strcmp (key, "Description") == 0) {
                // nothing
            } else if (strcmp (key, "License") == 0) {
                // nothing
            } else if (strcmp (key, "Unstable") == 0) {
                // You can override "beta" with secret=testing but you can't
                // override other things, e.g. "alpha" or "does not work".
                if (strcmp (value, "beta") == 0 && !pz_has_secret ("testing"))
                    pz_warning (_("Module %s is in beta. Use at your own risk."), mod->name);
                else
                    pz_warning (_("Module %s is unstable: %s. Use at your own risk."), mod->name, value);
            } else {
                pz_error (_("Unrecognized key <%s> in Module file for %s, ignored"), key,
                          mod->name? mod->name : strrchr (mod->podpath, '/'));
            }
        }
    }

    if (!mod->name) {
	pz_error (_("Module %s's module file provides no name! Using podfile name without the extension."), 
		  strrchr (mod->podpath, '/'));
	mod->name = malloc (strlen (strrchr (mod->podpath, '/') + 1) + 1);
	strcpy (mod->name, strrchr (mod->podpath, '/') + 1);
	if (strchr (mod->name, '.'))
	    *strchr (mod->name, '.') = 0;
    }
    if (!mod->longname) {
	mod->longname = malloc (strlen (mod->name) + 1);
	strcpy (mod->longname, mod->name);
    }
    if (!mod->author) {
	mod->author = malloc (strlen (_("Anonymous")) + 1);
	strcpy (mod->author, _("Anonymous"));
    }

    // Get the config path, now that we know the name.
#ifdef IPOD
#define CONFDIR "/etc/podzilla"
#else
#define CONFDIR "config"
#endif
    mod->cfgpath = malloc (strlen (CONFDIR "/modules/") + strlen (mod->name) + 1);
    sprintf (mod->cfgpath, "%s/modules/%s", CONFDIR, mod->name);
    mkdir (CONFDIR, 0755);
    mkdir (CONFDIR "/modules", 0755);
    if (mkdir (mod->cfgpath, 0755) < 0 && errno != EEXIST) {
	pz_warning (_("Unable to create %s's config dir %s: %s"), mod->name, mod->cfgpath,
		    strerror (errno));
    }

    free (buf);
}

#define SOFTDEP 1
#define HARDDEP 2
// Turns the depsstr into a list of deps. returns 0 for success, -1 for failure
// [initing] concerns the autoselection of deps for loading: if true, all
// dependencies will be marked to_load regardless of whether they are. If false,
// they have to be loaded beforehand.
static int fix_dependencies (PzModule *mod, int initing, int which) 
{
    char separator;
    char *str;
    char *next;
    PzModule **pdep;
    int ndeps = 0;
    
    str = (which == SOFTDEP) ? mod->sdepsstr : mod->depsstr;
    if (!str) return 0;
    
    if (strchr (str, ','))
	separator = ',';
    else if (strchr (str, ';'))
	separator = ';';
    else if (strchr (str, ':'))
	separator = ':';
    else
	separator = ' ';
    
    while (*str) {
	if (*str == separator) {
	    ndeps++;
	    while (*str && (isspace (*str) || (*str == separator))) str++;
	} else {
	    str++;
	}
    }

    if (mod->deps) {
	pdep = mod->deps;
	while (*pdep) pdep++;
	mod->deps = realloc(mod->deps, (pdep - mod->deps) + ndeps + 1);
	memset(pdep, 0, (ndeps + 1));
    } else {
	ndeps += 2; // the first one, the 0 at the end
    	mod->deps = calloc (ndeps, sizeof(PzModule *));
	pdep = mod->deps;
    }
    str = (which == SOFTDEP) ? mod->sdepsstr : mod->depsstr;
    while (isspace (*str)) str++; // trim WS on left

    do {
	if (strchr (str, separator)) {
	    next = strchr (str, separator);
	    *strchr (str, separator) = 0;
	    next++;
	    while (*next && (isspace (*next) || (*next == separator))) next++; // trim extra sep and WS on left
	} else {
	    next = 0;
	}
	while (strlen (str) && isspace (str[strlen (str) - 1])) str[strlen (str) - 1] = 0; // trim WS on right

	if (strlen (str)) {
	    // set *pdep++ to the module named [str] if it's loaded, or error out if not.
	    PzModule *cur = module_head;
	    while (cur) {
		if (cur->name && !strcmp (str, cur->name)) {
		    *pdep++ = cur;
		    break;
		}
		cur = cur->next;
	    }
	    if (!cur) {
		if (which == HARDDEP) {
		    pz_error ("Unresolved dependency for %s: %s. Please install.", mod->name, str);
		    return -1;
		} else {
		    str = next;
		    continue;
		}
	    }

	    if (!initing) {
		if (!cur->to_load) {
		    if (which == HARDDEP) {
			pz_error ("Module %s requires %s. You have it, but it isn't loaded. Please load it.", mod->name, str);
			return -1;
		    }
		}
	    } else {
		if (cur->to_load >= 0)
		    cur->to_load++;
		else
		    cur->to_load--;
	    }
	}

	str = next;
    } while (next);
    *pdep++ = 0;
    return 0;
}

static void do_load (PzModule *mod) 
{
    char *fname;
#ifndef IPOD
    struct stat st;
#endif
    
    fname = malloc (strlen (mod->mountpt) + strlen (mod->name) + 8);
#ifdef IPOD
    sprintf (fname, "%s/%s.mod.o", mod->mountpt, mod->name);
    mod->handle = uCdl_open (fname);
    free (fname);
    if (!mod->handle) {
	pz_error ("Could not load module %s: %s", mod->name, uCdl_error());
        mod->to_load = 0;
    }
#else
    sprintf (fname, "%s/%s.so", mod->mountpt, mod->name);
    if (stat (fname, &st) < 0) {
        free (fname);
        mod->handle = 0;
        mod->to_load = 0;
        return;
    }
    mod->handle = dlopen (fname, RTLD_NOW | RTLD_GLOBAL);
    free (fname);
    if (!mod->handle) {
/*
        pz_error ("Could not load module %s: %s", mod->name, dlerror());
*/
	fprintf( stderr, "Error: Could not load module %s: %s\n",
			mod->name, dlerror());
        mod->to_load = 0;
    }
#endif
}

static void do_init (PzModule *mod) 
{
    if (mod->to_load > 0) {
#ifdef IPOD
	mod->init = uCdl_sym (mod->handle, "__init_module__");
#else
	mod->init = dlsym (mod->handle, "__init_module__");
#endif
	if (mod->init)
	    (*mod->init)();
    } else if (mod->to_load < 0) {
        (*mod->init)();
    }
    mod->to_load = 0;
}


static void free_module (PzModule *mod) 
{
    if (mod->name) free (mod->name);
    if (mod->longname) free (mod->longname);
    if (mod->author) free (mod->author);
    
    if (mod->deps) free (mod->deps);
    if (mod->depsstr) free (mod->depsstr);
    if (mod->providesstr) free (mod->providesstr);
    
#ifdef MountPods
    if (mod->mountnr) {
    	char buf[64];
    	sprintf (buf, "umount " PODDIR "%d", mod->mountnr);
    	system (buf);
    	mod->mountnr = 0;
    }
#endif

    if (mod->podpath) free (mod->podpath);
    if (mod->mountpt) free (mod->mountpt);
    if (mod->cfgpath) free (mod->cfgpath);
    if (mod->handle && mod->cleanup) (*mod->cleanup)();
#ifdef IPOD
    if (mod->handle) uCdl_close (mod->handle);
#else
    if (mod->handle) dlclose (mod->handle);
#endif
    
    free (mod);
}


struct dep
{
    PzModule *mod;
    struct dep *next;
} *load_order = 0;

static void add_deps (PzModule *mod) 
{
    PzModule **pdep;
    struct dep *cur;

    if (mod->ordered) return;
    mod->ordered = 1;

    if (mod->deps) {
        for (pdep = mod->deps; *pdep; pdep++) {
            add_deps (*pdep);
        }
    }

    if (load_order) {
        cur = load_order;
        while (cur->next) cur = cur->next;
        cur->next = malloc (sizeof (struct dep));
        cur = cur->next;
    } else {
        cur = load_order = malloc (sizeof (struct dep));
    }
    cur->mod = mod;
    cur->next = 0;
}

// for use by check_version, which is called from module init funcs
PzModule *current_module;

static void find_modules (const char *dir)
{
    char buf[256];
    char *podpath = 0;
    struct dirent *d;
    struct stat st;
    PzModule *cur;
    DIR *dp;
    
    dp = opendir (dir);
    if (!dp) {
	pz_perror (dir);
	return;
    }

    while ((d = readdir (dp)) != NULL) {
        if (d->d_name[0] == '.') continue;

        podpath = malloc (strlen (dir) + strlen (d->d_name) + 2);
        sprintf (podpath, "%s/%s", dir, d->d_name);

        if (stat (podpath, &st) < 0) {
            pz_perror (podpath);
            free (podpath);
            continue;
        }

        if (S_ISDIR (st.st_mode)) {
            // Either a category dir (to search recursively)
            // or a module dir. Check for the `Module' file.
            sprintf (buf, "%s/" MODULE_INF_FILE, podpath);
            if (stat (buf, &st) < 0) {
                find_modules (podpath);
                free (podpath);
                continue;
            }
            // Module dir - fall through
        } else if (strcasecmp (d->d_name + strlen (d->d_name) - 4, ".pod") != 0) {
            // Non-pod file - ignore it
            free (podpath);
            continue;
        }
        
	if (module_head == 0) {
	    cur = module_head = calloc (1, sizeof(PzModule));
	} else {
	    cur = module_head;
	    while (cur->next) cur = cur->next;
	    cur->next = calloc (1, sizeof(PzModule));
	    cur = cur->next;
	}
	cur->podpath = podpath;
        if ((stat (podpath, &st) >= 0) && S_ISDIR (st.st_mode))
            cur->extracted = 1;
	cur->to_load = 1;
        cur->ordered = 0;
        cur->next = 0;
    }

    closedir (dp);
}


/* set text to NULL for updating, to a value for new window stuff */
static void updateprogress( TWindow * sliderwin, TWidget * slider,
			    int newVal, char * displayText,
			    char * lineTwo )
{
	ttk_color menu_bg_color = 0;
	ttk_color menu_fg_color = 0;
	int textw;

	if( newVal < 0 ) newVal = 0; /* jsut in case */

	/* if displayText is non-NULL, rerender the whole thing */
	if( displayText || lineTwo ) {
		if( ttk_ap_getx( "menu.bg" ))
			menu_bg_color = ttk_ap_getx("menu.bg")->color;
		if( ttk_ap_getx( "menu.fg" ))
			menu_fg_color = ttk_ap_getx("menu.fg")->color;

		if( menu_fg_color == menu_bg_color ) {
		    if( menu_fg_color == 0 || menu_fg_color == 3 ) { /*why 3?*/
			menu_fg_color = ttk_makecol( BLACK );
			menu_bg_color = ttk_makecol( WHITE );
		    }
		}

		ttk_fillrect(sliderwin->srf,0,0,
				sliderwin->w,sliderwin->h,
				menu_bg_color );

		if( displayText ) {
			textw = ttk_text_width(ttk_textfont,displayText);
			ttk_text(sliderwin->srf,ttk_textfont,
				ttk_screen->w/ 2 - textw/2, ttk_screen->h/2,
				menu_fg_color, displayText);
		}

		if( lineTwo ) {
			textw = ttk_text_width(ttk_textfont, lineTwo);
			ttk_text(sliderwin->srf,ttk_textfont,
				ttk_screen->w/ 2 - textw/2, 
				ttk_screen->h/2 + ttk_textfont.height + 5,
				menu_fg_color, lineTwo);
		}
	}

	ttk_slider_set_val(slider,newVal);
	ttk_slider_draw(slider,sliderwin->srf);
	ttk_draw_window(sliderwin);
	ttk_gfx_update(ttk_screen->srf);
//	ttk_delay(10);  //useful for testing
}

void pz_modules_init() 
{
#ifdef IPOD
#define MODULEDIR "/usr/lib"
#else
#define MODULEDIR "modules"
#endif

    /* Used for the progress bar */
    TWindow * sliderwin;
    TWidget  * slider;
    int sliderVal=0;
    int verbosity = pz_get_int_setting( pz_global_config, VERBOSITY );
    #define MAXSLIDERVAL (100)
    #define SETUPSECTIONS (6)

    PzModule *last, *cur;
    int i;
    int modCount=0;

    if (module_head) {
	pz_error (_("modules_init called more than once"));
	return;
    }

    /* set up the screen to show a pretty progress bar */
    sliderwin = ttk_new_window();
    ttk_window_hide_header(sliderwin);
    sliderwin->x = sliderwin->y = 0;
    slider = ttk_new_slider_widget(2, ttk_screen->h/3, ttk_screen->w - 8, 0,
		    MAXSLIDERVAL, &sliderVal, 0);
    slider->x = (sliderwin->w - slider->w)/2;
    ttk_add_widget(sliderwin, slider);

    sliderVal = (MAXSLIDERVAL/SETUPSECTIONS) * 1;
    updateprogress(sliderwin, slider, sliderVal, _("Scanning For Modules"), NULL);
    find_modules (MODULEDIR);

    sliderVal = (MAXSLIDERVAL/SETUPSECTIONS) * 2;
    updateprogress(sliderwin, slider, sliderVal, _("Reading Modules"), NULL);
    for (i = 0; i < __pz_builtin_number_of_init_functions; i++) {
	int found = 0;
	const char *name = __pz_builtin_names[i];

	cur = module_head;
	while (cur) {
	    char *p = strrchr (cur->podpath, '/');
	    if (!p) p = cur->podpath;
	    else p++;

	    if (!strncmp (p, name, strlen (name)) && !isalpha (p[strlen (name)])) {
		found = 1;
		break;
	    }
	    cur = cur->next;
	}
	if (!found) {
	    if (module_head) {
		cur = module_head; while (cur->next) cur = cur->next;
		cur->next = calloc (1, sizeof(PzModule));
		cur = cur->next;
	    } else {
		cur = module_head = calloc (1, sizeof(PzModule));
	    }

	    cur->podpath = 0;
	    cur->name = strdup (__pz_builtin_names[i]);
	    cur->init = __pz_builtin_init_functions[i];
	    cur->to_load = -1;
            cur->ordered = 0;
	    cur->next = 0;
	}
    }
    
    if (!module_head) {
        pz_message_title (_("Warning"), _("No modules. podzilla will probably be very boring."));
        return;
    }

    /* Used to initialize the window + progressbar used in loading the modules*/
    cur = module_head;
    while (cur) {
    	modCount++;
	cur = cur->next;
    }

    sliderVal = (MAXSLIDERVAL/SETUPSECTIONS) * 3;
    updateprogress(sliderwin, slider, sliderVal, _("Mounting Modules"), NULL);
    // Mount 'em
    cur = module_head;
    last = 0;
    while (cur) {
        if (cur->podpath && cur->extracted) {
            cur->mountpt = strdup (cur->podpath);
            last = cur;
            cur = cur->next;
        } else if (cur->podpath && mount_pod (cur) == -1) {
	    if (last) last->next = cur->next;
	    else module_head = cur->next;
	    free (cur->podpath);
	    free (cur);
	    cur = last? last->next : module_head;
	} else {
	    last = cur;
	    cur = cur->next;
	}
    }

    sliderVal = (MAXSLIDERVAL/SETUPSECTIONS) * 4;
    updateprogress(sliderwin, slider, sliderVal, _("Scanning Module Info"), NULL);
    // Load the module.inf's
    cur = module_head;
    while (cur) {
	load_modinf (cur);
	cur = cur->next;
    }

    sliderVal = (MAXSLIDERVAL/SETUPSECTIONS) * 5;
    updateprogress(sliderwin, slider, sliderVal, _("Module Dependencies"), NULL);
    // Figure out the dependencies
    cur = module_head;
    last = 0;
    while (cur) {
	if (fix_dependencies (cur, 1, SOFTDEP) == -1 ||
			fix_dependencies(cur, 1, HARDDEP) == -1) {
	    if (last) last->next = cur->next;
	    else module_head = cur->next;
	    free_module (cur);
	    cur = last? last->next : module_head;
	} else {
	    last = cur;
	    cur = cur->next;
	}
    }

    // Check which ones are linked in
    cur = module_head;
    last = 0;
    while (cur) {
	for (i = 0; i < __pz_builtin_number_of_init_functions; i++) {
	    if (!strcmp (__pz_builtin_names[i], cur->name)) {
		cur->init = __pz_builtin_init_functions[i];
		cur->to_load = -1;
                cur->ordered = 0;
	    }
	}
	cur = cur->next;
    }

    // XXX. For now, we use a slow method for deps, and it'll crash
    // for circular deps. davidc__ is working on a 
    // better solution.
    cur = module_head;
    while (cur) {
        add_deps (cur);
        cur = cur->next;
    }

    sliderVal = (MAXSLIDERVAL/SETUPSECTIONS) * 6;
    updateprogress(sliderwin, slider, sliderVal, _("Loading Modules"), NULL);

    struct dep *c = load_order;
    while (c) {
	if (c->mod->to_load > 0) {
	    do_load (c->mod);
	}
	if( verbosity < 2 && c->mod->longname ) {
	    updateprogress(sliderwin, slider, sliderVal, _("Loading Modules"), 
			    c->mod->longname );
	}
	c = c->next;        
    }
    c = load_order;


    sliderVal = 0;
    updateprogress(sliderwin, slider, sliderVal, _("Initializing Modules"), NULL);

    /* trigger the sliders to switch to init mode, restting the slider */
    sliderVal = 0;

    /* initialize the modules */
    while (c) {
	sliderVal += MAXSLIDERVAL / modCount;
        current_module = c->mod;

	updateprogress(sliderwin, slider, sliderVal, 
			_("Initializing Modules"), 
			(verbosity < 2)?  c->mod->longname : NULL );

        do_init (c->mod);
        c = c->next;
    }

    // Any modules with unrecoverable errors on loading set mod->to_free.
    // Oblige them.
    cur = module_head;
    last = 0;
    while (cur) {
        if (cur->to_free) {
	    if (last) last->next = cur->next;
	    else module_head = cur->next;
            free_module (cur);
	    cur = last? last->next : module_head;
	} else {
	    last = cur;
	    cur = cur->next;
        }
    }

    ttk_free_window (sliderwin);
}


// Does not free, just calls cleanup functions.
// Mainly useful on exit.
void pz_modules_cleanup() 
{
    PzModule *cur = module_head;
    while (cur) {
	if (cur->handle && cur->cleanup) {
	    (*cur->cleanup)();
	    cur->cleanup = 0;
	}
	cur = cur->next;
    }
}


PzModule *pz_register_module (const char *name, void (*cleanup)()) 
{
    PzModule *cur = module_head;
    while (cur) {
	if (!strcmp (cur->name, name))
	    break;
	cur = cur->next;
    }
    if (!cur) {
	pz_error ("Module registered with invalid name <%s>.", name);
	return 0;
    }
    cur->cleanup = cleanup;
    return cur;
}


const char *pz_module_get_cfgpath (PzModule *mod, const char *file) 
{
    static char ret[256];
    sprintf (ret, "%s/%s", mod->cfgpath, file);
    return ret;
}

const char *pz_module_get_datapath (PzModule *mod, const char *file) 
{
    static char ret[256];
    sprintf (ret, "%s/%s", mod->mountpt, file);
    return ret;
}

void pz_module_iterate (void (*fn)(const char *, const char *, const char *)) 
{
    PzModule *cur = module_head;
    while (cur) {
        (*fn)(cur->name, cur->longname, cur->author);
        cur = cur->next;
    }
}

void *pz_module_softdep (const char *modname, const char *symname) 
{
    PzModule *cur = module_head;
    while (cur) {
        if (!strcmp (modname, cur->name)) {
#ifndef IPOD
            void *ret = dlsym (cur->handle, symname);
            if (!ret) {
                // Some versions of OS X have symbols that start
                // with underscores...
                char *_symname = malloc (strlen (symname) + 2);
                sprintf (_symname, "_%s", symname);
                ret = dlsym (cur->handle, _symname);
                free (_symname);
            }
            return ret;
#else
            return uCdl_sym (cur->handle, symname);
#endif
        }
        cur = cur->next;
    }
    return 0;
}

int _pz_mod_check_version (int otherver) 
{
    int myver = PZ_API_VERSION;
    PzModule *module = current_module;

    /* version completely equal - OK */
    if (myver == otherver)
        return 1;

    /* version less - some stuff may be missing, won't work */
    if (myver < otherver) {
        pz_error ("Module %s compiled for a newer version of podzilla. Please upgrade.",
                  module->name);
        goto remove;
    }
    /* minor version more - only stuff added, OK */
    if (((myver & ~0xff) == (otherver & ~0xff)) && ((myver & 0xff) >= (otherver & 0xff))) {
        pz_warning ("Module %s compiled for a slightly older version of podzilla. "
                    "It will still probably work, but you should upgrade the module soon.",
                    module->name);
        return 1;
    }
    /* major version more - won't work */
    pz_error ("Module %s compiled for a significantly older version of podzilla; "
              "it will not work. Please upgrade the module.", module->name);
 remove:
    module->to_free = 1;
    return 0;
}

