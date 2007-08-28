/*
**  Copyright 1998-2003 University of Illinois Board of Trustees
**  Copyright 1998-2003 Mark D. Roth
**  All rights reserved.
**
**  output.c - libtar code to print out tar header blocks
**
**  Mark D. Roth <roth@uiuc.edu>
**  Campus Information Technologies and Educational Services
**  University of Illinois at Urbana-Champaign
*/

#include "libtar.h"
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <time.h>
#include <limits.h>

#ifndef _POSIX_LOGIN_NAME_MAX
# define _POSIX_LOGIN_NAME_MAX	9
#endif


void
th_print(TAR *t)
{
	puts("\nPrinting tar header:");
	printf("  name     = \"%.100s\"\n", t->th_buf.name);
	printf("  mode     = \"%.8s\"\n", t->th_buf.mode);
	printf("  uid      = \"%.8s\"\n", t->th_buf.uid);
	printf("  gid      = \"%.8s\"\n", t->th_buf.gid);
	printf("  size     = \"%.12s\"\n", t->th_buf.size);
	printf("  mtime    = \"%.12s\"\n", t->th_buf.mtime);
	printf("  chksum   = \"%.8s\"\n", t->th_buf.chksum);
	printf("  typeflag = \'%c\'\n", t->th_buf.typeflag);
	printf("  linkname = \"%.100s\"\n", t->th_buf.linkname);
	printf("  magic    = \"%.6s\"\n", t->th_buf.magic);
	/*printf("  version  = \"%.2s\"\n", t->th_buf.version); */
	printf("  version[0] = \'%c\',version[1] = \'%c\'\n",
	       t->th_buf.version[0], t->th_buf.version[1]);
	printf("  uname    = \"%.32s\"\n", t->th_buf.uname);
	printf("  gname    = \"%.32s\"\n", t->th_buf.gname);
	printf("  devmajor = \"%.8s\"\n", t->th_buf.devmajor);
	printf("  devminor = \"%.8s\"\n", t->th_buf.devminor);
	printf("  prefix   = \"%.155s\"\n", t->th_buf.prefix);
	printf("  padding  = \"%.12s\"\n", t->th_buf.padding);
	printf("  gnu_longname = \"%s\"\n",
	       (t->th_buf.gnu_longname ? t->th_buf.gnu_longname : "[NULL]"));
	printf("  gnu_longlink = \"%s\"\n",
	       (t->th_buf.gnu_longlink ? t->th_buf.gnu_longlink : "[NULL]"));
}

void strmode (mode_t mode, char *str)
{
    switch (mode & S_IFMT) {
    case S_IFREG:
	str[0] = '-';
	break;
    case S_IFBLK:
	str[0] = 'b';
	break;
    case S_IFCHR:
	str[0] = 'c';
	break;
    case S_IFDIR:
	str[0] = 'd';
	break;
    case S_IFLNK:
	str[0] = 'l';
	break;
    case S_IFIFO:
	str[0] = 'p';
	break;
    case S_IFSOCK:
	str[0] = 's';
	break;
    }

    str[1]  = (mode & S_IRUSR) ? 'r' : '-';
    str[2]  = (mode & S_IWUSR) ? 'w' : '-';
    str[3]  = (mode & S_IXUSR) ? 'x' : '-';
    str[4]  = (mode & S_IRGRP) ? 'r' : '-';
    str[5]  = (mode & S_IWGRP) ? 'w' : '-';
    str[6]  = (mode & S_IXGRP) ? 'x' : '-';
    str[7]  = (mode & S_IROTH) ? 'r' : '-';
    str[8]  = (mode & S_IWOTH) ? 'w' : '-';
    str[9]  = (mode & S_IXOTH) ? 'x' : '-';
    str[10] = '\0';

    if (mode & S_ISUID) str[3] = (str[3] == 'x') ? 's' : 'S';
    if (mode & S_ISGID) str[6] = (str[6] == 'x') ? 's' : 'S';
    if (mode & S_ISVTX) str[9] = (str[9] == 'x') ? 't' : 'T';    
}

void
th_print_long_ls(TAR *t)
{
	char modestring[12];
	struct passwd *pw;
	struct group *gr;
	uid_t uid;
	gid_t gid;
	char username[_POSIX_LOGIN_NAME_MAX];
	char groupname[_POSIX_LOGIN_NAME_MAX];
	time_t mtime;
	struct tm *mtm;

#ifdef HAVE_STRFTIME
	char timebuf[18];
#else
	const char *months[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
#endif

	uid = th_get_uid(t);
        snprintf(username, sizeof(username), "%d", uid);

	gid = th_get_gid(t);
        snprintf(groupname, sizeof(groupname), "%d", gid);

	strmode(th_get_mode(t), modestring);
	printf("%.10s %-8.8s %-8.8s ", modestring, username, groupname);

	if (TH_ISCHR(t) || TH_ISBLK(t))
		printf(" %3d, %3d ", th_get_devmajor(t), th_get_devminor(t));
	else
		printf("%9ld ", (long)th_get_size(t));

	mtime = th_get_mtime(t);
	mtm = localtime(&mtime);
#ifdef HAVE_STRFTIME
	strftime(timebuf, sizeof(timebuf), "%h %e %H:%M %Y", mtm);
	printf("%s", timebuf);
#else
	printf("%.3s %2d %2d:%02d %4d",
	       months[mtm->tm_mon],
	       mtm->tm_mday, mtm->tm_hour, mtm->tm_min, mtm->tm_year + 1900);
#endif

	printf(" %s", th_get_pathname(t));

	if (TH_ISSYM(t) || TH_ISLNK(t))
	{
		if (TH_ISSYM(t))
			printf(" -> ");
		else
			printf(" link to ");
		if ((t->options & TAR_GNU) && t->th_buf.gnu_longlink != NULL)
			printf("%s", t->th_buf.gnu_longlink);
		else
			printf("%.100s", t->th_buf.linkname);
	}

	putchar('\n');
}


