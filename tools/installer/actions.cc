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

template <class F, class Arg>
class RunnerThread : public QThread 
{
public:
    RunnerThread (F func, Arg arg)
        : _fn (func), _arg (arg)
    {}

protected:
    virtual void run() {
        (*_fn)(_arg);
    }

    F _fn;
    Arg _arg;
};

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

    for (int s = 1; s <= _sec; s++) {
        emit setCurrentAction (QString (tr ("Second %1:")).arg (s));
        sleep (1);
        emit setCurrentProgress (s);
    }
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
    }
    totalProgress->setValue (totalProgress->value() + 1);
    pkgProgress->setRange (0, 1);
    pkgProgress->setValue (0);
    specific->setText ("Done.");
}
