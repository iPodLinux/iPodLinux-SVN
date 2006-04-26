/* ipl/installer/installer.cc -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * Distributed under the GPL. See COPYING for details.
 */

#include "installer.h"
#include "actions.h"
#include "panes.h"
#include "rawpod/partition.h"
#include "rawpod/device.h"
#include "rawpod/fat32.h"
#include "rawpod/ext2.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPixmap>
#include <QCheckBox>
#include <QRadioButton>
#include <QSpinBox>

#include <string.h>
#include <ctype.h>

InstallerMode Mode;
LoaderType iPodLoader = UnknownLoader;

QList<Action*> *PendingActions;

int iPodLocation;
int iPodVersion;
PartitionTable iPodPartitionTable;
int iPodPartitionToShrink;
int iPodLinuxPartitionSize;
VFS::Device *iPodDevice;
VFS::Device *iPodFirmwarePartitionDevice, *iPodMusicPartitionDevice, *iPodLinuxPartitionDevice;
VFS::Filesystem *iPodMusicPartitionFS, *iPodLinuxPartitionFS;
bool iPodDoBackup;
QString iPodBackupLocation;
QString InstallerHome;
Installer *installer;

Installer::Installer (QWidget *parent)
    : ComplexWizard (parent)
{
    installer = this;

    setFirstPage (new IntroductionPage (this));
    setWindowTitle (tr ("iPodLinux Installer"));
    resize (530, 410);
    setMinimumSize (500, 410);
    setMaximumSize (640, 500);

    PendingActions = new QList<Action*>;
}

IntroductionPage::IntroductionPage (Installer *wizard)
    : InstallerPage (wizard)
{
    blurb = new QLabel;
    blurb->setWordWrap (true);
    blurb->setAlignment (Qt::AlignTop | Qt::AlignLeft);
    blurb->setText (tr ("<p><b>Welcome to the iPodLinux installer!</b></p>\n"
                        "<p>In just a few short steps, this program will help you install Linux "
                        "on your iPod. Please close all programs accessing your iPod "
#ifdef __linux__
                        "and unmount it "
#endif
                        "before continuing.</p>\n"
                        "<p>If something goes wrong during this process, and your iPod refuses to boot "
                        "normally, <i>don't panic!</i> Follow the directions at "
                        "<a href=\"http://ipodlinux.org/Key_Combinations\">the iPodLinux website</a> "
                        "to reboot the iPod to disk mode, then rerun this installer and choose to "
                        "restore the backup you hopefully made. <i>It is impossible to permanently "
                        "damage your iPod by installing iPodLinux!</i></p>\n"
                        "<p>Now, make sure your iPod is plugged in and nothing is using it, verify "
                        "that you are "
#ifdef __linux__
                        "root"
#else
                        "an administrator"
#endif
                        ", and press Next to run a few checks. This may take a few seconds;"
                        " be patient.</p>\n"));
    blurb->setMaximumWidth (370);

    QLabel *pic = new QLabel;
    pic->setPixmap (QPixmap (":/installer.png"));
    pic->setAlignment (Qt::AlignTop | Qt::AlignLeft);

    QHBoxLayout *layout = new QHBoxLayout;
    layout->addWidget (pic);
    layout->addSpacing (10);
    layout->addWidget (blurb);
    setLayout (layout);
}

WizardPage *IntroductionPage::nextPage() 
{
    return new PodLocationPage (wizard);
}

