#ifndef PANES_H
#define PANES_H

#include "installer.h"
#include "actions.h"

#include <QLabel>
#include <QProgressBar>

class IntroductionPage : public InstallerPage
{
    Q_OBJECT

public:
    IntroductionPage (Installer *wizard);
    
    bool isComplete() { return true; }
    WizardPage *nextPage();

private:
    QLabel *blurb;
};

class PodLocationPage : public InstallerPage
{
    Q_OBJECT

public:
    PodLocationPage (Installer *wizard);
    
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();
#if 1
    bool isLastPage() { return true; }
#endif
    
protected slots:
    void uninstallRadioClicked (bool clicked);
    void upgradeRadioClicked (bool clicked);
    
private:
    QLabel *blurb;
    QCheckBox *advancedCheck; /* enabled only for install */
    QRadioButton *upgradeRadio, *uninstallRadio; /* enabled only if already installed */
    QLabel *subblurb;
    int stateOK;
};

class PartitioningPage : public InstallerPage
{
    Q_OBJECT

#if 0
public:
    PartitioningPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();
    
private:
    QLabel *blurb;
    QRadioButton *partitionSmall, *partitionBig;
    QSpinBox *size;
    QLabel *sizeMax;
    QLabel *bigFatWarning;
#endif
};

class InstallPage : public InstallerPage
{
    Q_OBJECT

#if 0
public:
    SimpleInstallPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();

private:
    QLabel *blurb;
    QRadioButton *loader1apple, *loader1linux, *loader2;
    QCheckBox *makeBackup;
    QLabel *backupPathLabel;
    QLineEdit *backupPath;
    QPushButton *backupBrowse;
#endif
};

class DoInstallPage : public ActionOutlet
{
    Q_OBJECT

#if 0
public:
    DoInstallPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();

public slots:
    virtual void setTaskDescription (QString str) { action->setText (str); }
    virtual void setCurrentAction (QString str) {}
    virtual void setTotalProgress (int tp) { progress->setRange (0, tp); }
    virtual void setCurrentProgress (int cp) { progress->setValue (cp); }

private:
    QLabel *blurb;
    QLabel *action;
    QProgressBar *progress;
#endif
};

class PackagesPage : public InstallerPage
{
    Q_OBJECT

#if 0
public:
    PackagesPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();
    
private:
    QLabel *blurb;
    QTreeWidget *packages;
#endif
};

class DoActionsPage : public ActionOutlet
{
    Q_OBJECT

public:
    DoActionsPage (Installer *wizard, InstallerPage *next);
    void resetPage() {}
    WizardPage *nextPage() { return nextp; }
    bool isComplete() { return done; }
    bool isLastPage() { return !nextp; }

public slots:
    virtual void setTaskDescription (QString str) { action->setText (str); }
    virtual void setCurrentAction (QString str) { specific->setText (str); }
    virtual void setTotalProgress (int tp) { pkgProgress->setRange (0, tp); }
    virtual void setCurrentProgress (int cp) { pkgProgress->setValue (cp); }
    void nextAction();

private:
    InstallerPage *nextp;
    QLabel *blurb;
    QLabel *action;
    QLabel *specific; // download speed, or what file installing
    QLabel *pkgProgressLabel;
    QProgressBar *pkgProgress;
    QLabel *totalProgressLabel;
    QProgressBar *totalProgress;
    Action *currentAction;
    bool done;
};

class RestoreBackupPage : public InstallerPage
{
    Q_OBJECT

#if 0
public:
    RestoreBackupPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();

private:
    QLabel *blurb;
    QCheckBox *haveBackup;
    QLineEdit *backupLoc;
    QPushButton *backupBrowse;
#endif
};

class DoRestoreBackupPage : public InstallerPage
{
    Q_OBJECT

#if 0
public:
    DoRestoreBackupPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();
    
private:
    QLabel *blurb;
    QProgressBar *restoreProgress;
#endif
};

class DoHeuristicUninstallPage : public InstallerPage
{
    Q_OBJECT

#if 0
public:
    DoHeuristicUninstallPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();
    
private:
    QLabel *blurb;
    QLabel *action;
    QProgressBar *progress;
#endif
};

#endif
