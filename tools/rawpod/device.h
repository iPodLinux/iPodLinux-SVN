/* device.h -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * The rawpod tools, of which this file is a part, are licensed
 * under the GNU General Public License. See the file COPYING in the
 * source distribution for details.
 */

#ifndef _DEVICE_H_
#define _DEVICE_H_

#include "vfs.h"
#include <dirent.h>

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

class LocalDir : public VFS::Dir
{
public:
    LocalDir (const char *path)
        : _dp (opendir (path))
    {}
    ~LocalDir() { if (_dp) closedir (_dp); }
    
    int readdir (struct VFS::dirent *de);
    int close() { if (_dp) closedir (_dp); _dp = 0; return 0; }
#ifdef WIN32
    int error() { if (!_dp) return 1; return 0; }
#else
    int error() { if (!_dp) return errno; return 0; }
#endif
    
private:
    DIR *_dp;
};

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
#ifndef WIN32
    int fileno() { return _fd; }
#endif

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

