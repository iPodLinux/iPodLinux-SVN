/* Modified 2006-03-01 by Joshua Oreman to work with the rawpod interface. */

/*
   Filename:     mkdosfs.c
   Version:      0.3b (Yggdrasil)
   Author:       Dave Hudson
   Started:      24th August 1994
   Last Updated: 7th May 1998
   Updated by:   Roman Hodek <Roman.Hodek@informatik.uni-erlangen.de>
   Target O/S:   Linux (2.x)

   Description: Utility to allow an MS-DOS filesystem to be created
   under Linux.  A lot of the basic structure of this program has been
   borrowed from Remy Card's "mke2fs" code.

   As far as possible the aim here is to make the "mkdosfs" command
   look almost identical to the other Linux filesystem make utilties,
   eg bad blocks are still specified as blocks, not sectors, but when
   it comes down to it, DOS is tied to the idea of a sector (512 bytes
   as a rule), and not the block.  For example the boot block does not
   occupy a full cluster.

   Fixes/additions May 1998 by Roman Hodek
   <Roman.Hodek@informatik.uni-erlangen.de>:
   - Atari format support
   - New options -A, -S, -C
   - Support for filesystems > 2GB
   - FAT32 support
   
   Copying:     Copyright 1993, 1994 David Hudson (dave@humbug.demon.co.uk)

   Portions copyright 1992, 1993 Remy Card (card@masi.ibp.fr)
   and 1991 Linus Torvalds (torvalds@klaava.helsinki.fi)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */


/* Include the header files */

#include "vfs.h"

#define VERSION "2.10-rawpod"
#define VERSION_DATE "01 Mar 2006"

#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <setjmp.h>

#ifdef WIN32
typedef s64 loff_t;
#endif

#if __BYTE_ORDER == __BIG_ENDIAN

#define CF_LE_W(v) ((((v) & 0xff) << 8) | (((v) >> 8) & 0xff))
#define CF_LE_L(v) (((unsigned)(v)>>24) | (((unsigned)(v)>>8)&0xff00) | \
               (((unsigned)(v)<<8)&0xff0000) | ((unsigned)(v)<<24))
#define CT_LE_W(v) CF_LE_W(v)
#define CT_LE_L(v) CF_LE_L(v)
    
#else

#define CF_LE_W(v) (v)
#define CF_LE_L(v) (v)
#define CT_LE_W(v) (v)
#define CT_LE_L(v) (v)

#endif /* __BIG_ENDIAN */

/* Constant definitions */

#define TRUE 1			/* Boolean constants */
#define FALSE 0

#define TEST_BUFFER_BLOCKS 16
#define HARD_SECTOR_SIZE   512
#define SECTORS_PER_BLOCK ( BLOCK_SIZE / HARD_SECTOR_SIZE )
#define BLOCK_SIZE 512
#define BLOCK_SIZE_BITS 9

/* Macro definitions */

/* Report a failure message and return a failure error code */

#define die( str ) fatal_error( "%s: " str "\n" )


/* Mark a cluster in the FAT as bad */

#define mark_sector_bad( sector ) mark_FAT_sector( sector, FAT_BAD )

/* Compute ceil(a/b) */

inline int
cdiv (int a, int b)
{
  return (a + b - 1) / b;
}

/* MS-DOS filesystem structures -- I included them here instead of
   including linux/msdos_fs.h since that doesn't include some fields we
   need */

#define ATTR_RO      1		/* read-only */
#define ATTR_HIDDEN  2		/* hidden */
#define ATTR_SYS     4		/* system */
#define ATTR_VOLUME  8		/* volume label */
#define ATTR_DIR     16		/* directory */
#define ATTR_ARCH    32		/* archived */

#define ATTR_NONE    0		/* no attribute bits */
#define ATTR_UNUSED  (ATTR_VOLUME | ATTR_ARCH | ATTR_SYS | ATTR_HIDDEN)
	/* attribute bits that are copied "as is" */

/* FAT values */
#define FAT_EOF      (atari_format ? 0x0fffffff : 0x0ffffff8)
#define FAT_BAD      0x0ffffff7

#define MSDOS_EXT_SIGN 0x29	/* extended boot sector signature */
#define MSDOS_FAT12_SIGN "FAT12   "	/* FAT12 filesystem signature */
#define MSDOS_FAT16_SIGN "FAT16   "	/* FAT16 filesystem signature */
#define MSDOS_FAT32_SIGN "FAT32   "	/* FAT32 filesystem signature */

#define BOOT_SIGN 0xAA55	/* Boot sector magic number */

#define MAX_CLUST_12	((1 << 12) - 16)
#define MAX_CLUST_16	((1 << 16) - 16)
/* M$ says the high 4 bits of a FAT32 FAT entry are reserved and don't belong
 * to the cluster number. So the max. cluster# is based on 2^28 */
#define MAX_CLUST_32	((1 << 28) - 16)

#define FAT12_THRESHOLD	4078

#define OLDGEMDOS_MAX_SECTORS	32765
#define GEMDOS_MAX_SECTORS	65531
#define GEMDOS_MAX_SECTOR_SIZE	(16*1024)

#define BOOTCODE_SIZE		448
#define BOOTCODE_FAT32_SIZE	420

/* __attribute__ ((packed)) is used on all structures to make gcc ignore any
 * alignments */

struct msdos_volume_info {
  u8		drive_number;	/* BIOS drive number */
  u8		RESERVED;	/* Unused */
  u8		ext_boot_sign;	/* 0x29 if fields below exist (DOS 3.3+) */
  u8		volume_id[4];	/* Volume ID number */
  u8		volume_label[11];/* Volume label */
  u8		fs_type[8];	/* Typically FAT12 or FAT16 */
} __attribute__ ((packed));

