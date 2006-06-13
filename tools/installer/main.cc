/* ipl/installer/main.cc -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * Distributed under the GPL. See COPYING for details.
 */

#include <QApplication>
#include <QPlastiqueStyle>
#include <QHttp>
#include <QMetaType>
#include <QDir>
#include "installer.h"

#include "libtar/libtar.h"
#include "rawpod/partition.h"
#include "rawpod/device.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef WIN32
/* getopt.c is included in make_fw2.c */
extern "C" char *optarg;
extern "C" int optind, opterr, optopt, optreset;
extern "C" int getopt (int nargc, char *const *nargv, const char *ostr);
#else
#include <getopt.h>
#endif

void usage (int exitcode) 
{
    fprintf (stderr,
             "Usage: installer [-h] [-d DIR]\n"
             "\n"
             " Mode:\n"
             "       -h  This help screen.\n"
             "       -a  Auto-pilot install; don't ask any questions. Defaults are the same as\n"
             "           those for a normal install, except that no backup will be made unless -b\n"
             "           is specified.\n"
             "       -A  Install loader1 with the Apple firmware as default. [default]\n"
             "       -L  Install loader1 with Linux as default.\n"
             "       -2  Install loader2.\n"
             "  -b FILE  Back up the iPod firmware to FILE during the install.\n"
             "  -r FILE  Open, restore a backup from FILE, and quit.\n\n"
             " Options:\n"
#ifdef WIN32
             "       -C  Do not use a block cache. Expect MASSIVE SLOWNESS, but no problems\n"
             "           in case of a hastily unplugged iPod.\n"
             "       -c  Use a block cache. [default]\n"
#else
             "       -C  Do not use an additional block cache. [default]\n"
             "       -c  Use a block cache. On Linux, this is not really necessary, since the\n"
             "           kernel does a much better job at caching than we ever could.\n"
#endif
             "   -d DIR  Store temporary files in DIR instead of the current directory.\n"
             "  -i IPOD  Look for the iPod at IPOD instead of probing for it.\n"
             "   -I NUM  Allow writes to rest in cache no longer than NUM seconds before\n"
             "           being flushed to disk (the `commit interval'). [default 5]\n"
             "  -l FILE  Use FILE as the package list file.\n"
             "   -w COW  Perform all writes to the file COW instead of to the iPod. Useful\n"
             "           for testing. COW must exist, but it can be empty.\n"
             "   -s NUM  Set the cache size to NUM sectors. This will use NUM*515 bytes of\n"
             "           memory. [default 16384]\n"
             "\n");
    exit (exitcode);
}

int main (int argc, char *argv[]) 
{
    InstallerHome = QDir::currentPath();
    PackageListFile = "http://ipodlinux.org/iPodLinux:Installation_sources?action=raw";

#ifdef __linux__
    BlockCache::disable();
#endif

    QApplication app (argc, argv);

    char ch;
    while ((ch = getopt (argc, argv, "haAL2b:r:" "Ccw:d:i:l:I:s:")) != EOF) switch (ch) {
    case 'a':
        InstallAutomatically = true;
        Mode = StandardInstall;
        iPodLoader = Loader1Apple;
        iPodDoBackup = false;
        break;
    case 'A':
        iPodLoader = Loader1Apple;
        break;
    case 'L':
        iPodLoader = Loader1Linux;
        break;
    case '2':
        iPodLoader = Loader2;
        break;
    case 'b':
        iPodDoBackup = true;
        iPodBackupLocation = QString (optarg);
        break;
    case 'r':
        InstallAutomatically = true;
        Mode = Uninstall;
        iPodDoBackup = true;
        iPodBackupLocation = QString (optarg);
        break;
    case 'w':
        LocalRawDevice::setCOWFile (strdup (optarg));
        break;
    case 'd':
        InstallerHome = QString (optarg);
        break;
    case 'h':
        usage (0);
        break;
    case 'i':
        LocalRawDevice::setOverride (strdup (optarg));
        break;
    case 'I':
        BlockCache::setCommitInterval (atoi (optarg));
        break;
    case 's':
        LocalRawDevice::setCachedSectors (atoi (optarg));
        break;
    case 'l':
        PackageListFile = QString (optarg);
        break;
    case 'c':
        BlockCache::enable();
        break;
    case 'C':
        BlockCache::disable();
        break;
    case '?':
    default:
        usage (1);
        break;
    }
    
#ifdef __linux__
    app.setStyle (new QPlastiqueStyle);
#endif

    qRegisterMetaType <QHttpResponseHeader> ("QHttpResponseHeader");
    qRegisterMetaType <PartitionTable> ("PartitionTable");

    Installer *inst = new Installer;
    inst->show();

    return app.exec();
}
