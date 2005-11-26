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

  uint32 clustersize;

  fat32_file *filehandles[MAX_HANDLES];
  uint32      numHandles;
} fat_t;

fat_t fat;

uint8 *clusterBuffer = NULL;

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
  uint8  tmpBuff[512];

  offset = ((fat.offset+fat.number_of_reserved_sectors)*512) + prev * 4;
  block  = offset / fat.bytes_per_sector;
  offset = offset % fat.bytes_per_sector;
  //mlc_printf("FF %u/%u (%u)\n",block,offset,xxi++);

  ata_readblocks( tmpBuff, block, 1 );

  ret = ((uint32*)tmpBuff)[offset/4];

  return(ret);
}

static fat32_file *fat32_findfile(uint32 start, char *fname) {
  uint32 done,i,j,dir_lba,new_offset;
  uint8  buffer[512];
  char *next;
  fat32_file *fileptr;

  // "start" is in clusters, so lets translate to LBA
  dir_lba  = fat.offset + fat.number_of_reserved_sectors + (fat.number_of_fats * fat.sectors_per_fat);
  dir_lba += (start - 2) * fat.sectors_per_cluster;

  next = mlc_strchr( fname,'/' );

  //printf("--->findfile (LBA=%u,str=%s)<--\n",dir_lba,fname);
  // Find-in-dir
  done = 0;
  for(i=0;i<fat.sectors_per_cluster;i++) {
    //printf("  Reading block %u\n",dir_lba);
    ata_readblocks( buffer, dir_lba+i,1 );
    
    for(j=0;j<16;j++) { // 16 dirents per sector
      if(        buffer[j*32] == 0) { // EndOfDir
	//printf("--->end of findfile<--\n");
	return(NULL);
      } else if( ((buffer[j*32+0xB] & 0x1F) == 0) && (buffer[j*32] != 0xE5) ) { // Normal file

	if( (mlc_strncmp( &buffer[j*32], fname, mlc_strchr(fname,'.')-fname ) == 0) &&
	    (mlc_strncmp( &buffer[j*32+8], mlc_strchr(fname,'.')+1, 3 ) == 0) &&
	    (next == NULL) ) {
	  new_offset  = (buffer[j*32+0x15]<<8) + buffer[j*32+0x14]; 
	  new_offset  = new_offset << 16;
	  new_offset |= (buffer[j*32+0x1B]<<8) + buffer[j*32+0x1A]; 


	  //mlc_printf("YYYAAEEE!!! ");
	  fileptr = (fat32_file*)mlc_malloc( sizeof(fat32_file) );
	  fileptr->cluster  = new_offset;
	  fileptr->opened   = 1;
	  fileptr->position = 0;
	  fileptr->length   = (buffer[j*32+0x1F]<<8) + buffer[j*32+0x1E]; 
	  fileptr->length   = fileptr->length << 16;
	  fileptr->length  |= (buffer[j*32+0x1D]<<8) + buffer[j*32+0x1C]; 

	  return(fileptr);
	}
	//mlc_printf("File: %s\n",&buffer[j*32]);
      } else if( buffer[j*32+0xB] & (1<<4)) { // Directory
	if( mlc_strncmp( &buffer[j*32], fname, mlc_strchr(fname,'/')-fname ) == 0 ) {


	  //mlc_printf("MATCH ");
	  //mlc_printf("Dir : %11s\n",&buffer[j*32]);

	  new_offset  = (buffer[j*32+0x15]<<8) + buffer[j*32+0x14]; 
	  new_offset  = new_offset << 16;
	  new_offset |= (buffer[j*32+0x1B]<<8) + buffer[j*32+0x1A]; 

	  return( fat32_findfile( new_offset, next+1 ) );

	} else {
	  //mlc_printf("Dir : %11s\n",&buffer[j*32]);
	}
      } else {
	//printf("Unknown: %11s (0x%2X)\n",&buffer[j*32],buffer[j*32+0xB]);
      }
    }
  }

  //printf("--->end of findfile<--\n");
  return(NULL);
}

