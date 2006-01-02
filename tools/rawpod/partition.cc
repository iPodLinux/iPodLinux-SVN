#ifndef WIN32
#ifndef linux
#error Unknown platform
#endif
#endif

#include "partition.h"
#include "device.h"
#include <string.h>
#ifdef WIN32
#include <windows.h>
#else
#define stat __stat
#include <fcntl.h>
#undef stat
#include <unistd.h>
#include <sys/ioctl.h>
#define BLKRRPART  _IO(0x12,95) /* re-read partition table */
#endif

PartitionTable partCopyFromMBR (unsigned char *mbr) 
{
    int ptn;
    PartitionTable ret = new Partition[4];

    if (mbr[510] != 0x55 || mbr[511] != 0xaa)
        return 0;

    memcpy (ret, mbr + 446, 64);
    return ret;
}

int partFigureOutType (PartitionTable t) 
{
    if (t[0].active == 0x80 && t[1].active == 0x80 &&
        t[0].type == 0 && t[1].type == 0xb &&
        t[0].offset < 1024 && t[0].length < (200 << 11)) {
        // WinPod or LinPod
        if (t[2].type == 0x83) {
            // LinPod
            if (t[2].offset > t[0].offset && t[2].offset < t[1].offset)
                return PART_SLINPOD;
            return PART_BLINPOD;
        }
        return PART_WINPOD;
    }

    // XXX can't detect MacPods yet.
    
    return PART_NOT_IPOD;
}

int partShrinkAndAdd (PartitionTable t, int oldnr, int newnr, int newtype, int newsize) 
{
    if (oldnr >= 4 || newnr >= 4)
        return EINVAL;
    if (t[oldnr].length < newsize)
        return ENOSPC;
    if (t[newnr].type != 0)
        return EEXIST;
    
    t[oldnr].length -= newsize;
    t[newnr].active = 0;
    t[newnr].type = newtype;
    t[newnr].offset = t[oldnr].offset + t[oldnr].length;
    t[newnr].length = newsize;

    return 0;
}

void partCopyToMBR (PartitionTable t, unsigned char *mbr) 
{
    memcpy (mbr + 446, t, 64);
}

void partFreeTable (PartitionTable t) 
{
    delete[] t;
}


int devReadMBR (int devnr, unsigned char *buf)
{
#ifdef WIN32
    HANDLE fh;
    DWORD len;
    char drive[] = "\\\\.\\PhysicalDriveN";
    
    drive[17] = devnr + '0';
    fh = CreateFile (drive, GENERIC_READ | GENERIC_WRITE,
                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                     NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                     NULL);
    if (fh == INVALID_HANDLE_VALUE)
        return GetLastError();
    
    if (!ReadFile (fh, buf, 512, &len, NULL))
        return GetLastError();
    
    CloseHandle (fh);
#else
    int fd;
    char dev[] = "/dev/sdX";
    
    dev[7] = devnr + 'a';
    fd = open (dev, O_RDWR);
    if (fd < 0)
        return errno;

    if (read (fd, buf, 512) != 512)
        return errno;

    close (fd);
#endif

    return 0;
}

int devWriteMBR (int devnr, unsigned char *buf)
{
#ifdef WIN32
    HANDLE fh;
    DWORD len;
    char drive[] = "\\\\.\\PhysicalDriveN";
    
    drive[17] = devnr + '0';
    fh = CreateFile (drive, GENERIC_READ | GENERIC_WRITE,
                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                     NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                     NULL);
    if (fh == INVALID_HANDLE_VALUE)
        return GetLastError();
    
    if (!WriteFile (fh, buf, 512, &len, NULL))
        return GetLastError();

    CloseHandle (fh);
#else
    int fd;
    char dev[] = "/dev/sdX";
    
    dev[7] = devnr + 'a';
    fd = open (dev, O_RDWR);
    if (fd < 0)
        return errno;

    if (write (fd, buf, 512) < 0)
        return errno;

    if (ioctl (fd, BLKRRPART) < 0)
        return errno;

    close (fd);
#endif

    return 0;
}

int find_iPod() 
{
    int disknr;
    unsigned char mbr[512];
    for (disknr = 0; disknr < 8; disknr++) {
        PartitionTable ptbl;
        int type;

        if (devReadMBR (disknr, mbr) != 0) {
            continue;
        }
        if ((ptbl = partCopyFromMBR (mbr)) == 0) {
            continue;
        }
        if ((type = partFigureOutType (ptbl)) == PART_NOT_IPOD) {
            continue;
        }
        
        return disknr;
    }
    return -1;
}

VFS::Device *setup_partition (int disknr, int partnr)
{
    partnr--;

    LocalRawDevice *fulldev = new LocalRawDevice (disknr);
    if (fulldev->error()) {
        printf ("drive %d: %s\n", disknr, strerror (fulldev->error()));
        return 0;
    }

    unsigned char mbr[512];
    if (devReadMBR (disknr, mbr) != 0) {
        printf ("drive %d: could not read MBR\n", disknr);
        return 0;
    }

    PartitionTable ptbl;
    if ((ptbl = partCopyFromMBR (mbr)) == 0) {
        printf ("drive %d: invalid partition table\n", disknr);
        return 0;
    }

    PartitionDevice *partdev = new PartitionDevice (fulldev, ptbl[partnr].offset,
                                                    ptbl[partnr].length);
    partFreeTable (ptbl);
    
    return partdev;
}

