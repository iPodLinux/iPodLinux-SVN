/*-*-C++-*-*/
#ifndef PANES_H
#define PANES_H

#include "installer.h"
#include "actions.h"

#include <QMap>
#include <QHttp>
#include <QLabel>
#include <QProgressBar>
#include <QRadioButton>
#include <QMessageBox>

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
    bool isLastPage();
    
protected slots:
    void uninstallRadioClicked (bool clicked);
    void changeLoaderRadioClicked (bool clicked);
    void upgradeRadioClicked (bool clicked);
    void doBackupRestore (bool clicked);
    
private:
    QLabel *blurb;
    QCheckBox *advancedCheck; /* enabled only for install */
    QRadioButton *upgradeRadio, *changeLoaderRadio, *uninstallRadio; /* enabled only if already installed */
    QLabel *subblurb;
    int stateOK, wasError;
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

class ChangeLoaderPage : public InstallerPage
{
    Q_OBJECT

public:
    ChangeLoaderPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete() { return loader1apple->isChecked() || loader1linux->isChecked() ||
                            loader2->isChecked(); }

protected slots:
    void setLoader1Blurb (bool chk);
    void setLoader2Blurb (bool chk);

private:
    QLabel *blurb, *ldrchoiceblurb;
    QRadioButton *loader1apple, *loader1linux, *loader2;
};

class PackagesPage : public InstallerPage
{
    Q_OBJECT

    friend class PkgTreeWidgetItem;

public:
    PackagesPage (Installer *wizard, bool automatic = false);
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
    void done();
    void listClicked (QTreeWidgetItem *item, int column);
    void itemCollapsed (QTreeWidgetItem *item);
    void itemExpanded (QTreeWidgetItem *item);
    void doLoadExtraPackageList();

protected:
    void loadExternalPackageList (QString filename, bool markBold);
    Package *parsePackageListLine (QString line, bool makeBold = false);
    
private:
    QHttp *packlistHTTP;
    QLabel *blurb;
    QLabel *progressStmt;
    QString host;
    QTreeWidget *packages;
    QPushButton *loadpkg;
    QMap <int, PkgTreeWidgetItem*> resolvers;
    QMap <QString, QTreeWidgetItem*> categories;
    QMultiMap <QString, PkgTreeWidgetItem*> packageProvides;
    bool advok;
    bool errored;
    bool automatic;
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
    virtual void fatalError (QString str) { QMessageBox::critical (0, tr("iPodLinux Installer"), str, tr("Quit")); exit (1); }
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

class DonePage : public InstallerPage
{
public:
    DonePage (Installer *wizard);
    void resetPage() {}
    WizardPage *nextPage() { return 0; }
    bool isComplete() { return true; }
    bool isLastPage() { return true; }

private:
    QLabel *blurb;
};

class UninstallPage : public InstallerPage
{
    Q_OBJECT

public:
    UninstallPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();
    bool isLastPage() { return false; }

protected slots:
    void setBackupBlurb (bool clicked);
    void openBrowseDialog();

private:
    QLabel *blurb;
    QLabel *nobackupblurb;
    QCheckBox *haveBackup;
    QLabel *backupPathLabel;
    QLineEdit *backupPath;
    QPushButton *backupBrowse;
};

#endif
