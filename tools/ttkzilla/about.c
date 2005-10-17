/*
 * Copyright (C) 2004, 2005 Courtney Cavin
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define PZMOD
#include "pz.h"
#include "ipod.h"

#define BUFSIZR 2048
#define NUM_GENS 8

static char *buf = NULL;

extern TWindow *new_stringview_window(char *buf, char *title);

static void populate_credits() {
	int i, len = 0;
	char *cnames[] = {"Bernard Leach", "Matthew J. Sahagian",
			"Courtney Cavin", "matz-josh", "Matthis Rouch",
		       	"ansi", "Jens Taprogge", "Fredrik Bergholtz",
			"Jeffrey Nelson", "Scott Lawrence",
			"Cameron Nishiyama", "Prashant V", "Alastair Stuart",
			"David Carne", "Nik Rolls", "Filippo Forlani", 
			"Martin Kaltenbrunner", "Adam Johnston",
		        "Matthew Westcott", "Nils Schneider", "Damien Marchal",
			"Jonathan Bettencourt", "Joshua Oreman", 0};
	for (i = 0; cnames[i] != 0; i++)
		len += strlen(cnames[i]) + 9;
	buf = malloc(sizeof(char) * (len + 47));
	strcpy(buf, "Brought to you by:\n\n");
	for (i = 0; cnames[i] != 0; i++) {
		strcat(buf, "        ");
		strcat(buf, cnames[i]);
		strcat(buf, "\n");
	}
	strcat(buf, "\nhttp://www.ipodlinux.org\n");
}

static void populate_about() {
	int i;
	char kern[BUFSIZR], fstype[BUFSIZR];
	char *gens[NUM_GENS] = {"", "1st Generation", "2nd Generation",
		"3rd Generation", "Mini", "4th Generation", "Photo",
		"2nd Gen Mini"};
#ifdef IPOD
	char fslist[BUFSIZR], fsmount[11];
#endif
	FILE *ptr;

#if defined(__linux__) || defined(IPOD)
	if((ptr = popen("uname -rv", "r")) != NULL) {
#else
	if((ptr = popen("uname -r", "r")) != NULL) {
#endif
		while(fgets(kern, BUFSIZR, ptr)!=NULL);
		pclose(ptr);
	}

#ifdef IPOD
	if((ptr = fopen("/proc/mounts", "r")) != NULL) {
		while(fgets(fslist, BUFSIZR, ptr)!=NULL) {
			sscanf(fslist, "%s%*s%s", fsmount, fstype);
			if(strcmp(fsmount, "/dev/root")==0)
				break;
		}
		if(strncmp(fstype, "hfs", 3)==0)
			sprintf(fstype, "Mac iPod.");
		else
			sprintf(fstype, "Win iPod.");
		fclose(ptr);
	}
#else
	sprintf(fstype, "Not an iPod.");
#endif

	i = (int)(hw_version/10000);
	if (i > NUM_GENS || i < 0)
		i = 0;

	buf = malloc(sizeof(char) * (18 + strlen(PZ_VER) + strlen(kern) +
				strlen(fstype) + strlen(gens[i])));
	sprintf(buf, "%s\n\n%s\n%s  %s\n    Rev. %ld", PZ_VER, kern,
			fstype, gens[i], hw_version);
}

TWindow *about_podzilla()
{
	if (buf) {
	    free (buf);
	    buf = NULL;
	}
	populate_about();
	return new_stringview_window(buf, "About");
}

TWindow *show_credits()
{
	if (buf) {
	    free (buf);
	    buf = NULL;
	}
	populate_credits();
	return new_stringview_window(buf, "Credits");
}