struct msdos_boot_sector
{
  u8	        boot_jump[3];	/* Boot strap short or near jump */
  u8            system_id[8];	/* Name - can be used to special case
				   partition manager volumes */
  u8            sector_size[2];	/* bytes per logical sector */
  u8            cluster_size;	/* sectors/cluster */
  u16           reserved;	/* reserved sectors */
  u8            fats;		/* number of FATs */
  u8            dir_entries[2];	/* root directory entries */
  u8            sectors[2];	/* number of sectors */
  u8            media;		/* media code (unused) */
  u16           fat_length;	/* sectors/FAT */
  u16           secs_track;	/* sectors per track */
  u16           heads;		/* number of heads */
  u32           hidden;		/* hidden sectors (unused) */
  u32           total_sect;	/* number of sectors (if sectors == 0) */
  union {
    struct {
      struct msdos_volume_info vi;
      u8	boot_code[BOOTCODE_SIZE];
    } __attribute__ ((packed)) _oldfat;
    struct {
      u32	fat32_length;	/* sectors/FAT */
      u16	flags;		/* bit 8: fat mirroring, low 4: active fat */
      u8	version[2];	/* major, minor filesystem version */
      u32	root_cluster;	/* first cluster in root directory */
      u16	info_sector;	/* filesystem info sector */
      u16	backup_boot;	/* backup boot sector */
      u16	reserved2[6];	/* Unused */
      struct msdos_volume_info vi;
      u8	boot_code[BOOTCODE_FAT32_SIZE];
    } __attribute__ ((packed)) _fat32;
  } __attribute__ ((packed)) fstype;
  u16		boot_sign;
} __attribute__ ((packed));
#define fat32	fstype._fat32
#define oldfat	fstype._oldfat

struct fat32_fsinfo {
  u32		reserved1;	/* Nothing as far as I can tell */
  u32		signature;	/* 0x61417272L */
  u32		free_clusters;	/* Free cluster count.  -1 if unknown */
  u32		next_cluster;	/* Most recently allocated cluster.
				 * Unused under Linux. */
  u32		reserved2[4];
};

struct msdos_dir_entry
  {
    char	name[8], ext[3];	/* name and extension */
    u8          attr;			/* attribute bits */
    u8		lcase;			/* Case for base and extension */
    u8		ctime_ms;		/* Creation time, milliseconds */
    u16		ctime;			/* Creation time */
    u16		cdate;			/* Creation date */
    u16		adate;			/* Last access date */
    u16		starthi;		/* high 16 bits of first cl. (FAT32) */
    u16		time, date, start;	/* time, date and first cluster */
    u32		size;			/* file size (in bytes) */
  } __attribute__ ((packed));

/* The "boot code" we put into the filesystem... it writes a message and
   tells the user to try again */

char dummy_boot_jump[3] = { 0xeb, 0x3c, 0x90 };

char dummy_boot_jump_m68k[2] = { 0x60, 0x1c };

#define MSG_OFFSET_OFFSET 3
char dummy_boot_code[BOOTCODE_SIZE] =
  "\x0e"			/* push cs */
  "\x1f"			/* pop ds */
  "\xbe\x5b\x7c"		/* mov si, offset message_txt */
				/* write_msg: */
  "\xac"			/* lodsb */
  "\x22\xc0"			/* and al, al */
  "\x74\x0b"			/* jz key_press */
  "\x56"			/* push si */
  "\xb4\x0e"			/* mov ah, 0eh */
  "\xbb\x07\x00"		/* mov bx, 0007h */
  "\xcd\x10"			/* int 10h */
  "\x5e"			/* pop si */
  "\xeb\xf0"			/* jmp write_msg */
				/* key_press: */
  "\x32\xe4"			/* xor ah, ah */
  "\xcd\x16"			/* int 16h */
  "\xcd\x19"			/* int 19h */
  "\xeb\xfe"			/* foo: jmp foo */
				/* message_txt: */

  "This is not a bootable disk.  Please insert a bootable floppy and\r\n"
  "press any key to try again ... \r\n";

#define MESSAGE_OFFSET 29	/* Offset of message in above code */

/* Global variables - the root of all evil :-) - see these and weep! */

static char *program_name = "mkdosfs";	/* Name of the program */
static int atari_format = 0;	/* Use Atari variation of MS-DOS FS format */
static int check = FALSE;	/* Default to no readablity checking */
static int verbose = 0;		/* Default to verbose mode off */
static long volume_id;		/* Volume ID number */
static time_t create_time;	/* Creation time */
static char volume_name[] = "           "; /* Volume name */
static int blocks;		/* Number of blocks in filesystem */
static int sector_size = 512;	/* Size of a logical sector */
static int sector_size_set = 0; /* User selected sector size */
static int backup_boot = 0;	/* Sector# of backup boot sector */
static int reserved_sectors = 0;/* Number of reserved sectors */
static int badblocks = 0;	/* Number of bad blocks in the filesystem */
static int nr_fats = 2;		/* Default number of FATs to produce */
static int size_fat = 0;	/* Size in bits of FAT entries */
static int size_fat_by_user = 0; /* 1 if FAT size user selected */
static VFS::Device *dev = 0;		/* FS block device file handle */
static int  ignore_full_disk = 0; /* Ignore warning about 'full' disk devices */
static unsigned int currently_testing = 0;	/* Block currently being tested (if autodetect bad blocks) */
static struct msdos_boot_sector bs;	/* Boot sector data */
static int start_data_sector;	/* Sector number for the start of the data area */
static int start_data_block;	/* Block number for the start of the data area */
static unsigned char *fat;	/* File allocation table */
static unsigned char *info_sector;	/* FAT32 info sector */
static struct msdos_dir_entry *root_dir;	/* Root directory */
static int size_root_dir;	/* Size of the root directory in bytes */
static int sectors_per_cluster = 0;	/* Number of sectors per disk cluster */
static int root_dir_entries = 0;	/* Number of root directory entries */
static char *blank_sector;		/* Blank sector - all zeros */


/* Function prototype definitions */

