/* ipl/installer/installer.h -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * Distributed under the GPL. See COPYING for details.
 */

#ifndef INSTALLER_H
#define INSTALLER_H

#include "complexwizard.h"
#include "rawpod/partition.h"

// The human-readable version.
#define INSTALLER_VERSION         "2.0a"
// The integer version for the package list format.
#define INSTALLER_PACKAGE_VERSION  2
// A bitmask of working iPods.
// 0x18fe == 0b 0 0 0 1 1 0 0 0 1 1 1 1 1 1 1 0
//              ^-^-^ ^ ^ ^-^-^ ^ ^ ^ ^ ^ ^ ^ ^- no 0th gen (0)
//   No D,E,F ---+-+  | |  \ /  | | | | +-+-+- 1g-3g supported (1, 2, 3)
//    (C) Nano works -+ |   |   | | | +- Mini1g works (4)
//     (B) Video works -+ No 8, | | +- 4g works (5)
//                         9,A  | +- Photo/Color works (6)
//                              +- Mini2g works (7)
#define INSTALLER_WORKING_IPODS    0x18fe
#define INSTALLER_SUPPORTED_IPODS  0x00fe

enum InstallerMode { StandardInstall, AdvancedInstall,
                     Update, Uninstall };
extern InstallerMode Mode;
// Physical drive number of the iPod.
extern int iPodLocation;
// boardHwSwInterfaceRev >> 16 (0x4 = mini, 0xB = video, etc.)
extern int iPodVersion;
// partition number (1=first) to shrink
extern int iPodPartitionToShrink;
// new partition size in sectors
extern int iPodLinuxPartitionSize;
// iPod's partition table
extern PartitionTable iPodPartitionTable;
// VFS::Device for the whole iPod.
extern VFS::Device *iPodDevice;
// VFS::Device for each partition. NOTE: These are 0 for an install!
extern VFS::Device *iPodFirmwarePartitionDevice, *iPodMusicPartitionDevice, *iPodLinuxPartitionDevice;

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
class QTreeWidgetItem;

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

// Call once the partitions are set in stone.
void setupDevices();

#endif
