/* ipl/installer/actions.cc -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * Distributed under the GPL. See COPYING for details.
 */

#include "installer.h"
#include "actions.h"
#include "panes.h"
#include "rawpod/partition.h"
#include "rawpod/vfs.h"
#include "rawpod/device.h"

#include <QMessageBox>
#include <QGridLayout>

#define FATAL_T(str) do { emit fatalError(str); while(1); } while(0)
#define FATAL(str) FATAL_T(tr(str))

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
    
    emit setCurrentProgress (2);
    emit setCurrentAction (tr ("Writing MBR..."));
    
    if (devWriteMBR (_dev, mbr) != 0)
        FATAL ("Error writing the partition table. This shouldn't happen. Your iPod is probably fine, but "
               "it might not be; check and see.");

    if (iPodPartitionTable) partFreeTable (iPodPartitionTable);
    iPodPartitionTable = ptbl;
    setupDevices();

    emit setCurrentProgress (3);
    emit setCurrentAction (tr ("Done."));
}

void FormatAction::run() 
{
    int prog = 0;
    VFS::Device *dev = new PartitionDevice (iPodDevice, iPodPartitionTable[_part-1].offset,
                                            iPodPartitionTable[_part-1].length);

    emit setTaskDescription (tr (_str));
    emit setTotalProgress (0);
    emit setCurrentAction (tr ("Formatting..."));

    RunnerThread <int(*)(VFS::Device*), VFS::Device*> *rthr =
        new RunnerThread <int(*)(VFS::Device*), VFS::Device*> (_mkfsfunc, dev);
    rthr->start();
    while (rthr->isRunning()) {
        emit setCurrentProgress (prog++);
        usleep (300000);
    }

    delete rthr;
}

void DelayAction::run() 
{
    emit setTaskDescription (QString (tr ("Delaying for %1 seconds.")).arg (_sec));
    emit setTotalProgress (_sec);
    emit setCurrentProgress (0);

    for (int s = 0; s < _sec; s++) {
        emit setCurrentProgress (s);
        emit setCurrentAction (QString (tr ("%1 seconds left...")).arg (_sec - s));
        sleep (1);
    }
    emit setCurrentProgress (_sec);
}

void BackupAction::run() 
{
    emit setTaskDescription (QString (tr ("Backing up the iPod to %1...")).arg (_path));
    emit setTotalProgress (0);
    emit setCurrentProgress (0);
    emit setCurrentAction (tr ("Preparing..."));

    VFS::Device *ipoddev = new LocalRawDevice (_dev);
    VFS::Device *fwpart = setup_partition (_dev, 1);
    VFS::File *backup = new LocalFile (_path.toAscii().data(), OPEN_WRITE | OPEN_CREATE);
    if (backup->error())
        FATAL_T (tr ("Error creating the backup file %1: %2 (%3)").arg (_path).arg (strerror (backup->error())).arg (backup->error()));

    emit setCurrentAction (tr ("Backing up partition table"));
    u8 *buf = new u8[4096];
    ipoddev->lseek (0, SEEK_SET);
    if (ipoddev->read (buf, 512) != 512)
        FATAL ("Error reading the partition table from the iPod.");
    if (backup->write (buf, 512) != 512)
        FATAL ("Error writing the partition table to the backup file.");
    delete ipoddev;
    
    u32 fwplen = fwpart->lseek (0, SEEK_END);
    u32 fwpread = 0;
    int thisread, err;

    emit setTotalProgress (fwplen);
    fwpart->lseek (0, SEEK_SET);

    while (fwpread < fwplen) {
        emit setCurrentAction (tr ("Backing up the firmware partition: %1")
                               .arg (transferProgressText (fwpread, fwplen)));
        emit setCurrentProgress (fwpread);
        if ((thisread = fwpart->read (buf, 4096)) <= 0) {
            if (thisread == 0)
                FATAL ("Short read on the iPod's firmware partition.");
            else
                FATAL_T (tr ("Error reading the iPod's firmware partition: %1").arg (strerror (-thisread)));
        }
        if ((err = backup->write (buf, thisread)) != thisread) {
            FATAL_T (tr ("Error writing %1 bytes to %2: %3").arg (thisread).arg (_path).arg (strerror (-err)));
        }
        fwpread += thisread;
    }

    emit setCurrentAction (tr ("Backup complete."));
    emit setCurrentProgress (fwplen);

    delete fwpart;
    backup->close();
    delete backup;
}