static void fatal_error (const char *fmt_string) __attribute__((noreturn));
static void mark_FAT_cluster (int cluster, unsigned int value);
static void mark_FAT_sector (int sector, unsigned int value);
static long do_check (char *buffer, int Try, unsigned int current_block);
static void alarm_intr (int alnum);
static void check_blocks (void);
static void get_list_blocks (char *filename);
static int valid_offset (VFS::Device *dev, loff_t offset);
static int count_blocks();
static void check_mount (char *device_name);
static void establish_params (int size);
static void setup_tables (void);
static void write_tables (void);

static jmp_buf error_out;

/* The function implementations */

/* Handle the reporting of fatal errors.  Volatile to let gcc know that this doesn't return */

static void
fatal_error (const char *fmt_string)
{
  fprintf (stderr, fmt_string, program_name, "<rawpod device>");
  longjmp (error_out, 1);
}


/* Mark the specified cluster as having a particular value */

static void
mark_FAT_cluster (int cluster, unsigned int value)
{
  switch( size_fat ) {
    case 12:
      value &= 0x0fff;
      if (((cluster * 3) & 0x1) == 0)
	{
	  fat[3 * cluster / 2] = (unsigned char) (value & 0x00ff);
	  fat[(3 * cluster / 2) + 1] = (unsigned char) ((fat[(3 * cluster / 2) + 1] & 0x00f0)
						 | ((value & 0x0f00) >> 8));
	}
      else
	{
	  fat[3 * cluster / 2] = (unsigned char) ((fat[3 * cluster / 2] & 0x000f) | ((value & 0x000f) << 4));
	  fat[(3 * cluster / 2) + 1] = (unsigned char) ((value & 0x0ff0) >> 4);
	}
      break;

    case 16:
      value &= 0xffff;
      fat[2 * cluster] = (unsigned char) (value & 0x00ff);
      fat[(2 * cluster) + 1] = (unsigned char) (value >> 8);
      break;

    case 32:
      value &= 0xfffffff;
      fat[4 * cluster] =       (unsigned char)  (value & 0x000000ff);
      fat[(4 * cluster) + 1] = (unsigned char) ((value & 0x0000ff00) >> 8);
      fat[(4 * cluster) + 2] = (unsigned char) ((value & 0x00ff0000) >> 16);
      fat[(4 * cluster) + 3] = (unsigned char) ((value & 0xff000000) >> 24);
      break;

    default:
      die("Bad FAT size (not 12, 16, or 32)");
  }
}


/* Mark a specified sector as having a particular value in it's FAT entry */

static void
mark_FAT_sector (int sector, unsigned int value)
{
  int cluster;

  cluster = (sector - start_data_sector) / (int) (bs.cluster_size) /
	    (sector_size/HARD_SECTOR_SIZE);
  if (cluster < 0)
    die ("Invalid cluster number in mark_FAT_sector: probably bug!");

  mark_FAT_cluster (cluster, value);
}


/* Perform a test on a block.  Return the number of blocks that could be read successfully */

static long
do_check (char *buffer, int Try, unsigned int current_block)
{
  long got;

  if (dev->lseek ((loff_t)current_block * BLOCK_SIZE, SEEK_SET) != (loff_t)current_block * BLOCK_SIZE)
    die ("seek failed during testing for blocks");

  got = dev->read (buffer, Try * BLOCK_SIZE);	/* Try reading! */
  if (got < 0)
    got = 0;

  if (got & (BLOCK_SIZE - 1))
    printf ("Unexpected values in do_check: probably bugs\n");
  got /= BLOCK_SIZE;

  return got;
}


/* Alarm clock handler - display the status of the quest for bad blocks!  Then retrigger the alarm for five senconds
   later (so we can come here again) */

static void
alarm_intr (int alnum)
{
}


static void
check_blocks (void)
{
  int Try, got;
  int i;
  static char blkbuf[BLOCK_SIZE * TEST_BUFFER_BLOCKS];

  if (verbose)
    {
      printf ("Searching for bad blocks ");
      fflush (stdout);
    }
  currently_testing = 0;
  Try = TEST_BUFFER_BLOCKS;
  while (currently_testing < blocks)
    {
      if (currently_testing + Try > blocks)
	Try = blocks - currently_testing;
      got = do_check (blkbuf, Try, currently_testing);
      currently_testing += got;
      if (got == Try)
	{
	  Try = TEST_BUFFER_BLOCKS;
	  continue;
	}
      else
	Try = 1;
      if (currently_testing < start_data_block)
	die ("bad blocks before data-area: cannot make fs");

      for (i = 0; i < SECTORS_PER_BLOCK; i++)	/* Mark all of the sectors in the block as bad */
	mark_sector_bad (currently_testing * SECTORS_PER_BLOCK + i);
      badblocks++;
      currently_testing++;
    }

  if (verbose)
    printf ("\n");

  if (badblocks)
    printf ("%d bad block%s\n", badblocks,
	    (badblocks > 1) ? "s" : "");
}


static void
get_list_blocks (char *filename)
{
  int i;
  FILE *listfile;
  unsigned long blockno;

  listfile = fopen (filename, "r");
  if (listfile == (FILE *) NULL)
    die ("Can't open file of bad blocks");

  while (!feof (listfile))
    {
      fscanf (listfile, "%ld\n", &blockno);
      for (i = 0; i < SECTORS_PER_BLOCK; i++)	/* Mark all of the sectors in the block as bad */
	mark_sector_bad (blockno * SECTORS_PER_BLOCK + i);
      badblocks++;
    }
  fclose (listfile);

  if (badblocks)
    printf ("%d bad block%s\n", badblocks,
	    (badblocks > 1) ? "s" : "");
}


/* Given a file descriptor and an offset, check whether the offset is a valid offset for the file - return FALSE if it
   isn't valid or TRUE if it is */

