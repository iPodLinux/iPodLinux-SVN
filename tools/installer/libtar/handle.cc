/*
**  Copyright 1998-2003 University of Illinois Board of Trustees
**  Copyright 1998-2003 Mark D. Roth
**  All rights reserved.
**
**  handle.c - libtar code for initializing a TAR handle
**
**  Mark D. Roth <roth@uiuc.edu>
**  Campus Information Technologies and Educational Services
**  University of Illinois at Urbana-Champaign
*/

#include "libtar.h"
#include <stdlib.h>
#include <string.h>

const char libtar_version[] = "1.3.11-rawpod";

static int
tar_init(TAR **t, const char *pathname, tartype_t *type, int options)
{
	*t = (TAR *)calloc(1, sizeof(TAR));
	if (*t == NULL)
		return -1;

	(*t)->pathname = pathname;
	(*t)->options = options;
	(*t)->type = type;

        (*t)->h = libtar_hash_new(256,
                                  (libtar_hashfunc_t)path_hashfunc);
	if ((*t)->h == NULL)
	{
		free(*t);
		return -1;
	}

	return 0;
}


/* open a new tarfile handle */
int
tar_open(TAR **t, const char *pathname, tartype_t *type, int options)
{
	if (tar_init(t, pathname, type, options) == -1)
		return -1;

#ifdef O_BINARY
	oflags |= O_BINARY;
#endif

	(*t)->fh = (*((*t)->type->openfunc))(pathname, O_RDONLY);
	if (!(*t)->fh || (*t)->fh->error())
	{
		free(*t);
		return -1;
	}

	return 0;
}


int
tar_fhopen(TAR **t, VFS::File *fh, const char *pathname, tartype_t *type, int options)
{
	if (tar_init(t, pathname, type, options) == -1)
		return -1;

	(*t)->fh = fh;
	return 0;
}


VFS::File *
tar_fh (TAR *t)
{
	return t->fh;
}


/* close tarfile handle */
int
tar_close(TAR *t)
{
	int i;

	i = (*(t->type->closefunc))(t->fh);

	if (t->h != NULL)
            libtar_hash_free(t->h, free);
	free(t);

	return i;
}


