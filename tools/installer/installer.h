/* ipl/installer/installer.h -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * Distributed under the GPL. See COPYING for details.
 */

#ifndef INSTALLER_H
#define INSTALLER_H

#include "complexwizard.h"

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
class DownloadUpdatesPage;
class WhatToUpdatePage;
class PartitioningPage;
class LoaderSetupPage;
class KernelPage;
class UIPage;
class SimpleInstallPage;
class MakeBackupPage;
class DoBackupPage;
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
     * intro -> podloc -+
     *    If Advanced:  |-> partitioning -> loader -> kernel -> podzilla
     *           Else:  `-> simpleinst (default? what GUI?)
     * In any case: -> backup -> doinstall -> packages -> dopackages
     */

    /* Upgrade path:    +-> Upgrading kernel? If so, [kernel].
     *                  |
     *                  |-> Changing GUI? If so, [podzilla].
     * intro -> podloc -|
     *                  `-> In any case: [packages].  -> dopackages
     */

    /* Uninstall path:
     * intro -> podloc -> backup? -> douninst
     */

    IntroductionPage *introPage; /* I U X */
    PodLocationPage *podlocPage; /* I U X */

    DownloadUpdatesPage *dlupPage; /* I U */
    PartitioningPage *partPage; /* IA */
    LoaderSetupPage *loaderPage; /* IA */
    KernelPage *kernPage; /* IA UK */
    UIPage *uiPage; /* IA UP */
    SimpleInstallPage *simplePage; /* IB */
    MakeBackupPage *backupPage; /* I */
    DoBackupPage *dobackupPage; /* I */
    DoInstallPage *doinstPage; /* I */
    PackagesPage *pkgPage; /* I U */
    DoPackagesPage *dopkgPage; /* I U */
    
    RestoreBackupPage *restorePage; /* XB */
    DoRestoreBackupPage *dorestorePage; /* XB */
    DoHeuristicUninstallPage *douninstPage; /* XH */

    DonePage *donePage; /* I U X */

    friend class IntroductionPage;
    friend class PodLocationPage;
    friend class DownloadUpdatesPage;
    friend class WhatToUpdatePage;
    friend class PartitioningPage;
    friend class LoaderSetupPage;
    friend class KernelPage;
    friend class UIPage;
    friend class SimpleInstallPage;
    friend class MakeBackupPage;
    friend class DoBackupPage;
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
    InstallerPage (Installer *wizard)
        : WizardPage (wizard), wizard (wizard) {}

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
    QCheckBox *changeUICheck; /* enabled only for upgrade */
    QLabel *subblurb;
    int stateOK;
};

class DownloadUpdatesPage : public InstallerPage
{
    Q_OBJECT

#if 0
public:
    DownloadUpdatesPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();

private:
    QHttp *http;
    QLabel *blurb;
    QLabel *file;
    QProgressBar *progbar;
    QLabel *percent;
    QLabel *speed;
#endif
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

class LoaderSetupPage : public InstallerPage
{
    Q_OBJECT

#if 0
public:
    LoaderSetupPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();
    
private:
    QLabel *blurb;
    QRadioButton *loader1, *loader2;
    QCheckBox *makeLinuxDefault; // for loader1
    QTreeWidget *loaderConfig; // for loader2
    QLineEdit *loaderConfigEntry; // L2
    QPushButton *loaderConfigAdd, *loaderConfigRemove; // L2
#endif
};

class KernelPage : public InstallerPage
{
    Q_OBJECT

#if 0
public:
    KernelPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();
    
private:
    QLabel *blurb;
    QLineEdit *file;
    QPushButton *browse;
#endif
};

class UIPage : public InstallerPage
{
    Q_OBJECT

#if 0
public:
    UIPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();

private:
    QLabel *blurb;
    QRadioButton *podzilla0, *podzilla2;
    QCheckBox *nonstandardPZ; // only if pz0
    QLineEdit *nonstandardPZfile;
    QPushButton *nonstandardPZbrowse;
    QLabel *bigFatUnsupportedWarning;
#endif
};

class SimpleInstallPage : public InstallerPage
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
    QCheckBox *makeLinuxDefault;
#endif
};

class MakeBackupPage : public InstallerPage
{
    Q_OBJECT

#if 0
public:
    MakeBackupPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();

private:
    QLabel *blurb;
    QCheckBox *makeBackup;
    QLabel *backupPathLabel;
    QLineEdit *backupPath;
    QPushButton *backupBrowse;
#endif
};

class DoBackupPage : public InstallerPage
{
    Q_OBJECT

#if 0
public:
    DoBackupPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();
    
private:
    QLabel *blurb;
    QProgressBar *backupProgress;
#endif
};

class DoInstallPage : public InstallerPage
{
    Q_OBJECT

#if 0
public:
    DoInstallPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();
    
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

class DoPackagesPage : public InstallerPage
{
    Q_OBJECT

#if 0
public:
    DoPackagesPage (Installer *wizard);
    void resetPage();
    WizardPage *nextPage();
    bool isComplete();

private:
    QLabel *blurb;
    QLabel *action;
    QLabel *package;
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

