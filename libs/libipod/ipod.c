/*
 * ipod.c - generic iPod library
 *
 * Copyright (C) 2005 Courtney Cavin, Alastair Stuart, Bernard Leach 
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
#include <limits.h>
#include <unistd.h>

#include "ipod.h"
#include "ipodhardware.h"

long ipod_get_hw_version(void) 
{
	static long generation = -1;
	if (generation == -1) {
		int i;
		char cpuinfo[256];
		char *ptr;
		FILE *file;

		if ((file = fopen(CPUINFOLOC, "r")) != NULL) {
			while (fgets(cpuinfo, sizeof(cpuinfo), file) != NULL)
				if (strncmp(cpuinfo, "Revision", 8) == 0)
					break;
			fclose(file);
			for (i = 0; !isspace(cpuinfo[i]); i++);
			for (; isspace(cpuinfo[i]); i++);
			ptr = cpuinfo + i + 2;

			generation = strtol(ptr, NULL, 16);
		} else {
			generation = 0;
		}
	}
	
	return generation;
}

char ipod_get_fs_type(void)
{
	FILE *fp;
	char ret = 'U';
	char line[256];
	char mount[32];
	char fstype[32];

	if ((fp = fopen(MOUNTSLOC, "r")) != NULL) {
		while (fgets(line, 255, fp) != NULL) {
			sscanf(line, "%s%*s%s", mount, fstype);
			if (strcmp(mount, "/dev/root") == 0)
				break;
		}
		if (strncmp(fstype, "hfs", 3) == 0)
			ret = 'H';
		else
			ret = 'F';
		fclose(fp);
	}
	return ret;
}

void ipod_beep(void)
{
#ifdef IPOD
	if (ipod_get_hw_version() >= 0x40000) {
		int i, j;
		outl(inl(0x70000010) & ~0xc, 0x70000010);
		outl(inl(0x6000600c) | 0x20000, 0x6000600c); /* enable device */
		for (j = 0; j < 10; j++) {
			for (i = 0; i < 0x888; i++ ) {
				outl(0x80000000 | 0x800000 | i, 0x7000a000); /* set pitch */
			}
		}
		outl(0x0, 0x7000a000); /* piezo off */
	} else {
		static int fd = -1; 
		static char buf;

		if (fd == -1 && (fd = open("/dev/ttyS1", O_WRONLY)) == -1
				&& (fd = open("/dev/tts/1", O_WRONLY)) == -1) {
			return;
		}
		
		write(fd, &buf, 1);
	}
#else
	if (isatty(1)) {
		printf("\a");
	}
#endif
}

int ipod_read_apm(int *battery, int *charging)
{
#ifdef IPOD
	FILE *file;
	int ac_line_status = 0xff;
	int battery_status = 0xff;
	int battery_flag = 0xff;
	int percentage = -1;
	int time_units = -1;

	if ((file = fopen(APMLOC, "r")) != NULL) {
		fscanf(file, "%*s %*d.%*d 0x%*02x 0x%02x 0x%02x 0x%02x %d%% %d",
		       &ac_line_status, &battery_status,
		       &battery_flag, &percentage, &time_units);
		fclose(file);

		if (battery) {
			*battery = time_units;
		}
		if (charging) {
			*charging = (battery_status != 0xff && battery_status & 0x3) ? 1 : 0;
		}

		return 0;
	}
	if (battery) *battery = 512;
	if (charging) *charging = 0;
#else
	int t = time (0);
	int u = t/2, v = u/4;
	if (battery) {
		if ((u % 4) == 0) {
			*battery = 512;
		} else {
			srand ((unsigned)time (0));
			*battery = rand() % 512;
		}
	}
	if (charging) {
		if ((v % 2) == 0) {
			*charging = 1;
		} else {
			*charging = 0;
		}
	}
#endif
	return 0;
}

