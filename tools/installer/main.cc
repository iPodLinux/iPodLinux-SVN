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
             "    Usage: installer [-h] [-d DIR]\n"
             "\n"
             "  Options:\n"
             "       -h  This help screen.\n"
             "   -c COW  Perform all writes to the file COW instead of to the iPod. Useful\n"
             "           for testing. COW must exist, but it can be empty.\n"
             "   -d DIR  Store temporary files in DIR instead of the current directory.\n"
             "  -i IPOD  Look for the iPod at IPOD instead of probing for it.\n"
             "  -l FILE  Use FILE as the package list file.\n"
             "\n");
    exit (exitcode);
}

int main (int argc, char *argv[]) 
{
    InstallerHome = QDir::currentPath();
    PackageListFile = "http://ipodlinux.org/iPodLinux:Installation_sources?action=raw";

    QApplication app (argc, argv);

    char ch;
    while ((ch = getopt (argc, argv, "c:d:i:l:h")) != EOF) switch (ch) {
    case 'c':
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
    case 'l':
        PackageListFile = QString (optarg);
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
