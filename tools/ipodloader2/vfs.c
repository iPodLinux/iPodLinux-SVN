#include "bootloader.h"
#include "ata2.h"
#include "fat32.h"
#include "ext2.h"
#include "fwfs.h"
#include "vfs.h"
#include "minilibc.h"
#include "macpartitions.h"

#define MAX_FILES 10

static filesystem *fs[4]; /* Hardlimit of 4 registered filesystems */
static uint32 fsCnt;

typedef struct {
  uint32 fsIdx;
  int    fd;
} vfs_handle_t;

vfs_handle_t vfs_handle[MAX_FILES]; /* Hardlimit of 10 open files */
uint32 maxHandle;

int vfs_open(char *fname) {
  uint8 part;
  int   i;

  /* (hd0,0)/ == 8 chars */
  if( mlc_strncmp(fname,"(hd0,",5) != 0 ) return(-1);
  part = fname[5] - '0'; /* atoi, the old-fashioned way */

  if (fs[part]) {
    i = 0;
    while( (vfs_handle[i].fd != -1) && (i<MAX_FILES) ) i++;
  
    vfs_handle[i].fsIdx = part;
    vfs_handle[i].fd = fs[part]->open(fs[part]->fsdata,fname+8);

    if( vfs_handle[i].fd != -1 )
      return(i);
  }
  
  return(-1);
}

void vfs_close(int fd) {
  if(vfs_handle[fd].fd != -1) {
    uint32 part = vfs_handle[fd].fsIdx;
    if (fs[part]->close) {
      fs[part]->close( fs[part]->fsdata, vfs_handle[fd].fd );
    }
    vfs_handle[fd].fd = -1;
  }
}

int vfs_seek(int fd,long offset,int whence) {
  uint32 part;
  
  if(vfs_handle[fd].fd == -1) return(-1);

  part = vfs_handle[fd].fsIdx;

  return( fs[part]->seek( fs[part]->fsdata,vfs_handle[fd].fd,offset,whence) );
}

long vfs_tell(int fd) {
  uint32 part;
  
  if(vfs_handle[fd].fd == -1) return(-1);

  part = vfs_handle[fd].fsIdx;

  return( fs[part]->tell( fs[part]->fsdata,vfs_handle[fd].fd) );
}

int vfs_getinfo(int fd, long *out_chksum) {
  uint32 part;
  if(vfs_handle[fd].fd == -1) return(-1);
  part = vfs_handle[fd].fsIdx;
  if (!fs[part]->getinfo) return -1;
  return fs[part]->getinfo( fs[part]->fsdata, vfs_handle[fd].fd, out_chksum );
}

size_t vfs_read(void *ptr,size_t size, size_t nmemb,int fd) {
  uint32 part;
  
  if(vfs_handle[fd].fd == -1) return(-1);

  part = vfs_handle[fd].fsIdx;

  return( fs[part]->read( fs[part]->fsdata,ptr,size,nmemb,vfs_handle[fd].fd) );
}

void vfs_registerfs( filesystem *newfs ) {

  fs[newfs->partnum] = newfs;
}

void vfs_init(void) {
  uint8 *buff;
  uint32 i;

  fsCnt = 0;

  buff = mlc_malloc (512);
  
  ata_readblocks( buff, 0, 1 );

  for(i=0;i<MAX_FILES;i++) vfs_handle[i].fd = -1;

  if( (buff[510] == 0x55) && (buff[511] == 0xAA) ) {
    /* this is a WinPod with a DOS/ext2 partition scheme */

    mlc_printf("Detected WinPod partition\n");

    uint32 logBlkMultiplier = (buff[12] | buff[11]) / 2; // we usually find 02 00, 00 02 or 00 08 here
    if((logBlkMultiplier < 1) | (logBlkMultiplier > 4)) logBlkMultiplier = 1;
	
    /* Check each primary partition for a supported FS */
    for(i=0;i<4;i++) {
      uint32 offset;
      uint8  type;

      type   = buff[446 + (i*16) + 4];
      offset = (buff[446 + (i*16) + 11]<<24) | (buff[446 + (i*16) + 10]<<16) | (buff[446 + (i*16) + 9]<<8) | (buff[446 + (i*16) + 8]);

      switch(type) {
      case 0x00:
        fwfs_newfs(i,offset*logBlkMultiplier);
        break;
      case 0x83:
        ext2_newfs(i,offset*logBlkMultiplier);
        break;
      case 0xB:
        fat32_newfs(i,offset*logBlkMultiplier);
        break;
      default:
        /* printf("  Unsupported..\n"); */
        break;
      }
    }

  } else if( (buff[0] == 'E') && (buff[1] == 'R') ) {
    /* this is a MacPod with a HFS partition scheme */

    mlc_printf("Detected MacPod partition\n");

    check_mac_partitions (buff);

  } else {

    mlc_printf("Invalid MBR\n");
	mlc_hexdump (buff, 16);
	mlc_hexdump (buff+512-16, 16);
    mlc_show_critical_error();
    return;
  }

}
