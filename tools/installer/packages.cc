/* packages.cc -*- C++ -*- Copyright (c) 2006 Joshua Oreman. All rights reserved.
 * The iPodLinux installer, of which this file is a part, is licensed under the
 * GNU General Public License, which may be found in the file COPYING in the
 * source distribution.
 */

#include "installer.h"
#include "packages.h"
#include "panes.h"
#include "rawpod/ext2.h"

#include <QHttp>
#include <QRegExp>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QtDebug>
#include <QTextStream>

Package::Package()
    : _name ("unk"), _version ("0.1a"), _dest ("/"), _desc ("???"), _url ("http://127.0.0.1/"),
      _subfile ("."), _type (Archive), _reqs (QStringList()), _provs (QStringList()),
      _ipods (INSTALLER_WORKING_IPODS), _valid (false)
{}

Package::Package (QString line) 
    : _name ("unk"), _version ("0.0"), _dest ("/"), _desc ("???"), _url ("http://127.0.0.1/"),
      _subfile ("."), _type (Archive), _reqs (QStringList()), _provs (QStringList()),
      _ipods (INSTALLER_WORKING_IPODS), _valid (false)
{
    parseLine (line);
}

void Package::parseLine (QString line) 
{
    if (line.indexOf ('#') >= 0)
        line.truncate (line.indexOf ('#'));
    line = line.trimmed();
    if (line == "") return;

    QRegExp rx ("^"
                "\\s*" "\\[(kernel|loader|file|archive)\\]"
                "\\s*" "([a-zA-Z0-9_-]+)"
                "\\s+" "([0-9.YMDSVN-]+" "[abc]?)"
                "\\s*" ":"
                "\\s*" "\"([^\"]*)\""
                "\\s*" "at"
                "\\s+" "(\\S+)"
                "\\s*");
    
    if (rx.indexIn (line, 0) < 0) {
        qWarning ("Bad package list line: |%s| (doesn't match rx)", line.toAscii().data());
        return;
    }
    
    QString rest = line;
    rest = rest.remove (0, rx.matchedLength()).simplified();
    
    if (rx.cap (1) == "kernel")
        _type = Kernel;
    else if (rx.cap (1) == "loader")
        _type = Loader;
    else if (rx.cap (1) == "file")
        _type = File;
    else
        _type = Archive;

    _name = rx.cap (2);
    _version = rx.cap (3);
    _desc = rx.cap (4);
    _url = rx.cap (5);

    if (rest != "") {
        QStringList bits = rest.split (" ");
        QStringListIterator it (bits);
        while (it.hasNext()) {
            QString key = it.next();
            if (!it.hasNext()) {
                qWarning ("Bad package file line: |%s| (odd keyword)", line.toAscii().data());
                return;
            }
            QString value = it.next();
            
            if (key == "file") {
                _subfile = value;
            } else if (key == "to") {
                _dest = value;
            } else if (key == "provides") {
                if (!_provs.contains (value))
                    _provs << value;
            } else if (key == "requires") {
                if (!_reqs.contains (value))
                    _reqs << value;
            } else if (key == "supports") {
                _ipods = 0;
                for (int c = 0; c < value.length(); ++c) {
                    char ch = value[c].toAscii();
                    if (ch >= '0' && ch <= '9') {
                        _ipods ^= (1 << (ch - '0'));
                    } else if (toupper (ch) >= 'A' && toupper (ch) <= 'F') {
                        _ipods ^= (1 << (toupper (ch) - 'A' + 10));
                    } else if (ch == '~' || ch == '^' || ch == '!') {
                        _ipods = ~_ipods;
                    } else {
                        qWarning ("Bad package list line: |%s| (unsupported supports character `%c')",
                                  line.toAscii().data(), ch);
                    }
                }
            } else {
                qWarning ("Bad package list line: |%s| (unrecognized keyword %s)",
                          line.toAscii().data(), key.toAscii().data());
                return;
            }
        }
    }
    _valid = true;
}

