#include "bootloader.h"
#include "ata2.h"
#include "vfs.h"
#include "fwfs.h"
#include "minilibc.h"

#define MAX_HANDLES 10
#define MAX_IMAGES 10

static filesystem myfs;

typedef struct {
  uint32 offset; /* Offset to partition in LBA blocks */
  uint32 images;

  uint32 numHandles;

  fwfs_header_t head;

  fwfs_file *filehandle;

  fwfs_image_t *image;
} fwfs_t;

fwfs_t fwfs;

int fwfs_open(void *fsdata,char *fname) {
  uint32 i;
  fwfs_t *fs;

  fs = (fwfs_t*)fsdata;

  for(i=0;i<MAX_IMAGES;i++) {
    if( mlc_strncmp( (char*)&fs->image[i].type, fname, 4 ) == 0 ) { /* Found image */

      if( fs->numHandles < MAX_HANDLES ) {
	fs->filehandle[fs->numHandles].position  = 0;
	fs->filehandle[fs->numHandles].length    = fs->image[i].len;
	fs->filehandle[fs->numHandles].devOffset = fs->image[i].devOffset;

	//mlc_printf("Found the file\n");
	//for(;;);

	fs->numHandles++;
	return(fs->numHandles-1);
      } else return(-1);
    }
  }
  return(-1);
}

size_t fwfs_read(void *fsdata,void *ptr,size_t size,size_t nmemb,int fd) {
  fwfs_t *fs;
  static uint8   buff[512];
  uint32  block,off,read,toRead;

  fs = (fwfs_t*)fsdata;

  read   = 0;
  toRead = size * nmemb;
  off    = fs->filehandle[fd].devOffset + fs->filehandle[fd].position + (fs->offset * 512);
  if (fs->head.version == 3) {
	  off += 512;
  }

  block  = off / 512;
  off    = off % 512;

  if( off != 0 ) { /* Need to read a partial block at first */
    ata_readblocks( buff, block, 1 );
    mlc_memcpy( ptr, buff + off, 512 - off );
    read += 512 - off;
    block++;
  }

  while( (read+512) <= toRead ) {
    ata_readblocks( (uint8*)ptr + read, block, 1 );

    read  += 512;
    block++;
  }
  ata_readblocks( buff, block, 1 );
  mlc_memcpy( (uint8*)ptr+read, buff, toRead - read );

  read += (toRead - read);

  fs->filehandle[fd].position += read;

  return(read);
}

long fwfs_tell(void *fsdata,int fd) {
  fwfs_t *fs;

  fs = (fwfs_t*)fsdata;

  return( fs->filehandle[fd].position );
}

int fwfs_seek(void *fsdata,int fd,long offset,int whence) {
  fwfs_t *fs;

  fs = (fwfs_t*)fsdata;
  
  switch(whence) {
  case VFS_SEEK_SET:
    if( fs->filehandle[fd].length > offset ) {
      fs->filehandle[fd].position = offset;

      return(0);
    } else {
      return(-1);
    }
    break;
  case VFS_SEEK_END:
    if( fs->filehandle[fd].length > offset ) {
      fs->filehandle[fd].position = fs->filehandle[fd].length - offset;
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

  return(0);
}

void fwfs_newfs(uint8 part,uint32 offset) {
  uint32 block,i;
  static uint8  buff[512]; /* !!! Move from BSS */

  /* Verify that this is indeed a firmware partition */
  ata_readblocks( buff, offset,1 );
  if( mlc_strncmp((void*)((uint8*)buff+0x100),"]ih[",4) != 0 ) {
    return;
  } else {
  }

  /* copy the firmware header */
  mlc_memcpy(&fwfs.head, buff + 0x100, sizeof(fwfs_header_t));

  //mlc_printf("\nversion = %d\n", (int)fwfs.head.version);

  if (fwfs.head.version == 1) {
	  fwfs.head.bl_table = 0x4000;
  }

  block = offset + (fwfs.head.bl_table / 512);
 
  if (fwfs.head.version >= 2) {
 	block += 1;
  }

  //mlc_printf("\nblock = %d\n", (int)block);

  fwfs.filehandle = (fwfs_file*)mlc_malloc( sizeof(fwfs_file) * MAX_HANDLES );

  fwfs.image = (fwfs_image_t*)mlc_malloc(512);
  ata_readblocks( fwfs.image, block, 1 ); /* Reads the Bootloader image table */

  fwfs.images = 0;
  for(i=0;i<MAX_IMAGES;i++) {
    if( (fwfs.image[i].type != 0xFFFFFFFF) && (fwfs.image[i].type != 0x0) ) {

      fwfs.image[i].type = ((fwfs.image[i].type & 0xFF000000)>>24) | ((fwfs.image[i].type & 0x00FF0000)>>8) | 
	                   ((fwfs.image[i].type & 0x000000FF)<<24) | ((fwfs.image[i].type & 0x0000FF00)<<8);

      fwfs.images++;
    }
  }

  fwfs.filehandle = (fwfs_file *)mlc_malloc( sizeof(fwfs_file) * MAX_HANDLES );

  fwfs.offset     = offset;
  fwfs.numHandles = 0;

  myfs.open    = fwfs_open;
  myfs.tell    = fwfs_tell;
  myfs.seek    = fwfs_seek;
  myfs.read    = fwfs_read;
  myfs.fsdata  = (void*)&fwfs;
  myfs.partnum = part;

  //mlc_printf("Registering..\n");

  vfs_registerfs( &myfs);
}