static int
valid_offset (VFS::Device *dev, loff_t offset)
{
  char ch;

  if (dev->lseek (offset, SEEK_SET) != offset)
    return FALSE;
  if (dev->read (&ch, 1) < 1)
    return FALSE;
  return TRUE;
}


/* Given a device, look to see how many blocks of BLOCK_SIZE are present, returning the answer */

static int
count_blocks()
{
  loff_t high, low;

  /* first try SEEK_END, which should work on most devices nowadays */
  if ((low = dev->lseek(0, SEEK_END)) <= 0) {
      low = 0;
      for (high = 1; valid_offset (dev, high); high *= 2)
	  low = high;
      while (low + 1 < high) {
	  const loff_t mid = (low + high) / 2;
	  if (valid_offset (dev, mid))
	      low = mid;
	  else
	      high = mid;
      }
      ++low;
  }

  return low / BLOCK_SIZE;
}


/* Check to see if the specified device is currently mounted - abort if it is */

static void
check_mount (char *device_name)
{
}


/* Establish the geometry and media parameters for the device */

static void
establish_params()
{
    struct {
        u32 sectors, heads;
    } geometry;
    // We're just using what basically amounts to guestimated values for sectors and heads here.
    // Modern drives don't have a geometry anyway, so it's pretty much a moot point.
    int headvalues[] = { 16, 32, 64, 128, 255, 0 }; // from Large-Disk-HOWTO
    geometry.heads = 255;
    geometry.sectors = 63;
    for (int hv = 0; headvalues[hv]; hv++) {
        if ((1024 * headvalues[hv] * 63) > blocks) {
            geometry.heads = headvalues[hv];
            break;
        }
    }

    bs.secs_track = CT_LE_W(geometry.sectors);	/* Set up the geometry information */
    bs.heads = CT_LE_W(geometry.heads);
    bs.media = (char) 0xf8; /* Set up the media descriptor for a hard drive */
    bs.dir_entries[0] = (char) 0;	/* Default to 512 entries */
    bs.dir_entries[1] = (char) 2;
    if (size_fat == 32) {
        /* For FAT32, try to do the same as M$'s format command:
         * fs size < 256M: 0.5k clusters
         * fs size <   8G: 4k clusters
         * fs size <  16G: 8k clusters
         * fs size >= 16G: 16k clusters
         */
        unsigned long sz_mb =
            (blocks+(1<<(20-BLOCK_SIZE_BITS))-1) >> (20-BLOCK_SIZE_BITS);
        bs.cluster_size = (sz_mb >= 16*1024 ? 32 :
                           sz_mb >=  8*1024 ? 16 :
                           sz_mb >=     256 ?  8 :
                           1);
    }
    else {
        /* FAT12 and FAT16: start at 4 sectors per cluster */
        bs.cluster_size = (char) 4;
    }
}


/* Create the filesystem data tables */