void Package::readPackingList (VFS::Device *dev) 
{
    if (!dev) return;

    Ext2FS *ext2 = new Ext2FS (dev);
    ext2->setWritable (0);
    if (ext2->init() < 0) {
        qWarning ("Could not open ext2 filesystem to read packing list.");
        delete ext2;
        return;
    }
    
    VFS::Dir *dirp = ext2->opendir ("/etc/packages");
    if (dirp->error()) {
        qWarning ("/etc/packages: %s", strerror (dirp->error()));
        delete ext2;
        return;
    }

    struct VFS::dirent d;
    while (dirp->readdir (&d) > 0) {
        if (!strcmp (d.d_name, _name.toAscii().data())) {
            qDebug ("Found packing list for %s", d.d_name);
            VFS::File *packlist = ext2->open (QString ("/etc/packages/%1").arg (d.d_name).toAscii().data(),
                                              O_RDONLY);
            if (packlist->error()) {
                qWarning ("/etc/packages/%s: %s", d.d_name, strerror (packlist->error()));
                delete packlist;
                delete ext2;
                return;
            }

            char *buf = new char[16384];
            int rdlen = packlist->read (buf, 16384);
            if (rdlen <= 0) {
                if (rdlen < 0)
                    qWarning ("Reading packing list for %s: %s", d.d_name, strerror (-rdlen));
                else
                    qWarning ("Packling list for %s is empty", d.d_name);
                delete[] buf;
                delete packlist;
                delete ext2;
                return;
            } else if (rdlen >= 16383) {
                qWarning ("Packing list for %s too big; truncated", d.d_name);
            }
            buf[rdlen] = 0;

            QByteArray bary (buf, rdlen);
            QTextStream reader (&bary, QIODevice::ReadOnly);
            QString line;
            QRegExp vrx ("\\s*[Vv]ersion\\s+([0-9A-Za-z.-]+)\\s*");

            while (!((line = reader.readLine()).isNull())) {
                if (vrx.exactMatch (line)) { // the "version" line
                    _upgrade = (vrx.cap(1) != _version);
                } else {
                    _packlist << line;
                }
            }
        }
    }
}

PackagesPage::PackagesPage (Installer *wiz)
    : InstallerPage (wiz), advok (false), errored (false)
{
    blurb = new QLabel (tr ("<p>Here you may select packages to install for iPodLinux. Check the boxes "
                            "next to each package you would like to install.</p>"
                            "<p><i>Please wait while the package list is downloaded.</i></p>"));
    blurb->setWordWrap (true);
    progressStmt = new QLabel (tr ("<b>Initializing...</b>"));

    wizard->resize (640, 520);
    wizard->setMinimumSize (500, 410);
    wizard->setMaximumSize (1280, 1024);

    QStringList headers;
    headers << "Name" << "Version" << "Action" << "Description";
    packages = new QTreeWidget;
    packages->setHeaderLabels (headers);
    connect (packages, SIGNAL(itemChanged(QTreeWidgetItem*, int)),
             this, SLOT(listClicked(QTreeWidgetItem*, int)));
    
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget (blurb);
    layout->addSpacing (10);
    layout->addWidget (progressStmt);
    layout->addWidget (packages);
    layout->addStretch (1);
    packages->hide();
    setLayout (layout);

    packlistHTTP = new QHttp;
    packlistHTTP->setHost ("ipodlinux.org", 80);
    connect (packlistHTTP, SIGNAL(dataSendProgress(int, int)), this, SLOT(httpSendProgress(int, int)));
    connect (packlistHTTP, SIGNAL(dataReadProgress(int, int)), this, SLOT(httpReadProgress(int, int)));
    connect (packlistHTTP, SIGNAL(stateChanged(int)), this, SLOT(httpStateChanged(int)));
    connect (packlistHTTP, SIGNAL(requestFinished(int, bool)), this, SLOT(httpRequestFinished(int, bool)));
    connect (packlistHTTP, SIGNAL(done(bool)), this, SLOT(httpDone(bool)));
    connect (packlistHTTP, SIGNAL(responseHeaderReceived(const QHttpResponseHeader&)),
             this, SLOT(httpResponseHeaderReceived(const QHttpResponseHeader&)));
    packlistHTTP->get ("/iPodLinux:Installation_sources?action=raw");
}

