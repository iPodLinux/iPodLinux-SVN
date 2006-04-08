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
             "   -d DIR  Store temporary files in DIR instead of the current directory.\n"
             "\n");
    exit (exitcode);
}

int main (int argc, char *argv[]) 
{
    InstallerHome = QDir::currentPath();

    QApplication app (argc, argv);

    char ch;
    while ((ch = getopt (argc, argv, "d:h")) != EOF) switch (ch) {
    case 'd':
        InstallerHome = QString (optarg);
        break;
    case 'h':
        usage (0);
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

    Installer *inst = new Installer;
    inst->show();

    return app.exec();
}
