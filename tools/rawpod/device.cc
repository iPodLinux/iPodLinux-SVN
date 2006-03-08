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

#include <windows.h>

/**************************************************************/

LocalFile::LocalFile (const char *path, int flags) {
    int access = 0;
    if (flags & OPEN_READ) access |= GENERIC_READ;
    if (flags & OPEN_WRITE) access |= GENERIC_WRITE;
    
    _fh = CreateFile ((const TCHAR *)path, access, FILE_SHARE_READ | FILE_SHARE_WRITE,
                      NULL, (flags & OPEN_CREATE)? CREATE_ALWAYS : OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL, NULL);
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

LocalRawDevice::LocalRawDevice (int n)
    : BlockDevice()
{
#ifdef WIN32
    char drive[] = "\\\\.\\PhysicalDriveN";
    drive[17] = n + '0';
#else
    char drive[] = "/dev/sdX";
    drive[7] = n + 'a';
#endif
    setBlocksize (512);
    _f = new LocalFile (drive, OPEN_READ | OPEN_WRITE | OPEN_DEV);
    if (_f->error()) _valid = -_f->error();
    else {
        _valid = 1;

        u64 offset = _f->lseek (0, SEEK_END);
        _f->lseek (0, SEEK_SET);
        setSize ((offset <= 0)? 0 : (offset >> _blocksize_bits));
    }
}

int LocalRawDevice::doRead (void *buf, u64 sec) 
{
#ifdef RAW_DEVICE_ACCESS
    static void *alignedbuf = 0;
    if (!alignedbuf)
        alignedbuf = memalign (512, 512);
#endif

    s64 err;
    if (_valid <= 0) return _valid;
    if ((err = _f->lseek (sec << 9, SEEK_SET)) < 0) return err;
#ifdef RAW_DEVICE_ACCESS
    if ((err = _f->read (alignedbuf, 512) < 0)) return err;

    memcpy (buf, alignedbuf, 512);
#else
    if ((err = _f->read (buf, 512) < 0)) return err;
#endif
    return 0;
}

int LocalRawDevice::doWrite (const void *buf, u64 sec) 
{
#ifdef RAW_DEVICE_ACCESS
    static void *alignedbuf = 0;
    if (!alignedbuf)
        alignedbuf = memalign (512, 512);

    memcpy (alignedbuf, buf, 512);
#endif

    s64 err;
    if (_valid <= 0) return _valid;
    if ((err = _f->lseek (sec << 9, SEEK_SET)) < 0) return err;
#ifdef RAW_DEVICE_ACCESS
    if ((err = _f->write (alignedbuf, 512) < 0)) return err;
#else
    if ((err = _f->write (buf, 512) < 0)) return err;
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

