#include "rawpod.h"
#include "vfs.h"
#include "device.h"
#include <getopt.h>

struct cow_hdr 
{
#define COW_MAGIC    0x574F4372  /* 'rCOW' */
    u32 magic;
    u32 dummy;
    u64 blocks;
} __attribute__ ((packed));
#define COW_HDR_SIZE 16

void usage (int exitcode) 
{
    fprintf (stderr,
             "Usage: imoo [-i DEVICE] COWFILE\n"
             "   If no [device] is specified, searches for the iPod as usual.\n"
             "\n"
             "Writes a COW file back to a device it was used with.\n");
    exit (exitcode);
}

int main (int argc, char **argv) 
{
    char ch;
    while ((ch = getopt (argc, argv, "h" RAWPOD_OPTIONS_STR)) != EOF) switch (ch) {
    case 'h':
        usage (0);
        break;
    default:
        if (rawpod_parse_option (ch, optarg))
            break;
    case '?':
        usage (1);
        break;
    }

    argc -= optind;
    argv += optind;
    if (argc < 1 || !argv[0])
        usage (1);
    
    const char *cowfile = argv[0];
    int podloc = find_iPod();
    LocalRawDevice *device = new LocalRawDevice (podloc);
    LocalFile *cow = new LocalFile (cowfile, OPEN_READ);
    if (device->error()) {
        fprintf (stderr, "Error opening iPod (%d)\n", device->error());
        exit (2);
    }
    if (cow->error()) {
        fprintf (stderr, "Error opening COW file (%d)\n", cow->error());
        exit (3);
    }
    
    struct cow_hdr hdr;
    cow->read (&hdr, COW_HDR_SIZE);
    if (hdr.magic != COW_MAGIC) {
        fprintf (stderr, "%s is not a COW file\n", cowfile);
        exit (4);
    }
    if (hdr.blocks != device->size()) {
        fprintf (stderr, "%s is a COW file, but not for this device (size=%lld when it should be %lld)\n",
                 cowfile, hdr.blocks, device->size());
        exit (5);
    }

    // Load the block bitmap.
    int lastblk = 0;
    int nblk = 0;
    
    int blkmapsize = hdr.blocks >> 3;
    int blkmapoff = 0;
    int blkmapleft = 0;
    char *blkmapsec = new char[device->blocksize()];
    while (blkmapoff < blkmapsize) {
        if (blkmapleft <= 0) {
            cow->read (blkmapsec, device->blocksize());
            blkmapleft = device->blocksize();
        }

        int x = blkmapsec[blkmapoff & (device->blocksize() - 1)];

        if (x != 0) {
            lastblk = (blkmapoff + 1) << 3;
            for (int i = 0; i < 8; i++) {
                if (x & (1 << i)) nblk++;
            }
        }

        blkmapoff++;
        blkmapleft--;
    }

    printf ("COW file has %d sectors.\n", nblk);

    char *buf = new char[device->blocksize()];
    char *blkmap = new char[lastblk >> 3];
    cow->lseek (COW_HDR_SIZE, SEEK_SET);
    cow->read (blkmap, hdr.blocks >> 3);
    char *blkend = blkmap + (lastblk >> 3);
    int bs = device->blocksizeBits();
    int sector = 0;
    int blksofar = 0;
    while (blkmap != blkend) {
        if (*blkmap != 0) {
            int x = *blkmap;
            for (int i = 0; i < 8; i++) {
                if (x & (1 << i)) {
                    cow->lseek (COW_HDR_SIZE + (hdr.blocks >> 3) + (sector << bs), SEEK_SET);
                    cow->read (buf, device->blocksize());
                    device->lseek (sector << bs, SEEK_SET);
                    device->write (buf, device->blocksize());
                    blksofar++;
                }
            }
        }
        if (sector % 64 == 0 && *blkmap != 0)
            printf ("Writing: %6d/%d   %3d%%\r", blksofar, nblk, blksofar * 100 / nblk);
        sector += 8;
        blkmap++;
    }
    printf ("\rDone.                                     \n");
    delete cow; delete device;
    return 0;
}