void RestoreBackupAction::run() 
{
    emit setTaskDescription (QString (tr ("Restoring backup from %1...")).arg (_path));
    emit setTotalProgress (0);
    emit setCurrentProgress (0);
    emit setCurrentAction (tr ("Preparing..."));

    VFS::Device *ipoddev = new LocalRawDevice (_dev);
    VFS::File *backup = new LocalFile (_path.toAscii().data(), OPEN_READ);
    if (backup->error())
        FATAL_T (tr ("Error opening the backup file: %1").arg (strerror (backup->error())));

    emit setCurrentAction (tr ("Restoring the partition table"));
    u8 *buf = new u8[4096];
    ipoddev->lseek (0, SEEK_SET);
    if (backup->read (buf, 512) != 512)
        FATAL ("Error reading the partition table from the backup file..");
    if (devWriteMBR (_dev, buf) != 0)
        FATAL ("Error writing the partition table to the iPod.");
    delete ipoddev;

    VFS::Device *fwpart = setup_partition (_dev, 1);
    
    u32 bkplen = backup->lseek (0, SEEK_END);
    u32 bkpread = 512;
    int thisread, err;

    emit setTotalProgress (bkplen);
    backup->lseek (512, SEEK_SET);

    while (bkpread < bkplen) {
        emit setCurrentAction (tr ("Restoring the firmware partition: %1")
                               .arg (transferProgressText (bkpread, bkplen)));
        emit setCurrentProgress (bkpread);
        if ((thisread = backup->read (buf, 4096)) <= 0) {
            if (thisread == 0)
                FATAL ("Short read on the backup.");
            else
                FATAL_T (tr ("Error reading the backup: %1").arg (strerror (-thisread)));
        }
        if ((err = fwpart->write (buf, thisread)) != thisread) {
            FATAL_T (tr ("Error writing %1 bytes to iPod: %3").arg (thisread).arg (strerror (-err)));
        }
        bkpread += thisread;
    }

    emit setCurrentAction (tr ("Restore complete."));
    emit setCurrentProgress (bkplen);

    delete fwpart;
    backup->close();
    delete backup;
}

DoActionsPage::DoActionsPage (Installer *wiz, InstallerPage *next) 
    : ActionOutlet (wiz), nextp (next), done (false)
{
    wiz->setInfoText (tr ("<b>Installing</b>"),
                      tr ("Please wait while the requested actions are performed."));

    action = new QLabel (tr ("Initializing..."));
    action->setAlignment (Qt::AlignLeft);
    action->setIndent (10);
    specific = new QLabel (tr (""));
    specific->setAlignment (Qt::AlignLeft);
    specific->setIndent (10);
    pkgProgressLabel = new QLabel (tr ("Progress for this item:"));
    pkgProgressLabel->setAlignment (Qt::AlignRight);
    totalProgressLabel = new QLabel (tr ("Total progress:"));
    totalProgressLabel->setAlignment (Qt::AlignRight);

    pkgProgress = new QProgressBar;
    pkgProgress->setRange (0, 0); // barber-pole thingy
    totalProgress = new QProgressBar;
    totalProgress->setRange (0, PendingActions->size());
    totalProgress->setValue (0);

#ifdef WIN32
    QHBoxLayout *pkgProgressLayout = new QHBoxLayout;
    pkgProgressLayout->addWidget (pkgProgressLabel);
    pkgProgressLayout->addSpacing (5);
    pkgProgressLayout->addWidget (pkgProgress);
    QHBoxLayout *totalProgressLayout = new QHBoxLayout;
    totalProgressLayout->addWidget (totalProgressLabel);
    totalProgressLayout->addSpacing (5);
    totalProgressLayout->addWidget (totalProgress);
    QVBoxLayout *layout = new QVBoxLayout (this);
    layout->addWidget (action);
    layout->addSpacing (2);
    layout->addWidget (specific);
    layout->addSpacing (15);
    layout->addLayout (pkgProgressLayout);
    layout->addSpacing (10);
    layout->addLayout (totalProgressLayout);
    layout->addStretch (1);
#else
    QGridLayout *layout = new QGridLayout (this);
    layout->addWidget (action,             1, 0, 1, 3);
    layout->addWidget (specific,           3, 0, 1, 3);
    layout->addWidget (pkgProgressLabel,   5, 0, Qt::AlignRight);
    layout->addWidget (pkgProgress,        5, 2, Qt::AlignLeft);
    layout->addWidget (totalProgressLabel, 7, 0, Qt::AlignRight);
    layout->addWidget (totalProgress,      7, 2, Qt::AlignLeft);
    layout->setRowMinimumHeight (2,  2);
    layout->setRowMinimumHeight (4, 15);
    layout->setRowMinimumHeight (6, 10);
    layout->setColumnMinimumWidth (1, 5);
    layout->setRowStretch (8, 1);
#endif
    setLayout (layout);

    if (!PendingActions->size()) {
        totalProgress->setRange (0, 1);
        totalProgress->setValue (1);
        pkgProgress->setRange (0, 1);
        pkgProgress->setValue (1);
        action->setText ("Done.");
        specific->setText ("(There was nothing to do.)");
        done = true;
        emit completeStateChanged();
    } else {
        currentAction = PendingActions->takeFirst();
        connect (currentAction, SIGNAL(finished()), this, SLOT(nextAction()));
        currentAction->start (this);
    }
}

void DoActionsPage::nextAction() 
{
    delete currentAction;
    if (PendingActions->size()) {
        currentAction = PendingActions->takeFirst();
        connect (currentAction, SIGNAL(finished()), this, SLOT(nextAction()));
        currentAction->start (this);
    } else {
        done = true;
        action->setText ("Done.");
        specific->setText ("Press Next to continue.");
        totalProgress->hide();
        totalProgressLabel->hide();
        pkgProgress->hide();
        pkgProgressLabel->hide();
        emit completeStateChanged();
        wizard->clickNextButton();
        return;
    }
    totalProgress->setValue (totalProgress->value() + 1);
    pkgProgress->setRange (0, 1);
    pkgProgress->setValue (0);
    specific->setText ("Done.");
}
