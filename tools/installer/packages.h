/* packages.h -*- C++ -*- Copyright (c) 2006 Joshua Oreman. All rights reserved.
 * The iPodLinux installer, of which this file is a part, is licensed under the
 * GNU General Public License, which may be found in the file COPYING in the
 * source distribution.
 */

#ifndef _PACKAGES_H_
#define _PACKAGES_H_
#include "installer.h"
#include <QTreeWidget>
#include <QTextEdit>
#include <QDialog>

class Package 
{
public:
    enum Type { Kernel, Loader, File, Archive };

    Package();
    Package (QString line);

    void parseLine (QString line);
    void readPackingList (VFS::Device *dev);
    void writePackingList(); // automatically writes it to iPodLinuxPartitionFS
    bool fileAlreadyDownloaded (QString path);
    
    QString& name() { return _name; }
    QString& version() { return _version; }
    QString& destination() { return _dest; }
    QString& description() { return _desc; }
    QString& url() { return _url; }
    QString& subfile() { return _subfile; }
    QString& category() { return _category; }
    QStringList& requires() { return _reqs; }
    QStringList& provides() { return _provs; }
    Type& type() { return _type; }
    bool& unsupported() { return _unsupported; }
    quint16& supports() { return _ipods; }
    bool supports (int hw_ver) { return !!(_ipods & (1 << hw_ver)); }
    bool valid() { return _valid; }

    void makeRequired() { _required = true; }
    bool required() { return _required; }

    // Selected == should it be installed, once everything is done?
    bool selected() { return _selected; }
    // Changed == do I need to do anything with this package?
    bool changed() { return (_selected != _orig) || _upgrade; }
    // Upgrade == should I install this, even though (an old version of) it is already installed?
    bool upgrade() { return _upgrade; }

    void select() { _selected = _reallyselected = true; }
    void depend() { _selected = true; }
    void makeDefault() { _orig = _selected; }
    void deselect() { _selected = _reallyselected = false; }
    void exclude() { _selected = false; }
    bool constrained() { return (_reallyselected != _selected); }
    bool unconstrain() { if (_reallyselected != _selected) { _selected = _reallyselected; return true; } return false; }

    void debug();

    QStringList getPackingList() { return _packlist; }
    void clearPackingList() { _packlist.clear(); }

    // Adds [file] to the packing list. [file] should be a path
    // relative to the package's install destination. Returns
    // the full path added.
    QString addFile (QString file) { _packlist << (_dest + "/" + file); return _dest + "/" + file; }

    // Adds the package's one file to the packing list.
    // Returns true iff something was added.
    // Adds nothing in the case of an archive.
    bool addFile() {
        if (_type == File || _subfile != "." || (_type != Archive && !_url.contains (".tar."))) {
            _packlist << _dest;
            return true;
        }
        return false;
    }

    Package& operator= (Package& other);

protected:
    QString _name, _version, _dest, _desc, _url, _subfile, _category;
    enum { NoIdentifyingInfo, HasFileSize, HasMD5 } _idinfo;
    quint32 _filesize;
    quint8 _md5[16];
    Type _type;
    bool _unsupported;
    QStringList _reqs, _provs;
    quint16 _ipods;
    bool _valid, _orig, _upgrade, _selected, _reallyselected, _required;
    QStringList _packlist;
};

const int PackageItemType = QTreeWidgetItem::UserType + 0;
const int ProvidesHeaderType = QTreeWidgetItem::UserType + 1;
const int CategoryHeaderType = QTreeWidgetItem::UserType + 2;

class PkgTreeWidgetItem : public QTreeWidgetItem 
{
public:
    enum ChangeMode { ChangeIfPossible = 1, ChangeForce, ChangeDependency, ChangeAlways = 0x80 };

    PkgTreeWidgetItem (PackagesPage *page, QTreeWidget *widget, Package& pkg);
    PkgTreeWidgetItem (PackagesPage *page, QTreeWidgetItem *parent, Package& pkg);
    // If `force', then it'll force dependencies checked and competing provides unchecked
    // in order to get checked. In that case, it always returns true. Otherwise, it
    // returns true only if it was actually able to be checked based on deps and provides.
    bool select (ChangeMode mode);
    bool deselect (ChangeMode mode);
    void makeDefault() { _pkg.makeDefault(); }
    void update (ChangeMode mode = ChangeForce);
    Package& package() { return _pkg; }
    bool isProv() { return _prov; }
    void setProv (bool p) { _prov = p; }

private:
    void _chkconst(); // check constraints
    void _setsel();
    Package& _pkg;
    bool _changemarked;
    PackagesPage *_page;
    bool _prov;
};

class PackageEditDialog : public QDialog 
{
    Q_OBJECT

public:
    PackageEditDialog (QWidget *parent, Package& pkg, PkgTreeWidgetItem *item = 0);

public slots:
    void okPressed();

protected:
    QLineEdit *name, *version, *desc, *url, *dest, *subfile, *category, *requires, *provides;
    QCheckBox *unsupported;
    QRadioButton *typeFile, *typeArchive, *typeKernel, *typeLoader;
    QCheckBox *supp123G, *supp4G, *suppMini1G, *suppMini2G, *suppColor, *suppNano, *suppVideo;
    Package& _pkg;
    PkgTreeWidgetItem *_item;
};

#endif

