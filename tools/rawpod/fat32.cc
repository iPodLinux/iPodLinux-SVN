/* Rawpod simple FAT32 support - R/O, no LFN, etc.
 * Ported from ipodloader2 on 2005-01-23.
 * Original file copyright (C) 2005 James Jacobsson.
 * Modifications copyright (C) 2006 Joshua Oreman.
 * Released under the GPL. NO WARRANTY. NONE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vfs.h"
#include "fat32.h"

u8 *clusterBuffer = NULL;

/*
 * This routine sucks, and would benefit greately from having
 * the FAT cached in RAM.. But.. The FAT for a 4GB Nano comes
 * out to about 4MB, so that wouldn't be very feasible for
 * larger capacity drives..
 *
 * Block-caching would help a lot as well
 *
 */
s32 FATFile::findnextcluster(u32 prev) {
  u64 offset;
  u32 ret;

  offset = ((u64)_fat->number_of_reserved_sectors << 9) + prev * 4;

  _device->lseek (offset, SEEK_SET);
  _device->read (&ret, 4);

  return(ret);
}

int FATFile::findfile(u32 start, const char *fname) {
  u32 done,i,j,new_offset;
  u64 dir_lba;
  u8  buffer[512];
  char filepart[32];
  const char *next;
  int match = 1;

  /* "start" is in clusters, so lets translate to LBA */
  dir_lba  = _fat->number_of_reserved_sectors + (_fat->number_of_fats * _fat->sectors_per_fat);
  dir_lba += (u64)(start - 2) * _fat->sectors_per_cluster;

  next = strchr( fname,'/' );

  done = 0;
  for(i=0;i<_fat->sectors_per_cluster;i++) {
    _device->lseek ((dir_lba + i) << 9, SEEK_SET);
    _device->read (buffer, 512);
    
    for(j=0;j<16;j++) { /* 16 dirents per sector */
      strncpy (filepart, fname, 31);
      filepart[31] = 0;
      if (strchr (filepart, '/')) *strchr (filepart, '/') = 0;

      char *ext = strchr (filepart, '.');
      int extloc = ext? (ext - filepart) : strlen (filepart);

      match = !strncasecmp ((char *)buffer + (j << 5), filepart, extloc);
      if (ext)
        match = match && !strncasecmp ((char *)buffer + (j << 5) + 8,
                                       ext + 1, strlen (ext + 1));

      if(        buffer[j*32] == 0) { /* EndOfDir */
	return -ENOENT;
      } else if( ((buffer[j*32+0xB] & 0x1F) == 0) && (buffer[j*32] != 0xE5) ) { /* Normal file */
        if (match && (next == NULL)) {
	  new_offset  = (buffer[j*32+0x15]<<8) + buffer[j*32+0x14]; 
	  new_offset  = new_offset << 16;
	  new_offset |= (buffer[j*32+0x1B]<<8) + buffer[j*32+0x1A]; 

	  cluster  = new_offset;
	  opened   = 1;
	  position = 0;
	  length   = (buffer[j*32+0x1F]<<8) + buffer[j*32+0x1E]; 
	  length <<= 16;
	  length  |= (buffer[j*32+0x1D]<<8) + buffer[j*32+0x1C]; 

	  return 0;
	}
      } else if( buffer[j*32+0xB] & (1<<4)) { /* Directory */
	if (match) {
          if (!next) return -EISDIR;
	  new_offset  = (buffer[j*32+0x15]<<8) + buffer[j*32+0x14]; 
	  new_offset  = new_offset << 16;
	  new_offset |= (buffer[j*32+0x1B]<<8) + buffer[j*32+0x1A]; 

	  return findfile (new_offset, next+1);

	} else {
	}
      } else {
      }
    }
  }

  if (match) return -ENOTDIR;
  return -ENOENT;
}

FATFile::FATFile (FATFS *fat,const char *fname) {
  _fat    = fat;
  _device = _fat->_device;
  _err    = findfile (_fat->root_dir_first_cluster,fname);
}

