/* packages.cc -*- C++ -*- Copyright (c) 2006 Joshua Oreman. All rights reserved.
 * The iPodLinux installer, of which this file is a part, is licensed under the
 * GNU General Public License, which may be found in the file COPYING in the
 * source distribution.
 */

#include "installer.h"
#include "packages.h"
#include "panes.h"
#include "rawpod/vfs.h"
#include "rawpod/device.h"
#include "rawpod/ext2.h"
#include "zlib/zlib.h"
#include "libtar/libtar.h"

#include <QHttp>
#include <QRegExp>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QtDebug>
#include <QTextStream>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QTimer>
#include <QCheckBox>
#include <QPushButton>
#include <QRadioButton>
#include <QGroupBox>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QGridLayout>

#include <ctype.h>
#ifndef DONT_HAVE_LIBCRYPTO
#include <openssl/md5.h>
#endif

Package::Package()
    : _name ("unk"), _version ("0.1a"), _dest (""), _desc ("???"), _url ("http://127.0.0.1/"),
      _subfile ("."), _category (""), _idinfo (NoIdentifyingInfo), _filesize (0),
      _type (Archive), _unsupported (false), _reqs (QStringList()),
      _provs (QStringList()), _ipods (INSTALLER_WORKING_IPODS), _valid (false),
      _orig (false), _upgrade (false), _selected (false), _required (false)
{}

Package::Package (QString line) 
    : _name ("unk"), _version ("0.0"), _dest (""), _desc ("???"), _url ("http://127.0.0.1/"),
      _subfile ("."), _category (""), _idinfo (NoIdentifyingInfo), _filesize (0),
      _type (Archive), _unsupported (false), _reqs (QStringList()),
      _provs (QStringList()), _ipods (INSTALLER_WORKING_IPODS), _valid (false),
      _orig (false), _upgrade (false), _selected (false), _required (false)
{
    parseLine (line);
}

static int hex2nyb (char ch) 
{
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    return 0;
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
            } else if (key == "category" ) {
                _category = value;
            } else if (key == "size") {
                _idinfo = HasFileSize;
                _filesize = value.toInt();
#ifndef DONT_HAVE_LIBCRYPTO
            } else if (key == "md5") {
                if (value.length() == 32) {
                    _idinfo = HasMD5;
                    for (int i = 0; i < 16; i++) {
                        _md5[i] = (hex2nyb (value[2*i].toAscii()) << 4) | hex2nyb (value[2*i+1].toAscii());
                    }
                }
#endif
            } else if (key == "unsupported") {
                _unsupported = true;
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

            _selected = true;
            _orig = true;

            delete[] buf;
            delete packlist;
        }
    }
    delete dirp;
    delete ext2;

}

static void writeLine (VFS::File *out, QString line, int addnl = true)
{
    out->write (line.toAscii().data(), line.length());
    if (addnl) out->write ("\n", 1);
}

void Package::writePackingList() 
{
    iPodLinuxPartitionFS->mkdir ("/etc");
    iPodLinuxPartitionFS->mkdir ("/etc/packages");
    VFS::File *packlist = iPodLinuxPartitionFS->open (QString ("/etc/packages/%1").arg (_name).toAscii(),
                                                      O_WRONLY | O_TRUNC | O_CREAT);
    if (packlist->error()) {
        fprintf (stderr, "Error writing packing list: /etc/packages/%s: %s\n", _name.toAscii().data(),
                 strerror (packlist->error()));
        return;
    }

    writeLine (packlist, QString ("Version %1").arg (_version));

    QStringListIterator it (_packlist);
    while (it.hasNext()) {
        writeLine (packlist, it.next());
    }

    packlist->close();
    delete packlist;
}

