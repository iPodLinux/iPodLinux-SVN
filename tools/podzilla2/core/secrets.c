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
#include <ctype.h>
#include "pz.h"

static char *secrets;

#ifdef IPOD
#define SECRETS_FILE "/etc/secrets"
#else
#define SECRETS_FILE ".secrets"
#endif

void pz_secrets_init() 
{
    FILE *f = fopen (SECRETS_FILE, "r");
    int len;
    
    if (!f) {
	secrets = 0;
	return;
    }
    fseek (f, 0, SEEK_END);
    len = ftell (f);
    fseek (f, 0, SEEK_SET);
    secrets = malloc (len + 1);
    if (!secrets) {
	pz_error ("Out of memory for secrets list");
	return;
    }
    fread (secrets, len, 1, f);
    fclose (f);
}

int pz_has_secret (const char *key) 
{
    if (strstr (secrets, key) &&
	!isalpha (*(strstr (secrets, key) - 1)))
	return 1;
    return 0;
}
