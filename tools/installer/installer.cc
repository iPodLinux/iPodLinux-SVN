/* ipl/installer/installer.cc -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * Distributed under the GPL. See COPYING for details.
 */

#include "installer.h"
#include "rawpod/partition.h"
#include "rawpod/fat32.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPixmap>

#include <string.h>

Installer::Installer (QWidget *parent)
    : ComplexWizard (parent)
{
    introPage = new IntroductionPage (this);

    setFirstPage (introPage);
    setWindowTitle (tr ("iPodLinux Installer"));
    resize (450, 380);
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
                        ", and press Next to run a few checks.</p>\n"));
    blurb->setMaximumWidth (350);

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

    VFS::File *sysinfo = fat32->open ("/IPOD_C~1/DEVICE/SYSINFO", O_RDONLY);
    if (!sysinfo || sysinfo->error()) {
        status = FSErr;
        errno = sysinfo? sysinfo->error() : 0;
        goto err;
    }
    
    char sysinfo_contents[4096];
    int rdlen = sysinfo->read (sysinfo_contents, 4096);
    sysinfo->close();
    delete sysinfo;
    
    if (rdlen <= 0) {
        status = BadSysInfo;
        errno = -rdlen;
        goto err;
    } else if (rdlen >= 4096) {
        status = BadSysInfo;
        errno = E2BIG;
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
    if (!rev || !(rev >> 16))
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
            err = ("<p><b>Could not find your iPod.</b> I checked all the drives "
                   "in your system for something that looks like an iPod, but "
                   "I didn't find anything. Please verify the following.</p>\n"
                   "<ul><li>Verify that you have administrator privileges on the "
                   "system. If not, you will probably not be able to access raw "
                   "devices, and so this installer will not work.</li>\n"
                   "<li>Make sure you have exactly <i>one</i> iPod plugged in.</li>\n"
                   "<li>Make sure your iPod is a WinPod, not a MacPod. Use the Apple "
                   "restore utility if you need to convert it.</li>\n"
                   "<li>Make sure your iPod is indeed plugged in.</li>\n"
                   "<li>Be sure your iPod is not ejected; on Linux, it's okay to run "
                   "<tt>umount</tt> but not <tt>eject</tt>. On Windows, don't do anything "
                   "except exit programs that are using the iPod.</li></ul>\n"
                   "<i>If you've followed all these directions and nothing works, this "
                   "is a bug. Report it.</i>");
            break;
        case InvalidPartitionTable:
            err = ("<p><b>Invalid partition table.</b> The iPod I found didn't look "
                   "like it had a valid partition table. It's probably a MacPod; those don't "
                   "work.</p>");
            break;
        case FSErr:
            err = ("<p><b>Error accessing filesystem.</b> I was unable to properly access "
                   "the <tt>SysInfo</tt> file on your iPod; either it didn't exist, there's "
                   "something wrong with your iPod's filesystem, or (most likely) there's "
                   "a bug in the installer. Check if the file exists at "
                   "<i>iPod</i><tt>/iPod_Control/Device/SysInfo</tt>; if it does, report a bug. "
                   "If not, try restarting your iPod.</p>");
            break;
        case BadSysInfo:
            err = ("<p><b>Invalid SysInfo file.</b> There was something wrong with the syntax "
                   "of your SysInfo file; try restarting your iPod.</p>");
            break;
        case NotAnIPod:
            err = ("<p><b>Not an iPod.</b> The iPod I identified turned out not to be an iPod "
                   "at all. This shouldn't happen; did you unplug your iPod in the middle of "
                   "detection? If not, it's a bug.</p>");
            break;
        case MacPod:
            err = ("<p><b>iPod is a MacPod.</b> Sorry, but those aren't supported for Windows "
                   "and Linux installations. You may want to use the iPod Updater to convert it "
                   "to a WinPod.</p>");
            break;
        case SLinPod:
            err = ("<p><b>Invalid preexisting iPodLinux installation.</b> You have a 5G or nano "
                   "and you did not install Linux correctly when you installed it. Sorry, "
                   "but I don't know enough to fix the problem myself. Run the iPod Updater or "
                   "restore your backup, then re-run this installer.</p>");
            break;
        default:
            err = ("<p><b>Unknown error: Success.</b> Something's up. Report a bug.</p>");
            break;
        }
        stateOK = 0;
        err = QString ("<p><b><font color=\"red\">Sorry, but an error occurred. Installation "
                       "cannot continue.</font></b></p>\n") + err;
        blurb->setText (err);
        wizard->setInfoText ("Error", "Read the message below and press Cancel to exit.");

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget (blurb);
        setLayout (layout);
        return;
    }

    stateOK = 1;
}

void PodLocationPage::resetPage() 
{}

WizardPage *PodLocationPage::nextPage() 
{
    return 0;
}

bool PodLocationPage::isComplete() 
{
    return stateOK;
}
