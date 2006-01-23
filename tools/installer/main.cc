/* ipl/installer/main.cc -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * Distributed under the GPL. See COPYING for details.
 */

#include <QApplication>
#include <QPlastiqueStyle>
#include "installer.h"

int main (int argc, char *argv[]) 
{
    QApplication app (argc, argv);
    
#ifdef __linux__
    app.setStyle (new QPlastiqueStyle);
#endif

    Installer *inst = new Installer;
    inst->show();
    
    return app.exec();
}
