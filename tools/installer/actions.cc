/* ipl/installer/actions.cc -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * Distributed under the GPL. See COPYING for details.
 */

#include "installer.h"
#include "actions.h"
#include "rawpod/partition.h"

#include <QMessageBox>

#define FATAL(str) do { QMessageBox::critical (0, tr("iPodLinux Installer"), tr(str), tr("Quit")); exit(1); } while(0)

void PartitionAction::run()
{
    unsigned char mbr[512];
    PartitionTable ptbl;

    emit setTaskDescription (tr ("Partitioning the iPod."));
    emit setTotalProgress (3);
    emit setCurrentProgress (0);
    emit setCurrentAction (tr ("Reading MBR..."));

    if (devReadMBR (_dev, mbr) != 0)
        FATAL ("Error reading the partition table.");

    emit setCurrentProgress (1);
    emit setCurrentAction (tr ("Modifying the partition table..."));

    ptbl = partCopyFromMBR (mbr);
    if (!ptbl)
        FATAL ("Invalid MBR on the iPod.");
    if (partShrinkAndAdd (ptbl, _oldnr, _newnr, _newtype, _newsize) != 0)
        FATAL ("Error modifying the partition table. Nothing has been written to your iPod; your data is safe.");
    partCopyToMBR (ptbl, mbr);
    partFreeTable (ptbl);
    
    emit setCurrentProgress (2);
    emit setCurrentAction (tr ("Writing MBR..."));
    
    if (devWriteMBR (_dev, mbr) != 0)
        FATAL ("Error writing the partition table. This shouldn't happen. Your iPod is probably fine, but "
               "it might not be; check and see.");

    emit setCurrentProgress (3);
    emit setCurrentAction (tr ("Done."));
}

void DelayAction::run() 
{
    emit setTaskDescription (QString (tr ("Delaying for %1 seconds.")).arg (_sec));
    emit setTotalProgress (_sec);
    emit setCurrentProgress (0);

    for (int s = 1; s <= _sec; s++) {
        emit setCurrentAction (QString (tr ("Second %1:")).arg (s));
        sleep (1);
        emit setCurrentProgress (s);
    }
}
