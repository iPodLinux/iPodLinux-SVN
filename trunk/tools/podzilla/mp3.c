/*
 * Copyright (C) 2004 Bernard Leach
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

#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pz.h"

int is_mp3_type(char *extension)
{
	return strcmp(extension, ".mp3") == 0;
}


void new_mp3_window(char *filename)
{
	pid_t pid;

	GrClose();

	pid = vfork();
	if (pid == 0) {
		execl("/sbin/mp3example", "mp3example", filename);
		fprintf(stderr, "exec failed!\n");
		exit(1);
	}
	else {
		if (pid > 0) {
			int status;

			waitpid(pid, &status, 0);
		}
		else {
			fprintf(stderr, "vfork failed %d\n", pid);
		}
	}

	execl("/sbin/podzilla", "podzilla");
	fprintf(stderr, "Cannot restart podzilla!\n");
	exit(1);
}