int fat32_open(void *fsdata,char *fname) {
  fat_t      *fs;
  fat32_file *file;

  fs = (fat_t*)fsdata;

  //printf("FAT32_Open(%s)\n",fname);

  file = fat32_findfile(fs->root_dir_first_cluster,fname);

  if(file==NULL) mlc_printf("Couldnt find file\n");

  if(file != NULL) {
    if( fs->numHandles < MAX_HANDLES ) {
      fs->filehandles[fs->numHandles++] = file;
    } else return(-1);
  }

  return(0);
}

size_t fat32_read(void *fsdata,void *ptr,size_t size,size_t nmemb,int fd) {
  uint32 read,toRead,lba,clusterNum,cluster,i;
  uint32 offsetInCluster, toReadInCluster;
  fat_t *fs;

  fs = (fat_t*)fsdata;

  read   = 0;
  toRead = size*nmemb;
  if( toRead > (fs->filehandles[fd]->length + fs->filehandles[fd]->position) ) {
    toRead = fs->filehandles[fd]->length + fs->filehandles[fd]->position;
  }

  // FFWD to the cluster we're positioned at
  // !!! Huge speedup if we cache this for each file
  clusterNum = fs->filehandles[fd]->position / (fs->sectors_per_cluster * fs->bytes_per_sector);
  //printf("FFWD %u clusters\n",clusterNum);
  cluster = fs->filehandles[fd]->cluster;

  for(i=0;i<clusterNum;i++) {
    cluster = fat32_findnextcluster( cluster );
  }
  
  // "cluster" should now point to the cluster where our fileposition is within
  offsetInCluster = fs->filehandles[fd]->position % (fs->sectors_per_cluster * fs->bytes_per_sector);

  //printf("OFfset in cluster: %u\n",offsetInCluster);

  // Calculate LBA for the cluster
  lba  = fs->offset + fs->number_of_reserved_sectors + (fs->number_of_fats * fs->sectors_per_fat);
  lba += (cluster - 2) * fs->sectors_per_cluster;

  toReadInCluster = (fs->sectors_per_cluster * fs->bytes_per_sector) - offsetInCluster;

  //printf("Reading %u blocks\n",toReadInCluster / fs->bytes_per_sector );
  ata_readblocks( clusterBuffer, lba, toReadInCluster / fs->bytes_per_sector );

  //printf("ToRead: %u\n",toRead);
  if( toReadInCluster > toRead ) toReadInCluster = toRead; 

  //printf("Copying %u bytes\n",toReadInCluster);
  mlc_memcpy( ptr + read, clusterBuffer + offsetInCluster, toReadInCluster );

  read += toReadInCluster;

  //return(0);

  //printf("Read=%u\n",read);
  //printf("rem =%u\n",((toRead / fs->clustersize)*fs->clustersize) );

  // Loops through all complete clusters
  while(read < ((toRead / fs->clustersize)*fs->clustersize) ) {
    cluster = fat32_findnextcluster( cluster );
    lba  = fs->offset + fs->number_of_reserved_sectors + (fs->number_of_fats * fs->sectors_per_fat);
    lba += (cluster - 2) * fs->sectors_per_cluster;
    ata_readblocks( clusterBuffer, lba, fs->sectors_per_cluster );

    mlc_memcpy( ptr + read, clusterBuffer,fs->clustersize );

    read += fs->clustersize;
  }

  // And the final bytes in the last cluster of the file
  if( read < toRead ) {
    cluster = fat32_findnextcluster( cluster );
    lba  = fs->offset + fs->number_of_reserved_sectors + (fs->number_of_fats * fs->sectors_per_fat);
    lba += (cluster - 2) * fs->sectors_per_cluster;
    ata_readblocks( clusterBuffer, lba, fs->sectors_per_cluster );
    
    mlc_memcpy( ptr + read, clusterBuffer,toRead - read );

    read = toRead;
  }

  return(read / size);
}

