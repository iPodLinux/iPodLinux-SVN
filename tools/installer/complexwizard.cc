/****************************************************************************
 **
 ** Copyright (C) 2004-2005 Trolltech AS. All rights reserved.
 **
 ** This file is part of the documentation of the Qt Toolkit.
 **
 ** This file may be used under the terms of the GNU General Public
 ** License version 2.0 as published by the Free Software Foundation
 ** and appearing in the file LICENSE.GPL included in the packaging of
 ** this file.  Please review the following information to ensure GNU
 ** General Public Licensing requirements will be met:
 ** http://www.trolltech.com/products/qt/opensource.html
 **
 ** If you are unsure which license is appropriate for your use, please
 ** review the following information:
 ** http://www.trolltech.com/products/qt/licensing.html or contact the
 ** sales department at sales@trolltech.com.
 **
 ** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 ** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 **
 ****************************************************************************/

#include <QtGui>

#include "complexwizard.h"

ComplexWizard::ComplexWizard(QWidget *parent)
    : QWidget(parent)
{
    cancelButton = new QPushButton(tr("Cancel"));
    backButton = new QPushButton(tr("< &Back"));
    nextButton = new QPushButton(tr("Next >"));

    connect(cancelButton, SIGNAL(clicked()), qApp, SLOT(closeAllWindows()));
    connect(backButton, SIGNAL(clicked()), this, SLOT(backButtonClicked()));
    // nextButton is connected in switchPage

    buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addSpacing(5);
    buttonLayout->addWidget(backButton);
    buttonLayout->addWidget(nextButton);

    // nice generic titles for widgets that don't provide 'em
    pageTitle = new QLabel (tr ("<b>iPodLinux Installer</b>"));
    pageDesc = new QLabel (tr ("Follow the directions on this page and click Next."));
    pageDesc->setAlignment (Qt::AlignLeft);
    pageDesc->setIndent (20);

    toptextLayout = new QVBoxLayout;
    toptextLayout->addWidget (pageTitle);
    toptextLayout->addSpacing (10);
    toptextLayout->addWidget (pageDesc);

    icon = new QLabel;
    icon->setPixmap (QPixmap (":/icon.png"));
    
    topbarLayout = new QHBoxLayout;
    topbarLayout->addLayout (toptextLayout);
    topbarLayout->addStretch (1);
    topbarLayout->addWidget (icon);

    topbar = new QFrame;
    topbar->setLayout (topbarLayout);
    topbar->setFrameShape (QFrame::StyledPanel);
    topbar->setFrameShadow (QFrame::Raised);
    topbar->setLineWidth (2);
    topbar->setMaximumHeight (90);

    QPalette framePal = topbar->palette();
    framePal.setColor (QPalette::Background, QColor (240, 240, 240));
    topbar->setPalette (framePal);

    mainLayout = new QVBoxLayout;
    mainLayout->addLayout(buttonLayout);
    setLayout(mainLayout);
}

void ComplexWizard::setFirstPage(WizardPage *page)
{
    page->resetPage();
    history.append(page);
    switchPage(0);
}

void ComplexWizard::backButtonClicked()
{
    WizardPage *oldPage = history.takeLast();
    oldPage->resetPage();
    switchPage(oldPage);
}

void ComplexWizard::nextButtonClicked()
{
    WizardPage *oldPage = history.last();
    WizardPage *newPage = oldPage->nextPage();
    newPage->resetPage();
    history.append(newPage);
    switchPage(oldPage);
}

void ComplexWizard::completeStateChanged()
{
    WizardPage *currentPage = history.last();
    nextButton->setEnabled(currentPage->isComplete());
}

void ComplexWizard::switchPage(WizardPage *oldPage)
{
    if (oldPage) {
        oldPage->hide();
        mainLayout->removeWidget(oldPage);
        disconnect(oldPage, SIGNAL(completeStateChanged()),
                   this, SLOT(completeStateChanged()));
    }
    mainLayout->removeWidget(topbar);
    topbar->hide();

    WizardPage *newPage = history.last();
    if (history.size() == 1) {
        mainLayout->insertWidget(0, newPage);
    } else {
        mainLayout->insertWidget(0, newPage);
        mainLayout->insertWidget(0, topbar);
        topbar->show();
    }
    newPage->show();
    newPage->setFocus();
    connect(newPage, SIGNAL(completeStateChanged()),
            this, SLOT(completeStateChanged()));

    backButton->setEnabled(history.size() != 1);
    nextButton->setDefault(true);
    nextButton->setEnabled(false);
    if (newPage->isLastPage()) {
        disconnect (nextButton, SIGNAL(clicked()), this, 0);
        connect(nextButton, SIGNAL(clicked()), this, SLOT(accept()));
        nextButton->setText (tr ("&Finish"));
    } else {
        disconnect (nextButton, SIGNAL(clicked()), this, 0);
        connect(nextButton, SIGNAL(clicked()), this, SLOT(nextButtonClicked()));
        nextButton->setText (tr ("Next >"));
    }
    completeStateChanged();
}

WizardPage::WizardPage(QWidget *parent)
    : QWidget(parent)
{
    hide();
}

void WizardPage::resetPage()
{
}

WizardPage *WizardPage::nextPage()
{
    return 0;
}

bool WizardPage::isLastPage()
{
    return false;
}

bool WizardPage::isComplete()
{
    return true;
}