bool Package::fileAlreadyDownloaded (QString path) 
{
    QFile file (path);
    if (!file.exists()) return false;

    switch (_idinfo) {
    case NoIdentifyingInfo:
        return false; // we can't tell
    case HasFileSize:
        return (file.size() == _filesize);
    case HasMD5:
        break;
    }

    // Check the MD5 sum.
#ifndef DONT_HAVE_LIBCRYPTO
    if (!file.open (QIODevice::ReadOnly))
        return false;

    MD5_CTX ctx;
    MD5_Init (&ctx);
    char buf[4096];
    while (!file.atEnd()) {
        int rdlen = file.read (buf, 4096);
        if (rdlen <= 0) break;
        MD5_Update (&ctx, buf, rdlen);
    }

    unsigned char md5[16];
    MD5_Final (md5, &ctx);
    file.close();
    return !memcmp (_md5, md5, 16);
#endif

    return false;
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

PackagesPage::PackagesPage (Installer *wiz, bool atm)
    : InstallerPage (wiz), advok (false), errored (false), automatic (atm)
{
    if (automatic)
        blurb = new QLabel (tr ("<p>Please wait while the package list is downloaded and resolved "
                                "so the installer can make some decisions about what to install.</p>"));
    else if (Mode == Update)
        blurb = new QLabel (tr ("<p>Here you may modify your installed iPodLinux packages. Check the box "
                                "next to a package to have it installed; uncheck it to have it removed. "
                                "If an upgrade is available, the Status column will read \"Upgrade\"; "
                                "if you do not want this upgrade for some reason, uncheck the box "
                                "next to that text.</p>"
                                "<p><i>Please wait while the package list is downloaded.</i></p>"));
    else
        blurb = new QLabel (tr ("<p>Here you may select packages to install for iPodLinux. Check the boxes "
                                "next to each package you would like to install.</p>"
                                "<p><i>Please wait while the package list is downloaded.</i></p>"));
        
    blurb->setWordWrap (true);
    progressStmt = new QLabel (tr ("<b>Initializing...</b>"));

    if (!automatic) {
        wizard->resize (640, 520);
        wizard->setMinimumSize (500, 410);
        wizard->setMaximumSize (1280, 1024);
    }

    QStringList headers;
    headers << "Name" << "Version" << "Action" << "Description";
    packages = new QTreeWidget;
    packages->setHeaderLabels (headers);
    loadpkg = new QPushButton (tr ("Load package(s) from disk"));
    connect (packages, SIGNAL(itemChanged(QTreeWidgetItem*, int)),
             this, SLOT(listClicked(QTreeWidgetItem*, int)));
    connect (packages, SIGNAL(itemCollapsed(QTreeWidgetItem*)),
             this, SLOT(itemCollapsed(QTreeWidgetItem*)));
    connect (packages, SIGNAL(itemExpanded(QTreeWidgetItem*)),
             this, SLOT(itemExpanded(QTreeWidgetItem*)));
    connect (loadpkg, SIGNAL(released()), this, SLOT(loadExternalPackageList()));

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget (blurb);
    layout->addSpacing (10);
    layout->addWidget (progressStmt);
    layout->addWidget (packages);
    layout->addWidget (loadpkg);
    packages->hide();
    loadpkg->hide();
    if (automatic)
        layout->addStretch (1);
    setLayout (layout);
    
    if (QFile::exists (InstallerHome + "/packages.ipl")) {
        loadExternalPackageList (InstallerHome + "/packages.ipl", false);
        QTimer::singleShot (0, this, SLOT(done()));
    } else {
        packlistHTTP = new QHttp;
        packlistHTTP->setHost ("ipodlinux.org", 80);
        host = "ipodlinux.org";
        connect (packlistHTTP, SIGNAL(dataSendProgress(int, int)), this, SLOT(httpSendProgress(int, int)));
        connect (packlistHTTP, SIGNAL(dataReadProgress(int, int)), this, SLOT(httpReadProgress(int, int)));
        connect (packlistHTTP, SIGNAL(stateChanged(int)), this, SLOT(httpStateChanged(int)));
        connect (packlistHTTP, SIGNAL(requestFinished(int, bool)), this, SLOT(httpRequestFinished(int, bool)));
        connect (packlistHTTP, SIGNAL(done(bool)), this, SLOT(httpDone(bool)));
        connect (packlistHTTP, SIGNAL(responseHeaderReceived(const QHttpResponseHeader&)),
                 this, SLOT(httpResponseHeaderReceived(const QHttpResponseHeader&)));
        packlistHTTP->get ("/iPodLinux:Installation_sources?action=raw");
    }
}

void PackagesPage::doLoadExtraPackageList() 
{
    QString filename = QFileDialog::getOpenFileName (this, "Choose a package list to load:",
                                                     QString(), "iPodLinux package lists (*.ipl)");
    if (filename == "") return;

    loadExternalPackageList (filename, true);
}

void PackagesPage::loadExternalPackageList (QString filename, bool markBold) 
{
    QFile pkglist (filename);
    if (!pkglist.open (QIODevice::ReadOnly)) {
        QMessageBox::critical (this, tr ("Error opening package list"),
                               tr ("Could not open %1 for reading: %2").arg (filename).arg (pkglist.errorString()),
                               tr ("Cancel"));
        return;
    }

    QStringList filepath = filename.split ("/");
    filepath.removeAll ("");
    filepath.removeLast();
    QDir pkglistdir (filepath.join ("/"));

    QByteArray bline;
    while (!pkglist.atEnd() && !(bline = pkglist.readLine()).isNull()) {
        QString line = bline;
        Package *pkgp = parsePackageListLine (line, markBold);
        if (pkgp) { // munge URLs and such to be relative to the filename
            pkgp->url() = pkglistdir.absoluteFilePath (pkgp->url());
        }
    }

    pkglist.close();
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
        progressStmt->setText (tr ("<b>Connecting to %1...</b>").arg (host));
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

static QStringList requiredProvides;

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
                QRegExp vrx ("\\s*[Vv]ersion\\s+(\\d+)\\s*");
                QRegExp nrx ("\\s*[Ii]nstaller\\s+([0-9A-Za-z.-]+)\\s*");

                if (vrx.exactMatch (line)) {
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
                    parsePackageListLine (line);
                }
            }
            if (requiredProvides.size()) {
                QMessageBox::critical (this, QObject::tr ("Unsupported required provide"),
                                       QObject::tr ("A package providing `%1' is required for an iPodLinux "
                                                    "installation, but none of the ones in the list I have "
                                                    "is supported by the iPod version you have. Sorry, but "
                                                    "installation cannot continue.")
                                       .arg (requiredProvides[0]),
                                       QObject::tr ("Quit"));                
            }
        }
    }
}