static void
setup_tables (void)
{
  unsigned num_sectors;
  unsigned cluster_count = 0, fat_length;
  unsigned fatdata;			/* Sectors for FATs + data area */
  struct tm *ctime;
  struct msdos_volume_info *vi = (size_fat == 32 ? &bs.fat32.vi : &bs.oldfat.vi);
  
  if (atari_format)
      /* On Atari, the first few bytes of the boot sector are assigned
       * differently: The jump code is only 2 bytes (and m68k machine code
       * :-), then 6 bytes filler (ignored), then 3 byte serial number. */
    memcpy( bs.system_id-1, "mkdosf", 6 );
  else
    memcpy (bs.system_id, "mkdosfs", 7);
  if (sectors_per_cluster)
    bs.cluster_size = (char) sectors_per_cluster;
  if (size_fat == 32)
    {
      /* Under FAT32, the root dir is in a cluster chain, and this is
       * signalled by bs.dir_entries being 0. */
      bs.dir_entries[0] = bs.dir_entries[1] = (char) 0;
      root_dir_entries = 0;
    }
  else if (root_dir_entries)
    {
      /* Override default from establish_params() */
      bs.dir_entries[0] = (char) (root_dir_entries & 0x00ff);
      bs.dir_entries[1] = (char) ((root_dir_entries & 0xff00) >> 8);
    }
  else
    root_dir_entries = bs.dir_entries[0] + (bs.dir_entries[1] << 8);

  if (atari_format) {
    bs.system_id[5] = (unsigned char) (volume_id & 0x000000ff);
    bs.system_id[6] = (unsigned char) ((volume_id & 0x0000ff00) >> 8);
    bs.system_id[7] = (unsigned char) ((volume_id & 0x00ff0000) >> 16);
  }
  else {
    vi->volume_id[0] = (unsigned char) (volume_id & 0x000000ff);
    vi->volume_id[1] = (unsigned char) ((volume_id & 0x0000ff00) >> 8);
    vi->volume_id[2] = (unsigned char) ((volume_id & 0x00ff0000) >> 16);
    vi->volume_id[3] = (unsigned char) (volume_id >> 24);
  }

  if (!atari_format) {
    memcpy(vi->volume_label, volume_name, 11);
  
    memcpy(bs.boot_jump, dummy_boot_jump, 3);
    /* Patch in the correct offset to the boot code */
    bs.boot_jump[1] = ((size_fat == 32 ?
			(char *)&bs.fat32.boot_code :
			(char *)&bs.oldfat.boot_code) -
		       (char *)&bs) - 2;

    if (size_fat == 32) {
	int offset = (char *)&bs.fat32.boot_code -
		     (char *)&bs + MESSAGE_OFFSET + 0x7c00;
	if (dummy_boot_code[BOOTCODE_FAT32_SIZE-1])
	  printf ("Warning: message too long; truncated\n");
	dummy_boot_code[BOOTCODE_FAT32_SIZE-1] = 0;
	memcpy(bs.fat32.boot_code, dummy_boot_code, BOOTCODE_FAT32_SIZE);
	bs.fat32.boot_code[MSG_OFFSET_OFFSET] = offset & 0xff;
	bs.fat32.boot_code[MSG_OFFSET_OFFSET+1] = offset >> 8;
    }
    else {
	memcpy(bs.oldfat.boot_code, dummy_boot_code, BOOTCODE_SIZE);
    }
    bs.boot_sign = CT_LE_W(BOOT_SIGN);
  }
  else {
    memcpy(bs.boot_jump, dummy_boot_jump_m68k, 2);
  }
  if (verbose >= 2)
    printf( "Boot jump code is %02x %02x\n",
	    bs.boot_jump[0], bs.boot_jump[1] );

  if (!reserved_sectors)
      reserved_sectors = (size_fat == 32) ? 32 : 1;
  else {
      if (size_fat == 32 && reserved_sectors < 2)
	  die("On FAT32 at least 2 reserved sectors are needed.");
  }
  bs.reserved = CT_LE_W(reserved_sectors);
  if (verbose >= 2)
    printf( "Using %d reserved sectors\n", reserved_sectors );
  bs.fats = (char) nr_fats;
  if (!atari_format || size_fat == 32)
    bs.hidden = CT_LE_L(0);
  else
    /* In Atari format, hidden is a 16 bit field */
    memset( &bs.hidden, 0, 2 );

  num_sectors = (long long)blocks*BLOCK_SIZE/sector_size;
  if (!atari_format) {
    unsigned fatlength12, fatlength16, fatlength32;
    unsigned maxclust12, maxclust16, maxclust32;
    unsigned clust12, clust16, clust32;
    int maxclustsize;
    
    fatdata = num_sectors - cdiv (root_dir_entries * 32, sector_size) -
	      reserved_sectors;

    if (sectors_per_cluster)
      bs.cluster_size = maxclustsize = sectors_per_cluster;
    else
      /* An initial guess for bs.cluster_size should already be set */
      maxclustsize = 128;

    if (verbose >= 2)
      printf( "%d sectors for FAT+data, starting with %d sectors/cluster\n",
	      fatdata, bs.cluster_size );
    do {
      if (verbose >= 2)
	printf( "Trying with %d sectors/cluster:\n", bs.cluster_size );

      /* The factor 2 below avoids cut-off errors for nr_fats == 1.
       * The "nr_fats*3" is for the reserved first two FAT entries */
      clust12 = 2*((long long) fatdata *sector_size + nr_fats*3) /
	(2*(int) bs.cluster_size * sector_size + nr_fats*3);
      fatlength12 = cdiv (((clust12+2) * 3 + 1) >> 1, sector_size);
      /* Need to recalculate number of clusters, since the unused parts of the
       * FATS and data area together could make up space for an additional,
       * not really present cluster. */
      clust12 = (fatdata - nr_fats*fatlength12)/bs.cluster_size;
      maxclust12 = (fatlength12 * 2 * sector_size) / 3;
      if (maxclust12 > MAX_CLUST_12)
	maxclust12 = MAX_CLUST_12;
      if (verbose >= 2)
	printf( "FAT12: #clu=%u, fatlen=%u, maxclu=%u, limit=%u\n",
		clust12, fatlength12, maxclust12, MAX_CLUST_12 );
      if (clust12 > maxclust12-2) {
	clust12 = 0;
	if (verbose >= 2)
	  printf( "FAT12: too much clusters\n" );
      }

      clust16 = ((long long) fatdata *sector_size + nr_fats*4) /
	((int) bs.cluster_size * sector_size + nr_fats*2);
      fatlength16 = cdiv ((clust16+2) * 2, sector_size);
      /* Need to recalculate number of clusters, since the unused parts of the
       * FATS and data area together could make up space for an additional,
       * not really present cluster. */
      clust16 = (fatdata - nr_fats*fatlength16)/bs.cluster_size;
      maxclust16 = (fatlength16 * sector_size) / 2;
      if (maxclust16 > MAX_CLUST_16)
	maxclust16 = MAX_CLUST_16;
      if (verbose >= 2)
	printf( "FAT16: #clu=%u, fatlen=%u, maxclu=%u, limit=%u\n",
		clust16, fatlength16, maxclust16, MAX_CLUST_16 );
      if (clust16 > maxclust16-2) {
	if (verbose >= 2)
	  printf( "FAT16: too much clusters\n" );
	clust16 = 0;
      }
      /* The < 4078 avoids that the filesystem will be misdetected as having a
       * 12 bit FAT. */
      if (clust16 < FAT12_THRESHOLD && !(size_fat_by_user && size_fat == 16)) {
	if (verbose >= 2)
	  printf( clust16 < FAT12_THRESHOLD ?
		  "FAT16: would be misdetected as FAT12\n" :
		  "FAT16: too much clusters\n" );
	clust16 = 0;
      }

      clust32 = ((long long) fatdata *sector_size + nr_fats*8) /
	((int) bs.cluster_size * sector_size + nr_fats*4);
      fatlength32 = cdiv ((clust32+2) * 4, sector_size);
      /* Need to recalculate number of clusters, since the unused parts of the
       * FATS and data area together could make up space for an additional,
       * not really present cluster. */
      clust32 = (fatdata - nr_fats*fatlength32)/bs.cluster_size;
      maxclust32 = (fatlength32 * sector_size) / 4;
      if (maxclust32 > MAX_CLUST_32)
	maxclust32 = MAX_CLUST_32;
      if (verbose >= 2)
	printf( "FAT32: #clu=%u, fatlen=%u, maxclu=%u, limit=%u\n",
		clust32, fatlength32, maxclust32, MAX_CLUST_32 );
      if (clust32 > maxclust32) {
	clust32 = 0;
	if (verbose >= 2)
	  printf( "FAT32: too much clusters\n" );
      }

      if ((clust12 && (size_fat == 0 || size_fat == 12)) ||
	  (clust16 && (size_fat == 0 || size_fat == 16)) ||
	  (clust32 && size_fat == 32))
	break;

      bs.cluster_size <<= 1;
    } while (bs.cluster_size && bs.cluster_size <= maxclustsize);

    /* Use the optimal FAT size if not specified;
     * FAT32 is (not yet) choosen automatically */
    if (!size_fat) {
	size_fat = (clust16 > clust12) ? 16 : 12;
	if (verbose >= 2)
	  printf( "Choosing %d bits for FAT\n", size_fat );
    }

    switch (size_fat) {
      case 12:
	cluster_count = clust12;
	fat_length = fatlength12;
	bs.fat_length = CT_LE_W(fatlength12);
	memcpy(vi->fs_type, MSDOS_FAT12_SIGN, 8);
	break;

      case 16:
	if (clust16 < FAT12_THRESHOLD) {
	    if (size_fat_by_user) {
		fprintf( stderr, "WARNING: Not enough clusters for a "
			 "16 bit FAT! The filesystem will be\n"
			 "misinterpreted as having a 12 bit FAT without "
			 "mount option \"fat=16\".\n" );
	    }
	    else {
		fprintf( stderr, "This filesystem has an unfortunate size. "
			 "A 12 bit FAT cannot provide\n"
			 "enough clusters, but a 16 bit FAT takes up a little "
			 "bit more space so that\n"
			 "the total number of clusters becomes less than the "
			 "threshold value for\n"
			 "distinction between 12 and 16 bit FATs.\n" );
		die( "Make the file system a bit smaller manually." );
	    }
	}
	cluster_count = clust16;
	fat_length = fatlength16;
	bs.fat_length = CT_LE_W(fatlength16);
	memcpy(vi->fs_type, MSDOS_FAT16_SIGN, 8);
	break;

      case 32:
	cluster_count = clust32;
	fat_length = fatlength32;
	bs.fat_length = CT_LE_W(0);
	bs.fat32.fat32_length = CT_LE_L(fatlength32);
	memcpy(vi->fs_type, MSDOS_FAT32_SIGN, 8);
	break;
	
      default:
	die("FAT not 12, 16 or 32 bits");
    }
  }
  else {
    unsigned clusters, maxclust;
      
    /* GEMDOS always uses a 12 bit FAT on floppies, and always a 16 bit FAT on
     * hard disks. So use 12 bit if the size of the file system suggests that
     * this fs is for a floppy disk, if the user hasn't explicitly requested a
     * size.
     */
    if (!size_fat)
      size_fat = (num_sectors == 1440 || num_sectors == 2400 ||
		  num_sectors == 2880 || num_sectors == 5760) ? 12 : 16;
    if (verbose >= 2)
      printf( "Choosing %d bits for FAT\n", size_fat );

    /* Atari format: cluster size should be 2, except explicitly requested by
     * the user, since GEMDOS doesn't like other cluster sizes very much.
     * Instead, tune the sector size for the FS to fit.
     */
    bs.cluster_size = sectors_per_cluster ? sectors_per_cluster : 2;
    if (!sector_size_set) {
      while( num_sectors > GEMDOS_MAX_SECTORS ) {
	num_sectors >>= 1;
	sector_size <<= 1;
      }
    }
    if (verbose >= 2)
      printf( "Sector size must be %d to have less than %d log. sectors\n",
	      sector_size, GEMDOS_MAX_SECTORS );

    /* Check if there are enough FAT indices for how much clusters we have */
    do {
      fatdata = num_sectors - cdiv (root_dir_entries * 32, sector_size) -
		reserved_sectors;
      /* The factor 2 below avoids cut-off errors for nr_fats == 1 and
       * size_fat == 12
       * The "2*nr_fats*size_fat/8" is for the reserved first two FAT entries
       */
      clusters = (2*((long long)fatdata*sector_size - 2*nr_fats*size_fat/8)) /
		 (2*((int)bs.cluster_size*sector_size + nr_fats*size_fat/8));
      fat_length = cdiv( (clusters+2)*size_fat/8, sector_size );
      /* Need to recalculate number of clusters, since the unused parts of the
       * FATS and data area together could make up space for an additional,
       * not really present cluster. */
      clusters = (fatdata - nr_fats*fat_length)/bs.cluster_size;
      maxclust = (fat_length*sector_size*8)/size_fat;
      if (verbose >= 2)
	printf( "ss=%d: #clu=%d, fat_len=%d, maxclu=%d\n",
		sector_size, clusters, fat_length, maxclust );
      
      /* last 10 cluster numbers are special (except FAT32: 4 high bits rsvd);
       * first two numbers are reserved */
      if (maxclust <= (size_fat == 32 ? MAX_CLUST_32 : (1<<size_fat)-0x10) &&
	  clusters <= maxclust-2)
	break;
      if (verbose >= 2)
	printf( clusters > maxclust-2 ?
		"Too many clusters\n" : "FAT too big\n" );

      /* need to increment sector_size once more to  */
      if (sector_size_set)
	  die( "With this sector size, the maximum number of FAT entries "
	       "would be exceeded." );
      num_sectors >>= 1;
      sector_size <<= 1;
    } while( sector_size <= GEMDOS_MAX_SECTOR_SIZE );
    
    if (sector_size > GEMDOS_MAX_SECTOR_SIZE)
      die( "Would need a sector size > 16k, which GEMDOS can't work with");

    cluster_count = clusters;
    if (size_fat != 32)
	bs.fat_length = CT_LE_W(fat_length);
    else {
	bs.fat_length = 0;
	bs.fat32.fat32_length = CT_LE_L(fat_length);
    }
  }

  bs.sector_size[0] = (char) (sector_size & 0x00ff);
  bs.sector_size[1] = (char) ((sector_size & 0xff00) >> 8);

  if (size_fat == 32)
    {
      /* set up additional FAT32 fields */
      bs.fat32.flags = CT_LE_W(0);
      bs.fat32.version[0] = 0;
      bs.fat32.version[1] = 0;
      bs.fat32.root_cluster = CT_LE_L(2);
      bs.fat32.info_sector = CT_LE_W(1);
      if (!backup_boot)
	backup_boot = (reserved_sectors >= 7) ? 6 :
		      (reserved_sectors >= 2) ? reserved_sectors-1 : 0;
      else
	{
	  if (backup_boot == 1)
	    die("Backup boot sector must be after sector 1");
	  else if (backup_boot >= reserved_sectors)
	    die("Backup boot sector must be a reserved sector");
	}
      if (verbose >= 2)
	printf( "Using sector %d as backup boot sector (0 = none)\n",
		backup_boot );
      bs.fat32.backup_boot = CT_LE_W(backup_boot);
      memset( &bs.fat32.reserved2, 0, sizeof(bs.fat32.reserved2) );
    }
  
  if (atari_format) {
      /* Just some consistency checks */
      if (num_sectors >= GEMDOS_MAX_SECTORS)
	  die( "GEMDOS can't handle more than 65531 sectors" );
      else if (num_sectors >= OLDGEMDOS_MAX_SECTORS)
	  printf( "Warning: More than 32765 sector need TOS 1.04 "
		  "or higher.\n" );
  }
  if (num_sectors >= 65536)
    {
      bs.sectors[0] = (char) 0;
      bs.sectors[1] = (char) 0;
      bs.total_sect = CT_LE_L(num_sectors);
    }
  else
    {
      bs.sectors[0] = (char) (num_sectors & 0x00ff);
      bs.sectors[1] = (char) ((num_sectors & 0xff00) >> 8);
      if (!atari_format)
	  bs.total_sect = CT_LE_L(0);
    }

  if (!atari_format)
    vi->ext_boot_sign = MSDOS_EXT_SIGN;

  if (!cluster_count)
    {
      if (sectors_per_cluster)	/* If yes, die if we'd spec'd sectors per cluster */
	die ("Too many clusters for file system - try more sectors per cluster");
      else
	die ("Attempting to create a too large file system");
    }

  
  /* The two following vars are in hard sectors, i.e. 512 byte sectors! */
  start_data_sector = (reserved_sectors + nr_fats * fat_length) *
		      (sector_size/HARD_SECTOR_SIZE);
  start_data_block = (start_data_sector + SECTORS_PER_BLOCK - 1) /
		     SECTORS_PER_BLOCK;

  if (blocks < start_data_block + 32)	/* Arbitrary undersize file system! */
    die ("Too few blocks for viable file system");

  if (verbose)
    {
      printf("%s has %d head%s and %d sector%s per track,\n",
	     "<rawpod device>", CF_LE_W(bs.heads), (CF_LE_W(bs.heads) != 1) ? "s" : "",
	     CF_LE_W(bs.secs_track), (CF_LE_W(bs.secs_track) != 1) ? "s" : ""); 
      printf("logical sector size is %d,\n",sector_size);
      printf("using 0x%02x media descriptor, with %d sectors;\n",
	     (int) (bs.media), num_sectors);
      printf("file system has %d %d-bit FAT%s and %d sector%s per cluster.\n",
	     (int) (bs.fats), size_fat, (bs.fats != 1) ? "s" : "",
	     (int) (bs.cluster_size), (bs.cluster_size != 1) ? "s" : "");
      printf ("FAT size is %d sector%s, and provides %d cluster%s.\n",
	      fat_length, (fat_length != 1) ? "s" : "",
	      cluster_count, (cluster_count != 1) ? "s" : "");
      if (size_fat != 32)
	printf ("Root directory contains %d slots.\n",
		(int) (bs.dir_entries[0]) + (int) (bs.dir_entries[1]) * 256);
      printf ("Volume ID is %08lx, ", volume_id &
	      (atari_format ? 0x00ffffff : 0xffffffff));
      if ( strcmp(volume_name, "           ") )
	printf("volume label %s.\n", volume_name);
      else
	printf("no volume label.\n");
    }

  /* Make the file allocation tables! */

  if ((fat = (unsigned char *) malloc (fat_length * sector_size)) == NULL)
    die ("unable to allocate space for FAT image in memory");

  memset( fat, 0, fat_length * sector_size );

  mark_FAT_cluster (0, 0xffffffff);	/* Initial fat entries */
  mark_FAT_cluster (1, 0xffffffff);
  fat[0] = (unsigned char) bs.media;	/* Put media type in first byte! */
  if (size_fat == 32) {
    /* Mark cluster 2 as EOF (used for root dir) */
    mark_FAT_cluster (2, FAT_EOF);
  }

  /* Make the root directory entries */

  size_root_dir = (size_fat == 32) ?
		  bs.cluster_size*sector_size :
		  (((int)bs.dir_entries[1]*256+(int)bs.dir_entries[0]) *
		   sizeof (struct msdos_dir_entry));
  if ((root_dir = (struct msdos_dir_entry *) malloc (size_root_dir)) == NULL)
    {
      free (fat);		/* Tidy up before we die! */
      die ("unable to allocate space for root directory in memory");
    }

  memset(root_dir, 0, size_root_dir);
  if ( memcmp(volume_name, "           ", 11) )
    {
      struct msdos_dir_entry *de = &root_dir[0];
      memcpy(de->name, volume_name, 11);
      de->attr = ATTR_VOLUME;
      ctime = localtime(&create_time);
      de->time = CT_LE_W((unsigned short)((ctime->tm_sec >> 1) +
			  (ctime->tm_min << 5) + (ctime->tm_hour << 11)));
      de->date = CT_LE_W((unsigned short)(ctime->tm_mday +
					  ((ctime->tm_mon+1) << 5) +
					  ((ctime->tm_year-80) << 9)));
      de->ctime_ms = 0;
      de->ctime = de->time;
      de->cdate = de->date;
      de->adate = de->date;
      de->starthi = CT_LE_W(0);
      de->start = CT_LE_W(0);
      de->size = CT_LE_L(0);
    }

  if (size_fat == 32) {
    /* For FAT32, create an info sector */
    struct fat32_fsinfo *info;
    
    if (!(info_sector = (u8 *)malloc( sector_size )))
      die("Out of memory");
    memset(info_sector, 0, sector_size);
    /* fsinfo structure is at offset 0x1e0 in info sector by observation */
    info = (struct fat32_fsinfo *)(info_sector + 0x1e0);

    /* Info sector magic */
    info_sector[0] = 'R';
    info_sector[1] = 'R';
    info_sector[2] = 'a';
    info_sector[3] = 'A';

    /* Magic for fsinfo structure */
    info->signature = CT_LE_L(0x61417272);
    /* We've allocated cluster 2 for the root dir. */
    info->free_clusters = CT_LE_L(cluster_count - 1);
    info->next_cluster = CT_LE_L(2);

    /* Info sector also must have boot sign */
    *(u16 *)(info_sector + 0x1fe) = CT_LE_W(BOOT_SIGN);
  }
  
  if (!(blank_sector = (char *)malloc( sector_size )))
      die( "Out of memory" );
  memset(blank_sector, 0, sector_size);
}


