/* kernel/vfs.h -*- C++ -*- Copyright (c) 2005 Joshua Oreman
 * XenOS, of which this file is a part, is licensed under the
 * GNU General Public License. See the file COPYING in the
 * source distribution for details.
 */

#ifndef _VFS_H_
#define _VFS_H_

#define _FILE_OFFSET_BITS 64
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include <stdio.h>
#ifdef WIN32
#include "errno.h"
#else
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifndef DONT_REDEFINE_OPEN_CONSTANTS
#undef O_RDONLY
#undef O_WRONLY
#undef O_RDWR
#undef O_CREAT
#undef O_APPEND
#undef O_EXCL
#undef O_TRUNC
#define O_RDONLY 01
#define O_WRONLY 02
#define O_RDWR   04
#define O_CREAT  010
#define O_APPEND 020
#define O_EXCL   040
#define O_TRUNC  080
#endif

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

#undef st_atime
#undef st_ctime
#undef st_mtime

struct my_stat 
{
    u32           st_dev;      /* device */
    u32           st_ino;      /* inode */
    u32           st_mode;     /* protection */
    u32           st_nlink;    /* number of hard links */
    u16           st_uid;      /* user ID of owner */
    u16           st_gid;      /* group ID of owner */
    u32           st_rdev;     /* device type (if inode device) */
    u64           st_size;     /* total size, in bytes */
    u32           st_blksize;  /* blocksize for filesystem I/O */
    u32           st_blocks;   /* number of blocks allocated */
    u32           st_atime;    /* time of last access */
    u32           st_mtime;    /* time of last modification */
    u32           st_ctime;    /* time of last status change */    
};

#ifndef S_IFMT
#define S_IFMT     0170000
#define S_IFSOCK   0140000
#define S_IFLNK    0120000
#define S_IFREG    0100000
#define S_IFBLK    0060000
#define S_IFDIR    0040000
#define S_IFCHR    0020000
#define S_IFIFO    0010000
#define S_ISUID    0004000
#define S_ISGID    0002000
#define S_ISVTX    0001000
#define S_IRWXU    00700
#define S_IRUSR    00400
#define S_IWUSR    00200
#define S_IXUSR    00100
#define S_IRWXG    00070
#define S_IRGRP    00040
#define S_IWGRP    00020
#define S_IXGRP    00010
#define S_IRWXO    00007
#define S_IROTH    00004
#define S_IWOTH    00002
#define S_IXOTH    00001

#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#endif

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
        BlockDevice()
            : Device(), _pos (0), _blocksize (512), _blocksize_bits (512), _blocks (0)
        {}
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
        virtual int stat (struct my_stat *st) { return -ENOSYS; }

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
        virtual int stat (const char *path, struct my_stat *st) {
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
        virtual int lstat (const char *path, struct my_stat *st) {
            return stat (path, st);
        }

    protected:
	Device *_device;
    };
};

#endif
