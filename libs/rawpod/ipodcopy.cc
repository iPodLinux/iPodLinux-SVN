#include "device.h"
#include "vfs.h"
#include "partition.h"
#include "ext2.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <dirent.h>
#ifdef WIN32
#include <windows.h>
#endif

int recursive = 0;
Ext2FS *ext2;

int is_local (const char *path) 
{
    if (strchr (path, ':') && (strchr (path, ':') != path + 1))
        return 0;
    return 1;
}

int is_dir (const char *filename) 
{
    if (is_local (filename)) {
#ifdef WIN32
        DWORD attr = GetFileAttributes (filename);
        if (attr == INVALID_FILE_ATTRIBUTES) {
            TCHAR err[512];
            FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, 0, GetLastError(), 0, err, 512, 0);
            for (TCHAR *p = err; *p; p++) {
                putchar (*p);
            }
            printf ("\nExiting.\n");
            exit (1);
        }
        return !!(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
        struct stat st;
        if (::stat (filename, &st) < 0)
            return 0;
        return ((st.st_mode & S_IFMT) == S_IFDIR);
#endif
    } else {
        const char *ext2path = strchr (filename, ':') + 1;
        struct my_stat st;
        if (ext2->lstat (ext2path, &st) < 0)
            return 0;
        return ((st.st_mode & S_IFMT) == S_IFDIR);
    }
}

void copy (const char *src, const char *dest)
{
    static int copynr = 0;
    int srcisdir = 0, destisdir = 0;

    copynr++;

    srcisdir = is_dir (src);
    destisdir = is_dir (dest);
    
    if (copynr > 1 && !destisdir) {
        fprintf (stderr, "ipodcp[%d]: copying multiple files, but target is not a directory\n", copynr);
        exit (1);
    }

    if (srcisdir && !recursive) {
        fprintf (stderr, "ipodcp[%d]: skipping directory %s - if you want it copied, use -R\n", copynr, src);
        return;
    }

    char *dst;
    
    if (destisdir) {
        const char *basename = strrchr (src, '/');
        if (basename) basename++;
        else {
            basename = src;
            if (strchr (basename, ':')) basename = strchr (basename, ':') + 1;
        }
        
        dst = new char[strlen (dest) + strlen (basename) + 2];
        sprintf (dst, "%s/%s", dest, basename);
    } else {
        dst = strdup (dest);
    }

    int dstisdir = is_dir (dst);

    if (dstisdir && !srcisdir) {
        fprintf (stderr, "ipodcp[%d]: can't replace directory %s with non-directory %s\n",
                 copynr, dst, src);
        delete[] dst;
        return;
    }

    if (srcisdir) {
        if (!dstisdir) {
            printf ("making directory -> %s\n", dst);
            if (is_local (dst)) {
#ifdef WIN32
                CreateDirectory (dst, NULL);
#else
                mkdir (dst, 0777);
#endif
            } else {
                ext2->mkdir (strchr (dst, ':') + 1);
            }
        }

        printf ("%s -> descending into\n", src);

        VFS::Dir *dp;
        struct VFS::dirent de;
        
        if (is_local (src))
            dp = new LocalDir (src);
        else
            dp = ext2->opendir (src);
        if (dp->error()) {
            fprintf (stderr, "ipodcp[%d]: %s: %s\n", copynr, src, strerror (dp->error()));
            return;
        }

        while (dp->readdir (&de)) {
            if (!strcmp (de.d_name, ".") || !strcmp (de.d_name, ".."))
                continue;

            char *sub = new char[strlen (src) + strlen (de.d_name) + 2];
            sprintf (sub, "%s/%s", src, de.d_name);
            copy (sub, dst);
            delete[] sub;
        }
        delete[] dst;
        return;
    }

    VFS::File *sfp, *dfp;

    if (is_local (src))
        sfp = new LocalFile (src, OPEN_READ);
    else
        sfp = ext2->open (strchr (src, ':') + 1, O_RDONLY);    

    if (sfp->error()) {
        fprintf (stderr, "ipodcp[%d]: %s: %s\n", copynr, src, strerror (sfp->error()));
        delete sfp;
        delete[] dst;
        return;
    }

    if (is_local (dst)) {
        unlink (dst);
        dfp = new LocalFile (dst, OPEN_WRITE | OPEN_CREATE);
    } else {
        ext2->unlink (strchr (dst, ':') + 1);
        dfp = ext2->open (strchr (dst, ':') + 1, O_WRONLY | O_CREAT);
    }

    if (dfp->error()) {
        fprintf (stderr, "ipodcp[%d]: %s: %s\n", copynr, dst, strerror (dfp->error()));
        delete sfp;
        delete dfp;
        delete[] dst;
        return;
    }

    printf ("%s -> %s\n", src, dst);

    char buf[4096];
    int rdlen;
    int off = 0;
    while ((rdlen = sfp->read (buf, 4096)) > 0) {
        int err;
        if ((err = dfp->write (buf, rdlen)) < 0) {
            fprintf (stderr, "ipodcp[%d]: writing %s: %s\n", copynr, dst, strerror (-err));
            break;
        }
        off += rdlen;
        printf ("\r%6dKB\r", off>>10);
    }
    if (rdlen < 0) {
        fprintf (stderr, "ipodcp[%d]: reading %s: %s\n", copynr, src, strerror (-rdlen));
    }

    sfp->close();
    dfp->close();
    delete sfp;
    delete dfp;
    delete[] dst;
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
    ext2->setWritable (1);
    if (ext2->init() < 0) {
        // error already printed
        return 1;
    }

    ++argv; --argc;
    if (*argv && (**argv == '-')) {
        ++*argv;
        while (**argv) {
            if (**argv == 'r' || **argv == 'R') recursive = 1;
            ++*argv;
        }
        ++argv; --argc;
    }

    if (argc < 2) {
        fprintf (stderr,
                 "Usage: ipodcopy [-R] SRC DEST\n"
                 "       ipodcopy [-R] FILE [FILE [...]] DIRECTORY\n\n"

                 "  Each FILE is assumed to refer to the local filesystem unless\n"
                 "  it contains a colon, in which case it is assumed to refer to\n"
                 "  the iPod.  Exception: drive letter references (d:/foo) are assumed\n"
                 "  to be local. Do NOT refer to your iPod by its drive letter!\n\n"

                 "Example: ipodcopy podzilla iPod:/bin/podzilla\n\n");

        return 1;
    }

    if (argc == 2) {
        copy (argv[0], argv[1]);
    } else {
        char *dest = argv[argc-1];
        argv[argc-1] = 0;
        while (*argv)
            copy (*argv++, dest);
    }

    delete ext2;

    return 0;
}
