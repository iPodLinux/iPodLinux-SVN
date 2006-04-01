/* ipl/installer/main.cc -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * Distributed under the GPL. See COPYING for details.
 */

#include <QApplication>
#include <QPlastiqueStyle>
#include <QHttp>
#include <QMetaType>
#include "installer.h"

#include "libtar/libtar.h"
#include <stdio.h>

int main (int argc, char *argv[]) 
{
    QApplication app (argc, argv);
    
#ifdef __linux__
    app.setStyle (new QPlastiqueStyle);
#endif

    qRegisterMetaType <QHttpResponseHeader> ("QHttpResponseHeader");

    Installer *inst = new Installer;
    inst->show();

    return app.exec();
}
