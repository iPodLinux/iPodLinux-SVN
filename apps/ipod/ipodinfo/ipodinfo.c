/*
 * ipodinfo.c - output iPod revision and fstype
 *
 * Copyright (C) 2005 Courtney Cavin, Alastair Stuart
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

#define CPUINFOLOC "/proc/cpuinfo"
#define MOUNTSLOC "/proc/mounts"

int axtoi(char *hex) {
	int i, m, count;
	int intvalue = 0;
	int digit[16];
	char *ptr = hex;

	for (i = 0; i < 16; i++) {
		if (*ptr == '\0')
			break;
		if (*ptr > 0x29 && *ptr < 0x40)
			digit[i] = *ptr & 0x0f;
		else if (*ptr >='a' && *ptr <= 'f')
			digit[i] = (*ptr & 0x0f) + 9;
		else if (*ptr >='A' && *ptr <= 'F')
			digit[i] = (*ptr & 0x0f) + 9;
		else
			break;
		ptr++;
	}
	count = i;
	m = i - 1;

	for (i = 0; i < count; i++) {
		intvalue = intvalue | (digit[i] << (m << 2));
		m--;
	}
	return intvalue;
}

char ipod_get_fs_type()
{
	FILE *fp;
	char ret;
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

long ipod_get_hw_version(void)
{
	int i;
	char cpuinfo[512];
	char *ptr;
	FILE *file;

	if ((file = fopen(CPUINFOLOC, "r")) != NULL) {
		while (fgets(cpuinfo, sizeof(cpuinfo), file) != NULL)
			if (strncmp(cpuinfo, "Revision", 8) == 0)
				break;
		fclose(file);
	} else {
		return 0;
	}
	for (i = 0; !isspace(cpuinfo[i]); i++);
	for (; isspace(cpuinfo[i]); i++);
	ptr = cpuinfo + i + 2;

	return axtoi(ptr);
}

int main(int argc, char **argv) {
	char ch;
	char buf[64];

	while ((ch = getopt(argc, argv, "rf")) != -1) {
		switch (ch) {
		case 'r':
			sprintf(buf, "%ld", ipod_get_hw_version());
			break;
		case 'f':
			sprintf(buf, "%c", ipod_get_fs_type());
			break;
		case '?':
		default:
			fprintf(stderr, "%s [-r|-f]\n"
					"\t-r  get ipod revision\n"
					"\t-f  get filesystem type F|H\n",
					argv[0]);
			return 1;
		}
	}
	puts(buf);
	return 0;
}
