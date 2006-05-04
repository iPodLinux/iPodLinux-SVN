/* kernel/vfs.cc -*- C++ -*- Copyright (c) 2005 Joshua Oreman
 * XenOS, of which this file is a part, is licensed under the
 * GNU General Public License. See the file COPYING in the
 * source distribution for details.
 */

#include "vfs.h"
#include "device.h"
#include <string.h>

#ifndef MIN
#define MIN(x,y) (((x)<(y))?(x):(y))
#endif

namespace VFS
{
#define CHECKERR if (err) { if (tmp) delete[] tmp; return err; }

    int BlockDevice::read (void *d, int n) 
    {
        u64 start = _pos, blkend, end = _pos + n;
        char *tmp = 0;
        char *buf = (char *)d;
        int err = 0;
        
        if (start & (_blocksize - 1)) { // not aligned
            int soffs = start & (_blocksize - 1);
            
            tmp = new char[_blocksize];
            err = doRead (tmp, start >> _blocksize_bits); CHECKERR;
            memcpy (buf, tmp + soffs, MIN (n, _blocksize - soffs));
            buf += MIN (n, _blocksize - soffs);
            start += MIN (n, _blocksize - soffs);
        }
        
        blkend = end & ~(_blocksize - 1);
        while (start < blkend) {
            err = doRead (buf, start >> _blocksize_bits); CHECKERR;
            start += _blocksize;
            buf += _blocksize;
        }

        if (blkend < end && start < end) {
            if (!tmp) tmp = new char[_blocksize];
            err = doRead (tmp, blkend >> _blocksize_bits); CHECKERR;
            memcpy (buf, tmp, end - blkend);
        }

        if (tmp) delete[] tmp;
        _pos += n;
        return n;
    }

    int BlockDevice::write (const void *d, int n) 
    {
        u64 start = _pos, blkend, end = _pos + n;
        char *tmp = 0;
        char *buf = (char *)d;
        int err = 0;
        
        if (start & (_blocksize - 1)) { // not aligned
            int soffs = start & (_blocksize - 1);
            
            tmp = new char[_blocksize];
            err = doRead (tmp, start >> _blocksize_bits); CHECKERR;
            memcpy (tmp + soffs, buf, MIN (n, _blocksize - soffs));
            err = doWrite (tmp, start >> _blocksize_bits); CHECKERR;
            buf += MIN (n, _blocksize - soffs);
            start += MIN (n, _blocksize - soffs);
        }
        
        blkend = end & ~(_blocksize - 1);
        while (start < blkend) {
            err = doWrite (buf, start >> _blocksize_bits); CHECKERR;
            start += _blocksize;
            buf += _blocksize;
        }

        if (blkend < end && start < end) {
            if (!tmp) tmp = new char[_blocksize];
            err = doRead (tmp, blkend >> _blocksize_bits); CHECKERR;
            memcpy (tmp, buf, end - blkend);
            err = doWrite (tmp, blkend >> _blocksize_bits); CHECKERR;
        }

        if (tmp) delete[] tmp;
        _pos += n;
        return n;
    }

    s64 BlockDevice::lseek (s64 off, int whence) 
    {
        switch (whence) {
        case SEEK_SET:
            _pos = off;
            break;
        case SEEK_CUR:
            _pos += off;
            break;
        case SEEK_END:
            _pos = (_blocks << _blocksize_bits) + off;
            break;
        default:
            return -EINVAL;
        }

        return _pos;
    }

