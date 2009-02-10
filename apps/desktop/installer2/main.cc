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
#include "rawpod/rawpod.h"
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
             "Usage: installer [-h|-a|-r BACKUP] [-A|-L|-2] [-b BACKUP] [-d DIR] [-l PKGLIST]\n"
#ifdef WIN32
             "                 [-i IPOD] [-w COWFILE] [-C | [-c] [-I COMINT] [-s CSIZE]]\n"
#else
             "                 [-i IPOD] [-w COWFILE] [-C | -c [-I COMINT] [-s CSIZE]]\n"
#endif
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
             "   -d DIR   Store temporary files in DIR instead of the current directory.\n"
             "   -l FILE  Use FILE as the package list file.\n"
             "   -p PROXY Use PROXY as proxy for fetching package list file. Example: user:pass@ip:8080\n"
             "\n"
             RAWPOD_OPTIONS_USAGE);
    exit (exitcode);
}

int main (int argc, char *argv[]) 
{
    InstallerHome = QDir::currentPath();
    PackageListFile = "http://ipodlinux.org/w/index.php?title=iPodLinux:Installation_sources&action=raw";
    ProxyString = "";

#ifdef __linux__
    BlockCache::disable();
#endif

    QApplication app (argc, argv);

    signed char ch;
    while ((ch = getopt (argc, argv, "haAL2b:r:" "d:l:p:" RAWPOD_OPTIONS_STR)) != EOF) switch (ch) {
    case 'a':
        InstallAutomatically = true;
        Mode = StandardInstall;
        iPodLoader = Loader2;
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
    case 'd':
        InstallerHome = QString (optarg);
        break;
    case 'h':
        usage (0);
        break;
    case 'l':
        PackageListFile = QString (optarg);
        break;
    case 'p':
        ProxyString = QString (optarg);
        break;
    default:
        if (rawpod_parse_option (ch, optarg))
            break;
    case '?':
        usage (1);
        break;
    }
    
#ifdef __linux__
    app.setStyle (new QPlastiqueStyle);
#endif

    qRegisterMetaType <QHttpResponseHeader> ("QHttpResponseHeader");

    Installer *inst = new Installer;
    inst->show();

    return app.exec();
}