void PackagesPage::httpSendProgress (int done, int total) 
{
    if (total)
        progressStmt->setText (QString (tr ("<b>Sending request...</b> %1%")).arg (done * 100 / total));
    else
        progressStmt->setText (QString (tr ("<b>Sending request...</b> %1 bytes")).arg (done));
}

void PackagesPage::httpReadProgress (int done, int total) 
{
    if (total)
        progressStmt->setText (QString (tr ("<b>Transferring data...</b> %1%")).arg (done * 100 / total));
    else
        progressStmt->setText (QString (tr ("<b>Transferring data...</b> %1 bytes")).arg (done));
}

void PackagesPage::httpStateChanged (int state) 
{
    if (state == QHttp::HostLookup)
        progressStmt->setText (tr ("<b>Looking up host...</b>"));
    else if (state == QHttp::Connecting)
        progressStmt->setText (tr ("<b>Connecting...</b>"));
    else if (state == QHttp::Sending)
        progressStmt->setText (tr ("<b>Sending request...</b>"));
    else if (state == QHttp::Reading)
        progressStmt->setText (tr ("<b>Transferring data...</b>"));
    else if (state == QHttp::Connected)
        progressStmt->setText (tr ("<b>Connected.</b>"));
    else if (state == QHttp::Closing)
        progressStmt->setText (tr ("<b>Closing connection...</b>"));
    else if (state == QHttp::Unconnected)
        progressStmt->setText (tr ("Done."));
}

void PackagesPage::httpRequestFinished (int req, bool err) 
{
    (void)req;
    
    if (err) {
        qDebug ("Request %d has errored out (%s)", req, packlistHTTP->errorString().toAscii().data());
        if (!errored) {
            progressStmt->setText (QString (tr ("<b><font color=\"red\">Error: %1</font></b>"))
                                   .arg (packlistHTTP->errorString()));
            errored = true;
        }
        return;
    } else if (packlistHTTP->bytesAvailable()) {
        if (packlistHTTP->lastResponse().statusCode() == 200) {
            QStringList lines = QString (packlistHTTP->readAll().constData()).split ("\n");
            QStringListIterator it (lines);
            while (it.hasNext()) {
                QString line = it.next();
                QRegExp irx ("\\s*<<\\s*(\\S+)\\s*>>\\s*");
                QRegExp vrx ("\\s*[Vv]ersion\\s+(\\d+)\\s*");
                QRegExp nrx ("\\s*[Ii]nstaller\\s+([0-9A-Za-z.-]+)\\s*");
                if (irx.exactMatch (line)) {
                    QUrl url (irx.cap (1));
                    if (!url.isValid()) {
                        qWarning ("Invalid URL in package list: |%s|", irx.cap (1).toAscii().data());
                    } else {
                        packlistHTTP->setHost (url.host(), (url.port() > 0)? url.port() : 80);
                        packlistHTTP->get (url.toString (QUrl::RemoveScheme | QUrl::RemoveAuthority));
                    }
                } else if (vrx.exactMatch (line)) {
                    if (vrx.cap (1).toInt() != INSTALLER_PACKAGE_VERSION) {
                        if (!errored) {
                            progressStmt->setText (QString (tr ("<b><font color=\"red\">Error: Invalid "
                                                                "package list version (need %1, got %2)."))
                                                   .arg (INSTALLER_PACKAGE_VERSION).arg (vrx.cap (1)));
                            errored = true;
                        }
                        return;
                    }
                } else if (nrx.exactMatch (line)) {
                    // nothing yet
                } else {
                    Package pkg (line);
                    if (pkg.supports (iPodVersion) && pkg.valid()) {
                        QTreeWidgetItem *twi;
                        if (pkg.provides().size()) {
                            QList <QTreeWidgetItem *> provlist =
                                packages->findItems ("Packages providing `" + pkg.provides()[0] + "'",
                                                     Qt::MatchExactly);
                            if (provlist.size()) {
                                twi = new PkgTreeWidgetItem (provlist[0], pkg);
                            } else {
                                QTreeWidgetItem *provitem = new QTreeWidgetItem (packages,
                                                                                 QTreeWidgetItem::UserType);
                                provitem->setText (0, "Packages providing `" + pkg.provides()[0] + "'");
                                provitem->setFlags (0);
                                twi = new PkgTreeWidgetItem (provitem, pkg);
                            }
                        } else {
                            twi = new PkgTreeWidgetItem (packages, pkg);
                        }
                    }
                }
            }
        }
    }
}

