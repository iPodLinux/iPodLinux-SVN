/* device.cc -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * The rawpod tools, of which this file is a part, are licensed
 * under the GNU General Public License. See the file COPYING in the
 * source distribution for details.
 */

// If you uncomment this, reads will get about three times
// faster but writes will get about 50 times slower.
/* #define RAW_DEVICE_ACCESS */

#ifndef WIN32
#include <fcntl.h> /* get the *nix O_* constants, instead of
                    ours */
#define DONT_REDEFINE_OPEN_CONSTANTS
#endif
#include "device.h"

#ifdef WIN32

/**************************************************************/

LocalFile::LocalFile (const char *path, int flags) {
    int access = 0;
    if (flags & OPEN_READ) access |= GENERIC_READ;
    if (flags & OPEN_WRITE) access |= GENERIC_WRITE;
    
    TCHAR *tpath = (TCHAR *)malloc (strlen (path) * sizeof (TCHAR) + 2);
    for (int i = 0; i < strlen (path); i++) tpath[i] = path[i];
    tpath[strlen(path)] = 0;

    if (flags & OPEN_CREATE) DeleteFile (tpath);
    _fh = CreateFile (tpath, access, FILE_SHARE_READ | FILE_SHARE_WRITE,
                      NULL, (flags & OPEN_CREATE)? CREATE_ALWAYS : OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL, NULL);

    free (tpath);
}

LocalFile::~LocalFile() 
{
    close();
}

int LocalFile::read (void *buf, int len) {
    DWORD lr;
    if (!ReadFile (_fh, buf, len, &lr, NULL))
        return -GetLastError();
    return lr;
}

int LocalFile::write (const void *buf, int len) {
    DWORD lw;
    if (!WriteFile (_fh, buf, len, &lw, NULL))
        return -GetLastError();
    return lw;
}

s64 LocalFile::lseek (s64 off, int whence) {
    LARGE_INTEGER newptr;
    LARGE_INTEGER li_off;
    li_off.QuadPart = off;
    if (!SetFilePointerEx (_fh, li_off, &newptr, whence))
        return -GetLastError();
    return newptr.QuadPart;
}

int LocalFile::error() {
    if (GetLastError() == NO_ERROR)
        return 0;

    return GetLastError();
}

int LocalFile::close() {
    CloseHandle (_fh);
    return 0;
}

#else

#ifdef RAW_DEVICE_ACCESS
#include <malloc.h>
#include <string.h> /* memcpy */
#endif

/**************************************************************/

LocalFile::LocalFile (const char *file, int flags) {
    int mode = 0;
    if (flags & OPEN_READ) {
        if (flags & OPEN_WRITE) mode |= O_RDWR;
        else mode |= O_RDONLY;
    } else if (flags & OPEN_WRITE) mode |= O_WRONLY;
    
    if (flags & OPEN_CREATE) mode |= O_CREAT | O_TRUNC;
#ifdef RAW_DEVICE_ACCESS
    if (flags & OPEN_DEV)    mode |= O_DIRECT;
#endif
    
    _fd = open (file, mode, 0666);
}
LocalFile::~LocalFile() {
    ::close (_fd);
}

int LocalFile::read (void *buf, int len) {
    return ::read (_fd, buf, len);
}
int LocalFile::write (const void *buf, int len) {
    return ::write (_fd, buf, len);
}
s64 LocalFile::lseek (s64 off, int whence) {
    return ::lseek (_fd, off, whence);
}
int LocalFile::error() {
    if (_fd < 0) return errno;
    return 0;
}
int LocalFile::close() 
{
    return ::close (_fd);
}

#endif

/**************************************************************/

std::list <BlockCache *> BlockCache::_caches;
int BlockCache::_cleaning_up = 0;

BlockCache::BlockCache (int nsec, int ssize)
    : _nsec (nsec), _ssize (ssize), _log_ssize (0)
{
    int t = 1;
    while (t < _ssize) {
        t <<= 1;
        _log_ssize++;
    }
    _ssize = (1 << _log_ssize);
    _cache = (char *)malloc (_nsec * _ssize);
    _sectors = new u64[_nsec];
    _atimes = new u64[_nsec];
    memset (_sectors, 0, _nsec * sizeof(u64));
    memset (_atimes, 0, _nsec * sizeof(u64));

    _caches.push_back (this);
}

BlockCache::~BlockCache() 
{
    if (!_cleaning_up) _caches.remove (this);
    
    flush();
    invalidate();

    delete[] _sectors;
    delete[] _atimes;
    free (_cache);
}

