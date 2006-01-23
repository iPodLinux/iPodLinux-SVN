/* ipl/installer/installer.cc -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * Distributed under the GPL. See COPYING for details.
 */

#include "installer.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QPixmap>

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

