/*
 * Copyright (C) 2004-2006 Rebecca G. Bettencourt
 * http://www.kreativekorp.com
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/param.h>
#include <sys/ioctl.h>

#define FALSE 0
#define TRUE !FALSE

extern int alphasort();

static int term_rows()
{
	struct winsize win;
	ioctl(1, TIOCGWINSZ, &win);
	return win.ws_row;
}

static int term_cols()
{
	struct winsize win;
	ioctl(1, TIOCGWINSZ, &win);
	return win.ws_col;
}

int main(int argc, char **argv)
{
	char pathname[MAXPATHLEN];
	int count, i;
	struct direct **files;
	int file_select();
	int ch;
	int lines = term_rows();
	if (argc < 2)
	{
		if ( getcwd(pathname, MAXPATHLEN) == NULL )
		{
			printf("Error getting path.\n");
			return 0;
		}
	} else {
		strncpy(pathname, argv[1], MAXPATHLEN);
	}
	/* printf("Current Working Directory = %s\n",pathname); */
	count = scandir(pathname, &files, file_select, alphasort);
	if (count == 0)
	{
		/* printf("No files in this directory.\n"); */
	} else {
		/* printf("Number of files = %d.\n",count); */
		for (i = 0; i < count; ++i)
		{
			printf("%s\n",files[i]->d_name);
			if ((i % (lines-1)) == (lines-2))
			{
				printf("--More--");
				while ((ch = getchar()) != '\n' && ch != EOF);
				printf("\n");
			}
		}
	}
	return 0;
}

int file_select(struct direct * entry)
{
	if ((strcmp(entry->d_name, ".") == 0) || (strcmp(entry->d_name, "..") == 0))
	{
		return FALSE;
	} else {
		return TRUE;
	}
}
