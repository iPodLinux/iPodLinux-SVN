/*
 * Copyright (C) 2004 David Carne, matz-josh
 * Copyright (C) 2005 Courtney Cavin
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PZMOD
#include "pz.h"

int is_text_type(char * extension)
{
	return strcmp(extension, ".txt") == 0;
}

int is_ascii_file(char *filename)
{
	FILE *fp;
	unsigned char buf[20], *ptr;
	long file_len;
	struct stat ftype; 

	stat(filename, &ftype); 
	if(S_ISBLK(ftype.st_mode)||S_ISCHR(ftype.st_mode))
		return 0;
	if((fp=fopen(filename, "r"))==NULL) {
		perror(filename);
		return 0;
	}
	fseek(fp, 0, SEEK_END);
	file_len = ftell(fp);
	rewind(fp);
	fread(buf, file_len<20?file_len:20, 1, fp);
	fclose(fp);
	
	for(ptr=buf;ptr-buf<(file_len<20?file_len:20);ptr++)
		if(*ptr<7||*ptr>127)
			return 0;
	return 1;
}

TWindow *new_textview_window(char *filename)
{
    struct stat st;
    if (stat (filename, &st) < 0)
	return 0;
    char *buf = malloc (st.st_size);
    if (!buf)
	return 0;
    int fd = open (filename, O_RDONLY);
    if (fd < 0)
	return 0;
    read (fd, buf, st.st_size);
    close (fd);
    
    TWindow *ret = ttk_new_window();
    ttk_add_widget (ret, ttk_new_textarea_widget (ret->w, ret->h, buf, ttk_textfont, ttk_text_height (ttk_textfont) + 2));
    ttk_window_title (ret, strrchr (filename, '/')? strrchr (filename, '/') + 1 : filename);
    free (buf); // strdup'd in textarea
    return ret;
}

TWindow *new_stringview_window(char *buf, char *title)
{
    TWindow *ret = ttk_new_window();
    ttk_add_widget (ret, ttk_new_textarea_widget (ret->w, ret->h, buf, ttk_textfont, ttk_text_height (ttk_textfont) + 2));
    ttk_window_title (ret, title);
    return ret;
}

