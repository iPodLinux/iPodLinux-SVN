/* device.h -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * The rawpod tools, of which this file is a part, are licensed
 * under the GNU General Public License. See the file COPYING in the
 * source distribution for details.
 */

#ifndef _DEVICE_H_
#define _DEVICE_H_

#include "vfs.h"

#define OPEN_READ    1
#define OPEN_WRITE   2
#define OPEN_CREATE  4
#define OPEN_DEV     8

#ifdef WIN32
#include <windows.h>
#ifndef SEEK_SET
#define SEEK_SET  FILE_BEGIN
#define SEEK_CUR  FILE_CURRENT
#define SEEK_END  FILE_END
#endif
#else
#include <unistd.h>
#endif

class LocalFile : public VFS::File 
{
public:
    LocalFile (const char *path, int flags);
    ~LocalFile();
    
    int read (void *buf, int len);
    int write (const void *buf, int len);
    s64 lseek (s64 off, int whence);
    int error();
    int close();

private:
#ifdef WIN32
    HANDLE _fh;
#else
    int _fd;
#endif
};

class LocalRawDevice : public VFS::BlockDevice
{
public:
    LocalRawDevice (int n);

    int error() {
        if (_valid < 0) return _valid;
        return 0;
    }

protected:
    virtual int doRead (void *buf, u64 sec);
    virtual int doWrite (const void *buf, u64 sec);

private:
    LocalFile *_f;
    int _valid;
        
};

class PartitionDevice : public VFS::BlockDevice
{
public:
    PartitionDevice (VFS::Device *dev, u64 startsec, u64 nsecs);

protected:
    virtual int doRead (void *buf, u64 sec);
    virtual int doWrite (const void *buf, u64 sec);
    
private:
    VFS::Device *_dev;
    u64 _start;
    u64 _length;
};

#endif

