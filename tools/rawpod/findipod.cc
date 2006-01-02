#include "device.h"
#include "partition.h"

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

#ifdef FIND_IPOD_APP
int main() 
{
    find_iPod();
}
#endif
