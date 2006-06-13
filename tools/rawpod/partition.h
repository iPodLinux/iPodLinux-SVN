#ifndef _PARTITION_H_
#define _PARTITION_H_

#include "vfs.h"

typedef struct 
{
    unsigned char active;
    char chs1[3];
    unsigned char type;
    char chs2[3];
    unsigned int offset; // sectors
    unsigned int length; // sectors
} Partition;

typedef Partition *PartitionTable;

PartitionTable partCopyFromMBR (unsigned char *mbr);

#define PART_NOT_IPOD  0
#define PART_WINPOD    1 // p1 = firmware, p2 = music
#define PART_MACPOD    2 // p1 = pmap, p2 = firmware, p3 = music
#define PART_SLINPOD   3 // LinPod with p3 after p1, before p2, using space from firmware partition
#define PART_BLINPOD   4 // LinPod with p3 after p2, or between p's 1&2 but taking space out of music ptn
int partFigureOutType (PartitionTable t, unsigned char *mbr);

// returns 0 for success, nonzero for failure
int partShrinkAndAdd (PartitionTable t, int oldnr, int newnr,
                      int newtype, int newsize /* sectors */);

void partCopyToMBR (PartitionTable t, unsigned char *mbr);

PartitionTable partDupTable (PartitionTable t);

void partFreeTable (PartitionTable t);


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
