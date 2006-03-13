/* packages.h -*- C++ -*- Copyright (c) 2006 Joshua Oreman. All rights reserved.
 * The iPodLinux installer, of which this file is a part, is licensed under the
 * GNU General Public License, which may be found in the file COPYING in the
 * source distribution.
 */

#ifndef _PACKAGES_H_
#define _PACKAGES_H_
#include "installer.h"
#include <QTreeWidget>

class Package 
{
public:
    enum Type { Kernel, Loader, File, Archive };

    Package();
    Package (QString line);

    void parseLine (QString line);
    void readPackingList (VFS::Device *dev);
    
    QString& name() { return _name; }
    QString& version() { return _version; }
    QString& destination() { return _dest; }
    QString& description() { return _desc; }
    QString& url() { return _url; }
    QString& subfile() { return _subfile; }
    QStringList& requires() { return _reqs; }
    QStringList& provides() { return _provs; }
    Type& type() { return _type; }
    bool supports (int hw_ver) { return !!(_ipods & (1 << hw_ver)); }
    bool valid() { return _valid; }

    // Selected == should it be installed, once everything is done?
    bool selected() { return _selected; }
    // Changed == do I need to do anything with this package?
    bool changed() { return (_selected != _orig) || _upgrade; }
    // Upgrade == should I install this, even though (an old version of) it is already installed?
    bool upgrade() { return _upgrade; }

    void select() { _selected = true; }
    void makeDefault() { _orig = _selected; }
    void deselect() { _selected = false; }

    void debug();

protected:
    QString _name, _version, _dest, _desc, _url, _subfile;
    Type _type;
    QStringList _reqs, _provs;
    quint16 _ipods;
    bool _valid, _orig, _upgrade, _selected;
    QStringList _packlist;
};

class PkgTreeWidgetItem : public QTreeWidgetItem 
{
public:
    PkgTreeWidgetItem (QTreeWidget *widget, Package pkg);
    PkgTreeWidgetItem (QTreeWidgetItem *parent, Package pkg);
    void select() { _pkg.select(); _setsel(); }
    void deselect() { _pkg.deselect(); _setsel(); }
    void makeDefault() { _pkg.makeDefault(); }
    void update();
    Package& package() { return _pkg; }

private:
    void _setsel();
    Package _pkg;
    bool _changemarked;
};

#endif