Package *PackagesPage::parsePackageListLine (QString line, bool makeBold) 
{
    QRegExp crx ("\\s*\\[category\\]\\s*([a-zA-Z0-9_-]+)\\s*:\\s*\"([^\"]*)\"");
    if (crx.exactMatch (line)) {
        QTreeWidgetItem *catitem = new QTreeWidgetItem (packages, QTreeWidgetItem::UserType + 2);
        catitem->setText (0, crx.cap (2));
        catitem->setFlags (0);
        packages->setItemExpanded (catitem, true);
        categories.insert (crx.cap (1), catitem);
        return 0;
    }

    QRegExp srx ("\\s*(de)?select\\s+([a-zA-Z0-9_-]+)\\s*");
    if (srx.exactMatch (line)) {
        QList <QTreeWidgetItem *> pkglist =
            packages->findItems (srx.cap (2), Qt::MatchRecursive | Qt::MatchExactly);
        if (pkglist.size() == 1) {
            ((PkgTreeWidgetItem *)pkglist[0])->select();
        }
        return 0;
    }

    QRegExp irx ("\\s*<<\\s*(\\S+)\\s*>>\\s*");
    if (irx.exactMatch (line)) {
        if (irx.cap (1).contains ("://")) {
            QUrl url (irx.cap (1));
            if (!url.isValid()) {
                qWarning ("Invalid URL in package list: |%s|", irx.cap (1).toAscii().data());
            } else {
                host = url.host();
                packlistHTTP->setHost (url.host(), (url.port() > 0)? url.port() : 80);
                packlistHTTP->get (url.toString (QUrl::RemoveScheme | QUrl::RemoveAuthority));
            }
        } else {
            loadExternalPackageList (irx.cap (1), makeBold);
        }
        return 0;
    }

    Package *pkgp = new Package (line);
    Package &pkg = *pkgp;
    if (!pkg.supports (iPodVersion) && pkg.required()) {
        if (!pkg.provides().size()) {
            QMessageBox::critical (this, QObject::tr ("Unsupported required package"),
                                   QObject::tr ("The package `%1' is required for an iPodLinux "
                                                "installation, but it is not supported on the "
                                                "iPod version you have. Sorry, but installation "
                                                "cannot continue.")
                                   .arg (pkg.name()),
                                   QObject::tr ("Quit"));
            exit (1);
        }
        requiredProvides << pkg.provides()[0];
    }
    if (pkg.supports (iPodVersion) && pkg.valid()) {
        PkgTreeWidgetItem *twi;
        QTreeWidgetItem *pkgparent = 0;
        if (pkg.category() != "" && categories[pkg.category()]) {
            pkgparent = categories[pkg.category()];
        }
        
        if (pkg.provides().size()) {
            QList <QTreeWidgetItem *> provlist =
                packages->findItems ("Packages providing `" + pkg.provides()[0] + "'",
                                     Qt::MatchExactly | Qt::MatchRecursive);
            if (provlist.size()) {
                twi = new PkgTreeWidgetItem (this, provlist[0], pkg);
                if (provlist[0]->parent() && !pkg.requires().contains (provlist[0]->parent()->text(0))) {
                    // Take the `Packages providing whatever' header item out of
                    // the children of the package required by one of the providers
                    // but not all, and put it into the top-level list.
                    provlist[0]->parent()->takeChild (provlist[0]->parent()->indexOfChild (provlist[0]));
                    packages->addTopLevelItem (provlist[0]);
                }
            } else if (categories[pkg.provides()[0]]) {
                twi = new PkgTreeWidgetItem (this, categories[pkg.provides()[0]], pkg);
            } else {
                QTreeWidgetItem *provitem;
                if (pkgparent)
                    provitem = new QTreeWidgetItem (pkgparent,
                                                    QTreeWidgetItem::UserType);
                else
                    provitem = new QTreeWidgetItem (packages,
                                                    QTreeWidgetItem::UserType);
                
                provitem->setText (0, "Packages providing `" + pkg.provides()[0] + "'");
                provitem->setFlags (0);
                packages->setItemExpanded (provitem, true);
                twi = new PkgTreeWidgetItem (this, provitem, pkg);
            }
            twi->setProv (true);
        } else {
            if (pkgparent)
                twi = new PkgTreeWidgetItem (this, pkgparent, pkg);
            else
                twi = new PkgTreeWidgetItem (this, packages, pkg);
        }
        if (pkg.unsupported())
            twi->setTextColor (0, Qt::darkRed);
        if (makeBold) {
            QFont boldFont = twi->font (0);
            boldFont.setBold (true);
            twi->setFont (0, boldFont);
        }
        
        packageProvides.insert (pkg.name(), twi);
        QStringListIterator pi (pkg.provides());
        while (pi.hasNext()) {
            QString prov = pi.next();
            if (requiredProvides.contains (prov)) {
                requiredProvides.removeAll (prov);
                pkg.select();
                pkg.makeRequired();
            }
            packageProvides.insert (prov, twi);
        }
        
        if (pkg.url().contains ("YYYYMMDD") || pkg.url().contains ("NNN")) {
            QString dir = pkg.url();
            dir.truncate (dir.lastIndexOf ('/'));
            if (dir.contains ("://")) {
                // it's a URL; get a directory listing. It'll be parsed in httpRequestFinished.
                QUrl url (dir);
                host = url.host();
                packlistHTTP->setHost (url.host(), (url.port() > 0)? url.port() : 80);
                resolvers [packlistHTTP->get (url.toString (QUrl::RemoveScheme |
                                                            QUrl::RemoveAuthority))] = twi;
            } else {
                QDir pkgdir (dir);
                QString pat = pkg.url();
                pat.remove (0, pat.lastIndexOf ('/') + 1);
                if (pat.contains ("YYYYMMDD"))
                    pat.replace ("YYYYMMDD", "([0-9][0-9][0-9][0-9]-?[0-9][0-9]-?[0-9][0-9])");
                if (pat.contains ("NNN"))
                    pat.replace ("NNN", "([0-9][0-9][0-9][0-9]?[0-9]?)");
                
                QRegExp rx (pat);
                QStringList files = pkgdir.entryList();
                QStringListIterator it (files);
                QString cap = "";
                while (it.hasNext()) {
                    QString file = it.next();
                    if (rx.exactMatch (file) >= 0)
                        cap = rx.cap(1);
                }
                if (cap != "") {
                    pkg.version().replace ("YYYYMMDD", cap);
                    pkg.version().replace ("NNN", cap);
                    pkg.url().replace ("YYYYMMDD", cap);
                    pkg.url().replace ("NNN", cap);
                    pkg.subfile().replace ("YYYYMMDD", cap);
                    pkg.subfile().replace ("NNN", cap);
                }

                QList <QTreeWidgetItem *> items =
                    packages->findItems (pkg.name(), Qt::MatchRecursive | Qt::MatchExactly);                
                if (items.size()) ((PkgTreeWidgetItem *)items[0])->update();
            }
        }
    } else if (Mode == ChangeLoader && pkg.type() == Package::Loader) {
        // If we're changing loader, and this loader is installed but not
        // the new one, remove it.
        pkg.readPackingList (iPodLinuxPartitionDevice);
        if (pkg.selected())
            PendingActions->append (new PackageRemoveAction (pkg, tr ("Removing ")));
    } else {
        delete pkgp;
        return 0;
    }
    return pkgp;
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
        done();
    }
}