/* Write the new filesystem's data tables to wherever they're going to end up! */

#define error(str)				\
  do {						\
    free (fat);					\
    if (info_sector) free (info_sector);	\
    free (root_dir);				\
    die (str);					\
  } while(0)

#define seekto(pos,errstr)						\
  do {									\
    u64 __pos = (pos);							\
    if (dev->lseek (__pos, SEEK_SET) != __pos)				\
	error ("seek to " errstr " failed whilst writing tables");	\
  } while(0)

#define writebuf(buf,size,errstr)			\
  do {							\
    int __size = (size);				\
    if (dev->write (buf, __size) != __size)		\
	error ("failed whilst writing " errstr);	\
  } while(0)


static void
write_tables (void)
{
  int x;
  int fat_length;

  fat_length = (size_fat == 32) ?
	       CF_LE_L(bs.fat32.fat32_length) : CF_LE_W(bs.fat_length);

  seekto( 0, "start of device" );
  /* clear all reserved sectors */
  for( x = 0; x < reserved_sectors; ++x )
    writebuf( blank_sector, sector_size, "reserved sector" );
  /* seek back to sector 0 and write the boot sector */
  seekto( 0, "boot sector" );
  writebuf( (char *) &bs, sizeof (struct msdos_boot_sector), "boot sector" );
  /* on FAT32, write the info sector and backup boot sector */
  if (size_fat == 32)
    {
      seekto( CF_LE_W(bs.fat32.info_sector)*sector_size, "info sector" );
      writebuf( info_sector, 512, "info sector" );
      if (backup_boot != 0)
	{
	  seekto( backup_boot*sector_size, "backup boot sector" );
	  writebuf( (char *) &bs, sizeof (struct msdos_boot_sector),
		    "backup boot sector" );
	}
    }
  /* seek to start of FATS and write them all */
  seekto( reserved_sectors*sector_size, "first FAT" );
  for (x = 1; x <= nr_fats; x++)
    writebuf( fat, fat_length * sector_size, "FAT" );
  /* Write the root directory directly after the last FAT. This is the root
   * dir area on FAT12/16, and the first cluster on FAT32. */
  writebuf( (char *) root_dir, size_root_dir, "root directory" );

  if (info_sector) free( info_sector );
  free (root_dir);   /* Free up the root directory space from setup_tables */
  free (fat);  /* Free up the fat table space reserved during setup_tables */
}


