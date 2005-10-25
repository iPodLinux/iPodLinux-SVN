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
#ifdef linux
#include <sys/mount.h>
#include <linux/loop.h>
#else
#ifdef MountPods
#error can only mount pods under Linux
#endif
#endif

#include "pz.h"
#include "ucdl.h"

typedef struct _pz_Module
{
    const char *name;
    const char *longname;
    const char *author;
    const char *signature;
    int verified;

    struct _pz_Module **deps; // terminated with a 0 entry
    const char *podpath;
    const char *mountpt;
    const char *cfgpath;
    void *handle;

    struct _pz_Module *next;
} PzModule;

static PzModule *module_head;

#ifdef IPOD
#define MountPods
#else
#ifdef MountPods
#warning You need to: have a 2.6 kernel; run podzilla as root; make module-2.6, load podfs.ko; pass max_loop=128 on the kernel command line; and be using devfs. As you can see, we do not recommend this.
#endif
#endif

// sets mod->mountpt = 0 on failure
static void mount_pod (PzModule *mod) 
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
	return;
    }
    podfd = open (mod->podpath, O_RDONLY);
    if (podfd < 0) {
	pz_perror (mod->podpath);
	free (mod->mountpt);
	mod->mountpt = 0;
	close (devfd);
	return;
    }
    
    ioctl (devfd, LOOP_CLR_FD, 0);
    if (ioctl (devfd, LOOP_SET_FD, podfd) < 0) {
	pz_perror ("LOOP_SET_FD");
	free (mod->mountpt);
	mod->mountpt = 0;
	close (podfd);
	close (devfd);
	return;
    }
    close (podfd);
    close (devfd);
    
    // mount it
    if (mount (devstr, mod->mountpt, "podfs", MS_RDONLY, 0) < 0) {
	pz_perror ("mount");
	free (mod->mountpt);
	mod->mountpt = 0;
	return;
    }
#else
    mod->mountpt = malloc (strlen ("xpods/") + strlen (strrchr (mod->podpath, '/')) + 1);
    sprintf (mod->mountpt, "xpods/%s", strrchr (mod->podpath, '/') + 1);
    if (stat (mod->mountpt, &st) < 0 || !S_ISDIR (st.st_mode)) {
	pz_error ("Couldn't access xpod dir for %s: %s", mod->mountpt, strerror (errno));
	free (mod->mountpt);
	mod->mountpt = 0;
	return;
    }
#endif
}

void pz_modules_init() 
{
#ifdef IPOD
#define MODULEDIR "/usr/share/podzilla/modules/"
#else
#define MODULEDIR "modules/"
#endif

    struct stat st;
    PzModule *cur;
    int nmods = 0;
    struct dirent *d;
    DIR *dp = opendir (MODULEDIR);
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
	char *podpath = malloc (strlen (MODULEDIR) + strlen (d->d_name) + 1);
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
	}

	if (module_head == 0) {
	    cur = module_head = malloc (sizeof(PzModule));
	} else {
	    cur = module_head;
	    while (cur->next) cur = cur->next;
	    cur->next = malloc (sizeof(PzModule));
	    cur = cur->next;
	}
    }
    closedir (dp);

    if (!module_head) {
	pz_message_title (_("Warning"), _("No modules."));
	return;
    }

    // Mount 'em
    cur = module_head;
    while (cur) {
	mount_pod (cur);
	if (!cur->mountpt)
	    pz_error ("Module %s will not be loaded.\n", strrchr (cur->podpath, "/"));
	cur = cur->next;
    }

    // . . .
}

