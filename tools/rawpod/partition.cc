#ifdef __CYGWIN32__
#define WIN32
#endif

#ifndef WIN32
#ifndef linux
#ifndef __APPLE__
#error Unknown platform
#endif
#endif
#endif

#include "partition.h"
#include "device.h"
#include <string.h>
#ifdef WIN32
#include <windows.h>
#include <winioctl.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#define BLKRRPART  _IO(0x12,95) /* re-read partition table */
#define BLKGETSIZE _IO(0x12,96) /* get size of device in 512-byte blocks (long *arg) */
#endif

#ifdef __APPLE__
    #include <sys/types.h>
    #include <sys/disk.h>
    #include <sys/stat.h>
#endif

void DOSPartition::setType (Partition::Type t) 
{
    switch (t) {
    case Firmware:
        _desc->type = 0;
        break;
    case FAT32:
        _desc->type = 0xB;
        break;
    case Ext2:
        _desc->type = 0x83;
        break;
    case HFS:
    case Other:
        break;
    }
}

void DOSPartitionTable::readFrom (VFS::Device *dev) 
{
    unsigned char buf[512];

    if (dev->lseek (0, SEEK_SET) < 0) {
        valid = false;
        return;
    }

    if (dev->read (buf, 512) != 512) {
        valid = false;
        return;
    }

    if (buf[510] != 0x55 || buf[511] != 0xAA) {
        valid = false;
        return;
    }

    memcpy (_pdata, buf + 446, 64);
    
    for (int i = 0; i < 4; i++) {
        parts[i] = new DOSPartition (_pdata + i);
    }

    valid = true;
}

int DOSPartitionTable::writeTo (VFS::Device *dev) 
{
    unsigned char buf[512];
    int err;

    if ((err = dev->lseek (0, SEEK_SET)) < 0)
        return -err;
    if ((err = dev->read (buf, 512)) != 512)
        return -err;

    memcpy (buf + 446, _pdata, 64);

    if ((err = dev->lseek (0, SEEK_SET)) < 0)
        return -err;
    if ((err = dev->write (buf, 512)) != 512)
        return -err;

#ifdef linux
    LocalRawDevice *lrdev;
    if ((lrdev = dynamic_cast<LocalRawDevice*>(dev)) != 0) {
        ioctl (lrdev->fileno(), BLKRRPART);
    }
#endif

    return 0;
}

int DOSPartitionTable::figureOutType (unsigned char *mbr) 
{
    if (!memcmp (mbr + 0x1ae, "Apple iPod", 10) &&
        parts[0]->type() == Partition::Firmware && parts[1]->type() == Partition::FAT32 &&
        parts[0]->offset() < 1024 && parts[0]->length() < (200 << 11)) {
        // WinPod or LinPod
        if (parts[2]->type() == Partition::Ext2) {
            // LinPod
            if (parts[2]->offset() > parts[0]->offset() && parts[2]->offset() < parts[1]->offset() &&
                parts[2]->offset() < (64 << 11))
                return PART_SLINPOD;
            return PART_BLINPOD;
        }
        return PART_WINPOD;
    }

    return PART_NOT_IPOD;
}

int PartitionTable::shrinkAndAdd (int oldnr, int newnr, Partition::Type newtype, int newsize) 
{
    oldnr--; newnr--;
    
    if (oldnr >= 4 || newnr >= 4)
        return EINVAL;
    if (parts[oldnr]->length() < (unsigned int)newsize)
        return ENOSPC;
    if (parts[newnr]->type() != Partition::Firmware) /* == Empty */
        return EEXIST;

    // Round it to the nearest cylinder.
    unsigned int cylsize = 255 /* heads */ * 63 /* sectors/track */;
    int roundDownFuzz = newsize % cylsize, roundUpFuzz = cylsize - roundDownFuzz;
    if (newsize < cylsize) roundDownFuzz = -1;
    if ((newsize + cylsize) >= parts[oldnr]->length()) roundUpFuzz = -1;
    if (roundDownFuzz != -1 || roundUpFuzz != -1) {
        if (roundDownFuzz > roundUpFuzz && roundUpFuzz != -1)
            newsize = newsize + cylsize - 1;
        newsize = newsize - (newsize % cylsize);
    }
    
    parts[oldnr]->setLength (parts[oldnr]->length() - newsize);
    parts[newnr]->setActive (false);
    parts[newnr]->setType (newtype);
    parts[newnr]->setOffset (parts[oldnr]->offset() + parts[oldnr]->length());
    parts[newnr]->setLength (newsize);

    return 0;
}

