#include "bootloader.h"
#include "ata2.h"
#include "vfs.h"
#include "fat32.h"
#include "minilibc.h"

#define MAX_HANDLES 10

static filesystem myfs;

typedef struct {
  uint32 offset;
  
  uint16 bytes_per_sector;
  uint8  sectors_per_cluster;
  uint16 number_of_reserved_sectors;
  uint8  number_of_fats;
  uint32 sectors_per_fat;
  uint32 root_dir_first_cluster;
  uint32 data_area_offset;

  uint32 clustersize;

  fat32_file *filehandles[MAX_HANDLES];
  uint32      numHandles;
} fat_t;

fat_t fat;

static uint8 *clusterBuffer = NULL;
static uint8 *gBlkBuf = 0;

/*
 * This routine sucks, and would benefit greately from having
 * the FAT cached in RAM.. But.. The FAT for a 4GB Nano comes
 * out to about 4MB, so that wouldn't be very feasible for
 * larger capacity drives..
 *
 * Block-caching would help a lot as well
 *
 */
uint32 xxi = 0;
static uint32 fat32_findnextcluster(uint32 prev) {
  uint32 block,offset,ret;

  // this calculates the FAT block number
  offset = ((fat.offset+fat.number_of_reserved_sectors)*512) + prev * 4;
  block  = offset / fat.bytes_per_sector;
  offset = offset % fat.bytes_per_sector;

  ata_readblocks( gBlkBuf, block, 1 );

  ret = ((uint32*)gBlkBuf)[offset/4];

  return(ret);
}

static uint32 calc_lba (uint32 start, int isRootDir) {
  uint32 lba;
  lba  = fat.offset + fat.number_of_reserved_sectors + (fat.number_of_fats * fat.sectors_per_fat);
  lba += (start - 2) * fat.sectors_per_cluster + (isRootDir?0:fat.data_area_offset);
  return lba;
}

static fat32_file *fat32_findfile(uint32 start, int isRoot, char *fname) {
  uint32 done,i,j,dir_lba,new_offset;
  uint8  buffer[512];
  char *next;
  fat32_file *fileptr;

  /* "start" is in clusters, so lets translate to LBA */
  dir_lba = calc_lba (start, isRoot);

  next = mlc_strchr( fname,'/' );

  done = 0;
  for(i=0;i<fat.sectors_per_cluster;i++) {
    ata_readblocks( buffer, dir_lba+i,1 );
    
    for(j=0;j<16;j++) { /* 16 dirents per sector */
      if (         buffer[j*32] == 0) { /* EndOfDir */
        return(NULL);
      } else if( ((buffer[j*32+0xB] & 0x1F) == 0) && (buffer[j*32] != 0xE5) ) { /* Normal file */

        if( (mlc_strncasecmp( (char*)&buffer[j*32], fname, mlc_strchr(fname,'.')-fname ) == 0) &&
            (mlc_strncasecmp( (char*)&buffer[j*32+8], mlc_strchr(fname,'.')+1, 3 ) == 0) &&
            (next == NULL) ) {
          new_offset  = (buffer[j*32+0x15]<<8) + buffer[j*32+0x14]; 
          new_offset  = new_offset << 16;
          new_offset |= (buffer[j*32+0x1B]<<8) + buffer[j*32+0x1A]; 

          fileptr = (fat32_file*)mlc_malloc( sizeof(fat32_file) );
          fileptr->cluster  = new_offset;
          fileptr->opened   = 1;
          fileptr->position = 0;
          fileptr->length   = (buffer[j*32+0x1F]<<8) + buffer[j*32+0x1E]; 
          fileptr->length   = fileptr->length << 16;
          fileptr->length  |= (buffer[j*32+0x1D]<<8) + buffer[j*32+0x1C]; 

          return(fileptr);
        }
      } else if( buffer[j*32+0xB] & (1<<4)) { /* Directory */
        if( mlc_strncasecmp( (char*)&buffer[j*32], fname, mlc_strchr(fname,'/')-fname ) == 0 ) {

          new_offset  = (buffer[j*32+0x15]<<8) + buffer[j*32+0x14]; 
          new_offset  = new_offset << 16;
          new_offset |= (buffer[j*32+0x1B]<<8) + buffer[j*32+0x1A]; 

          return( fat32_findfile( new_offset, 0, next+1 ) );

        } else {
        }
      } else {
      }
    }
  }

  return(NULL);
}

static int fat32_open(void *fsdata,char *fname) {
  fat_t      *fs;
  fat32_file *file;

  fs = (fat_t*)fsdata;

  file = fat32_findfile(fs->root_dir_first_cluster,1,fname);

  if(file==NULL) {
    //mlc_printf("%s not found\n", fname);
    return(-1);
  }

  if(file != NULL) {
    if( fs->numHandles < MAX_HANDLES ) {
      fs->filehandles[fs->numHandles++] = file;
    } else return(-1);
  }

  return(fs->numHandles-1);
}

static void fat32_close (void *fsdata, int fd)
{
  fat_t *fs = (fat_t*)fsdata;
  if (fd == fs->numHandles-1) {
    --fs->numHandles;
  }
}