PodLocationPage::PodLocationPage (Installer *wizard)
    : InstallerPage (wizard), wasError (0)
{
    enum { CantFindIPod, InvalidPartitionTable, FSErr, BadSysInfo,
           NotAnIPod, MacPod, WinPod, SLinPod, BLinPod, UnsupPod } status;
    int hw_ver = 0;
    int podloc = find_iPod();
    int ipodtype = PART_NOT_IPOD;
    unsigned char mbr[512];
    PartitionTable ptbl;
    char *p = 0;
    int rev = 0;
    VFS::Device *part = 0;
    FATFS *fat32 = 0;

    blurb = subblurb = 0;
    advancedCheck = 0;
    upgradeRadio = changeLoaderRadio = uninstallRadio = 0;
    stateOK = 0;
    
    wizard->resize (530, 440);
    wizard->setMinimumSize (500, 410);
    wizard->setMaximumSize (640, 500);

    int rdlen;
    VFS::File *sysinfo;
    char sysinfo_contents[4096];

    if (podloc < 0) { status = CantFindIPod; goto err; }
    if (devReadMBR (podloc, mbr) != 0) { status = InvalidPartitionTable; goto err; }
    ptbl = partCopyFromMBR (mbr);
    if (!ptbl) { status = InvalidPartitionTable; goto err; }

    ipodtype = partFigureOutType (ptbl, mbr);
    
    switch (ipodtype) {
    case PART_WINPOD:
        status = WinPod;
        break;
    case PART_MACPOD:
        status = MacPod;
        goto err;
    case PART_SLINPOD:
        status = SLinPod;
        break;
    case PART_BLINPOD:
        status = BLinPod;
        break;
    default:
    case PART_NOT_IPOD:
        status = NotAnIPod;
        goto err;
    }

    part = setup_partition (podloc, 2);
    if (!part) { status = CantFindIPod; goto err; }
    
    fat32 = new FATFS (part);
    int e;
    if ((e = fat32->init()) < 0) { status = FSErr; errno = -e; goto err; }

    sysinfo = fat32->open ("/IPOD_C~1/DEVICE/SYSINFO", O_RDONLY);
    if (!sysinfo || sysinfo->error()) {
        status = FSErr;
#ifndef WIN32
        errno = sysinfo? sysinfo->error() : 0;
#endif
        goto err;
    }
    
    rdlen = sysinfo->read (sysinfo_contents, 4096);
    sysinfo->close();
    delete sysinfo;
    
    if (rdlen <= 0) {
        status = BadSysInfo;
#ifndef WIN32
        errno = -rdlen;
#endif
        goto err;
    } else if (rdlen >= 4096) {
        status = BadSysInfo;
#ifndef WIN32
        errno = E2BIG;
#endif
        goto err;
    }

    p = sysinfo_contents;
    rev = 0;
    while (p) {
        while (isspace (*p)) p++;

        if (!strncmp (p, "boardHwSwInterfaceRev:", strlen ("boardHwSwInterfaceRev:"))) {
            if ((p = strchr (p, ':'))) {
                p++;
                while (isspace (*p)) p++;
                rev = strtol (p, 0, 0);
                break;
            }
            status = BadSysInfo;
            break;
        }

        p = strchr (p, '\n');
    }
    if (!rev || !(rev >> 16) || (rev >> 20))
        status = BadSysInfo;
    else
        hw_ver = rev >> 16;

    if (!(INSTALLER_WORKING_IPODS & (1 << hw_ver)))
        status = UnsupPod;

 err:
    delete fat32;
    delete part;
    
    if (!hw_ver || (hw_ver >= 0xA && status == SLinPod)) { // error
        blurb = new QLabel;
        blurb->setWordWrap (true);
        blurb->setAlignment (Qt::AlignTop | Qt::AlignLeft);
        bool restoreOK = false;

        wasError = 1;

        QString err;
        switch (status) {
        case CantFindIPod:
            err = tr("<p><b>Could not find your iPod.</b> I checked all the drives "
                     "in your system for something that looks like an iPod, but "
                     "I didn't find anything. Please verify the following.</p>\n"
                     "<ul><li>Make sure you have administrator privileges on the "
                     "system. You need them.</li>\n"
                     "<li>Make sure your iPod is a WinPod, not a MacPod. Use the Apple "
                     "restore utility if you need to convert it.</li>\n"
                     "<li>Make sure exactly one iPod is indeed plugged in.</li>\n"
                     "<li>Be sure your iPod is not ejected; "
#ifdef __linux__
                     "it's okay to run <tt>umount</tt> but not <tt>eject</tt>."
#else
                     "just exit programs that are using the iPod."
#endif
                     "</li></ul>\n"
                     "<i>If you've followed all these directions and nothing works, this "
                     "is a bug. Report it.</i>");
            break;
        case InvalidPartitionTable:
            err = tr("<p><b>Invalid partition table.</b> The iPod I found didn't look "
                     "like it had a valid partition table. It's probably a MacPod; those don't "
                     "work.</p>");
            restoreOK = true;
            break;
        case FSErr:
            err = tr("<p><b>Error accessing filesystem.</b> I was unable to properly access "
                     "the <tt>SysInfo</tt> file on your iPod; either it didn't exist, there's "
                     "something wrong with your iPod's filesystem, or (most likely) there's "
                     "a bug in the installer. Check if the file exists at "
                     "<i>iPod</i><tt>/iPod_Control/Device/SysInfo</tt>; if it does, report a bug. "
                     "If not, try restarting your iPod into the Apple firmware to recreate it.</p>");
            restoreOK = true;
            break;
        case BadSysInfo:
            err = tr("<p><b>Invalid SysInfo file.</b> There was something wrong with the syntax "
                     "of your SysInfo file; try restarting your iPod.</p>");
            restoreOK = true;
            break;
        case NotAnIPod:
            err = tr("<p><b>Not an iPod.</b> The iPod I identified turned out not to be an iPod "
                     "at all. This shouldn't happen; did you unplug your iPod in the middle of "
                     "detection? If not, it's a bug.</p>");
            break;
        case MacPod:
            err = tr("<p><b>iPod is a MacPod.</b> Sorry, but those aren't supported for Windows "
                     "and Linux installations. You may want to use the iPod Updater to convert it "
                     "to a WinPod.</p>");
            break;
        case SLinPod:
            err = tr("<p><b>Invalid preexisting iPodLinux installation.</b> You have a 5G or nano "
                     "and you did not install Linux correctly when you installed it. Sorry, "
                     "but I don't know enough to fix the problem myself. Run the iPod Updater or "
                     "restore your backup, then re-run this installer.</p>");
            restoreOK = true;
            break;
        case UnsupPod:
            err = tr("<p><b>Unsupported iPod type.</b> You appear to have a very new iPod that "
                     "we don't know about and thus can't support. Please be patient, and "
                     "don't bug the developers about this. Support will be developed eventually.</p>");
            restoreOK = true;
            break;
        default:
            err = tr("<p><b>Unknown error.</b> Something's up. Report a bug.</p>");
            break;
        }
        stateOK = 0;
        emit completeStateChanged();
#ifdef WIN32
        char e[512];
        TCHAR E[512];
        FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, 0, GetLastError(), 0, E, 512, 0);
        TCHAR *p = E;
        char *q = e;
        while (*p) *q++ = *p++;
#endif
        err = QString (tr ("<p><b><font color=\"red\">Sorry, but an error occurred. Installation "
                           "cannot continue.</font></b></p>\n")) + err
#ifdef WIN32
            + QString (tr ("<p>Error code: %1 (%2)</p>")).arg (GetLastError()).arg (e)
#else
            + QString (tr ("<p>Error code: %1 (%2)</p>")).arg (errno).arg (errno? strerror (errno) :
                                                                           tr ("Success?!"))
#endif
            ;
        blurb->setText (err);
        wizard->setInfoText (tr ("<b>Error</b>"),
                             tr ("Read the message below and press Cancel to exit."));

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget (blurb);
        if (restoreOK) {
            iPodLocation = podloc;
            iPodDevice = new LocalRawDevice (podloc);

            QPushButton *restoreBackup = new QPushButton (tr ("Restore a previously-made backup"));
            connect (restoreBackup, SIGNAL(clicked(bool)), this, SLOT(doBackupRestore(bool)));
            layout->addSpacing (20);
            layout->addWidget (restoreBackup);
        }
        layout->addStretch (1);
        setLayout (layout);
        return;
    }

    const char *gens[] = { "generation 0x0", // 0x0
                           "first generation", // 0x1
                           "second generation", // 0x2
                           "third generation", // 0x3
                           "first generation mini", // 0x4
                           "black-and-white fourth generation (Click Wheel)", // 0x5
                           "photo", // 0x6
                           "second generation mini", // 0x7
                           "generation 0x8",
                           "generation 0x9",
                           "generation 0xA",
                           "fifth generation (video)", // 0xB
                           "nano", // 0xC
                           "generation 0xD",
                           "generation 0xE",
                           "generation 0xF" };

    blurb = new QLabel;
    blurb->setWordWrap (true);
    blurb->setAlignment (Qt::AlignTop | Qt::AlignLeft);
    blurb->setText (QString ("<p>I found a <b>%1GB</b> iPod at physical drive <b>%2</b>. "
                             "It seems to be a <b>%3</b> iPod, ")
                    .arg ((devGetSize(podloc) + 1000000) / 2000000)
                    .arg (podloc)
                    .arg (gens[hw_ver]));
    if (status == WinPod) {
        blurb->setText (blurb->text() +
                        tr ("and does not have iPodLinux installed. If you continue, "
                            "it will be installed. If you already have iPodLinux installed, "
                            "<i>do not continue</i>.</p>\n"));
        if (hw_ver >= 0xA) {
            blurb->setText (blurb->text() +
                            tr ("<p><font color=\"red\">You seem to have a Nano or Video. Due to some "
                                "complicated suspend-to-disk logic in the Apple firmware, I will have to "
                                "shrink your music partition to make room for iPodLinux. Thus, "
                                "<b>all data on the iPod will be lost!</b></font></p>\n"));
        } else {
            if (hw_ver >= 0x4) {
                blurb->setText (blurb->text() +
                                tr ("<p>You seem to have a fourth-generation or newer iPod. While these iPods "
                                    "should work, be aware that they are <b>UNSUPPORTED</b>.</p>\n"));
            }
            blurb->setText (blurb->text() +
                            tr ("<p>Due to the layout of your iPod's hard drive, I should be able to install "
                                "Linux without erasing anything on it. However, if this goes wrong, you may need "
                                "to use the Apple updater to \"restore\" the iPod, which <i>does</i> erase data. "
                                "Thus, <b>please</b> ensure that you have a backup of all data on your iPod!</p>\n"));
        }
        wizard->setInfoText (tr ("<b>Installation Information</b>"),
                             tr ("Read the information below and choose your installation type."));
        advancedCheck = new QCheckBox (tr ("Advanced partitioning and package selection"));
        upgradeRadio = changeLoaderRadio = uninstallRadio = 0;
        subblurb = 0;

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget (blurb);
        layout->addStretch (1);
        layout->addWidget (advancedCheck);
        layout->addSpacing (5);
        setLayout (layout);
        stateOK = 1;
        emit completeStateChanged();
    } else {
        blurb->setText (blurb->text() + tr ("with iPodLinux already installed.</p>"));
        wizard->setInfoText (tr ("<b>What to do?</b>"), tr ("Select an action below."));
        
        advancedCheck = 0;
        upgradeRadio = new QRadioButton (tr ("Update my existing installation"));
        changeLoaderRadio = new QRadioButton (tr ("Change loader type and/or boot order"));
        uninstallRadio = new QRadioButton (tr ("Uninstall iPodLinux"));
        subblurb = new QLabel (tr ("Please choose either an update or an uninstall "
                                   "so I can display something more informative here."));
        subblurb->setAlignment (Qt::AlignTop | Qt::AlignLeft);
        subblurb->setIndent (10);
        subblurb->setWordWrap (true);
        subblurb->resize (width(), 100);

        connect (upgradeRadio, SIGNAL(clicked(bool)), this, SLOT(upgradeRadioClicked(bool)));
        connect (changeLoaderRadio, SIGNAL(clicked(bool)), this, SLOT(changeLoaderRadioClicked(bool)));
        connect (uninstallRadio, SIGNAL(clicked(bool)), this, SLOT(uninstallRadioClicked(bool)));

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget (blurb);
        layout->addSpacing (10);
        layout->addWidget (upgradeRadio);
        layout->addWidget (changeLoaderRadio);
        layout->addWidget (uninstallRadio);
        layout->addStretch (1);
        layout->addWidget (subblurb);
        setLayout (layout);
        stateOK = 0;
        emit completeStateChanged();
    }

    iPodLocation = podloc;
    iPodDevice = new LocalRawDevice (podloc);
    iPodVersion = hw_ver;
    iPodPartitionTable = ptbl;
}

