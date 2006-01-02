/* kernel/vfs.h -*- C++ -*- Copyright (c) 2005 Joshua Oreman
 * XenOS, of which this file is a part, is licensed under the
 * GNU General Public License. See the file COPYING in the
 * source distribution for details.
 */

#ifndef _VFS_H_
#define _VFS_H_

#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long ulong;
typedef unsigned long long u64;
typedef   signed char s8;
typedef   signed short s16;
typedef   signed int s32;
typedef   signed long slong;
typedef   signed long long s64;

namespace VFS
{
    class Device 
    {
    public:
	Device() {}
	virtual ~Device() {}
	
	virtual int read (void *buf, int n) = 0;
	virtual int write (const void *buf, int n) = 0;
	virtual s64 lseek (s64 off, int whence) { return -1; }
    };

    class BlockDevice : public Device
    {
    public:
        BlockDevice() : Device() {}
        virtual ~BlockDevice() {}

        virtual int read (void *buf, int n);
        virtual int write (const void *buf, int n);
        virtual s64 lseek (s64 off, int whence);
        u64 size() { return _blocks; }
        int blocksize() { return _blocksize; }

    protected:
        virtual int doRead (void *buf, u64 sec) = 0;
        virtual int doWrite (const void *buf, u64 sec) = 0;

        void setBlocksize (int bsz) {
            int bit = 1;
            _blocksize = bsz;
            _blocksize_bits = 0;
            while (!(_blocksize & bit)) {
                _blocksize_bits++;
                bit <<= 1;
            }
            if (_blocksize & ~(1 << _blocksize_bits)) {
                printf ("Unaligned blocksize, expect trouble\n");
            }
        }
        void setSize (u64 blocks) { _blocks = blocks; }

    private:
        u64 _pos;
        int _blocksize, _blocksize_bits;
        u64 _blocks;
    };

    class File
    {
    public:
	File() {}
	virtual ~File() {}
	
	virtual int read (void *buf, int n) = 0;
	virtual int write (const void *buf, int n) = 0;
	virtual s64 lseek (s64 off, int whence) { return -ESPIPE; }
        virtual int error() { return 0; }
        virtual int chown (int uid, int gid = -1) { return -EPERM; }
        virtual int chmod (int mode) { return -EPERM; }
        virtual int truncate() { return -EROFS; }
        virtual int stat (struct stat *st) { return -ENOSYS; }

        virtual int close() {}
    };

    class ErrorFile : public File
    {
    public:
        ErrorFile (int err) { _err = err; if (_err < 0) _err = -_err; }
        int read (void *buf, int n) { return -EBADF; }
        int write (const void *buf, int n) { return -EBADF; }
        int error() { return _err; }

    private:
        int _err;
    };

    class BlockFile : public File
    {
    public:
        BlockFile (int blocksize, int bits) 
            : _blocksize (blocksize), _blocksize_bits (bits), _pos (0), _size (0)
        {}

        virtual ~BlockFile() {}

	virtual int read (void *buf, int n);
	virtual int write (const void *buf, int n);
	virtual s64 lseek (s64 off, int whence);

    protected:
        virtual int readblock (void *buf, u32 block) = 0;
        virtual int writeblock (void *buf, u32 block) = 0;

        int _blocksize, _blocksize_bits;
        u64 _pos, _size;
    };

    struct dirent 
    {
	u32 d_ino;
	char d_name[256];
    };

    class Dir
    {
    public:
	Dir() {}
	virtual ~Dir() {}

	virtual int readdir (struct dirent *buf) = 0;
	virtual int close() {}
        virtual int error() { return 0; }
    };

    class ErrorDir : public Dir
    {
    public:
        ErrorDir (int err) { _err = err; if (_err < 0) _err = -_err; }
        int readdir (struct dirent *de) { return -EBADF; }
        int error() { return _err; }

    private:
        int _err;
    };

    class Filesystem
    {
    public:
	Filesystem (Device *d)
	    : _device (d)
	{}
	virtual ~Filesystem() {}

        // Init function - called to actually init the fs
        // Not in constructor so we can return error codes.
        virtual int init() = 0;
	
        // Sense function - returns 1 if dev contains this fs
        static int probe (Device *dev) {};
        
	// Opening functions - mandatory
	virtual File *open (const char *path, int flags) = 0;
	virtual Dir *opendir (const char *path) = 0;

	// Creating/destroying functions - optional.
        // EPERM = "Filesystem does not support blah-blah."
	virtual int mkdir (const char *path) { return -EPERM; }
	virtual int rmdir (const char *path) { return -EPERM; }
	virtual int unlink (const char *path) { return -EPERM; }

	// Other functions - optional
        virtual int rename (const char *oldpath, const char *newpath) { return -EROFS; }
	virtual int link (const char *oldpath, const char *newpath) { return -EPERM; }
	virtual int symlink (const char *dest, const char *path) { return -EPERM; }
        virtual int readlink (const char *path, char *buf, int len) { return -EINVAL; }
        virtual int stat (const char *path, struct stat *st) {
            File *fp = open (path, O_RDONLY);
            int err = 0;
            if (fp->error())
                err = fp->error();
            else
                err = fp->stat (st);
            fp->close();
            delete fp;
            return err;
        }
        virtual int lstat (const char *path, struct stat *st) {
            return stat (path, st);
        }

    protected:
	Device *_device;
    };
};

#endif
