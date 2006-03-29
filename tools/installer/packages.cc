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
#include <QMessageBox>

Package::Package()
    : _name ("unk"), _version ("0.1a"), _dest ("/"), _desc ("???"), _url ("http://127.0.0.1/"),
      _subfile ("."), _type (Archive), _reqs (QStringList()), _provs (QStringList()),
      _ipods (INSTALLER_WORKING_IPODS), _valid (false),
      _orig (false), _upgrade (false), _selected (false), _required (false)
{}

Package::Package (QString line) 
    : _name ("unk"), _version ("0.0"), _dest ("/"), _desc ("???"), _url ("http://127.0.0.1/"),
      _subfile ("."), _type (Archive), _reqs (QStringList()), _provs (QStringList()),
      _ipods (INSTALLER_WORKING_IPODS), _valid (false),
      _orig (false), _upgrade (false), _selected (false), _required (false)
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
                "\\s+" "([0-9.YMDNCVS-]+" "[abc]?)"
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
    _valid = true;

    if (rest != "") {
        QStringList bits = rest.split (" ");
        QStringListIterator it (bits);
        while (it.hasNext()) {
            QString key = it.next();
            if (!it.hasNext()) {
                qWarning ("Bad package file line: |%s| (odd keyword)", line.toAscii().data());
                _valid = false;
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
                // Flags in _ipods: 0x10000 - specific 'pod type set
                //                  0x20000 - supports WinPod
                //                  0x40000 - supports MacPod
                _ipods = 0;
                for (int c = 0; c < value.length(); ++c) {
                    char ch = value[c].toAscii();
                    if (ch >= '0' && ch <= '9') {
                        _ipods ^= (1 << (ch - '0'));
                    } else if (toupper (ch) >= 'A' && toupper (ch) <= 'F') {
                        _ipods ^= (1 << (toupper (ch) - 'A' + 10));
                    } else if (ch == 'w' || ch == 'W') {
                        _ipods |= 0x10000;
                        _ipods ^= 0x20000;
                    } else if (ch == 'm' || ch == 'M') {
                        _ipods |= 0x10000;
                        _ipods ^= 0x40000;
                    } else if (ch == '~' || ch == '^' || ch == '!') {
                        _ipods = ~(_ipods & ~0x10000) | (_ipods & 0x10000);
                    } else {
                        qWarning ("Bad package list line: |%s| (unsupported supports character `%c')",
                                  line.toAscii().data(), ch);
                    }
                }
                if ((_ipods & 0x10000) && !(_ipods & 0x20000))
                    _ipods = 0;
                _ipods &= 0xffff;
            } else if (key == "default") {
                _selected = _required = true;
            } else if (key == "loadreq") {
                if (!((value == "loader1" && (iPodLoader == Loader1Apple || iPodLoader == Loader1Linux)) ||
                      (value == "loader2" && iPodLoader == Loader2) ||
                      (value == "loader1apple" && iPodLoader == Loader1Apple) ||
                      (value == "loader1linux" && iPodLoader == Loader1Linux)))
                    _valid = false;
            } else {
                qWarning ("Bad package list line: |%s| (unrecognized keyword %s)",
                          line.toAscii().data(), key.toAscii().data());
                _valid = false;
                return;
            }
        }
    }
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

    _orig = _selected = _upgrade = false;

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

            delete[] buf;
            delete packlist;
        }
    }
    delete dirp;
    delete ext2;

    _selected = true;
    _orig = true;
}