void PodLocationPage::uninstallRadioClicked (bool clicked) 
{
    (void)clicked;
    
    subblurb->setText (tr ("<p>OK, I guess you really do want to uninstall iPodLinux. "
                           "Please be sure you have the backup you made handy if you "
                           "want an easy uninstall. If you've lost it, I'll make do, "
                           "but I might not get everything right.</p>"
                           "<p>Press Next when you're ready.</p>"));
    stateOK = 1;
    emit completeStateChanged();
}

void PodLocationPage::upgradeRadioClicked (bool clicked) 
{
    (void)clicked;
    
    subblurb->setText (tr ("<p>The update will check for new versions of all packages "
                           "you have installed; you will be given the chance to upgrade "
                           "any that have updates, uninstall some packages, install some "
                           "new ones, or any combination of those.</p>\n"
                           "<p>Press Next to continue.</p>"));
    stateOK = 1;
    emit completeStateChanged();
}

void PodLocationPage::changeLoaderRadioClicked (bool clicked) 
{
    (void)clicked;
    
    subblurb->setText (tr ("<p>This will give you the opportunity to change the boot order "
                           "(whether Linux or the Apple firmware is default) or, if you so "
                           "desire, to change to iPodLoader2.</p>\n"
                           "<p>Press Next to continue.</p>"));
    stateOK = 1;
    emit completeStateChanged();
}