int BlockCache::cleanup() 
{
    _cleaning_up = 1;

    std::list<BlockCache*>::iterator it = _caches.begin();
    while (it != _caches.end()) {
        delete *it;
        ++it;
    }
}

/**************************************************************/

int LocalDir::readdir (struct VFS::dirent *de) 
{
    if (!_dp) return 0;

    struct dirent *d = ::readdir (_dp);
    if (!d) return 0;
    
    de->d_ino = 0;
    strcpy (de->d_name, d->d_name);
    return sizeof(*de);
}

/**************************************************************/

const char *LocalRawDevice::_override = 0;
const char *LocalRawDevice::_cowfile = 0;

LocalRawDevice::LocalRawDevice (int n)
    : BlockDevice(), BlockCache(16384)
{
#ifdef WIN32
    char drive[] = "\\\\.\\PhysicalDriveN";
    drive[17] = n + '0';
#else
    char drive[] = "/dev/sdX";
    drive[7] = n + 'a';
#endif
    setBlocksize (512);
    if (_override)
        _f = new LocalFile (_override, OPEN_READ | OPEN_WRITE);
    else
        _f = new LocalFile (drive, OPEN_READ | OPEN_WRITE | OPEN_DEV);
    if (_f->error()) _valid = -_f->error();
    else {
        _valid = 1;

        u64 offset = _f->lseek (0, SEEK_END);
        _f->lseek (0, SEEK_SET);
        setSize ((offset <= 0)? 0 : (offset >> _blocksize_bits));
    }

    if (_cowfile) {
        _wf = new LocalFile (_cowfile, OPEN_READ | OPEN_WRITE);
        if (_wf->error()) {
            fprintf (stderr, "Warning: error opening %s, COW disabled\n", _cowfile);
            delete _wf;
            _wf = _f;
        }
    } else _wf = _f;
}

int LocalRawDevice::doRawRead (void *buf, u64 sec) 
{
#ifdef RAW_DEVICE_ACCESS
    static void *alignedbuf = 0;
    if (!alignedbuf)
        alignedbuf = memalign (512, 512);
#endif

    s64 err;
    if (_valid <= 0) return _valid;
    // Check the COW file first
    if (_wf != _f) {
        if (_wf->lseek (sec << 9, SEEK_SET) >= 0) {
            if ((err = _wf->read (buf, 512)) > 0) {
                for (int i = 0; i < err; i++) {
                    if (((char *)buf)[i] != 0)
                        return err;
                }
            }
        }
    }
    // Else read from the real dev
    if ((err = _f->lseek (sec << 9, SEEK_SET)) < 0) return err;
#ifdef RAW_DEVICE_ACCESS
    if ((err = _f->read (alignedbuf, 512) < 0)) return err;

    memcpy (buf, alignedbuf, 512);
#else
    if ((err = _f->read (buf, 512) < 0)) return err;
#endif
    return 0;
}

int LocalRawDevice::doRawWrite (const void *buf, u64 sec) 
{
#ifdef RAW_DEVICE_ACCESS
    static void *alignedbuf = 0;
    if (!alignedbuf)
        alignedbuf = memalign (512, 512);

    memcpy (alignedbuf, buf, 512);
#endif

    s64 err;
    if (_valid <= 0) return _valid;
    if ((err = _wf->lseek (sec << 9, SEEK_SET)) < 0) return err;
#ifdef RAW_DEVICE_ACCESS
    if ((err = _wf->write (alignedbuf, 512) < 0)) return err;
#else
    if ((err = _wf->write (buf, 512) < 0)) return err;
#endif
    return 0;
}

/**************************************************************/

PartitionDevice::PartitionDevice (VFS::Device *dev, u64 startsec, u64 nsecs)
    : _dev (dev), _start (startsec), _length (nsecs)
{
    setBlocksize (512);
    setSize (_length);
}

int PartitionDevice::doRead (void *buf, u64 sec) 
{
    s64 err;
    
    if (sec > _length)
        return 0;
    if ((err = _dev->lseek ((_start + sec) << 9, SEEK_SET)) < 0)
        return err;
    if ((err = _dev->read (buf, 512)) < 0)
        return err;
    return 0;
}

int PartitionDevice::doWrite (const void *buf, u64 sec) 
{
    s64 err;
    
    if (sec > _length)
        return 0;
    if ((err = _dev->lseek ((_start + sec) << 9, SEEK_SET)) < 0)
        return err;
    if ((err = _dev->write (buf, 512)) < 0)
        return err;
    return 0;
}