int FATFile::read(void *ptr, int size) {
  u32 read,toRead,clusterNum,clust,i;
  u64 lba;
  u32 offsetInCluster, toReadInCluster;

  read   = 0;
  toRead = size;
  if( toRead > length - position ) {
    toRead = length - position;
  }

  /*
   * FFWD to the cluster we're positioned at
   * !!! Huge speedup if we cache this for each file
   * (Hmm.. With the addition of the block-cache, this isn't as big of an issue, but it's still an issue though)
   */
  clusterNum = position / (_fat->sectors_per_cluster * _fat->bytes_per_sector);
  clust = cluster;

  for(i=0;i<clusterNum;i++) {
    clust = findnextcluster( clust );
  }
  
  offsetInCluster = position % (_fat->sectors_per_cluster * _fat->bytes_per_sector);

  /* Calculate LBA for the cluster */
  lba  = _fat->number_of_reserved_sectors + (_fat->number_of_fats * _fat->sectors_per_fat);
  lba += (u64)(clust - 2) * (u64)_fat->sectors_per_cluster;

  toReadInCluster = (_fat->sectors_per_cluster * _fat->bytes_per_sector) - offsetInCluster;

  if ((_err = _device->lseek (lba << 9, SEEK_SET)) < 0)
      return _err;
  if ((_err = _device->read (clusterBuffer, toReadInCluster)) < 0)
      return _err;

  if( toReadInCluster > toRead ) toReadInCluster = toRead; 

  memcpy( (u8*)ptr + read, clusterBuffer + offsetInCluster, toReadInCluster );

  read += toReadInCluster;

  /* Loops through all complete clusters */
  while(read < ((toRead / _fat->clustersize)*_fat->clustersize) ) {
    clust = findnextcluster( clust );
    lba  = _fat->number_of_reserved_sectors + (_fat->number_of_fats * _fat->sectors_per_fat);
    lba += (u64)(clust - 2) * (u64)_fat->sectors_per_cluster;
    if ((_err = _device->lseek (lba << 9, SEEK_SET)) < 0)
        return _err;
    if ((_err = _device->read (clusterBuffer, _fat->sectors_per_cluster << 9)) < 0)
        return _err;

    memcpy( (u8*)ptr + read, clusterBuffer, _fat->clustersize );

    read += _fat->clustersize;
  }

  /* And the final bytes in the last cluster of the file */
  if( read < toRead ) {
    clust = findnextcluster( clust );
    lba  = _fat->number_of_reserved_sectors + (_fat->number_of_fats * _fat->sectors_per_fat);
    lba += (u64)(clust - 2) * (u64)_fat->sectors_per_cluster;
    if ((_err = _device->lseek (lba << 9, SEEK_SET)) < 0)
        return _err;
    if ((_err = _device->read (clusterBuffer, _fat->sectors_per_cluster << 9)) < 0)
        return _err;
    
    memcpy( (u8*)ptr + read, clusterBuffer,toRead - read );

    read = toRead;
  }

  position += toRead;

  return read;
}

s64 FATFile::lseek(s64 offset,int whence) {
  switch(whence) {
  case SEEK_SET:
    position = (offset < 0)? 0 : offset;
    break;
  case SEEK_CUR:
    if (offset < -position) position = 0;
    else position += offset;
    break;
  case SEEK_END:
    if (offset > length) position = 0;
    else position = length - offset;
    break;
  }

  if (position > length) position = length - 1;
  return position;
}

FATFS::FATFS (VFS::Device *d)
    : VFS::Filesystem (d) 
{}

int FATFS::init() {
  u8 buff[512];

  /* Verify that this is a FAT32 partition */
  _device->lseek (0, SEEK_SET);
  _device->read (buff, 512);
  if( (buff[510] != 0x55) || (buff[511] != 0xAA) ) {
    printf ("FAT-fs: Not a valid FAT filesystem\n");
    return -EINVAL;
  }

  /* Clear all filehandles */
  bytes_per_sector           = (buff[0xC] << 8) | buff[0xB];
  sectors_per_cluster        =  buff[0xD];
  number_of_reserved_sectors = (buff[0xF] << 8) | buff[0xE];
  number_of_fats             =  buff[0x10];
  sectors_per_fat            = (buff[0x27] << 24) | (buff[0x26] << 16) | (buff[0x25] << 8) | buff[0x24];
  root_dir_first_cluster     = (buff[0x2F] << 24) | (buff[0x2E] << 16) | (buff[0x2D] << 8) | buff[0x2C];
  
  clustersize = bytes_per_sector * sectors_per_cluster;

  if( clusterBuffer == NULL ) {
    clusterBuffer = (u8*)malloc( clustersize );
  }

  return 0;
}

int FATFS::probe (VFS::Device *dev) 
{
  u8 buf[512];
  
  dev->lseek (0, SEEK_SET);
  dev->read (buf, 512);
  return (buf[510] == 0x55 && buf[511] == 0xAA);
}

VFS::File *FATFS::open (const char *path, int flags) 
{
    (void)flags;
    while (*path == '/') path++;
    return new FATFile (this, path);
}