void PodLocationPage::resetPage() 
{
    if (advancedCheck) advancedCheck->setChecked (0);
    if (upgradeRadio) upgradeRadio->setChecked (0);
    if (changeLoaderRadio) changeLoaderRadio->setChecked (0);
    if (uninstallRadio) uninstallRadio->setChecked (0);
    if (subblurb) subblurb->setText (tr ("Please choose either an update or an uninstall "
                                         "so I can display something more informative here."));
    stateOK = !upgradeRadio;
    emit completeStateChanged();
}

void Installer::setupDevices (PartitionTable ptbl)
{
    if (iPodFirmwarePartitionDevice) delete iPodFirmwarePartitionDevice;
    if (iPodMusicPartitionDevice) delete iPodMusicPartitionDevice;
    if (iPodLinuxPartitionDevice) delete iPodLinuxPartitionDevice;
    
    if (iPodPartitionTable != ptbl) iPodPartitionTable = partDupTable (ptbl);

    iPodFirmwarePartitionDevice = new PartitionDevice (iPodDevice,
                                                       iPodPartitionTable[0].offset,
                                                       iPodPartitionTable[0].length);
    iPodMusicPartitionDevice = new PartitionDevice (iPodDevice,
                                                    iPodPartitionTable[1].offset,
                                                    iPodPartitionTable[1].length);
    iPodLinuxPartitionDevice = new PartitionDevice (iPodDevice,
                                                    iPodPartitionTable[2].offset,
                                                    iPodPartitionTable[2].length);
}

void Installer::setupFilesystems() 
{
    int err;
    
    iPodMusicPartitionFS = new FATFS (iPodMusicPartitionDevice);
    if ((err = iPodMusicPartitionFS->init()) < 0) {
        QMessageBox::critical (0, tr ("Error"),
                               tr ("Error creating filesystem accessor for music partition: %1").arg (strerror (-err)),
                               tr ("Quit"));
        exit (1);
    }
    iPodLinuxPartitionFS = new Ext2FS (iPodLinuxPartitionDevice);
    if ((err = iPodLinuxPartitionFS->init()) < 0) {
        QMessageBox::critical (0, tr ("Error"),
                               tr ("Error creating filesystem accessor for ext2 partition: %1").arg (strerror (-err)),
                               tr ("Quit"));
        exit (1);
    }
    return;
}

void PodLocationPage::doBackupRestore (bool c) 
{
    (void)c;
    
    Mode = Uninstall;
    wizard->changePage (new UninstallPage (wizard));
}

