/* ipl/installer/installer.cc -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * Distributed under the GPL. See COPYING for details.
 */

#include "installer.h"
#include "actions.h"
#include "panes.h"
#include "rawpod/partition.h"
#include "rawpod/fat32.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPixmap>
#include <QCheckBox>
#include <QRadioButton>

#include <string.h>

enum { StandardInstall, AdvancedInstall,
       Update, Uninstall } Mode;

QList<Action*> *PendingActions;

Installer::Installer (QWidget *parent)
    : ComplexWizard (parent)
{
    introPage = new IntroductionPage (this);

    setFirstPage (introPage);
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
    return (wizard->podlocPage = new PodLocationPage (wizard));
}

PodLocationPage::PodLocationPage (Installer *wizard)
    : InstallerPage (wizard)
{
    enum { CantFindIPod, InvalidPartitionTable, FSErr, BadSysInfo,
           NotAnIPod, MacPod, WinPod, SLinPod, BLinPod } status;
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
    upgradeRadio = uninstallRadio = 0;
    stateOK = 0;
    
    wizard->resize (530, 410);
    wizard->setMinimumSize (500, 410);
    wizard->setMaximumSize (640, 500);

    int rdlen;
    VFS::File *sysinfo;
    char sysinfo_contents[4096];

    if (podloc < 0) { status = CantFindIPod; goto err; }
    if (devReadMBR (podloc, mbr) != 0) { status = InvalidPartitionTable; goto err; }
    ptbl = partCopyFromMBR (mbr);
    if (!ptbl) { status = InvalidPartitionTable; goto err; }

    ipodtype = partFigureOutType (ptbl);
    partFreeTable (ptbl);
    
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
    if (fat32->init() < 0) { status = FSErr; goto err; }

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

 err:
    delete fat32;
    delete part;
    
    if (!hw_ver || (hw_ver >= 0xA && status == SLinPod)) { // error
        blurb = new QLabel;
        blurb->setWordWrap (true);
        blurb->setAlignment (Qt::AlignTop | Qt::AlignLeft);

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
            break;
        case FSErr:
            err = tr("<p><b>Error accessing filesystem.</b> I was unable to properly access "
                     "the <tt>SysInfo</tt> file on your iPod; either it didn't exist, there's "
                     "something wrong with your iPod's filesystem, or (most likely) there's "
                     "a bug in the installer. Check if the file exists at "
                     "<i>iPod</i><tt>/iPod_Control/Device/SysInfo</tt>; if it does, report a bug. "
                     "If not, try restarting your iPod.</p>");
            break;
        case BadSysInfo:
            err = tr("<p><b>Invalid SysInfo file.</b> There was something wrong with the syntax "
                     "of your SysInfo file; try restarting your iPod.</p>");
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
            break;
        default:
            err = tr("<p><b>Unknown error: Success.</b> Something's up. Report a bug.</p>");
            break;
        }
        stateOK = 0;
        emit completeStateChanged();
        err = QString (tr ("<p><b><font color=\"red\">Sorry, but an error occurred. Installation "
                           "cannot continue.</font></b></p>\n")) + err
#ifndef WIN32
              + QString (tr ("<p>Error code: %1 (%2)</p>")).arg (errno).arg (errno? strerror (errno) :
                                                                           tr ("Success?!"))
#endif
	      ;
        blurb->setText (err);
        wizard->setInfoText (tr ("<b>Error</b>"),
                             tr ("Read the message below and press Cancel to exit."));

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget (blurb);
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
                    .arg (devGetSize(podloc) >> 21)
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
        advancedCheck = new QCheckBox (tr ("Customize partitioning (experienced users only)"));
        upgradeRadio = uninstallRadio = 0;
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
        uninstallRadio = new QRadioButton (tr ("Uninstall iPodLinux"));
        subblurb = new QLabel (tr ("Please choose either an update or an uninstall "
                                   "so I can display something more informative here."));
        subblurb->setAlignment (Qt::AlignTop | Qt::AlignLeft);
        subblurb->setIndent (10);
        subblurb->setWordWrap (true);
        subblurb->resize (width(), 100);

        connect (upgradeRadio, SIGNAL(clicked(bool)), this, SLOT(upgradeRadioClicked(bool)));
        connect (uninstallRadio, SIGNAL(clicked(bool)), this, SLOT(uninstallRadioClicked(bool)));

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget (blurb);
        layout->addSpacing (10);
        layout->addWidget (upgradeRadio);
        layout->addWidget (uninstallRadio);
        layout->addStretch (1);
        layout->addWidget (subblurb);
        setLayout (layout);
        stateOK = 0;
        emit completeStateChanged();
    }
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
    
    subblurb->setText (tr ("<p>The update will check for new versions of the kernel and "
                           "the podzilla you have installed; you will be given the chance "
                           "to install them if new versions are present. "
                           "You will also be able to change the packages you have installed.</p>\n"
                           "<p>Press Next to continue.</p>"));
    stateOK = 1;
    emit completeStateChanged();
}

void PodLocationPage::resetPage() 
{
    if (advancedCheck) advancedCheck->setChecked (0);
    if (upgradeRadio) upgradeRadio->setChecked (0);
    if (uninstallRadio) uninstallRadio->setChecked (0);
    if (subblurb) subblurb->setText (tr ("Please choose either an update or an uninstall "
                                         "so I can display something more informative here."));
    stateOK = !upgradeRadio;
    emit completeStateChanged();
}

WizardPage *PodLocationPage::nextPage() 
{
    if (upgradeRadio) {
        if (upgradeRadio->isChecked()) {
            Mode = Update;
        } else {
            Mode = Uninstall;
        }
    } else {
        if (advancedCheck && advancedCheck->isChecked()) {
            Mode = AdvancedInstall;
        } else {
            Mode = StandardInstall;
        }
    }

    switch (Mode) {
    case StandardInstall:
        /* add an action for partitioning */
        return wizard->instPage;
    case AdvancedInstall:
        return wizard->partPage;
    case Update:
        return wizard->pkgPage;
    case Uninstall:
        return wizard->restorePage;
    }

    return 0;
}

bool PodLocationPage::isComplete() 
{
    return stateOK;
}
