/*-*-C++-*-*/
#ifndef PANES_H
#define PANES_H

#include "installer.h"
#include "actions.h"

#include <QMap>
#include <QHttp>
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

public:
    PartitioningPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();

private slots:
    void setBigStuff (bool chk); 
    void setSmallStuff (bool chk); 
    void setStuff (int newVal = -1); 
    
private:
    QLabel *topblurb;
    QRadioButton *partitionSmall, *partitionBig;
    QSpinBox *size;
    QLabel *sizeBlurb;
    QLabel *spaceLeft;
};

class InstallPage : public InstallerPage
{
    Q_OBJECT

public:
    InstallPage (Installer *wizard);
    void resetPage() {}
    WizardPage *nextPage();
    bool isComplete();

protected slots:
    void setLoader1Blurb (bool chk);
    void setLoader2Blurb (bool chk);
    void setBackupBlurb (bool chk); 
    void openBrowseDialog();

private:
    QLabel *topblurb, *ldrblurb, *bkpblurb, *ldrchoiceblurb, *bkpchoiceblurb;
    QRadioButton *loader1apple, *loader1linux, *loader2;
    QCheckBox *makeBackup;
    QLabel *backupPathLabel;
    QLineEdit *backupPath;
    QPushButton *backupBrowse;
};

class PackagesPage : public InstallerPage
{
    Q_OBJECT

    friend class PkgTreeWidgetItem;

public:
    PackagesPage (Installer *wizard);
    void resetPage() {}
    WizardPage *nextPage();
    bool isComplete() { return advok; }

protected slots:
    void httpSendProgress (int done, int total); 
    void httpReadProgress (int done, int total);
    void httpStateChanged (int state);
    void httpRequestFinished (int req, bool err);
    void httpDone (bool err);
    void httpResponseHeaderReceived (const QHttpResponseHeader& resp);
    void listClicked (QTreeWidgetItem *item, int column);
    
private:
    QHttp *packlistHTTP;
    QLabel *blurb;
    QLabel *progressStmt;
    QTreeWidget *packages;
    QMap <int, PkgTreeWidgetItem*> resolvers;
    QMultiMap <QString, PkgTreeWidgetItem*> packageProvides;
    bool advok;
    bool errored;
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
