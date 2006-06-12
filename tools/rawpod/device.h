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

class BlockCache 
{
protected:
    BlockCache (int nsec = 2048, int ssize = 512);
    virtual ~BlockCache();
    
    virtual int doRead (void *buf, u64 sec) { return doRawRead (buf, sec); }
    virtual int doWrite (const void *buf, u64 sec) { return doRawWrite (buf, sec); }
    virtual int doRawRead (void *buf, u64 sec) = 0;
    virtual int doRawWrite (const void *buf, u64 sec) = 0;

    int flush() { return 0; }
    int invalidate() { return 0; }
    
    static void cleanup();
    
private:
    int _nsec;
    int _ssize, _log_ssize;
    char *_cache;
    u64 *_sectors;
    u64 *_atimes;
    BlockCache *_cache_next;
    static BlockCache *_cache_head;
};

class LocalRawDevice : public VFS::BlockDevice, public BlockCache
{
public:
    LocalRawDevice (int n);
    ~LocalRawDevice() { if (_wf != _f) delete _wf; delete _f; }

    int error() {
        if (_valid < 0) return _valid;
        return 0;
    }

    static void setOverride (const char *filename) {
        _override = filename;
    }
    static bool overridden() { return (_override != 0); }
    static void setCOWFile (const char *filename) {
        _cowfile = filename;
    }

protected:
    virtual int doRead (void *buf, u64 sec) { return BlockCache::doRead (buf, sec); }
    virtual int doWrite (const void *buf, u64 sec) { return BlockCache::doWrite (buf, sec); }
    virtual int doRawRead (void *buf, u64 sec);
    virtual int doRawWrite (const void *buf, u64 sec);

private:
    LocalFile *_f, *_wf;
    int _valid;
    static const char *_override;
    static const char *_cowfile;
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