void PackagesPage::done() 
{
    if (!automatic) {
        blurb->setText (tr ("<p>Here you may select packages to install for iPodLinux. Check the boxes "
                            "next to each package you would like to install.</p>"));
        progressStmt->hide();
    }
    
    QList<QTreeWidgetItem *> provlist = packages->findItems ("Packages providing `[0-9A-Za-z._-]+'",
                                                             Qt::MatchRegExp | Qt::MatchRecursive);
    QListIterator<QTreeWidgetItem *> provit (provlist);
    while (provit.hasNext()) {
        QTreeWidgetItem *item = provit.next();
        switch (item->childCount()) {
        case 1:
            if (item->parent())
                    item->parent()->insertChild (item->parent()->indexOfChild (item), item->takeChild (0));
            else
                packages->insertTopLevelItem (packages->indexOfTopLevelItem (item), item->takeChild (0));
            /* FALLTHRU */
        case 0:
            packages->takeTopLevelItem (packages->indexOfTopLevelItem (item));
            delete item;
            break;
        default:
            break;
        }
    }
    
    QList <QTreeWidgetItem *> allItems = packages->findItems (".*", Qt::MatchRegExp | Qt::MatchRecursive);
    QListIterator <QTreeWidgetItem *> it (allItems);
    
    while (it.hasNext()) {
        QTreeWidgetItem *i = it.next();
        PkgTreeWidgetItem *item;
        if ((item = dynamic_cast<PkgTreeWidgetItem *>(i)) != 0) {
            item->package().readPackingList (iPodLinuxPartitionDevice);
            if (item->package().selected()) item->select();
            else if (item->package().required() && (!item->isProv() || !item->parent())) item->select();
            // XXX this is a rather silly special case, but I was too lazy to do a whole
            // "default if certain iPod gens" thing.
            if ((Mode == StandardInstall || Mode == AdvancedInstall) &&
                item->package().name() == "pzmodules" && iPodVersion == 0xB) item->select();
        }
    }
    
    if (!automatic) {
        packages->show();
        loadpkg->show();
    }
    advok = true;
    emit completeStateChanged();
    if (automatic) wizard->clickNextButton();
}

