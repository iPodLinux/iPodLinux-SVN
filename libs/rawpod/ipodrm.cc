#include "device.h"
#include "vfs.h"
#include "partition.h"
#include "ext2.h"
#include <string.h>
#include <time.h>

Ext2FS *ext2;

void process_arg (const char *arg) 
{
    int err;
    if ((err = ext2->unlink (arg)) < 0) {
        printf ("%s: %s\n", arg, strerror(-err));
    }
}

int main (int argc, char **argv) 
{
    int disknr = find_iPod();
    if (disknr < 0) {
        printf ("iPod not found\n");
        return 1;
    }
    
    VFS::Device *part = setup_partition (disknr, 3);
    ext2 = new Ext2FS (part);
    if (ext2->init() < 0) {
        // error already printed
        return 1;
    }

    while (*argv) {
        process_arg (*argv++);
    }

    delete ext2;

    return 0;
}