/* Report the command usage and return a failure error code */

void
usage (void)
{
  fatal_error("\
Usage: mkdosfs [-A] [-c] [-C] [-v] [-I] [-l bad-block-file] [-b backup-boot-sector]\n\
       [-m boot-msg-file] [-n volume-name] [-i volume-id]\n\
       [-s sectors-per-cluster] [-S logical-sector-size] [-f number-of-FATs]\n\
       [-F fat-size] [-r root-dir-entries] [-R reserved-sectors]\n\
       /dev/name [blocks]\n");
}

/* The "main" entry point into the utility - we pick up the options and attempt to process them in some sort of sensible
   way.  In the event that some/all of the options are invalid we need to tell the user so that something can be done! */

int
CreateFATFilesystem (VFS::Device *d)
{
  int c;
  char *tmp;
  char *listfile = NULL;
  FILE *msgfile;
  int i = 0, pos, ch;
  int create = 0;

  dev = d;
  
  time(&create_time);
  volume_id = (long)create_time;	/* Default volume ID = creation time */
  
  printf ("%s " VERSION " (" VERSION_DATE ")\n",
	   program_name);

  size_fat = 32;
  size_fat_by_user = 1;

  if (setjmp (error_out))
      return 1;

  blocks = count_blocks();

  establish_params();	
                                /* Establish the media parameters */

  setup_tables();		/* Establish the file system tables */

  write_tables();		/* Write the file system tables away! */

  return 0;			/* Terminate with no errors! */
}

#ifdef TEST
#include "partition.h"
int main (int argc, char **argv) 
{
    if (argc != 2) {
        fprintf (stderr, "Usage: mkdosfs <path>\n");
        return 1;
    }

    VFS::File *file = new LocalFile (argv[1]);
    if (file->error()) {
        fprintf (stderr, "%s: %s\n", argv[1], strerror (file->error()));
    }
    VFS::Device *device = new VFS::DeviceFile (file);
    printf ("Are you SURE you want to create a FAT32 filesystem on %s?\n"
            "ALL DATA WILL BE ERASED.\n"
            "Press Enter to continue or Ctrl+C to cancel...\n", argv[1]);
    getchar();
    CreateFATFilesystem (device);
    delete device;
    delete file;
}
#endif

/* That's All Folks */
/* Local Variables: */
/* tab-width: 8     */
/* End:             */