static size_t fat32_read(void *fsdata,void *ptr,size_t size,size_t nmemb,int fd) {
  uint32 read,toRead,lba,clusterNum,cluster,i;
  uint32 offsetInCluster, toReadInCluster;
  fat_t *fs;

  fs = (fat_t*)fsdata;

  read   = 0;
  toRead = size*nmemb;
  if( toRead > (fs->filehandles[fd]->length + fs->filehandles[fd]->position) ) {
    toRead = fs->filehandles[fd]->length + fs->filehandles[fd]->position;
  }

  /*
   * FFWD to the cluster we're positioned at
   * !!! Huge speedup if we cache this for each file
   * (Hmm.. With the addition of the block-cache, this isn't as big of an issue, but it's still an issue though)
   */
  clusterNum = fs->filehandles[fd]->position / (fs->sectors_per_cluster * fs->bytes_per_sector);
  cluster = fs->filehandles[fd]->cluster;

  for(i=0;i<clusterNum;i++) {
    cluster = fat32_findnextcluster( cluster );
  }
  
  offsetInCluster = fs->filehandles[fd]->position % (fs->sectors_per_cluster * fs->bytes_per_sector);

  /* Calculate LBA for the cluster */
  lba = calc_lba (cluster, 0);

  toReadInCluster = (fs->sectors_per_cluster * fs->bytes_per_sector) - offsetInCluster;

  ata_readblocks( clusterBuffer, lba, toReadInCluster / fs->bytes_per_sector );

  if( toReadInCluster > toRead ) toReadInCluster = toRead; 

  mlc_memcpy( (uint8*)ptr + read, clusterBuffer + offsetInCluster, toReadInCluster );

  read += toReadInCluster;

  /* Loops through all complete clusters */
  while(read < ((toRead / fs->clustersize)*fs->clustersize) ) {
    cluster = fat32_findnextcluster( cluster );
    lba = calc_lba (cluster, 0);
    ata_readblocks( clusterBuffer, lba, fs->sectors_per_cluster );

    mlc_memcpy( (uint8*)ptr + read, clusterBuffer,fs->clustersize );

    read += fs->clustersize;
  }

  /* And the final bytes in the last cluster of the file */
  if( read < toRead ) {
    cluster = fat32_findnextcluster( cluster );
    lba = calc_lba (cluster, 0);
    ata_readblocks( clusterBuffer, lba, fs->sectors_per_cluster );
    
    mlc_memcpy( (uint8*)ptr + read, clusterBuffer,toRead - read );

    read = toRead;
  }

  fs->filehandles[fd]->position += toRead;

  return(read / size);
}

static long fat32_tell(void *fsdata,int fd) {
  fat_t *fs;

  fs = (fat_t*)fsdata;

  return( fs->filehandles[fd]->position );
}

static int fat32_seek(void *fsdata,int fd,long offset,int whence) {
  fat_t *fs;

  fs = (fat_t*)fsdata;
  
  switch(whence) {
  case VFS_SEEK_CUR:
    offset += fs->filehandles[fd]->position;
    break;
  case VFS_SEEK_SET:
    break;
  case VFS_SEEK_END:
    offset += fs->filehandles[fd]->length;
    break;
  default:
    return -2;
  }

  if( offset < 0 || offset > fs->filehandles[fd]->length ) {
    return -1;
  }

  fs->filehandles[fd]->position = offset;
  return 0;
}


void fat32_newfs(uint8 part,uint32 offset) {

  gBlkBuf = (uint8*)mlc_malloc(512);

  /* Verify that this is a FAT32 partition */
  ata_readblocks( gBlkBuf, offset,1 );
  if( (gBlkBuf[510] != 0x55) || (gBlkBuf[511] != 0xAA) ) {
    mlc_printf("Not valid FAT superblock\n");
    mlc_show_critical_error();
    return;
  }

  /* Clear all filehandles */
  fat.numHandles = 0;

  fat.offset =  offset;

  fat.bytes_per_sector           = (gBlkBuf[0xC] << 8) | gBlkBuf[0xB];
  fat.sectors_per_cluster        =  gBlkBuf[0xD];
  fat.number_of_reserved_sectors = (gBlkBuf[0xF] << 8) | gBlkBuf[0xE];
  fat.number_of_fats             =  gBlkBuf[0x10];
  if (mlc_strncmp ("FAT16   ", (char*)&gBlkBuf[54], 8) == 0) {
    // FAT16 partition
    fat.sectors_per_fat            = (gBlkBuf[23] << 8) | gBlkBuf[22];
    fat.root_dir_first_cluster     = 2;
    fat.data_area_offset           = (((gBlkBuf[18] << 8) | gBlkBuf[17]) * 32 + 511) / 512; // root directory size
  } else if (mlc_strncmp ("FAT32   ", (char*)&gBlkBuf[82], 8) == 0) {
    // FAT32 partition
    fat.sectors_per_fat            = (gBlkBuf[0x27] << 24) | (gBlkBuf[0x26] << 16) | (gBlkBuf[0x25] << 8) | gBlkBuf[0x24];
    fat.root_dir_first_cluster     = (gBlkBuf[0x2F] << 24) | (gBlkBuf[0x2E] << 16) | (gBlkBuf[0x2D] << 8) | gBlkBuf[0x2C];
    fat.data_area_offset           = 0;
  } else {
    mlc_printf("Neither FAT16 nor FAT32\n");
    mlc_show_critical_error();
    return;
  }
  
  fat.clustersize = fat.bytes_per_sector * fat.sectors_per_cluster;

  if( clusterBuffer == NULL ) {
    clusterBuffer = (uint8*)mlc_malloc( fat.clustersize );
  }

  myfs.open   = fat32_open;
  myfs.close  = fat32_close;
  myfs.tell   = fat32_tell;
  myfs.seek   = fat32_seek;
  myfs.read   = fat32_read;
  myfs.getinfo= 0;
  myfs.fsdata = (void*)&fat;
  myfs.partnum = part;

  vfs_registerfs( &myfs);
}
