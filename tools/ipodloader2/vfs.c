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
  uint32 fd;
} vfs_handle_t;

vfs_handle_t vfs_handle[20]; /* Hardlimit of 20 open files */

int vfs_open(char *fname) {
  return( fs[0]->open(fs[0]->fsdata,fname) );
}
int vfs_seek(int fd,long offset,int whence) {
  return( fs[0]->seek(fs[0]->fsdata,fd,offset,whence) );
}
long vfs_tell(int fd) {
  return( fs[0]->tell(fs[0]->fsdata,fd) );
}
size_t vfs_read(void *ptr,size_t size, size_t nmemb,int fd) {
  return( fs[0]->read(fs[0]->fsdata,ptr,size,nmemb,fd) );
}

void vfs_registerfs( filesystem *newfs ) {
  fs[maxfs] = newfs;

  maxfs++;
}

void vfs_init(void) {
  uint8  buff[512];
  uint32 i;

  maxfs = 0;

  ata_readblocks( buff, 0, 1 );

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
      ext2_newfs(offset);
      break;
    case 0xB:
      fat32_newfs(offset);
      break;
    default:
      /* printf("  Unsupported..\n"); */
      break;
    }
  }

  
}
