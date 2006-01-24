#ifndef _FAT32_H_
#define _FAT32_H_

#include "vfs.h"

class FATFile;
class FATFS : public VFS::Filesystem
{
    friend class FATFile;
    
public:
    FATFS (VFS::Device *d);
    ~FATFS() {}
    int init();
    
    static int probe (VFS::Device *dev);
    
    virtual VFS::File *open (const char *path, int flags);
    virtual VFS::Dir  *opendir (const char *path) { (void)path; return 0; }

protected:
    u16 bytes_per_sector;
    u8  sectors_per_cluster;
    u16 number_of_reserved_sectors;
    u8  number_of_fats;
    u32 sectors_per_fat;
    u32 root_dir_first_cluster;
    
    u32 clustersize;
};

class FATFile : public VFS::File
{
public:
    FATFile (FATFS *fat, const char *filename);
    ~FATFile() {}

    virtual int read (void *buf, int n);
    virtual int write (const void *buf, int n) { (void)buf, (void)n; return -EROFS; }
    virtual s64 lseek (s64 off, int whence);
    virtual int error() { if (_err < 0) return -_err; return _err; }
    
protected:
    FATFS *_fat;
    VFS::Device *_device;

    s32  findnextcluster (u32 prev);
    int  findfile (u32 start,const char *name);

    int _err;
    u32 cluster;
    u32 length;
    u32 opened;
    u32 position;
};

#endif
