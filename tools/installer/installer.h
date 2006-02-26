/* ipl/installer/installer.h -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * Distributed under the GPL. See COPYING for details.
 */

#ifndef INSTALLER_H
#define INSTALLER_H

#include "complexwizard.h"
#include "actions.h"

class QCheckBox;
class QGroupBox;
class QHttp;
class QLabel;
class QLineEdit;
class QRadioButton;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTreeWidget;

class IntroductionPage;
class PodLocationPage;
class PartitioningPage;
class InstallPage;
class DoInstallPage;
class PackagesPage;
class DoPackagesPage;
class RestoreBackupPage;
class DoRestoreBackupPage;
class DoHeuristicUninstallPage;
class DonePage;

class Installer : public ComplexWizard
{
    Q_OBJECT;
public:
    Installer (QWidget *parent = 0);
    
protected:
    /* Install path:
     * intro -> podloc -.
     *    If Advanced:  `-> partitioning
     * In any case: -> install -> doinstall -> packages -> dopackages
     */

    /* Upgrade path:
     * intro -> podloc -> packages -> dopackages
     */

    /* Uninstall path:
     * intro -> podloc -> backuppath -> douninst
     */

    IntroductionPage *introPage; /* I U X */
    PodLocationPage *podlocPage; /* I U X */
    
    PartitioningPage *partPage; /* IA */
    InstallPage *instPage; /* IB */
    DoInstallPage *doinstPage; /* I */

    PackagesPage *pkgPage; /* I U */
    DoPackagesPage *dopkgPage; /* I U */
    
    RestoreBackupPage *restorePage; /* XB */
    DoRestoreBackupPage *dorestorePage; /* XB */
    DoHeuristicUninstallPage *douninstPage; /* XH */

    DonePage *donePage; /* I U X */

    friend class IntroductionPage;
    friend class PodLocationPage;
    friend class PartitioningPage;
    friend class InstallPage;
    friend class DoInstallPage;
    friend class PackagesPage;
    friend class DoPackagesPage;
    friend class RestoreBackupPage;
    friend class DoRestoreBackupPage;
    friend class DoHeuristicUninstallPage;
    friend class DonePage;
};

class InstallerPage : public WizardPage
{
    Q_OBJECT
    
public:
    InstallerPage (Installer *wiz)
        : WizardPage (wiz), wizard (wiz) {}

    InstallerPage()
        : WizardPage (0), wizard (0) {}
    
protected:
    Installer *wizard;
};

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
    bool isLastPage() { return 1; }
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

class DoInstallPage : public InstallerPage, public ActionOutlet
{
    Q_OBJECT

#if 0
public:
    DoInstallPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();

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

class DoPackagesPage : public InstallerPage, public ActionOutlet
{
    Q_OBJECT

#if 0
public:
    DoPackagesPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();

    virtual void setTaskDescription (QString str) { action->setText (str); }
    virtual void setCurrentAction (QString str) { specific->setText (str); }
    virtual void setTotalProgress (int tp) { pkgProgress->setRange (0, tp); }
    virtual void setCurrentProgress (int cp) { pkgProgress->setCurrentProgress (cp); }

private:
    QLabel *blurb;
    QLabel *action;
    QLabel *specific; // download speed, or what file installing
    QLabel *pkgProgressLabel;
    QProgressBar *pkgProgress;
    QLabel *totalProgressLabel;
    QProgressBar *totalProgress;
#endif
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