WizardPage *PodLocationPage::nextPage() 
{
    if (upgradeRadio) {
        if (upgradeRadio->isChecked()) {
            Mode = Update;
        } else if (changeLoaderRadio->isChecked()) {
            Mode = ChangeLoader;
        } else {
            Mode = Uninstall;
        }
        iPodPartitionToShrink = 0;
    } else {
        if (advancedCheck && advancedCheck->isChecked()) {
            Mode = AdvancedInstall;
        } else {
            Mode = StandardInstall;
        }
        if (iPodVersion >= 0xA) {
            iPodPartitionToShrink = 2;
            iPodLinuxPartitionSize = 128 * 2048; /* 128M */
            if (iPodPartitionTable[1].length > 8192 * 2048)
                iPodLinuxPartitionSize = 256 * 2048; /* 256M if data ptn is >=8GB */
        } else {
            iPodPartitionToShrink = 1;
            iPodLinuxPartitionSize = iPodPartitionTable[0].length / 2;
            if (iPodLinuxPartitionSize < 24 * 2048)
                iPodLinuxPartitionSize = 24 * 2048; /* make it at least 24M */
            if ((iPodPartitionTable[0].length - iPodLinuxPartitionSize) < 8 * 2048)
                iPodLinuxPartitionSize = iPodPartitionTable[0].length - 8 * 2048; /* keep at least 8MB for the firmware */
        }
    }

    VFS::File *fh;

    switch (Mode) {
    case StandardInstall:
        return new InstallPage (wizard);
    case AdvancedInstall:
        return new PartitioningPage (wizard);
    case Update:
    case ChangeLoader:
        ((Installer *)wizard)->setupDevices (iPodPartitionTable);
        ((Installer *)wizard)->setupFilesystems();

        fh = iPodLinuxPartitionFS->open ("/etc/loadertype", O_RDONLY);
        if (fh && !fh->error()) {
            char buf[4] = "?";
            fh->read (buf, 3);
            buf[3] = 0;
            switch (buf[0]) {
            case 'a':
            case 'A':
                iPodLoader = Loader1Apple;
                break;
            case 'l':
            case 'L':
                iPodLoader = Loader1Linux;
                break;
            case '2':
                iPodLoader = Loader2;
                break;
            default:
                iPodLoader = UnknownLoader;
                break;
            }
            fh->close();
        }
        delete fh;
        if (Mode == ChangeLoader)
            return new ChangeLoaderPage (wizard);
        else
            return new PackagesPage (wizard);
    case Uninstall:
        return new UninstallPage (wizard);
    }

    return 0;
}

bool PodLocationPage::isComplete() 
{
    return stateOK;
}

bool PodLocationPage::isLastPage() 
{
    return wasError;
}

PartitioningPage::PartitioningPage (Installer *wiz)
    : InstallerPage (wiz)
{
    wiz->setInfoText (tr ("<b>Advanced Partitioning</b>"), tr ("Fill in the requested information and click Next."));
    topblurb = new QLabel (tr ("<p>If you've chosen this path, I assume you know what you're doing. "
                               "If you don't understand this, just click Next; the defaults are "
                               "OK.</p><p>Make room for iPodLinux by:</p>"));
    topblurb->setWordWrap (true);
    partitionSmall = new QRadioButton (tr ("Shrinking the firmware partition"));
    partitionBig = new QRadioButton (tr ("Shrinking the data partition"));
    if (iPodPartitionToShrink == 2)
        partitionBig->setChecked (true);
    else
        partitionSmall->setChecked (true);
    size = new QSpinBox;
    size->setValue (iPodLinuxPartitionSize / 2048);
    size->setSuffix (" MB");
    sizeBlurb = new QLabel (tr ("How big should the Linux partition be?"));
    sizeBlurb->setAlignment (Qt::AlignRight);
    sizeBlurb->setIndent (10);
    spaceLeft = new QLabel;
    spaceLeft->setWordWrap (true);

    QVBoxLayout *layout = new QVBoxLayout;
    QHBoxLayout *szlayout = new QHBoxLayout;
    szlayout->addWidget (sizeBlurb);
    szlayout->addWidget (size);
    layout->addWidget (topblurb);
    layout->addWidget (partitionSmall);
    layout->addWidget (partitionBig);
    layout->addSpacing (15);
    layout->addLayout (szlayout);
    layout->addWidget (spaceLeft);
    layout->addStretch (1);
    layout->addWidget (new QLabel (tr ("Press Next to continue.")));
    setLayout (layout);
    setStuff();

    connect (size, SIGNAL(valueChanged(int)), this, SLOT(setStuff(int)));
    connect (partitionSmall, SIGNAL(toggled(bool)), this, SLOT(setSmallStuff(bool)));
    connect (partitionBig, SIGNAL(toggled(bool)), this, SLOT(setBigStuff(bool)));
}

void PartitioningPage::setStuff (int newVal) 
{
    (void)newVal;
    
    if (iPodPartitionToShrink == 2) {
        size->setRange (24, (iPodPartitionTable[1].length / 2048) / 2);
    } else {
        size->setRange (16, (iPodPartitionTable[0].length / 2048) - 8);
    }
    spaceLeft->setText (QString (tr ("This size configuration gives <b>%1MB</b> of space "
                                     "left for music and data."))
                        .arg ((iPodPartitionTable[iPodPartitionToShrink - 1].length / 2048) -
                              size->value()));
    if (iPodPartitionToShrink != 2) spaceLeft->hide();
    else spaceLeft->show();
}

void PartitioningPage::setBigStuff (bool chk) 
{
    if (chk) {
        iPodPartitionToShrink = 2;
        iPodLinuxPartitionSize = 128 * 2048; /* 128M */
        if (iPodPartitionTable[1].length > 8192 * 2048)
            iPodLinuxPartitionSize = 256 * 2048; /* 256M if data ptn is >=8GB */
        setStuff(0);
        size->setValue (iPodLinuxPartitionSize / 2048);
    }
}

