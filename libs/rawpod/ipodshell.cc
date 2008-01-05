#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "ext2.h"
#include "fat32.h"
#include "partition.h"
using namespace VFS;

VFS::Filesystem *fs;

int main (int argc, char *argv[]) 
{
    int disknr = find_iPod();
    if (disknr < 0) {
        printf ("iPod not found\n");
        return 1;
    }
    
    if (argv[1] && !strcmp (argv[1], "-fat32")) {
        VFS::Device *part = setup_partition (disknr, 2);
        fs = new FATFS (part);
    } else {
        VFS::Device *part = setup_partition (disknr, 3);
        fs = new Ext2FS (part);
    }
    
    if (fs->init() < 0) {
        // error already printed
        return 1;
    }

    printf ("Initialized.\n");

    while (1) {
        char buf[256];
        printf ("> ");
        fgets (buf, 256, stdin);
        buf[strlen (buf) - 1] = 0;

        char *args[16];
        char *p = buf;
        char **argp = args;
        int nargs = 0;
        
        while (p) {
            char *next = strchr (p, ' ');
            while (next && isspace (*next)) next++;
            if (next) *strchr (p, ' ') = 0;
            *argp++ = p;
            nargs++;
            p = next;
        }
        *argp = 0;

        if (args[0][0] == 0)
            continue;
        
        if (!strcmp (args[0], "list")) {
            if (nargs != 2) {
                fprintf (stderr, "Usage: list <dir>\n");
            } else {
                Dir *dp = fs->opendir (args[1]);
                if (!dp) {
                    fprintf (stderr, "%s: error\n", args[1]);
                } else if (dp->error()) {
                    fprintf (stderr, "%s: %s\n", args[1], strerror (dp->error()));
                } else {
                    struct VFS::dirent d;
                    while (dp->readdir (&d)) {
                        printf ("[%8d] %s\n", d.d_ino, d.d_name);
                    }
                    dp->close();
                    delete dp;
                }
            }
        } else if (!strcmp (args[0], "read")) {
            if (nargs != 2) {
                fprintf (stderr, "Usage: read <file>\n");
            } else {
                File *fp = fs->open (args[1], O_RDONLY);
                if (!fp) {
                    fprintf (stderr, "%s: error\n", args[1]);
                } else if (fp->error()) {
                    fprintf (stderr, "%s: %s\n", args[1], strerror (fp->error()));
                } else {
                    char buf[513];
                    int len;
                    while ((len = fp->read (buf, 512)) > 0) {
                        buf[len] = 0;
                        fwrite (buf, len, 1, stdout);
                    }
                    fp->close();
                    delete fp;
                    printf ("<EOF>\n");
                }
            }
        } else if (!strcmp (args[0], "write")) {
            if (nargs != 2) {
                fprintf (stderr, "Usage: write <file>\n");
            } else {
                File *fp = fs->open (args[1], O_WRONLY | O_CREAT);
                if (!fp) {
                    fprintf (stderr, "%s: error\n", args[1]);
                } else if (fp->error()) {
                    fprintf (stderr, "%s: %s\n", args[1], strerror (fp->error()));
                } else {
                    char buf[512];
                    int len;
                    while (fgets (buf, 512, stdin)) {
                        fp->write (buf, strlen (buf));
                    }
                    fp->close();
                    delete fp;
                    printf ("Written.\n");
                }
            }
        } else if (!strcmp (args[0], "truncate")) {
            if (nargs != 2) {
                fprintf (stderr, "Usage: truncate <file>\n");
            } else {
                File *fp = fs->open (args[1], O_WRONLY);
                if (!fp) {
                    fprintf (stderr, "%s: error\n", args[1]);
                } else if (fp->error()) {
                    fprintf (stderr, "%s: %s\n", args[1], strerror (fp->error()));
                } else {
                    fp->truncate();
                    fp->close();
                    delete fp;
                }
            }
        } else if (!strcmp (args[0], "chmod")) {
            if (nargs != 3) {
                fprintf (stderr, "Usage: chmod <file> <mode>\n");
            } else {
                File *fp = fs->open (args[1], O_WRONLY);
                if (!fp) {
                    fprintf (stderr, "%s: error\n", args[1]);
                } else if (fp->error()) {
                    fprintf (stderr, "%s: %s\n", args[1], strerror (fp->error()));
                } else {
                    fp->chmod (strtol (args[2], 0, 8));
                    fp->close();
                    delete fp;
                }
            }
        } else if (!strcmp (args[0], "mkdir")) {
            if (nargs != 2) {
                fprintf (stderr, "Usage: mkdir <dir>\n");
            } else {
                int err;
                if ((err = fs->mkdir (args[1])) < 0) {
                    fprintf (stderr, "%s: %s\n", args[1], strerror (err));
                }
            }
        } else if (!strcmp (args[0], "unlink")) {
            if (nargs != 2) {
                fprintf (stderr, "Usage: unlink <file>\n");
            } else {
                int err;
                if ((err = fs->unlink (args[1])) < 0) {
                    fprintf (stderr, "%s: %s\n", args[1], strerror (err));
                }
            }
        } else if (!strcmp (args[0], "rmdir")) {
            if (nargs != 2) {
                fprintf (stderr, "Usage: rmdir <dir>\n");
            } else {
                int err;
                if ((err = fs->rmdir (args[1])) < 0) {
                    fprintf (stderr, "%s: %s\n", args[1], strerror (err));
                }
            }
        } else if (!strcmp (args[0], "rename")) {
            if (nargs != 3) {
                fprintf (stderr, "Usage: rename <old> <new>\n");
            } else {
                int err;
                if ((err = fs->rename (args[1], args[2])) < 0) {
                    fprintf (stderr, "%s -> %s: %s\n", args[1], args[2], strerror (err));
                }
            }
        } else if (!strcmp (args[0], "link")) {
            if (nargs != 3) {
                fprintf (stderr, "Usage: link <old> <new>\n");
            } else {
                int err;
                if ((err = fs->link (args[1], args[2])) < 0) {
                    fprintf (stderr, "%s <=> %s: %s\n", args[1], args[2], strerror (err));
                }
            }
        } else if (!strcmp (args[0], "symlink")) {
            if (nargs != 3) {
                fprintf (stderr, "Usage: symlink <dest> <link>\n");
            } else {
                int err;
                if ((err = fs->symlink (args[1], args[2])) < 0) {
                    fprintf (stderr, "%s: %s\n", args[2], strerror (err));
                }
            }
        } else if (!strcmp (args[0], "stat") || !strcmp (args[0], "lstat")) {
            if (nargs != 2) {
                fprintf (stderr, "Usage: stat <file>\n");
            } else {
                struct my_stat st;
                int err;
                if (!strcmp (args[0], "lstat"))
                    err = fs->lstat (args[1], &st);
                else
                    err = fs->stat (args[1], &st);

                if (err) {
                    fprintf (stderr, "%s: %s\n", args[1], strerror (err));
                } else {
                    printf ("  File: `%s'\n", args[1]);
                    printf ("  Size: %-15lld Blocks: %-12d %s\n", st.st_size,
                            st.st_blocks, (S_ISREG (st.st_mode)? "regular file" :
                                           S_ISDIR (st.st_mode)? "directory" :
                                           S_ISLNK (st.st_mode)? "symbolic link" :
                                           S_ISCHR (st.st_mode)? "character device" :
                                           S_ISBLK (st.st_mode)? "block device" :
                                           S_ISFIFO(st.st_mode)? "FIFO (named pipe)" :
                                           S_ISSOCK(st.st_mode)? "socket" :
                                           "bad type"));
                    printf (" Inode: %-15d  Links: %-12d\n", st.st_ino, st.st_nlink);
                    printf ("  Mode: %05o              UID: %-12d GID: %d\n",
                            st.st_mode & 07777, st.st_uid, st.st_gid);
                    printf ("Access: %-15d Modify: %-12d Change: %d\n",
                            st.st_atime, st.st_mtime, st.st_ctime);
                }
            }
        } else if (!strcmp (args[0], "quit") || !strcmp (args[0], "exit")) {
            break;
        } else if (!strcmp (args[0], "help")) {
            fprintf (stderr,
"This is ipodshell, an application intended to let you interactively do\n"
"various things with the ext2 filesystem on your iPod. Commands are listed\n"
"below. Paths do not need a leading slash, but you can put one if you want.\n\n"

"    list <directory> . . . . . . . Lists the files in a directory, along with\n"
"                                   the inode number of each (in [brackets]).\n"
"    read <file>. . . . . . . . . . Displays the contents of the specified file.\n"
"   write <file>. . . . . . . . . . Creates a file, allowing you to input data\n"
"                                   from the keyboard. When finished, press Ctrl+D.\n"
"truncate <file>. . . . . . . . . . Truncates the file to zero length. If you do\n"
"                                   not use this before `write'ing to an existing\n"
"                                   file, you may get some old data at the end.\n"
"   chmod <file> <mode> . . . . . . Changes the mode (permission bits) of <file>\n"
"                                   to <mode>. <mode> must be specified in octal.\n"
"   mkdir <directory> . . . . . . . Creates a directory.\n"
"  unlink <file>. . . . . . . . . . Unlinks (removes) a file. Do not use this for\n"
"                                   directories!\n"
"   rmdir <directory> . . . . . . . Removes an empty directory. Please ensure that\n"
"                                   the directory is indeed empty! It is not checked.\n"
"  rename <old> <new> . . . . . . . Renames the file <old> to <new>. <new> may be in\n"
"                                   a different directory than <old>.\n"
"    link <old> <new> . . . . . . . Like rename, but keeps the old file around too.\n"
"                                   Creates a hard link - that is, two names for the\n"
"                                   same file. <old> will be identical to <new>, and\n"
"                                   you need to delete both for the file to really be\n"
"                                   gone.\n"
" symlink <target> <link> . . . . . Creates a symbolic link (like a shortcut) at <link>\n"
"                                   pointing to <target>.\n"
"    stat <file>. . . . . . . . . . Gets information on <file>.\n"
"   lstat <file>. . . . . . . . . . Like `stat', but will not dereference symbolic links.\n"
"    exit\n"
"    quit . . . . . . . . . . . . . Quit this application.\n"
"    help . . . . . . . . . . . . . This information.\n"
                     );
        } else {
            fprintf (stderr, "%s: %s: unrecognized command, type `help' for help\n", argv[0], args[0]);
        }
    }
    return 0;
}
