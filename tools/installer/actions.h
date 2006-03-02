/* ipl/installer/actions.h -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * Distributed under the GPL. See COPYING for details.
 */

#ifndef ACTIONS_H
#define ACTIONS_H

#include "installer.h"
#include <QThread>
#include <QList>

class ActionOutlet : public InstallerPage
{
    Q_OBJECT

public:
    ActionOutlet (Installer *wiz = 0)
        : InstallerPage (wiz)
    {}

public slots:
    virtual void setTaskDescription (QString str) = 0;
    virtual void setCurrentAction (QString str) = 0;
    virtual void setTotalProgress (int tp) = 0;
    virtual void setCurrentProgress (int cp) = 0;
};

class Action : public QThread
{
    Q_OBJECT

public:
    Action() : QThread() {}

    virtual void start (ActionOutlet *outlet) {
        _outlet = outlet;
        connect (this, SIGNAL(setTaskDescription(QString)), _outlet, SLOT(setTaskDescription(QString)));
        connect (this, SIGNAL(setCurrentAction(QString)), _outlet, SLOT(setCurrentAction(QString)));
        connect (this, SIGNAL(setTotalProgress(int)), _outlet, SLOT(setTotalProgress(int)));
        connect (this, SIGNAL(setCurrentProgress(int)), _outlet, SLOT(setCurrentProgress(int)));
        QThread::start();
    }
    
signals:
    void setTaskDescription (QString str);
    void setCurrentAction (QString str);
    void setTotalProgress (int tp);
    void setCurrentProgress (int cp);

protected:
    ActionOutlet *_outlet;
};

class PartitionAction : public Action
{
public:
    PartitionAction (int device, int oldpart, int newpart, int newtype, int newsize)
        : _dev (device), _oldnr (oldpart), _newnr (newpart),
          _newtype (newtype), _newsize (newsize)
    {}

protected:
    virtual void run();
    
    int _dev;
    int _oldnr, _newnr;
    int _newtype;
    int _newsize;
};

class DelayAction : public Action
{
public:
    DelayAction (int nsec)
        : _sec (nsec)
    {}

protected:
    virtual void run();
    
    int _sec;
};

extern QList<Action*> *PendingActions;

#endif