void PartitioningPage::setSmallStuff (bool chk)
{
    if (chk) {
        iPodPartitionToShrink = 1;
        iPodLinuxPartitionSize = iPodPartitionTable[0].length / 2;
        if (iPodLinuxPartitionSize < 24 * 2048)
            iPodLinuxPartitionSize = 24 * 2048; /* make it at least 24M */
        if ((iPodPartitionTable[0].length - iPodLinuxPartitionSize) < 8 * 2048)
            iPodLinuxPartitionSize = iPodPartitionTable[0].length - 8 * 2048; /* keep at least 8MB for the firmware */
        setStuff(0);
        size->setValue (iPodLinuxPartitionSize / 2048);
    }
}

void PartitioningPage::resetPage() 
{}

WizardPage *PartitioningPage::nextPage() 
{
    iPodLinuxPartitionSize = size->value() * 2048;
    return new InstallPage (wizard);
}

bool PartitioningPage::isComplete() 
{
    return true;
}

ChangeLoaderPage::ChangeLoaderPage (Installer *wiz)
    : InstallerPage (wiz)
{
    wiz->setInfoText (tr ("<b>Loader Selection</b>"), tr ("Select the radio button for the loader to which you would like to change and click Next."));
    
    blurb = new QLabel (tr ("Please select the radio button for the load order or loader you want to "
                            "change to below. The currently selected one is your current loader."));
    blurb->setWordWrap (true);
    loader1apple = new QRadioButton (tr ("Standard loader with Apple firmware default"));
    loader1linux = new QRadioButton (tr ("Standard loader with iPodLinux default"));
    loader2 = new QRadioButton (tr ("iPodLoader2 (nice menu interface, but still experimental)"));
    ldrchoiceblurb = new QLabel;
    ldrchoiceblurb->setWordWrap (true);

    connect (loader1apple, SIGNAL(toggled(bool)), this, SLOT(setLoader1Blurb(bool)));
    connect (loader1linux, SIGNAL(toggled(bool)), this, SLOT(setLoader1Blurb(bool)));
    connect (loader2, SIGNAL(toggled(bool)), this, SLOT(setLoader2Blurb(bool)));

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget (blurb);
    layout->addWidget (loader1apple);
    layout->addWidget (loader1linux);
    layout->addWidget (loader2);
    layout->addWidget (ldrchoiceblurb);
    layout->addStretch (1);
    setLayout (layout);

    resetPage();
}

void ChangeLoaderPage::resetPage() 
{
    loader1apple->setChecked (false);
    loader1linux->setChecked (false);
    loader2->setChecked (false);

    if (iPodLoader == Loader1Apple)
        loader1apple->setChecked (true);
    else if (iPodLoader == Loader1Linux)
        loader1linux->setChecked (true);
    else if (iPodLoader == Loader2)
        loader2->setChecked (true);
}

void ChangeLoaderPage::setLoader1Blurb (bool chk) 
{
    if (chk) {
        ldrchoiceblurb->setText (tr ("To boot the non-default OS, hold rewind as your iPod restarts."));
    }
    emit completeStateChanged();
}

void ChangeLoaderPage::setLoader2Blurb (bool chk) 
{
    if (chk) {
        ldrchoiceblurb->setText (tr ("iPodLoader2 can also load Rockbox, multiple kernels, etc; see "
                                     "the iPodLinux wiki for more information."));
    }
    emit completeStateChanged();
}

WizardPage *ChangeLoaderPage::nextPage() 
{
    LoaderType newLoader;

    if (loader2->isChecked()) newLoader = Loader2;
    else if (loader1linux->isChecked()) newLoader = Loader1Linux;
    else newLoader = Loader1Apple;
    
    PendingActions->append (new ChangeLoaderAction (iPodLoader, newLoader));

    if ((iPodLoader == Loader2 && !loader2->isChecked()) ||
        (iPodLoader != Loader2 &&  loader2->isChecked())) {
        iPodLoader = newLoader;
        return new PackagesPage (wizard, true);
    } else {
        PendingActions->append (new FirmwareRecreateAction);
        return new DoActionsPage (wizard, new DonePage (wizard));
    }
}

UninstallPage::UninstallPage (Installer *wiz)
    : InstallerPage (wiz)
{
    wiz->setInfoText (tr ("<b>Uninstallation Information</b>"), tr ("Fill in the information below and click Next."));

    blurb = new QLabel (tr ("Uninstallation can be done most reliably if you made a backup. If you did, "
                            "please browse to its path below. If not, uncheck the box and I'll see what "
                            "I can do."));
    blurb->setWordWrap (true);
    nobackupblurb = new QLabel (tr ("OK, I guess you didn't make one. In the future, please do so. I'll "
                                    "try my best to restore your iPod to normal, but I may not be able to; "
                                    "in this case, you'll need to use the Apple updater to restore it, "
                                    "erasing all music and data. Sorry. Click Next."));
    nobackupblurb->setWordWrap (true);
    haveBackup = new QCheckBox (tr ("Yes! I made a backup."));
    haveBackup->setChecked (true);
    backupPathLabel = new QLabel (tr ("Load backup:"));
    backupPath = new QLineEdit ("");
    backupBrowse = new QPushButton (tr ("Browse..."));

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget (blurb);
    layout->addSpacing (10);
    layout->addWidget (haveBackup);
    QHBoxLayout *bkplayout = new QHBoxLayout;
    bkplayout->addWidget (backupPathLabel);
    bkplayout->addWidget (backupPath);
    bkplayout->addWidget (backupBrowse);
    layout->addLayout (bkplayout);
    layout->addWidget (nobackupblurb);
    layout->addStretch (1);
    nobackupblurb->hide();
    setLayout (layout);

    connect (haveBackup, SIGNAL(toggled(bool)), this, SLOT(setBackupBlurb(bool)));
    connect (backupPath, SIGNAL(textChanged(QString)), this, SIGNAL(completeStateChanged()));
    connect (backupBrowse, SIGNAL(released()), this, SLOT(openBrowseDialog()));

    setBackupBlurb (true);
}