PartitionTable *PartitionTable::create (VFS::Device *dev) 
{
    unsigned char mbr[512];

    if (dev->lseek (0, SEEK_SET) < 0)
        return 0;
    
    if (dev->read (mbr, 512) != 512)
        return 0;
    
    if (mbr[510] == 0x55 && mbr[511] == 0xAA)
        return new DOSPartitionTable (dev);
    
    // XXX add MacPod detection stuff here
    return 0;
}

int devReadMBR (int devnr, unsigned char *buf)
{
    LocalRawDevice dev (devnr, false);
    if (dev.read (buf, 512) != 512)
        return dev.error();
    return 0;
}

int devWriteMBR (int devnr, unsigned char *buf)
{
    if (LocalRawDevice::overridden()) {
        LocalRawDevice dev (devnr);
        if (dev.write (buf, 512) != 512)
            return dev.error();
        return 0;
    }

    //!TT why not use the same code here as above when overridden?? (ok, linux also appears to need the ioctl call, but that could be added here still)
    
#ifdef WIN32
    HANDLE fh;
    DWORD len;
    TCHAR drive[] = TEXT("\\\\.\\PhysicalDriveN");

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
#ifdef __APPLE__
    char dev[] = "/dev/rdiskX";
    dev[strlen(dev)-1] = devnr + '0';
#else
    char dev[] = "/dev/sdX";
    dev[7] = devnr + 'a';
#endif

    fd = open (dev, O_RDWR);
    if (fd < 0)
        return errno;

    if (write (fd, buf, 512) < 0)
        return errno;

#ifndef __APPLE__
    ioctl (fd, BLKRRPART);
#endif

    close (fd);
#endif

    return 0;
}

u64 devGetSize (int devnr) 
{
  // josh, here I replaced the individual code with just this call,
  // which seems to work just fine - or do i miss something here?
    u64 l = LocalRawDevice(devnr, false).size();
    return l;
}

#ifdef WIN32
void ERR(int e) {
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = e; 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    lpDisplayBuf = LocalAlloc(LMEM_ZEROINIT, 
        (strlen((const char *)lpMsgBuf)+40)*sizeof(TCHAR)); 
    wsprintf((TCHAR *)lpDisplayBuf, 
        TEXT("Reading MBR failed with error %d: %s"), 
        dw, lpMsgBuf); 
    MessageBox(NULL, (TCHAR *)lpDisplayBuf, TEXT("Error"), MB_OK); 

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
}
#endif

int find_iPod() 
{
    if (LocalRawDevice::overridden()) {
        printf ("Pretending override file is an iPod\n");
        return 0;
    }

    int disknr;
    unsigned char mbr[512];
    for (disknr = 0; disknr < 8; disknr++) {
        PartitionTable *ptbl = PartitionTable::create (disknr, false);
        int type, err;

        if ((err = devReadMBR (disknr, mbr)) != 0) {
            printf ("Disk %d: cannot read MBR (%d)\n", disknr, err);
#ifdef WIN32
#ifdef DEBUG
            ERR(err);
#endif
#endif
            delete ptbl;
            continue;
        }
        
        if (!ptbl || !*ptbl) {
            printf ("Disk %d: cannot set up partition table\n", disknr);
            delete ptbl;
            continue;
        }

        if ((type = ptbl->figureOutType (mbr)) == PART_NOT_IPOD) {
            printf ("Disk %d: not an iPod\n", disknr);
            delete ptbl;
            continue;
        }
        
        delete ptbl;
        return disknr;
    }
    return -1;
}

VFS::Device *setup_partition (int disknr, int partnr)
{
    partnr--;

    LocalRawDevice *fulldev = new LocalRawDevice (disknr, true);
    if (fulldev->error()) {
        printf ("drive %d: %s\n", disknr, strerror (fulldev->error()));
        return 0;
    }

    PartitionTable *ptbl = PartitionTable::create (fulldev);
    if (!ptbl || !*ptbl) {
        printf ("drive %d: invalid partition table\n", disknr);
        return 0;
    }

    PartitionDevice *partdev = new PartitionDevice (fulldev, (*ptbl)[partnr]->offset(),
                                                    (*ptbl)[partnr]->length());

    delete ptbl;
    return partdev;
}

VFS::Device *setup_partition (VFS::Device *fulldev, int partnr) 
{
    partnr--;

    PartitionTable *ptbl = PartitionTable::create (fulldev);
    if (!ptbl || !*ptbl)
        return 0;

    PartitionDevice *partdev = new PartitionDevice (fulldev, (*ptbl)[partnr]->offset(),
                                                    (*ptbl)[partnr]->length());

    delete ptbl;
    return partdev;
}
