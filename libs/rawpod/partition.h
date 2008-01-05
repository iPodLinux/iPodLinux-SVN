/*-*-C++-*-*/
#ifndef _PARTITION_H_
#define _PARTITION_H_

#include "vfs.h"
#include "device.h"

#ifdef RAWPOD_BIG_ENDIAN
#ifdef __APPLE__
#define bswap_32(x) OSSwapInt32(x)
#else
#include <byteswap.h>
#endif
#endif

class Partition 
{
public:
    Partition() {}

    enum Type { Firmware, FAT32, Ext2, HFS, Other };
    
    virtual Type type() = 0;
    virtual unsigned int offset() = 0;
    virtual unsigned int length() = 0;
    virtual bool active() { return false; }

    virtual void setType (Type t) = 0;
    virtual void setOffset (unsigned int off) = 0;
    virtual void setLength (unsigned int len) = 0;
    virtual void setActive (bool act) { (void)act; }
};

typedef struct 
{
    unsigned char active;
    char chs1[3];
    unsigned char type;
    char chs2[3];
    unsigned int offset; // sectors
    unsigned int length; // sectors
} DOSPartitionDesc;

class DOSPartition : public Partition 
{
public:
    DOSPartition (DOSPartitionDesc *desc)
        : Partition(), _desc (desc)
    {
    }

    virtual Type type() { return ((_desc->type == 0)? Firmware : (_desc->type == 0xB)? FAT32 :
                                  (_desc->type == 0x83)? Ext2 : Other); }
    virtual bool active() { return (_desc->active == 0x80); }

#ifdef RAWPOD_BIG_ENDIAN
    virtual unsigned int offset() { return bswap_32 (_desc->offset); }
    virtual unsigned int length() { return bswap_32 (_desc->length); }
    virtual void setOffset (unsigned int off) { _desc->offset = bswap_32 (off); }
    virtual void setLength (unsigned int len) { _desc->length = bswap_32 (len); }
#else
    virtual unsigned int offset() { return _desc->offset; }
    virtual unsigned int length() { return _desc->length; }
    virtual void setOffset (unsigned int off) { _desc->offset = off; }
    virtual void setLength (unsigned int len) { _desc->length = len; }
#endif

    virtual void setType (Type t);
    virtual void setActive (bool act) { _desc->active = (act? 0x80 : 0); }

protected:
    DOSPartitionDesc *_desc;
};

class PartitionTable 
{
public:
    PartitionTable() : parts (0), valid (false) {}
    virtual ~PartitionTable() { delete[] parts; }

    Partition **parts;
    bool valid;

    Partition *operator[](int idx) { return parts[idx]; }
    operator bool() { return valid; }

    unsigned int length (int idx) { return parts[idx]->length(); }
    unsigned int offset (int idx) { return parts[idx]->offset(); }
    Partition::Type type (int idx) { return parts[idx]->type(); }
    bool active (int idx) { return parts[idx]->active(); }

    void setLength (int idx, unsigned int length) { parts[idx]->setLength (length); }
    void setOffset (int idx, unsigned int offset) { parts[idx]->setOffset (offset); }
    void setType (int idx, Partition::Type type) { parts[idx]->setType (type); }
    void setActive (int idx, bool active) { parts[idx]->setActive (active); }

    virtual void readFrom (VFS::Device *dev) = 0;
    virtual int writeTo (VFS::Device *dev) = 0;
    virtual int figureOutType (unsigned char *mbr) = 0;
    int shrinkAndAdd (int oldnr, int newnr, Partition::Type newtype, int newsize);

    static PartitionTable *create (int devnr, bool writable) { LocalRawDevice dev (devnr, writable); return create (&dev); }
    static PartitionTable *create (VFS::Device *dev);

protected:
    PartitionTable (int nptns) { parts = new Partition*[nptns]; }
};

class DOSPartitionTable : public PartitionTable
{
public:
    DOSPartitionTable() : PartitionTable() {}
    DOSPartitionTable (VFS::Device *dev) : PartitionTable (4) { readFrom (dev); }
    ~DOSPartitionTable() { for (int i = 0; i < 4; i++) { delete parts[i]; } }

    virtual void readFrom (VFS::Device *dev);
    virtual int writeTo (VFS::Device *dev);

#define PART_NOT_IPOD  0
#define PART_WINPOD    1 // p1 = firmware, p2 = music
#define PART_MACPOD    2 // p1 = pmap, p2 = firmware, p3 = music
#define PART_SLINPOD   3 // LinPod with p3 after p1, before p2, using space from firmware partition
#define PART_BLINPOD   4 // LinPod with p3 after p2, or between p's 1&2 but taking space out of music ptn
    virtual int figureOutType (unsigned char *mbr);

protected:
    DOSPartitionDesc _pdata[4];
};

// return 0 for success, nonzero for failure
int devReadMBR (int devnr, unsigned char *buf);
int devWriteMBR (int devnr, unsigned char *buf);
// returns the size in 512-byte sectors for success, 0 or negative for failure
u64 devGetSize (int devnr);

// returns -1 for failure, or the devnr
int find_iPod();

// returns 0 for failure and prints a message
VFS::Device *setup_partition (int disknr, int partnr);
VFS::Device *setup_partition (VFS::Device *disk, int partnr);

#endif