void UninstallPage::setBackupBlurb (bool chk) 
{
    if (chk) {
        nobackupblurb->hide();
        backupPathLabel->show();
        backupPath->show();
        backupBrowse->show();
    } else {
        nobackupblurb->show();
        backupPathLabel->hide();
        backupPath->hide();
        backupBrowse->hide();
    }
    emit completeStateChanged();
}

void UninstallPage::openBrowseDialog() 
{
    QString ret = QFileDialog::getOpenFileName (this, "Choose a backup file to restore:",
                                                QString(), "Firmware backup files (*.fw)");
    if (ret != "")
        backupPath->setText (ret);
}

WizardPage *UninstallPage::nextPage()
{
    if (haveBackup->isChecked()) {
        PendingActions->append (new RestoreBackupAction (iPodLocation, backupPath->text()));
    } else {
        PendingActions->append (new HeuristicUninstallAction (iPodLocation));
    }
    return new DoActionsPage (wizard, new DonePage (wizard));
}

void UninstallPage::resetPage() 
{}

bool UninstallPage::isComplete() 
{
    return (!haveBackup->isChecked() || (backupPath->text().length() && QFile::exists (backupPath->text())));
}

InstallPage::InstallPage (Installer *wiz)
    : InstallerPage (wiz)
{
    wiz->setInfoText (tr ("<b>Installation Information</b>"), tr ("Fill in the information below and click Next."));
    
    topblurb = new QLabel (tr ("I'm almost ready to install, but first I need some info."));

    ldrblurb = new QLabel (tr ("First, you need to pick how you're going to load Linux. There are three ways: "));
    ldrblurb->setWordWrap (true);
    loader1apple = new QRadioButton (tr ("Standard loader with Apple firmware default"));
    loader1apple->setChecked (true);
    loader1linux = new QRadioButton (tr ("Standard loader with iPodLinux default"));
    loader2 = new QRadioButton (tr ("iPodLoader2 (nice menu interface, but still experimental)"));
    ldrchoiceblurb = new QLabel;
    ldrchoiceblurb->setWordWrap (true);

    bkpblurb = new QLabel (tr ("Second, it is <i>highly</i> recommended that you make a backup of "
                               "your iPod's firmware partition. It will be 40 to 120 MB in size."));
    bkpblurb->setWordWrap (true);
    bkpchoiceblurb = new QLabel (tr ("Are you sure? A backup is <b>highly recommended</b>. Without one, "
                                     "we can't guarantee that uninstallation will go smoothly."));
    bkpchoiceblurb->setWordWrap (true);
    makeBackup = new QCheckBox (tr ("Yes, I want to save a backup."));
    makeBackup->setChecked (true);
    backupPathLabel = new QLabel (tr ("Save as:"));
    backupPath = new QLineEdit (InstallerHome + "/ipod_os_backup.fw");
    backupBrowse = new QPushButton (tr ("Browse..."));
    
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget (topblurb);
    layout->addSpacing (10);
    layout->addWidget (ldrblurb);
    layout->addWidget (loader1apple);
    layout->addWidget (loader1linux);
    layout->addWidget (loader2);
    layout->addWidget (ldrchoiceblurb);
    layout->addSpacing (15);
    layout->addWidget (bkpblurb);
    layout->addWidget (makeBackup);
    QHBoxLayout *bkplayout = new QHBoxLayout;
    bkplayout->addWidget (backupPathLabel);
    bkplayout->addWidget (backupPath);
    bkplayout->addWidget (backupBrowse);
    layout->addLayout (bkplayout);
    layout->addWidget (bkpchoiceblurb);
    setLayout (layout);

    connect (loader1apple, SIGNAL(toggled(bool)), this, SLOT(setLoader1Blurb(bool)));
    connect (loader1linux, SIGNAL(toggled(bool)), this, SLOT(setLoader1Blurb(bool)));
    connect (loader2, SIGNAL(toggled(bool)), this, SLOT(setLoader2Blurb(bool)));
    connect (makeBackup, SIGNAL(toggled(bool)), this, SLOT(setBackupBlurb(bool)));
    connect (backupPath, SIGNAL(textChanged(QString)), this, SIGNAL(completeStateChanged()));
    connect (backupBrowse, SIGNAL(released()), this, SLOT(openBrowseDialog()));

    setLoader1Blurb (true);
    setBackupBlurb (true);
}

void InstallPage::setLoader1Blurb (bool chk) 
{
    if (chk) {
        ldrchoiceblurb->setText (tr ("To boot the non-default OS, hold rewind as your iPod restarts."));
    }
}

void InstallPage::setLoader2Blurb (bool chk) 
{
    if (chk) {
        ldrchoiceblurb->setText (tr ("iPodLoader2 can also load Rockbox, multiple kernels, etc; see "
                                     "the iPodLinux wiki for more information."));
    }
}

