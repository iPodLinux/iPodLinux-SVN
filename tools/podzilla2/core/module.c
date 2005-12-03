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
    char *signature;
    int verified;

    struct _pz_Module **deps; // terminated with a 0 entry
    char *depsstr;
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

    mod->mountpt = malloc (strlen ("xpods/") + strlen (strrchr (mod->podpath, '/')) + 1);
    sprintf (mod->mountpt, "xpods/%s", strrchr (mod->podpath, '/') + 1);
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
    char buf[80];
    FILE *fp;

    if (!mod->podpath) { // static mod, no POD
	// mod->name already set
	mod->longname = strdup (mod->name);
	mod->author = strdup ("Podzilla Team");
	return;
    }

    sprintf (buf, "%s/" MODULE_INF_FILE, mod->mountpt);
    fp = fopen (buf, "r");
    if (!fp) {
	pz_perror (buf);
    } else {
        while (fgets (buf, 79, fp)) {
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
            } else if (strcmp (key, "Signature") == 0) {
                mod->signature = malloc (strlen (value) + 1);
                strcpy (mod->signature, value);
            } else if (strcmp (key, "Dependencies") == 0) {
                mod->depsstr = malloc (strlen (value) + 1);
                strcpy (mod->depsstr, value);
            } else if (strcmp (key, "Provides") == 0) {
                mod->providesstr = malloc (strlen (value) + 1);
                strcpy (mod->providesstr, value);
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

    // XXX. Check signature here.
}

// Turns the depsstr into a list of deps. returns 0 for success, -1 for failure
// [initing] concerns the autoselection of deps for loading: if true, all
// dependencies will be marked to_load regardless of whether they are. If false,
// they have to be loaded beforehand.
static int fix_dependencies (PzModule *mod, int initing) 
{
    char separator;
    char *str = mod->depsstr;
    char *next;
    PzModule **pdep;
    int ndeps = 0;
    
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

    ndeps += 2; // the first one, the 0 at the end
    mod->deps = calloc (ndeps, sizeof(PzModule *));
    str = mod->depsstr;
    while (isspace (*str)) str++; // trim WS on left
    pdep = mod->deps;

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
		pz_error ("Unresolved dependency for %s: %s. Please install.", mod->name, str);
		return -1;
	    }

	    if (!initing) {
		if (!cur->to_load) {
		    pz_error ("Module %s requires %s. You have it, but it isn't loaded. Please load it.", mod->name, str);
		    return -1;
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
    fname = malloc (strlen (mod->mountpt) + strlen (mod->name) + 8);
#ifdef IPOD
    sprintf (fname, "%s/%s.mod.o", mod->mountpt, mod->name);
    mod->handle = uCdl_open (fname);
    free (fname);
    if (!mod->handle) {
	pz_error ("Could not load module %s: %s", mod->name, uCdl_error());
    }
#else
    sprintf (fname, "%s/%s.so", mod->mountpt, mod->name);
    mod->handle = dlopen (fname, RTLD_NOW | RTLD_GLOBAL);
    free (fname);
    if (!mod->handle) {
	pz_error ("Could not load module %s: %s", mod->name, dlerror());
    }
#endif
    else {
#ifdef IPOD
	mod->init = uCdl_sym (mod->handle, "__init_module__");
	if (!mod->init) pz_warning (_("Could not do modinit function for %s: %s"), mod->name, uCdl_error());
#else
	mod->init = dlsym (mod->handle, "__init_module__");
	if (!mod->init) pz_warning (_("Could not do modinit function for %s: %s"), mod->name, dlerror());
#endif
	else {
	    (*mod->init)();
	}
    }
    mod->to_load = 0;
}


static void free_module (PzModule *mod) 
{
    if (mod->name) free (mod->name);
    if (mod->longname) free (mod->longname);
    if (mod->author) free (mod->author);
    if (mod->signature) free (mod->signature);
    
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

void pz_modules_init() 
{
#ifdef IPOD
#define MODULEDIR "/usr/lib/"
#else
#ifdef MountPods
#define MODULEDIR "pods/"
#else
#define MODULEDIR "xpods/"
#endif
#endif

    struct stat st;
    PzModule *last, *cur;
    int nmods = 0;
    struct dirent *d;
    DIR *dp;
    int i;

    dp = opendir (MODULEDIR);
    if (!dp) {
	pz_perror (MODULEDIR);
	return;
    }

    if (module_head) {
	pz_error (_("modules_init called more than once"));
	closedir (dp);
	return;
    }

    while ((d = readdir (dp)) != 0) {
	char *podpath;
	if (d->d_name[0] == '.') continue;

        podpath = malloc (strlen (MODULEDIR) + strlen (d->d_name) + 1);
        strcpy (podpath, MODULEDIR);
        strcat (podpath, d->d_name);
        
	if (strlen (d->d_name) < 4 || strcmp (d->d_name + strlen (d->d_name) - 4, ".pod") != 0) {
            if (stat (podpath, &st) < 0 || !S_ISDIR (st.st_mode)) {
                free (podpath);
                continue;
            }
        }

#ifdef MountPods
	if (stat (podpath, &st) < 0 || (!S_ISREG (st.st_mode) && !S_ISDIR (st.st_mode))) {
	    pz_perror (podpath);
	    free (podpath);
	    continue;
	}
#endif
	nmods++;
	if (nmods > 120) {
	    pz_error (_("Too many modules (120+). Trim down your podzilla. Please. Modules after 120 will be ignored."));
	    break;
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
        if (stat (podpath, &st) >= 0 && S_ISDIR (st.st_mode))
            cur->extracted = 1;
	cur->to_load = 1;
        cur->ordered = 0;
    }
    closedir (dp);

    for (i = 0; i < __pz_builtin_number_of_init_functions; i++) {
	int found = 0;
	char *name = malloc (strlen (__pz_builtin_names[i]) + 5);

	sprintf (name, "%s.pod", __pz_builtin_names[i]);
	cur = module_head;
	while (cur) {
	    char *p = strrchr (cur->podpath, '/');
	    if (!p) p = cur->podpath;
	    else p++;

	    if (!strncmp (p, name, strlen (name))) {
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

	free (name);
    }

    if (!module_head) {
	pz_message_title (_("Warning"), _("No modules. Podzilla will probably be very boring."));
	return;
    }

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

    // Load the module.inf's
    cur = module_head;
    while (cur) {
	load_modinf (cur);
	cur = cur->next;
    }

    // Figure out the dependencies
    cur = module_head;
    last = 0;
    while (cur) {
	if (fix_dependencies (cur, 1) == -1) {
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

    struct dep *c = load_order;
    while (c) {
	if (c->mod->to_load > 0) {
	    do_load (c->mod);
	} else if (c->mod->to_load < 0) {
	    (*c->mod->init)();
	    c->mod->to_load = 0;
	}
	c = c->next;        
    }
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
