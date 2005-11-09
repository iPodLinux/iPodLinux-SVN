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
#define LOOP_SET_FD     0x4C00
#define LOOP_CLR_FD     0x4C01
#else
#ifdef MountPods
#error can only mount pods under Linux
#endif
#endif

#ifdef IPOD
#include "ucdl.h"
#else
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

    void *handle;
    int to_load;       // positive - load from module. negative - initialize builtin. 0 - nothing.
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
    char devstr[64];
    int devfd, podfd;
    struct stat st;
    static int nextmount = 0;
    
#ifdef MountPods

#ifdef IPOD
#define PODDIR "/tmp/modules/"
#else
#define PODDIR "pods/"
#endif
    mod->mountpt = malloc (strlen (PODDIR) + 10);
    sprintf (mod->mountpt, PODDIR "%d/", nextmount++);
    
    sprintf (devstr, "/dev/loop/%d", nextmount + 1); // yes, it starts from 2. that's intentional.
    devfd = open (devstr, O_RDONLY);
    if (devfd < 0) {
	pz_perror (devstr);
	free (mod->mountpt);
	mod->mountpt = 0;
	return -1;
    }
    podfd = open (mod->podpath, O_RDONLY);
    if (podfd < 0) {
	pz_perror (mod->podpath);
	free (mod->mountpt);
	mod->mountpt = 0;
	close (devfd);
	return -1;
    }
    
    ioctl (devfd, LOOP_CLR_FD, 0);
    if (ioctl (devfd, LOOP_SET_FD, podfd) < 0) {
	pz_perror ("LOOP_SET_FD");
	free (mod->mountpt);
	mod->mountpt = 0;
	close (podfd);
	close (devfd);
	return -1;
    }
    close (podfd);
    close (devfd);
    
    // mount it
    if (mount (devstr, mod->mountpt, "podfs", MS_RDONLY, 0) < 0) {
	pz_perror ("mount");
	free (mod->mountpt);
	mod->mountpt = 0;
	return -1;
    }
#else
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
    sprintf (buf, "%s/" MODULE_INF_FILE, mod->mountpt);
    FILE *fp = fopen (buf, "r");
    if (!fp) {
	pz_perror (buf);
	return;
    }

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
	pz_warning ("Unable to create %s's config dir %s: %s", mod->name, mod->cfgpath,
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
    
    if (!str) return;
    
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
		cur->to_load++;
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
    sprintf (fname, "%s/%s.o", mod->mountpt, mod->name);
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
	if (!mod->init) pz_warning ("Could not do modinit function for %s: %s", mod->name, uCdl_error());
#else
	mod->init = dlsym (mod->handle, "__init_module__");
	if (!mod->init) pz_warning ("Could not do modinit function for %s: %s", mod->name, dlerror());
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


void pz_modules_init() 
{
#ifdef IPOD
#define MODULEDIR "/usr/share/podzilla/modules/"
#else
#define MODULEDIR "pods/"
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
	strcpy (podpath, MODULEDIR); // has trailing /
	strcat (podpath, d->d_name);
#ifdef MountPods
	if (stat (podpath, &st) < 0 || !S_ISREG (st.st_mode)) {
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
	cur->to_load = 1;
    }
    closedir (dp);

    if (!module_head) {
	pz_message_title (_("Warning"), _("No modules. Podzilla will probably be very boring."));
	return;
    }

    // Mount 'em
    cur = module_head;
    last = 0;
    while (cur) {
	if (mount_pod (cur) == -1) {
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
	    }
	}
	cur = cur->next;
    }

    // XXX. For now, we load them in directory order. That will
    // wreak havoc with dependencies. davidc__ is working on a 
    // better solution.
    cur = module_head;
    while (cur) {
	if (cur->to_load > 0) {
	    do_load (cur);
	} else if (cur->to_load < 0) {
	    (*cur->init)();
	    cur->to_load = 0;
	}
	cur = cur->next;
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