long fat32_tell(void *fsdata,int fd) {
  fat_t *fs;

  fs = (fat_t*)fsdata;

  return( fs->filehandles[fd]->position );
}

int fat32_seek(void *fsdata,int fd,long offset,int whence) {
  fat_t *fs;

  fs = (fat_t*)fsdata;
  
  switch(whence) {
  case VFS_SEEK_SET:
    if( fs->filehandles[fd]->length > offset ) {
      fs->filehandles[fd]->position = offset;
      return(0);
    } else {
      return(-1);
    }
    break;
  case VFS_SEEK_END:
    if( fs->filehandles[fd]->length > offset ) {
      fs->filehandles[fd]->position = fs->filehandles[fd]->length - offset;
      return(0);
    } else {
      return(-1);
    }
    break;
  default:
    mlc_printf("NOOOTT IMPLEMENTED\n");
    for(;;);
    return(-1);
  }
  //printf("fat32_seek()\n");

  return(0);
}


void fat32_newfs(uint32 offset) {
  uint8 buff[512];
  //uint32 fat_begin_lba;
  //uint32 cluster_begin_lba;
  //uint32 rootdir_lba;
  //uint32 tmp;

  // Verify that this is a FAT32 partition
  ata_readblocks( buff, offset,1 );
  if( (buff[510] != 0x55) || (buff[511] != 0xAA) ) {
    mlc_printf("Not valid FAT32 superblock\n");
    return;
  }

  // Clear all filehandles
  fat.numHandles = 0;

  fat.offset =  offset;

  fat.bytes_per_sector           = (buff[0xC] << 8) | buff[0xB];
  fat.sectors_per_cluster        =  buff[0xD];
  fat.number_of_reserved_sectors = (buff[0xF] << 8) | buff[0xE];
  fat.number_of_fats             =  buff[0x10];
  fat.sectors_per_fat            = (buff[0x27] << 24) | (buff[0x26] << 16) | (buff[0x25] << 8) | buff[0x24];
  fat.root_dir_first_cluster     = (buff[0x2F] << 24) | (buff[0x2E] << 16) | (buff[0x2D] << 8) | buff[0x2C];
  
  fat.clustersize = fat.bytes_per_sector * fat.sectors_per_cluster;

  if( clusterBuffer == NULL ) {
    clusterBuffer = (uint8*)mlc_malloc( fat.clustersize );
  }

  /*
  printf(" --[ FAT32 Data ]--\n");
  printf("BPB_BytsPerSec: %u\n",fat.bytes_per_sector);
  printf("BPB_SecPerClus: %u\n",fat.sectors_per_cluster);
  printf("BPB_RsvdSecCnt: 0x%x\n",fat.number_of_reserved_sectors);
  printf("BPB_NumFATs   : %u\n",fat.number_of_fats);
  printf("BPB_FATSz32   : %u\n",fat.sectors_per_fat);
  printf("BPB_RootClus  : 0x%x\n",fat.root_dir_first_cluster);
  
  fat_begin_lba     = offset + fat.number_of_reserved_sectors;
  cluster_begin_lba = fat_begin_lba + (fat.number_of_fats * fat.sectors_per_fat);
  rootdir_lba  = cluster_begin_lba + (fat.root_dir_first_cluster - 2) * fat.sectors_per_cluster;
  
  printf("RootDir LBA: %u\n",rootdir_lba);
  */
  //tmp = fat32_findfile(fat.root_dir_first_cluster,"NOTES      /HELLO      ");


  //printf("Found file at cluster %u\n",tmp);


  myfs.open   = fat32_open;
  myfs.tell   = fat32_tell;
  myfs.seek   = fat32_seek;
  myfs.read   = fat32_read;
  myfs.fsdata = (void*)&fat;


  vfs_registerfs( &myfs);
}
