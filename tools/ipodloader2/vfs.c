#include "bootloader.h"
#include "ata2.h"
#include "fat32.h"
#include "ext2.h"
#include "vfs.h"
#include "minilibc.h"

#define MAX_FILES 10

static filesystem *fs[4]; /* Hardlimit of 4 registered filesystems */
uint32 maxfs;

typedef struct {
  uint32 fs;
  int    fd;
} vfs_handle_t;

vfs_handle_t vfs_handle[MAX_FILES]; /* Hardlimit of 20 open files */
uint32 maxHandle;

int vfs_open(char *fname) {
  uint8 part;
  int   i;

  /* (hd0,0)/ == 8 chars */
  if( mlc_strncmp(fname,"(hd0,",5) != 0 ) return(-1);
  part = fname[5] - '0'; /* atoi, the old-fashioned way */

  i = 0;
  while( (vfs_handle[i].fd != -1) && (i<MAX_FILES) ) i++;
  
  vfs_handle[i].fs = part;
  vfs_handle[i].fd = fs[part]->open(fs[part]->fsdata,fname+8);

  if( vfs_handle[i].fd != -1 )
    return(i);

  return(-1);
}
int vfs_seek(int fd,long offset,int whence) {
  uint32 part;
  
  if(vfs_handle[fd].fd == -1) return(-1);

  part = vfs_handle[fd].fs;

  return( fs[part]->seek( fs[part]->fsdata,vfs_handle[fd].fd,offset,whence) );
}

long vfs_tell(int fd) {
  uint32 part;
  
  if(vfs_handle[fd].fd == -1) return(-1);

  part = vfs_handle[fd].fs;

  return( fs[part]->tell( fs[part]->fsdata,vfs_handle[fd].fd) );
}
size_t vfs_read(void *ptr,size_t size, size_t nmemb,int fd) {
  uint32 part;
  
  if(vfs_handle[fd].fd == -1) return(-1);

  part = vfs_handle[fd].fs;

  return( fs[part]->read( fs[part]->fsdata,ptr,size,nmemb,vfs_handle[fd].fd) );
}

void vfs_registerfs( filesystem *newfs ) {

  fs[newfs->partnum] = newfs;
}

void vfs_init(void) {
  uint8  buff[512];
  uint32 i;

  maxfs = 0;

  ata_readblocks( buff, 0, 1 );

  for(i=0;i<MAX_FILES;i++) vfs_handle[i].fd = -1;

  if( (buff[510] != 0x55) || (buff[511] != 0xAA) ) {
    mlc_printf("Invalid MBR\n");
    return;
  }


  /* Check each primary partition for a supported FS */
  for(i=0;i<4;i++) {
    uint32 offset;
    uint8  type;

    type   = buff[446 + (i*16) + 4];
    offset = (buff[446 + (i*16) + 11]<<24) | (buff[446 + (i*16) + 10]<<16) | (buff[446 + (i*16) + 9]<<8) | (buff[446 + (i*16) + 8]);

    switch(type) {
    case 0x83:
      ext2_newfs(i,offset);
      break;
    case 0xB:
      fat32_newfs(i,offset);
      break;
    default:
      /* printf("  Unsupported..\n"); */
      break;
    }
  }

  
}
