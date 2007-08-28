/*
 * iod.c		- Iterate a function on each entry of a directory
 *
 * Copyright (C) 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                           Laboratoire MASI, Institut Blaise Pascal
 *                           Universite Pierre et Marie Curie (Paris VI)
 *
 * This file can be redistributed under the terms of the GNU Library General
 * Public License
 */

/*
 * History:
 * 93/10/30	- Creation
 */

#include "e2p.h"
#include <stdlib.h>
#include <string.h>

int iterate_on_dir (const char * dir_name,
		    int (*func) (const char *, struct dirent *, void *),
		    void * priv)
{
	DIR * dir;
	struct dirent *de, *dep;
	int	max_len = 256, len;

	max_len += sizeof(struct dirent);

	de = (struct dirent *)malloc(max_len+1);
	if (!de)
		return -1;
	memset(de, 0, max_len+1);

	dir = opendir (dir_name);
	if (dir == NULL) {
		free(de);
		return -1;
	}
	while ((dep = readdir (dir))) {
		len = sizeof(struct dirent);
		memcpy(de, dep, len);
		(*func) (dir_name, de, priv);
	}
	free(de);
	closedir(dir);
	return 0;
}