void PackagesPage::httpResponseHeaderReceived (const QHttpResponseHeader& resp) 
{
#if 0
    fprintf (stderr, "<HDR Received>\n");
    fprintf (stderr, "> %d %s\n", resp.statusCode(), resp.reasonPhrase().toAscii().data());
    QListIterator <QPair <QString, QString> > valit (resp.values());
    while (valit.hasNext()) {
        QPair <QString, QString> kv = valit.next();
        fprintf (stderr, "%s: %s\n", kv.first.toAscii().data(), kv.second.toAscii().data());
    }
    fprintf (stderr, "</HDR> ");
#endif

    if (resp.statusCode() >= 300 && resp.statusCode() < 400) { // redirect
        QUrl url (resp.value ("location"));
        host = url.host();
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

    if (_prov && parent()) {
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
    if (_pkg.required() && (!_prov || !parent())) {
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

    if (_prov && parent()) {
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

static int ice = 0;

void PackagesPage::itemCollapsed (QTreeWidgetItem *item) 
{
    if (!ice) {
        ice++;
        if (dynamic_cast <PkgTreeWidgetItem *> (item) != 0) {
            packages->expandItem (item);
        }
        ice--;
    }
}

void PackagesPage::itemExpanded (QTreeWidgetItem *item) 
{
    if (!ice) {
        ice++;
        if (dynamic_cast <PkgTreeWidgetItem *> (item) != 0) {
            packages->collapseItem (item);
        }
        ice--;
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

    // First of all, downloads.
    while (it.hasNext()) {
        PkgTreeWidgetItem *item;
        if ((item = dynamic_cast<PkgTreeWidgetItem *>(it.next())) != 0) {
            if (item->package().changed() && item->package().selected() &&
                item->package().url().startsWith ("http://")) {
                PendingActions->append (new PackageDownloadAction (item->package(), tr ("Downloading ")));
            }
        }
    }

    // Next, do all the removes...
    it = allItems;
    while (it.hasNext()) {
        PkgTreeWidgetItem *item;
        if ((item = dynamic_cast<PkgTreeWidgetItem *>(it.next())) != 0) {
            if (item->package().changed()) {
                if (!item->package().selected()) {
                    // remove
                    PendingActions->append (new PackageRemoveAction (item->package(), tr ("Removing ")));
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
                        if (item->checkState(2) == Qt::Checked) {
                            // remove, install
                            PendingActions->append (new PackageRemoveAction (item->package(),
                                                                             tr ("Upgrading "
                                                                                 "(uninstalling old version) ")));
                            PendingActions->append (new PackageInstallAction (item->package(),
                                                                              tr ("Upgrading "
                                                                                  "(installing new version) ")));
                        }
                    } else {
                        // install
                        PendingActions->append (new PackageInstallAction (item->package(), tr ("Installing ")));
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

    return new DoActionsPage (wizard, new DonePage (wizard));
}

PkgTreeWidgetItem::PkgTreeWidgetItem (PackagesPage *page, QTreeWidget *parent, Package& pkg)
    : QTreeWidgetItem (parent, UserType), _pkg (pkg), _changemarked (false), _page (page), _prov (false)
{
    setFlags (Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    update();
    _setsel();
}

PkgTreeWidgetItem::PkgTreeWidgetItem (PackagesPage *page, QTreeWidgetItem *parent, Package& pkg)
    : QTreeWidgetItem (parent, UserType), _pkg (pkg), _changemarked (false), _page (page), _prov (false)
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
        setCheckState (0, Qt::Checked);
        ice++;
        _page->packages->expandItem (this);
        ice--;
        if (_pkg.upgrade()) {
            setText (2, QObject::tr ("Upgrade"));
            setCheckState (2, Qt::Checked);
        } else if (_pkg.changed()) {
            setText (2, QObject::tr ("Install"));
        } else {
            setText (2, QObject::tr ("Keep"));
        }
    } else {
        setCheckState (0, Qt::Unchecked);
        ice++;
        _page->packages->collapseItem (this);
        ice--;
        if (_pkg.changed()) {
            setText (2, QObject::tr ("Uninstall"));
        } else {
            setText (2, QObject::tr ("None"));
        }
    }
}

void PackageRemoveAction::run() 
{
    printf ("Removing %s.\n", _pkg.name().toAscii().data());

    emit setTaskDescription (_label + _pkg.name() + "-" + _pkg.version());
    emit setTotalProgress (_pkg.getPackingList().size());

    int prog = 0;
    QStringListIterator it (_pkg.getPackingList());
    while (it.hasNext()) {
        QString file = it.next();
        emit setCurrentProgress (prog++);
        emit setCurrentAction (file);
        iPodLinuxPartitionFS->unlink (file.toAscii());
    }
    emit setCurrentProgress (prog);
    emit setCurrentAction ("Removing package metadata...");
    iPodLinuxPartitionFS->unlink ((QString ("/etc/packages/") + _pkg.name()).toAscii());
    emit setCurrentAction ("Done.");
}

void PackageDownloadAction::run() 
{
    printf ("Downloading %s.\n", _pkg.name().toAscii().data());

    hostReq = headReq = getReq = 0;
    contentLength = 0; out = 0;
    complete = false;

    if (!_pkg.url().contains ("://"))
        return;

    QDir::current().mkdir (InstallerHome + "/dl_packages");
    QUrl pkgurl (_pkg.url());

    if (_pkg.fileAlreadyDownloaded (InstallerHome + "/dl_packages/" + pkgurl.path().split ("/").last()))
        return;

    http = new QHttp;

    host = pkgurl.host();
    hostReq = http->setHost (host, (pkgurl.port() > 0)? pkgurl.port() : 80);
    connect (http, SIGNAL(dataSendProgress(int, int)), this, SLOT(httpSendProgress(int, int)));
    connect (http, SIGNAL(dataReadProgress(int, int)), this, SLOT(httpReadProgress(int, int)));
    connect (http, SIGNAL(stateChanged(int)), this, SLOT(httpStateChanged(int)));
    connect (http, SIGNAL(requestFinished(int, bool)), this, SLOT(httpRequestFinished(int, bool)));
    connect (http, SIGNAL(done(bool)), this, SLOT(httpDone(bool)));
    connect (http, SIGNAL(responseHeaderReceived(const QHttpResponseHeader&)),
             this, SLOT(httpResponseHeaderReceived(const QHttpResponseHeader&)));
    headReq = http->head (pkgurl.toString (QUrl::RemoveScheme | QUrl::RemoveAuthority));

    emit setTaskDescription (_label + _pkg.name() + "-" + _pkg.version() + "...");
    emit setTotalProgress (0);
    emit setCurrentProgress (0);
    emit setCurrentAction ("Initializing...");

    QThread::exec();
}

void PackageDownloadAction::httpSendProgress (int done, int total) 
{
    (void)total;
    emit setCurrentProgress (done);
}

void PackageDownloadAction::httpReadProgress (int done, int total) 
{
    emit setCurrentAction (tr ("Transferring data... %1").arg (transferProgressText (done, total)));
    emit setTotalProgress (total);
    emit setCurrentProgress (done);
}

void PackageDownloadAction::httpStateChanged (int state) 
{
    if (state == QHttp::HostLookup)
        emit setCurrentAction (tr ("Looking up host..."));
    else if (state == QHttp::Connecting)
        emit setCurrentAction (tr ("Connecting to %1...").arg (host));
    else if (state == QHttp::Sending)
        emit setCurrentAction (tr ("Sending request..."));
    else if (state == QHttp::Reading)
        emit setCurrentAction (tr ("Transferring data..."));
    else if (state == QHttp::Connected)
        emit setCurrentAction (tr ("Connected."));
    else if (state == QHttp::Closing)
        emit setCurrentAction (tr ("Closing connection..."));
    else if (state == QHttp::Unconnected)
        emit setCurrentAction (tr ("Done."));

}

void PackageDownloadAction::httpRequestFinished (int req, bool err)
{
    if (req == headReq) {
        QUrl pkgurl (_pkg.url());
        _pkg.url() = InstallerHome + "/dl_packages/" + pkgurl.path().split ("/").last();
        
        out = new QFile (_pkg.url());
        if (out->open (QIODevice::ReadOnly)) {
            if (out->size() == contentLength) {
                complete = true;
                return; // already downloaded
            }
            out->close();
        }

        if (!out->open (QIODevice::WriteOnly)) {
            emit fatalError ("Could not open " + _pkg.url() + " for writing. Check permissions.");
            while(1);
        }
        
        getReq = http->get (pkgurl.toString (QUrl::RemoveScheme | QUrl::RemoveAuthority), out);
    }

    if (req == getReq) {
        complete = true;
    }

    if (err) {
        emit fatalError ("Package " + _pkg.name() + " could not be downloaded from " +
                         http->currentRequest().path() + ": " + http->errorString());
        while(1);
    }
}

void PackageDownloadAction::httpDone (bool err) 
{
    if (complete || err) {
        if (out) {
            out->close();
            delete out;
        }
        QThread::quit();
    }
}

void PackageDownloadAction::httpResponseHeaderReceived (const QHttpResponseHeader& resp) 
{
    if (resp.statusCode() >= 300 && resp.statusCode() < 400) { // redirect
        /* Yes, there used to be redirect-handling code here, but it didn't work. */
        emit fatalError ("Redirects not supported");
        while(1);
    } else if (resp.statusCode() != 200) {
        emit fatalError ("Error fetching " + http->currentRequest().path() + " for package " +
                         _pkg.name() + ": " + resp.reasonPhrase());
        while(1);
    } else {
        contentLength = resp.contentLength();
    }
}

class CompressedFile : public VFS::File 
{
public:
    CompressedFile (const char *name) { errno = 0; _zfp = gzopen (name, "rb"); }
    ~CompressedFile() { close(); }
    
    virtual int read (void *buf, int n) { return gzread (_zfp, buf, n); }
    virtual int write (const void *buf, int n) { return gzwrite (_zfp, buf, n); }
    virtual s64 lseek (s64 off, int whence) { return gzseek (_zfp, off, whence); }
    virtual int error() { if (_zfp) return 0; if (!errno) return EINVAL; return errno; }
    virtual int close() { if (_zfp) gzclose (_zfp); _zfp = 0; return 0; }

protected:
    gzFile _zfp;
};

static void *tar_rawpod_open (const char *path, int mode, ...) 
{
    (void)path, (void)mode;
    return 0;
}

static int tar_rawpod_close (void *fh) 
{
    VFS::File *f = (VFS::File *)fh;
    return f->close();
}

static int tar_rawpod_read (void *fh, void *buf, size_t size) 
{
    VFS::File *f = (VFS::File *)fh;
    return f->read (buf, size);
}

static int tar_rawpod_write (void *fh, const void *buf, size_t size) 
{
    VFS::File *f = (VFS::File *)fh;
    return f->write (buf, size);
}

void PackageInstallAction::update_progress (TAR *t) 
{
    VFS::File *f = (VFS::File *)t->fh;
    emit setCurrentProgress (f->lseek (0, SEEK_CUR));
}

void PackageInstallAction::run()
{
    printf ("Installing %s.\n", _pkg.name().toAscii().data());

    emit setTaskDescription (_label + _pkg.name() + "-" + _pkg.version());
    emit setTotalProgress (0);
    emit setCurrentAction (tr ("Initializing..."));

    int uncompressed_length = 0;
    gzFile zfp = gzopen (_pkg.url().toAscii(), "rb");
    if (zfp) {
        if (gzdirect (zfp)) {
            FILE *fp = fopen (_pkg.url().toAscii(), "rb");
            fseek (fp, 0, SEEK_END);
            uncompressed_length = ftell (fp);
            fclose (fp);
        } else {
            // skip everything
            gzseek (zfp, 0x7ffff000, SEEK_CUR);
            // get the offset
            uncompressed_length = gztell (zfp);
            if (uncompressed_length < 0 || uncompressed_length >= 0x10000000) {
                emit fatalError (tr ("Unable to determine size of %1").arg (_pkg.url()));
                while(1);
            }
        }
        gzclose (zfp);
    }

    emit setTotalProgress (uncompressed_length);

    VFS::File *zf = new CompressedFile (_pkg.url().toAscii());
    if (zf->error()) {
        emit fatalError (tr ("Cannot open %1: %2").arg (_pkg.url()).arg (strerror (zf->error())));
        delete zf;
        while(1);
    }
    
    _pkg.clearPackingList();

    if (!(_pkg.type() == Package::File || (_pkg.type() != Package::Archive && !_pkg.url().contains (".tar.")))) {
        // handle archive or file-in-archive
        bool singleFile = _pkg.addFile();
        
        tartype_t rawpod_type = { tar_rawpod_open, tar_rawpod_close, tar_rawpod_read, tar_rawpod_write };
        TAR *tarfile;
        int err;
        
        if (tar_fhopen (&tarfile, (void *)zf, _pkg.url().toAscii(), &rawpod_type, TAR_CHECK_MAGIC) < 0) {
            emit fatalError (tr ("Cannot initialize libtar handle from %1: %2").arg (_pkg.url())
                             .arg (strerror (errno)));
            while(1);
        }
        tarfile->data = this;
        tarfile->progressfunc = update_progress_shim;
        
        while ((err = th_read (tarfile)) == 0) {
            emit setCurrentProgress (zf->lseek (0, SEEK_CUR));
            if (singleFile && th_get_pathname (tarfile) != _pkg.subfile()) {
                if (TH_ISREG (tarfile))
                    tar_skip_regfile (tarfile);
            } else {
                emit setCurrentAction (tr ("Extracting %1...").arg (th_get_pathname (tarfile)));
                if (singleFile)
                    err = tar_extract_file (tarfile, iPodLinuxPartitionFS, _pkg.destination().toAscii());
                else {
                    err = tar_extract_file (tarfile, iPodLinuxPartitionFS,
                                            _pkg.addFile (th_get_pathname (tarfile)).toAscii());
                }
		if (err != 0) {
		    if (err < 0) emit fatalError (strerror (-err));
		    else emit fatalError ("error extracting");
		    while(1);
		}
            }
        }
	if (err < 0) {
	    emit fatalError (strerror (-err));
	}
        tar_close (tarfile);
    } else {
        // handle single file
        _pkg.addFile();
        VFS::File *of = iPodLinuxPartitionFS->open (_pkg.destination().toAscii(), O_WRONLY|O_CREAT|O_TRUNC);
        if (of->error()) {
            emit fatalError (tr ("Unable to open %1 for writing on the iPod: %2").arg (_pkg.destination())
                             .arg (strerror (of->error())));
            while(1);
        }
        emit setCurrentAction (tr ("Extracting..."));
        char buf[4096];
        int rdlen;
        while ((rdlen = zf->read (buf, 4096)) > 0) {
            emit setCurrentProgress (zf->lseek (0, SEEK_CUR));

            int err;
            if ((err = of->write (buf, rdlen)) != rdlen) {
                emit fatalError (tr ("Error writing to %1: %2").arg (_pkg.destination())
                                 .arg (rdlen < 0? strerror (-rdlen) : "short write"));
                while(1);
            }
        }
        if (rdlen < 0) {
            emit fatalError (tr ("Error reading %1: %2").arg (_pkg.url()).arg (strerror (-rdlen)));
            while(1);
        }
        of->chmod (0755);
        of->close();
        delete of;
    }

    zf->close();
    delete zf;

    _pkg.writePackingList();

    emit setCurrentProgress (uncompressed_length);
    emit setCurrentAction ("Done.");
}

PackageEditDialog::PackageEditDialog (Package& pkg)
    : _pkg (pkg)
{
    name = new QLineEdit (_pkg.name());
    version = new QLineEdit (_pkg.version());
    desc = new QLineEdit (_pkg.description());
    url = new QLineEdit (_pkg.url());
    dest = new QLineEdit (_pkg.destination());
    subfile = new QLineEdit (_pkg.subfile());
    category = new QLineEdit (_pkg.category());
    requires = new QLineEdit (_pkg.requires().join (", "));
    provides = new QLineEdit (_pkg.provides().join (", "));

    unsupported = new QCheckBox (tr ("Package is unsupported by the iPodLinux team"));
    unsupported->setChecked (_pkg.unsupported());

    typeFile = new QRadioButton (tr ("Single file"));
    typeArchive = new QRadioButton (tr ("Archive (.tar.gz)"));
    typeKernel = new QRadioButton (tr ("Kernel image"));
    typeLoader = new QRadioButton (tr ("Loader"));
    if (_pkg.type() == Package::Kernel) typeKernel->setChecked (true);
    else if (_pkg.type() == Package::Loader) typeLoader->setChecked (true);
    else if (_pkg.type() == Package::File) typeFile->setChecked (true);
    else typeArchive->setChecked (true);
    
    supp123G = new QCheckBox (tr ("1G, 2G, 3G"));
    supp4G = new QCheckBox (tr ("4G"));
    suppMini1G = new QCheckBox (tr ("Mini 1G"));
    suppMini2G = new QCheckBox (tr ("Mini 2G"));
    suppColor = new QCheckBox (tr ("Color"));
    suppNano = new QCheckBox (tr ("Nano"));
    suppVideo = new QCheckBox (tr ("Video"));
    
    supp123G->setChecked (_pkg.supports (3));
    supp4G->setChecked (_pkg.supports (5));
    suppMini1G->setChecked (_pkg.supports (4));
    suppMini2G->setChecked (_pkg.supports (7));
    suppColor->setChecked (_pkg.supports (6));
    suppNano->setChecked (_pkg.supports (0xC));
    suppVideo->setChecked (_pkg.supports (0xB));
    
    QGroupBox *tboxbox = new QGroupBox (tr ("Package information"));
    QGridLayout *tboxlayout = new QGridLayout (tboxbox);
    tboxlayout->addWidget (new QLabel (tr ("Name (no spaces):")), 0, 0, Qt::AlignRight);
    tboxlayout->addWidget (name, 0, 1);
    tboxlayout->addWidget (new QLabel (tr ("Version:")), 0, 2, Qt::AlignRight);
    tboxlayout->addWidget (version, 0, 3);
    tboxlayout->addWidget (new QLabel (tr ("Description:")), 1, 0, Qt::AlignRight);
    tboxlayout->addWidget (desc, 1, 1, 1, 3);
    tboxlayout->addWidget (new QLabel (tr ("Source path/URL:")), 2, 0, Qt::AlignRight);
    tboxlayout->addWidget (url, 2, 1, 1, 3);
    tboxlayout->addWidget (new QLabel (tr ("Destination path:")), 3, 0, Qt::AlignRight);
    tboxlayout->addWidget (dest, 3, 1, 1, 3);
    tboxlayout->addWidget (new QLabel (tr ("File within archive:")), 4, 0, Qt::AlignRight);
    tboxlayout->addWidget (subfile, 4, 1);
    tboxlayout->addWidget (new QLabel (tr ("Category:")), 4, 2, Qt::AlignRight);
    tboxlayout->addWidget (category, 4, 3);
    tboxlayout->addWidget (new QLabel (tr ("Requires:")), 5, 0, Qt::AlignRight);
    tboxlayout->addWidget (requires, 5, 1);
    tboxlayout->addWidget (new QLabel (tr ("Provides:")), 5, 2, Qt::AlignRight);
    tboxlayout->addWidget (provides, 5, 3);
    tboxbox->setLayout (tboxlayout);
    
    QGroupBox *typebox = new QGroupBox (tr ("Package type"));
    QGridLayout *typelayout = new QGridLayout (typebox);
    typelayout->addWidget (typeFile, 0, 0);
    typelayout->addWidget (typeArchive, 0, 1);
    typelayout->addWidget (typeKernel, 1, 0);
    typelayout->addWidget (typeLoader, 1, 1);
    typebox->setLayout (typelayout);

    QGroupBox *suppbox = new QGroupBox (tr ("Supported iPods"));
    QGridLayout *supplayout = new QGridLayout (suppbox);
    supplayout->addWidget (supp123G, 0, 0);
    supplayout->addWidget (supp4G, 1, 0);
    supplayout->addWidget (suppMini1G, 0, 1);
    supplayout->addWidget (suppMini2G, 0, 2);
    supplayout->addWidget (suppColor, 1, 1);
    supplayout->addWidget (suppNano, 1, 2);
    supplayout->addWidget (suppVideo, 2, 1);
    suppbox->setLayout (supplayout);

    QPushButton *cancelButton = new QPushButton (tr ("Cancel"));
    QPushButton *okButton = new QPushButton (tr ("OK"));
    okButton->setDefault (true);
    connect (cancelButton, SIGNAL(clicked(bool)), this, SLOT(reject()));
    connect (okButton, SIGNAL(clicked(bool)), this, SLOT(okPressed()));
    QHBoxLayout *buttlayout = new QHBoxLayout;
    buttlayout->addStretch (1);
    buttlayout->addWidget (cancelButton);
    buttlayout->addWidget (okButton);

    QVBoxLayout *toplayout = new QVBoxLayout (this);
    toplayout->addWidget (tboxbox);
    toplayout->addWidget (typebox);
    toplayout->addWidget (suppbox);
    toplayout->addStretch (1);
    toplayout->addLayout (buttlayout);
    this->setLayout (toplayout);
}

void PackageEditDialog::okPressed() 
{
    _pkg.name() = name->text();
    _pkg.version() = version->text();
    _pkg.description() = desc->text();
    _pkg.subfile() = subfile->text();
    _pkg.category() = category->text();
    _pkg.destination() = dest->text();
    _pkg.requires() = requires->text().replace (", ", ",").split (",");
    _pkg.provides() = provides->text().replace (", ", ",").split (",");
    _pkg.unsupported() = unsupported->isChecked();

    if (typeFile->isChecked()) _pkg.type() = Package::File;
    else if (typeArchive->isChecked()) _pkg.type() = Package::Archive;
    else if (typeKernel->isChecked()) _pkg.type() = Package::Kernel;
    else if (typeLoader->isChecked()) _pkg.type() = Package::Loader;

    _pkg.supports() &= ~0x18fe;
    if (supp123G->isChecked()) _pkg.supports() |= (1 << 1) | (1 << 2) | (1 << 3);
    if (supp4G->isChecked()) _pkg.supports() |= (1 << 5);
    if (suppMini1G->isChecked()) _pkg.supports() |= (1 << 4);
    if (suppMini2G->isChecked()) _pkg.supports() |= (1 << 7);
    if (suppColor->isChecked()) _pkg.supports() |= (1 << 6);
    if (suppNano->isChecked()) _pkg.supports() |= (1 << 12);
    if (suppVideo->isChecked()) _pkg.supports() |= (1 << 11);

    accept();
}