void Package::debug() 
{
    qDebug ("Package: %s v%s (\"%s\")", _name.toAscii().data(),
            _version.toAscii().data(), _desc.toAscii().data());
    qDebug ("    URL: %s (subfile %s) (installing to %s)", _url.toAscii().data(),
            _subfile.toAscii().data(), _dest.toAscii().data());
    qDebug ("   Reqs: %s", _reqs.join (", ").toAscii().data());
    qDebug ("  Provs: %s", _provs.join (", ").toAscii().data());
    qDebug ("  Flags: %c%c%c%c", _valid? 'V' : 'v', _orig? 'I' : 'i',
            _upgrade? 'U' : 'u', _selected? 'S' : 's');
    qDebug ("   Type: %d", _type);
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
    } else if (resolvers [req] && packlistHTTP->bytesAvailable()) {
        PkgTreeWidgetItem *item = resolvers [req];
        QString pat = item->package().url();
        pat.remove (0, pat.lastIndexOf ('/') + 1);
        if (pat.contains ("YYYYMMDD"))
            pat.replace ("YYYYMMDD", "([0-9][0-9][0-9][0-9]-?[0-9][0-9]-?[0-9][0-9])");
        if (pat.contains ("NNN"))
            pat.replace ("NNN", "([0-9][0-9][0-9][0-9]?[0-9]?)");

        QRegExp rx ("<[Aa] [Hh][Rr][Ee][Ff]=\"[^\"]+\">" + pat + "</[Aa]>");
        QStringList lines = QString (packlistHTTP->readAll().constData()).split ("\n");
        QStringListIterator it (lines);
        QString cap = "";
        while (it.hasNext()) {
            QString line = it.next();
            if (rx.indexIn (line) >= 0)
                cap = rx.cap(1);
        }
        if (cap != "") {
            item->package().version().replace ("YYYYMMDD", cap);
            item->package().version().replace ("NNN", cap);
            item->package().url().replace ("YYYYMMDD", cap);
            item->package().url().replace ("NNN", cap);
            item->package().subfile().replace ("YYYYMMDD", cap);
            item->package().subfile().replace ("NNN", cap);
        }
        item->update();
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
                                                                "package list version (need %1, got %2)."
                                                                "</font></b>"))
                                                   .arg (INSTALLER_PACKAGE_VERSION).arg (vrx.cap (1)));
                            errored = true;
                        }
                        return;
                    }
                } else if (nrx.exactMatch (line)) {
                    if (nrx.cap (1) != INSTALLER_VERSION) {
                        if (!errored) {
                            progressStmt->setText (QString (tr ("<b><font color=\"red\">Error: Out-of date "
                                                                "installer (this is %1, current is %2). Please "
                                                                "update.</font></b>"))
                                                   .arg (INSTALLER_VERSION).arg (nrx.cap (1)));
                            errored = true;
                        }
                        return;
                    }
                } else {
                    Package pkg (line);
                    if (pkg.supports (iPodVersion) && pkg.valid()) {
                        PkgTreeWidgetItem *twi;
                        if (pkg.provides().size()) {
                            QList <QTreeWidgetItem *> provlist =
                                packages->findItems ("Packages providing `" + pkg.provides()[0] + "'",
                                                     Qt::MatchExactly);
                            if (provlist.size()) {
                                twi = new PkgTreeWidgetItem (this, provlist[0], pkg);
                            } else {
                                QTreeWidgetItem *provitem = new QTreeWidgetItem (packages,
                                                                                 QTreeWidgetItem::UserType);
                                provitem->setText (0, "Packages providing `" + pkg.provides()[0] + "'");
                                provitem->setFlags (0);
                                packages->setItemExpanded (provitem, true);
                                twi = new PkgTreeWidgetItem (this, provitem, pkg);
                            }
                        } else {
                            twi = new PkgTreeWidgetItem (this, packages, pkg);
                        }
                        packageProvides.insert (pkg.name(), twi);
                        QStringListIterator pi (pkg.provides());
                        while (pi.hasNext()) {
                            packageProvides.insert (pi.next(), twi);
                        }

                        if (pkg.url().contains ("YYYYMMDD") || pkg.url().contains ("NNN")) {
                            QString dir = pkg.url();
                            dir.truncate (dir.lastIndexOf ('/'));
                            QUrl url (dir);
                            packlistHTTP->setHost (url.host(), (url.port() > 0)? url.port() : 80);
                            resolvers [packlistHTTP->get (url.toString (QUrl::RemoveScheme |
                                                                        QUrl::RemoveAuthority))] = twi;
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

        QList<QTreeWidgetItem *> provlist = packages->findItems ("Packages providing `[0-9A-Za-z._-]+'",
                                                                 Qt::MatchRegExp);
        QListIterator<QTreeWidgetItem *> provit (provlist);
        while (provit.hasNext()) {
            QTreeWidgetItem *item = provit.next();
            switch (item->childCount()) {
            case 1:
                packages->addTopLevelItem (item->takeChild (0));
                /* FALLTHRU */
            case 0:
                packages->takeTopLevelItem (packages->indexOfTopLevelItem (item));
                delete item;
                break;
            default:
                break;
            }
        }

        for (int i = 0; i < packages->topLevelItemCount(); i++) {
            QTreeWidgetItem *it = packages->topLevelItem(i);
            PkgTreeWidgetItem *item;
            if ((item = dynamic_cast<PkgTreeWidgetItem *>(it)) != 0) {
                item->package().readPackingList (iPodLinuxPartitionDevice);
                if (item->package().selected()) item->select();
            }
        }

        packages->show();
        advok = true;
        emit completeStateChanged();
    }
}

void PackagesPage::httpResponseHeaderReceived (const QHttpResponseHeader& resp) 
{
    if (resp.statusCode() >= 300 && resp.statusCode() < 400) { // redirect
        QUrl url (resp.value ("location"));
        packlistHTTP->setHost (url.host(), (url.port() > 0)? url.port() : 80);
        resolvers [packlistHTTP->get (url.toString (QUrl::RemoveScheme | QUrl::RemoveAuthority))] =
            resolvers [packlistHTTP->currentId()];
    } else if (resp.statusCode() != 200) {
        if (!errored) {
            progressStmt->setText (QString (tr ("<b><font color=\"red\">Error: %1</font></b>"))
                                   .arg (resp.reasonPhrase()));
            errored = true;
        }
    }
}

void PkgTreeWidgetItem::select() 
{
    _pkg.select();

    QStringListIterator rit (_pkg.requires());
    while (rit.hasNext()) {
        QString req = rit.next();
        bool haveAny = false;

        QList <PkgTreeWidgetItem *> satisfants = _page->packageProvides.values (req);
        for (int i = 0; i < satisfants.size(); i++) {
            if (satisfants[i]->package().selected())
                haveAny = true;
        }

        if (!haveAny) {
            if (satisfants.size()) {
                satisfants[0]->setCheckState (0, Qt::Checked);
                satisfants[0]->select();
            } else {
                QMessageBox::warning (_page, QObject::tr ("Missing dependency"),
                                      QObject::tr ("The package you selected, `%1', has a dependency \n"
                                                   "on `%2' which I was not able to satisfy. Sorry, \n"
                                                   "but you can't select this package.")
                                      .arg (package().name())
                                      .arg (req),
                                      QObject::tr ("Ok"));
                setCheckState (0, Qt::Unchecked);
                deselect();
            }
        }
    }

    if (parent()) {
        for (int i = 0; i < parent()->childCount(); i++) {
            PkgTreeWidgetItem *ptwi;

            if (parent()->child(i) == this) continue;
            if (parent()->child(i)->checkState(0) == Qt::Checked) {
                parent()->child(i)->setCheckState (0, Qt::Unchecked);
                if ((ptwi = dynamic_cast<PkgTreeWidgetItem*>(parent()->child(i))) != 0) {
                    ptwi->deselect();
                }
            }
        }
    }
    
    _setsel();
}

void PkgTreeWidgetItem::deselect() 
{
    if (_pkg.required() && !parent()) {
        setCheckState (0, Qt::Checked);
        return;
    }

    _pkg.deselect();

    QList <QTreeWidgetItem *> allItems = _page->packages->findItems (".*", Qt::MatchRegExp | Qt::MatchRecursive);
    QListIterator <QTreeWidgetItem *> it (allItems);

    while (it.hasNext()) {
        QTreeWidgetItem *i = it.next();
        PkgTreeWidgetItem *item;
        if (i->checkState(0) == Qt::Checked && (item = dynamic_cast<PkgTreeWidgetItem*>(i)) != 0) {
            QStringListIterator rit (item->_pkg.requires());
            while (rit.hasNext()) {
                QString req = rit.next();
                bool haveAny = false;
                
                QList <PkgTreeWidgetItem *> satisfants = _page->packageProvides.values (req);
                for (int i = 0; i < satisfants.size(); i++) {
                    if (satisfants[i]->package().selected())
                        haveAny = true;
                }
                
                if (!haveAny) {
                    item->setCheckState (0, Qt::Unchecked);
                    item->deselect();
                }
            }
        }
    }

    if (parent()) {
        bool required_provide = false;

        for (int i = 0; i < parent()->childCount(); i++) {
            PkgTreeWidgetItem *ptwi;
            
            if ((ptwi = dynamic_cast<PkgTreeWidgetItem*>(parent()->child(i))) != 0) {
                if (ptwi->_pkg.required()) {
                    required_provide = true;
                    break;
                }
            }
        }

        if (required_provide) {
            for (int i = 0; i < parent()->childCount(); i++) {
                PkgTreeWidgetItem *ptwi;
                
                if (parent()->child(i) == this) continue;
                if (parent()->child(i)->checkState(0) == Qt::Unchecked) {
                    parent()->child(i)->setCheckState (0, Qt::Checked);
                    if ((ptwi = dynamic_cast<PkgTreeWidgetItem*>(parent()->child(i))) != 0) {
                        ptwi->select();
                    }
                }
            }
        }
    }

    _setsel();
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

    depth--;
}

WizardPage *PackagesPage::nextPage() 
{
    wizard->resize (530, 440);
    wizard->setMinimumSize (500, 410);
    wizard->setMaximumSize (640, 500);

    QList <QTreeWidgetItem *> allItems = packages->findItems (".*", Qt::MatchRegExp | Qt::MatchRecursive);
    QListIterator <QTreeWidgetItem *> it (allItems);

    bool needsReLoader = false, needsReKernel = false;

    // First, do all the removes...
    while (it.hasNext()) {
        PkgTreeWidgetItem *item;
        if ((item = dynamic_cast<PkgTreeWidgetItem *>(it.next())) != 0) {
            if (item->package().changed()) {
                if (!item->package().selected()) {
                    // remove
                    PendingActions->append (new PackageRemoveAction (item->package(), tr ("Removing:")));
                }
            }
        }
    }

    // ... then, do the upgrades and installs.
    it = allItems;
    while (it.hasNext()) {
        PkgTreeWidgetItem *item;
        if ((item = dynamic_cast<PkgTreeWidgetItem *>(it.next())) != 0) {
            if (item->package().changed()) {
                if (item->package().selected()) {
                    if (item->package().upgrade()) {
                        // remove, install
                        PendingActions->append (new PackageRemoveAction (item->package(),
                                                                         tr ("Upgrading "
                                                                             "(uninstalling old version):")));
                        PendingActions->append (new PackageInstallAction (item->package(),
                                                                          tr ("Upgrading "
                                                                              "(installing new version):")));
                    } else {
                        // install
                        PendingActions->append (new PackageInstallAction (item->package(), tr ("Installing:")));
                    }
                }

                if (item->package().type() == Package::Kernel)
                    needsReKernel = true;
                if (item->package().type() == Package::Loader)
                    needsReLoader = true;
            }
        }
    }

    if (needsReLoader || needsReKernel) {
        PendingActions->append (new FirmwareRecreateAction);
    }

    return new DoActionsPage (wizard, /* new DonePage */0);
}

PkgTreeWidgetItem::PkgTreeWidgetItem (PackagesPage *page, QTreeWidget *parent, Package pkg)
    : QTreeWidgetItem (parent, UserType), _pkg (pkg), _changemarked (false), _page (page)
{
    setFlags (Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    update();
    _setsel();
}

PkgTreeWidgetItem::PkgTreeWidgetItem (PackagesPage *page, QTreeWidgetItem *parent, Package pkg)
    : QTreeWidgetItem (parent, UserType), _pkg (pkg), _changemarked (false), _page (page)
{
    setFlags (Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    update();
    _setsel();
}

void PkgTreeWidgetItem::update() 
{
    if (_pkg.selected())
        setCheckState (0, Qt::Checked);
    else
        setCheckState (0, Qt::Unchecked);
    setText (0, _pkg.name());
    setText (1, _pkg.version());
    setText (3, _pkg.description());
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
