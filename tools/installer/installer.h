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
class PartitioningPage;
class InstallPage;
class PackagesPage;
class RestoreBackupPage;
class DoActionsPage;
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
    PackagesPage *pkgPage; /* I U */    
    RestoreBackupPage *restorePage; /* XB */

    DonePage *donePage; /* I U X */

    friend class IntroductionPage;
    friend class PodLocationPage;
    friend class PartitioningPage;
    friend class InstallPage;
    friend class PackagesPage;
    friend class RestoreBackupPage;
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

#endif