void PackagesPage::httpDone (bool err) 
{
    if (err) {
        if (!errored) {
            progressStmt->setText (QString (tr ("<b><font color=\"red\">Error: %1</font></b>"))
                                   .arg (packlistHTTP->errorString()));
            errored = true;
        }
    } else {
        blurb->setText (tr ("<p>Here you may select packages to install for iPodLinux. Check the boxes "
                            "next to each package you would like to install.</p>"));
        progressStmt->hide();
        packages->show();
        advok = true;
    }
}

void PackagesPage::httpResponseHeaderReceived (const QHttpResponseHeader& resp) 
{
    if (resp.statusCode() >= 300 && resp.statusCode() < 400) { // redirect
        QUrl url (resp.value ("location"));
        packlistHTTP->setHost (url.host(), (url.port() > 0)? url.port() : 80);
        packlistHTTP->get (url.toString (QUrl::RemoveScheme | QUrl::RemoveAuthority));
    } else if (resp.statusCode() != 200) {
        if (!errored) {
            progressStmt->setText (QString (tr ("<b><font color=\"red\">Error: %1</font></b>"))
                                   .arg (resp.reasonPhrase()));
            errored = true;
        }
    }
}

void PackagesPage::listClicked (QTreeWidgetItem *item, int column) 
{
    // This is connected to itemChanged, which is connected to setCheckState,
    // which this function calls, so we need recursion protection.
    static int depth = 0;
    if (depth) return;
    depth++;

    if (column) { depth--; return; }

    PkgTreeWidgetItem *ptwi;
    if ((ptwi = dynamic_cast<PkgTreeWidgetItem*>(item)) != 0) {
        if (ptwi->checkState(0) == Qt::Checked)
            ptwi->select();
        else
            ptwi->deselect();
    }

    if (!item->parent()) { depth--; return; }

    // Items are mutually exclusive with their siblings.
    if (item->checkState(0) == Qt::Checked) {
        for (int i = 0; i < item->parent()->childCount(); i++) {
            if (item->parent()->child(i) == item) continue;
            if (item->parent()->child(i)->checkState(0) == Qt::Checked) {
                item->parent()->child(i)->setCheckState (0, Qt::Unchecked);
                if ((ptwi = dynamic_cast<PkgTreeWidgetItem*>(item->parent()->child(i))) != 0) {
                    ptwi->deselect();
                }
            }
        }
    }

    depth--;
}

WizardPage *PackagesPage::nextPage() 
{
    wizard->resize (530, 440);
    wizard->setMinimumSize (500, 410);
    wizard->setMaximumSize (640, 500);
    // Add package-installing and -removing actions
    return new DoActionsPage (wizard, /* new DonePage */0);
}

PkgTreeWidgetItem::PkgTreeWidgetItem (QTreeWidget *parent, Package pkg)
    : QTreeWidgetItem (parent, UserType), _pkg (pkg), _changemarked (false)
{
    setFlags (Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    setCheckState (0, Qt::Unchecked);
    setText (0, pkg.name());
    setText (1, pkg.version());
    setText (3, pkg.description());
    _setsel();
}

PkgTreeWidgetItem::PkgTreeWidgetItem (QTreeWidgetItem *parent, Package pkg)
    : QTreeWidgetItem (parent, UserType), _pkg (pkg), _changemarked (false)
{
    setFlags (Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    setCheckState (0, Qt::Unchecked);
    setText (0, pkg.name());
    setText (1, pkg.version());
    setText (3, pkg.description());
    _setsel();
}

void PkgTreeWidgetItem::_setsel() 
{
    if (_pkg.selected()) {
        if (_pkg.upgrade()) {
            setText (2, QObject::tr ("Upgrade"));
        } else if (_pkg.changed()) {
            setText (2, QObject::tr ("Install"));
        } else {
            setText (2, QObject::tr ("Keep"));
        }
    } else {
        if (_pkg.changed()) {
            setText (2, QObject::tr ("Uninstall"));
        } else {
            setText (2, QObject::tr ("None"));
        }
    }
}
