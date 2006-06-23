/* device.cc -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * The rawpod tools, of which this file is a part, are licensed
 * under the GNU General Public License. See the file COPYING in the
 * source distribution for details.
 */

// If you uncomment this, reads will get about three times
// faster but writes will get about 50 times slower.
/* #define RAW_DEVICE_ACCESS */

#ifdef __darwin__
#include <sys/disk.h>
#endif

#ifdef WIN32
#include <windows.h>
#include <winioctl.h>
#else
#include <fcntl.h> /* get the *nix O_* constants, instead of ours */
#include <sys/ioctl.h>
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

int LocalFile::ioctl (unsigned long code, void *dataPtr, size_t dataSize) {
    DWORD junk;
    if (!DeviceIoControl (_fh, code, NULL, 0, dataPtr, dataSize, &junk, NULL))
        return -GetLastError();
    return 0;
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
int LocalFile::ioctl (unsigned long code, void *dataPtr, size_t) {
    return ::ioctl (_fd, code, dataPtr);
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

#include <sys/time.h>

BlockCache *BlockCache::_cache_head = 0;
bool BlockCache::_disabled = false;
int BlockCache::_comint = 5;

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
    _mtimes = new u64[_nsec];
    memset (_sectors, 0, _nsec * sizeof(u64));
    memset (_atimes, 0, _nsec * sizeof(u64));
    memset (_mtimes, 0, _nsec * sizeof(u64));

    if (!_cache_head) _cache_head = this;
    else {
        BlockCache *cur = _cache_head;
        while (cur->_cache_next) cur = cur->_cache_next;
        cur->_cache_next = this;
    }
    _cache_next = 0;
}

BlockCache::~BlockCache() 
{
    BlockCache *cur = _cache_head;
    if (cur == this) _cache_head = this->_cache_next;
    else {
        while (cur && cur->_cache_next != this) cur = cur->_cache_next;
        if (cur) cur->_cache_next = this->_cache_next;
    }

    invalidate();

    delete[] _sectors;
    delete[] _atimes;
    delete[] _mtimes;
    free (_cache);
}

void BlockCache::invalidate() 
{
    memset (_atimes, 0, _nsec * sizeof(u64));
    memset (_mtimes, 0, _nsec * sizeof(u64));
}

s64 BlockCache::getTimeval() 
{
    struct timeval tv;
    struct { int a; int b; } tz;
    gettimeofday (&tv, (struct timezone *)&tz);
    return ((s64)tv.tv_sec << 20) + tv.tv_usec;
}

int BlockCache::dirtySectors()
{
    int ret = 0;
    for (int i = 0; i < _nsec; i++) {
        if (_mtimes[i]) ret++;
    }
    return ret;
}

int BlockCache::isDirty (int idx)
{
    return !!_mtimes[idx];
}

int BlockCache::flushIndex (int idx)
{
    int ret;
    if (_mtimes[idx]) {
        ret = doRawWrite (_cache + (idx << _log_ssize), _sectors[idx]);
        if (ret < 0) return ret;
        _mtimes[idx] = 0;
    }
    return 0;
}

int BlockCache::flushOlderThan (s64 us)
{
    if (_disabled) return 0;

    int ret;
    for (int i = 0; i < _nsec; i++) {
        if (_mtimes[i] && ((us < 0) || (_mtimes[i] < us))) {
            ret = doRawWrite (_cache + (i << _log_ssize), _sectors[i]);
            if (ret < 0) return ret;
            _mtimes[i] = 0;
        }
    }
    return 0;
}

int BlockCache::doRead (void *buf, u64 sec) 
{
    if (_disabled) return doRawRead (buf, sec);

    int lruIdx = 0;
    u64 lruTime = _atimes[0];
    int ret;
    for (int i = 0; i < _nsec; i++) {
        if ((_sectors[i] == sec) && (_atimes[i] != 0)) {
            memcpy (buf, _cache + (i << _log_ssize), _ssize);
            _atimes[i] = getTimeval();
            return 0;
        }
        if (lruTime && (_atimes[i] < lruTime)) {
            lruIdx = i;
            lruTime = _atimes[i];
        }
    }
    if (_mtimes[lruIdx] != 0) {
        ret = doRawWrite (_cache + (lruIdx << _log_ssize), _sectors[lruIdx]);
        if (ret < 0) return ret;
        _mtimes[lruIdx] = 0;
    }

    ret = doRawRead (buf, sec);
    if (ret < 0) return ret;

    _sectors[lruIdx] = sec;
    _atimes[lruIdx] = getTimeval();
    memcpy (_cache + (lruIdx << _log_ssize), buf, _ssize);
    return 0;
}

int BlockCache::doWrite (const void *buf, u64 sec) 
{
    if (_disabled) return doRawWrite (buf, sec);

    int lruIdx = 0;
    u64 lruTime = _atimes[0];
    int ret;

    ret = flushOlderThan (getTimeval() - (_comint << 20));
    if (ret < 0) return ret;

    for (int i = 0; i < _nsec; i++) {
        if ((_sectors[i] == sec) && _atimes[i]) {
            if (memcmp (_cache + (i << _log_ssize), buf, _ssize) != 0) {
                memcpy (_cache + (i << _log_ssize), buf, _ssize);
                _mtimes[i] = _atimes[i] = getTimeval();
                return 0;
            } else {
                _atimes[i] = getTimeval();
                return 0;
            }
        }
        if (lruTime && (_atimes[i] < lruTime)) {
            lruIdx = i;
            lruTime = _atimes[i];
        }
    }
    if (_mtimes[lruIdx] != 0) {
        ret = doRawWrite (_cache + (lruIdx << _log_ssize), _sectors[lruIdx]);
        if (ret < 0) return ret;
        _mtimes[lruIdx] = 0;
    }

    _sectors[lruIdx] = sec;
    _atimes[lruIdx] = _mtimes[lruIdx] = getTimeval();
    memcpy (_cache + (lruIdx << _log_ssize), buf, _ssize);
    return 0;
}

void BlockCache::cleanup() 
{
    while (_cache_head) {
        _cache_head->flush();
        delete _cache_head;
    }
}

struct CacheCleaner { ~CacheCleaner() { BlockCache::cleanup(); } } __cleaner;

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
int LocalRawDevice::_cachesize = 16384;

LocalRawDevice::LocalRawDevice (int n, bool writable)
    : BlockDevice (), BlockCache (_cachesize)
{
#ifdef WIN32
    char drive[] = "\\\\.\\PhysicalDriveN";
    drive[17] = n + '0';
#elif defined (Q_OS_DARWIN)
    char drive[] = "/dev/rdiskX";
    drive[10] = n + '0';
#else
    char drive[] = "/dev/sdX";
    drive[7] = n + 'a';
#endif
    setBlocksize (512);
    if (_override)
        _f = new LocalFile (_override, OPEN_READ | (writable?OPEN_WRITE:0) );
    else
        _f = new LocalFile (drive, OPEN_READ | OPEN_DEV | (writable?OPEN_WRITE:0) );
    if (_f->error()) _valid = -_f->error();
    else {
        _valid = 1;

#ifdef __darwin__
        _f->ioctl (DKIOCGETBLOCKCOUNT, &sectors, sizeof(sectors));
        setSize (sectors);
#else
        // this does not work on OSX (neither with /dev/diskX nor with /dev/rdiskX):
        s64 offset = _f->lseek (0, SEEK_END);
        _f->lseek (0, SEEK_SET);
        setSize ((offset <= 0)? 0 : (offset >> _blocksize_bits));
#endif
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

LocalRawDevice::~LocalRawDevice() 
{
    BlockCache::flush();
    if (_wf != _f) delete _wf;
    delete _f;
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
    
    if (sec >= _length)
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
    
    if (sec >= _length)
        return 0;
    if ((err = _dev->lseek ((_start + sec) << 9, SEEK_SET)) < 0)
        return err;
    if ((err = _dev->write (buf, 512)) < 0)
        return err;
    return 0;
}