    int BlockFile::read (void *d, int n) 
    {
        u64 start = _pos, blkend, end = _pos + n;
        char *tmp = 0;
        u8 *buf = (u8 *)d;
        int err = 0;
        
        if (end >= _size) {
            end = _size;
            n = end - _pos;
            if (n <= 0)
                return 0;
        }
        
        if (start & (_blocksize - 1)) { // not aligned
            int soffs = start & (_blocksize - 1);
            
            tmp = new char[_blocksize];
            err = readblock (tmp, start >> _blocksize_bits); CHECKERR;
            memcpy (buf, tmp + soffs, MIN (n, _blocksize - soffs));
            buf += MIN (n, _blocksize - soffs);
            start += MIN (n, _blocksize - soffs);
        }
        
        blkend = end & ~(_blocksize - 1);
        while (start < blkend) {
            err = readblock (buf, start >> _blocksize_bits); CHECKERR;
            start += _blocksize;
            buf += _blocksize;
        }
        
        if (blkend < end && start < end) {
            if (!tmp) tmp = new char[_blocksize];
            err = readblock (tmp, start >> _blocksize_bits); CHECKERR;
            memcpy (buf, tmp, end - blkend);
        }
        
        if (tmp) delete[] tmp;
        _pos += n;
        return n;
    }

    int BlockFile::write (const void *d, int n) 
    {
        u64 start = _pos, blkend, end = _pos + n;
        char *tmp = 0;
        char *buf = (char *)d;
        int err = 0;

        if (start & (_blocksize - 1)) { // not aligned
            int soffs = start & (_blocksize - 1);
            
            tmp = new char[_blocksize];
            err = readblock (tmp, start >> _blocksize_bits); CHECKERR;
            memcpy (tmp + soffs, buf, MIN (n, _blocksize - soffs));
            err = writeblock (tmp, start >> _blocksize_bits); CHECKERR;
            buf += MIN (n, _blocksize - soffs);
            start += MIN (n, _blocksize - soffs);
        }
        
        blkend = end & ~(_blocksize - 1);
        while (start < blkend) {
            err = writeblock (buf, start >> _blocksize_bits); CHECKERR;
            start += _blocksize;
            buf += _blocksize;
        }

        if (blkend < end && start < end) {
            if (!tmp) tmp = new char[_blocksize];
            err = readblock (tmp, blkend >> _blocksize_bits); CHECKERR;
            memcpy (tmp, buf, end - blkend);
            err = writeblock (tmp, blkend >> _blocksize_bits); CHECKERR;
        }

        if (tmp) delete[] tmp;
        _pos += n;
        if (_pos > _size) {
            _size = _pos;
        }
        return n;
    }

    s64 BlockFile::lseek (s64 off, int whence) 
    {
        switch (whence) {
        case SEEK_SET:
            _pos = off;
            break;
        case SEEK_CUR:
            _pos += off;
            break;
        case SEEK_END:
            _pos = _size + off;
            break;
        default:
            return -EINVAL;
        }
        return _pos;
    }

    File *MountedFilesystem::open (const char *path, int flags) {
        int mode = OPEN_READ;
        if (flags & O_RDWR) mode = OPEN_READ | OPEN_WRITE;
        if (flags & O_WRONLY) mode = OPEN_WRITE;
        if (flags & O_CREAT) mode |= OPEN_CREATE;
        return new LocalFile (_resolve (path), mode);
    }
    Dir *MountedFilesystem::opendir (const char *path) {
        return new LocalDir (_resolve (path));
    }    

#ifndef WIN32
    void MountedFilesystem::convert_stat (struct stat *s, struct my_stat *st) {
        st->st_dev = s->st_dev;
        st->st_ino = s->st_ino;
        st->st_mode = s->st_mode;
        st->st_nlink = s->st_nlink;
        st->st_uid = s->st_uid;
        st->st_gid = s->st_gid;
        st->st_rdev = s->st_rdev;
        st->st_size = s->st_size;
        st->st_blksize = s->st_blksize;
        st->st_blocks = s->st_blocks;
#ifdef ST_XTIME_ARE_MACROS
        st->st_atime = s->st_atim.tv_sec;
        st->st_mtime = s->st_mtim.tv_sec;
        st->st_ctime = s->st_ctim.tv_sec;
#else
        st->st_atime = s->st_atime;
        st->st_mtime = s->st_mtime;
        st->st_ctime = s->st_ctime;
#endif
    }
#endif
}
