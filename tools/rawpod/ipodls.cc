#include "device.h"
#include "vfs.h"
#include "partition.h"
#include "ext2.h"
#include <string.h>
#include <time.h>

int lflag = 0;
Ext2FS *ext2;

void print_file (const char *name, struct stat *st, const char *linktarget)
{
#define BRIGHT (1<<15)
    int color = 0;
    char endch = ' ';
    char mode[11];
    char date[14];
    struct tm tmdate, tmnow;
    time_t datesec;

    switch (st->st_mode & S_IFMT) {
    case S_IFREG:
        mode[0] = '-'; color = 0; endch = ' ';
        if (st->st_mode & 0111)
            color = 32 | BRIGHT, endch = '*';
        break;
    case S_IFBLK:
        mode[0] = 'b'; color = 33 | BRIGHT; endch = '#';
        break;
    case S_IFCHR:
        mode[0] = 'c'; color = 33 | BRIGHT; endch = '%';
        break;
    case S_IFDIR:
        mode[0] = 'd'; color = 34 | BRIGHT; endch = '/';
        break;
    case S_IFLNK:
        mode[0] = 'l'; color = 36 | BRIGHT; endch = '@';
        break;
    case S_IFIFO:
        mode[0] = 'p'; color = 33; endch = '|';
        break;
    case S_IFSOCK:
        mode[0] = 's'; color = 35 | BRIGHT; endch = '=';
        break;
    }

    mode[1]  = (st->st_mode & 0400) ? 'r' : '-';
    mode[2]  = (st->st_mode & 0200) ? 'w' : '-';
    mode[3]  = (st->st_mode & 0100) ? 'x' : '-';
    mode[4]  = (st->st_mode & 0040) ? 'r' : '-';
    mode[5]  = (st->st_mode & 0020) ? 'w' : '-';
    mode[6]  = (st->st_mode & 0010) ? 'x' : '-';
    mode[7]  = (st->st_mode & 0004) ? 'r' : '-';
    mode[8]  = (st->st_mode & 0002) ? 'w' : '-';
    mode[9]  = (st->st_mode & 0001) ? 'x' : '-';
    mode[10] = '\0';

    if (st->st_mode & S_ISUID) mode[3] = (mode[3] == 'x') ? 's' : 'S';
    if (st->st_mode & S_ISGID) mode[6] = (mode[6] == 'x') ? 's' : 'S';
    if (st->st_mode & S_ISVTX) mode[9] = (mode[9] == 'x') ? 't' : 'T';

    datesec = st->st_mtime;
    localtime_r (&datesec, &tmdate);
    datesec = time (0);
    localtime_r (&datesec, &tmnow);
    if ((((tmdate.tm_mon < tmnow.tm_mon) ||
	  ((tmdate.tm_mon == tmnow.tm_mon) && (tmdate.tm_mday < tmnow.tm_mday))) &&
	 (tmdate.tm_year < tmnow.tm_year)) ||
	(tmdate.tm_year > tmnow.tm_year))
	strftime (date, 13, "%b %d  %Y", &tmdate);
    else
	strftime (date, 13, "%b %d %H:%M", &tmdate);

    if (lflag)
        printf ("%10s %4d %-4d %-4d %8llu %12s ", mode, st->st_nlink, st->st_uid, st->st_gid,
                (u64)st->st_size, date);

    printf ("\033[%s%dm%s\033[0m", (color & BRIGHT)? "1;" : "",
            color & ~BRIGHT, name);

    if (endch != ' ')
        printf ("%c", endch);

    if (lflag && S_ISLNK (st->st_mode)) {
        printf (" -> %s", linktarget);
    }
    printf ("\n");
}

void process_arg (const char *arg) 
{
    struct stat st;
    int err;
    char linkbuf[256];
    if ((err = ext2->lstat (arg, &st)) < 0) {
        printf ("%s: %s\n", arg, strerror (-err));
        return;
    }

    if (S_ISDIR (st.st_mode)) {
        VFS::Dir *dirp = ext2->opendir (arg);
        if (dirp->error()) {
            printf ("%s: %s\n", arg, strerror (dirp->error()));
            return;
        }

        struct VFS::dirent d;
        while (dirp->readdir (&d) > 0) {
            if (!(!strcmp (d.d_name, ".") || !strcmp (d.d_name, ".."))) {
                char *file = new char[strlen (arg) + strlen (d.d_name) + 2];
                sprintf (file, "%s/%s", arg, d.d_name);
                if ((err = ext2->lstat (file, &st)) < 0) {
                    printf ("%s: %s\n", file, strerror (-err));
                } else {
                    if ((st.st_mode & 0770000) == 0120000) {
                        if ((err = ext2->readlink (file, linkbuf, 256)) < 0) {
                            sprintf (linkbuf, "?? [err %d (%s)]", -err, strerror (-err));
                        }
                    }
                    print_file (d.d_name, &st, linkbuf);
                }
            }
        }
        dirp->close();
        delete dirp;
    } else {
        if ((st.st_mode & 0770000) == 0120000) {
            if ((err = ext2->readlink (arg, linkbuf, 256)) < 0) {
                sprintf (linkbuf, "?? [err %d (%s)]", -err, strerror (-err));
            }
        }
        print_file (arg, &st, linkbuf);
    }
}

int main (int argc, char **argv) 
{
    int disknr = find_iPod();
    if (disknr < 0) {
        printf ("iPod not found\n");
        return 1;
    }
    
    VFS::Device *part = setup_partition (disknr, 3);
    ext2 = new Ext2FS (part);
    ext2->setWritable (0);
    if (ext2->init() < 0) {
        // error already printed
        return 1;
    }

    argv++;
    if (*argv && !strcmp (*argv, "-l")) {
        lflag = 1;
        argv++;
    }
    while (*argv) {
        process_arg (*argv++);
    }

    delete ext2;

    return 0;
}