void InstallPage::setBackupBlurb (bool chk) 
{
    if (chk) {
        bkpchoiceblurb->hide();
        backupPathLabel->show();
        backupPath->show();
        backupBrowse->show();
    } else {
        bkpchoiceblurb->show();
        backupPathLabel->hide();
        backupPath->hide();
        backupBrowse->hide();
    }
    emit completeStateChanged();
}

void InstallPage::openBrowseDialog() 
{
    QString ret = QFileDialog::getSaveFileName (this, "Choose a place to save your backup.",
                                                QString(), "Firmware backup files (*.fw)");
    if (ret != "")
        backupPath->setText (ret);
}

WizardPage *InstallPage::nextPage() 
{
    if (makeBackup->isChecked()) {
        iPodDoBackup = true;
        iPodBackupLocation = backupPath->text();
        PendingActions->append (new BackupAction (iPodLocation, iPodBackupLocation));
    } else {
        iPodDoBackup = false;
        iPodBackupLocation = QString ("");
    }

    if (loader1apple->isChecked()) {
        iPodLoader = Loader1Apple;
    } else if (loader1linux->isChecked()) {
        iPodLoader = Loader1Linux;
    } else {
        iPodLoader = Loader2;
    }

    PendingActions->append (new PartitionAction (iPodLocation, iPodPartitionToShrink,
                                                 3, 0x83, iPodLinuxPartitionSize));
    if (iPodPartitionToShrink == 2) {
        PendingActions->append (new FormatAction (2, CreateFATFilesystem, "Formatting the music partition."));
    }
    PendingActions->append (new FormatAction (3, CreateExt2Filesystem, "Formatting the Linux partition."));
    PendingActions->append (new FSSetupAction);
    return new PackagesPage (wizard, (Mode == StandardInstall));
}

bool InstallPage::isComplete() 
{
    return (!makeBackup->isChecked() || backupPath->text().length());
}

QString sizeToString (int amount) 
{
    int leftovers = 0;
    const char *suffix = "BkMGTPEZY";

    if (amount < 1024) return QString::number (amount);
    
    while (suffix[1] && (amount > 1024)) {
        leftovers = amount & 0x3ff;
        amount >>= 10;
        suffix++;
    }

    // turn it to 000...999 instead of 0...1023
    leftovers = leftovers * 1000 / 1024;

    // Significant digit handling
    int sigdig = 0;
    if (amount == 0) {
        sigdig = 3;
    } else if (amount < 10) {
        sigdig = 2;
        leftovers /= 10;
        while (leftovers >= 100) leftovers /= 10;
    } else if (amount < 100) {
        sigdig = 1;
        leftovers /= 100;
    }

    QString decpart = "";
    if (sigdig) decpart = QObject::tr (".%1").arg (leftovers, sigdig, 10, QChar ('0'));

    return QObject::tr ("%1%2%3").arg (amount).arg (decpart).arg (*suffix);
}

QString transferProgressText (int done, int total) 
{
    return QObject::tr ("%1/%2").arg (sizeToString (done)).arg (sizeToString (total));
}

DonePage::DonePage (Installer *wiz)
    : InstallerPage (wiz)
{
    wizard->setInfoText (tr ("<b>Finished</b>"), tr ("Installation is complete."));

    blurb = new QLabel;
    blurb->setWordWrap (true);

    switch (Mode) {
    case StandardInstall:
    case AdvancedInstall:
        blurb->setText (tr ("<p><b>Congratulations!</b></p>\n"
                            "<p>You have successfully installed iPodLinux.</p>\n"
                            "<p>Should you wish to install more packages, get "
                            "updates, change your loader default, or uninstall "
                            "iPodLinux, simply rerun this installer. It will "
                            "detect your existing installation and offer you "
                            "new options.</p>\n"
                            "<p>If you need help with something, we have some forums "
                            "at <a href=\"http://ipodlinux.org/forums\">http://ipodlinux.org/forums/</a> "
                            "and an IRC channel (<b>#ipodlinux</b> on irc.freenode.net). "
                            "<b><i>Please</b></i> thoroughly search the wiki "
                            "(<a href=\"http://ipodlinux.org\">http://ipodlinux.org</a>) "
                            "and forums before asking us for help!</p>\n"
                            "<p>Thank you, and have fun!</p>"));
        break;
    case Update:
        blurb->setText (tr ("<p><b>Your updates and package changes have been successfully installed.</b></p>\n"
                            "<p>Should you wish to install more packages, get "
                            "updates, change your loader default, or uninstall "
                            "iPodLinux, simply rerun this installer. It will "
                            "detect your existing installation and offer you "
                            "some choices.</p>\n"
                            "<p>Thank you, and have fun!</p>"));
        break;
    case ChangeLoader:
        blurb->setText (tr ("<p><b>Your loader selection has been successfully changed.</b></p>\n"
                            "<p>Should you wish to install more packages, get "
                            "updates, change your loader default, or uninstall "
                            "iPodLinux, simply rerun this installer. It will "
                            "detect your existing installation and offer you "
                            "some choices.</p>\n"
                            "<p>Thank you, and have fun!</p>"));
        break;
    case Uninstall:
        blurb->setText (tr ("<p>iPodLinux has been successfully uninstalled.</p>"));
        break;
    }

    QVBoxLayout *vbl = new QVBoxLayout (this);
    vbl->addWidget (blurb);
    vbl->addStretch (1);
    setLayout (vbl);
}
