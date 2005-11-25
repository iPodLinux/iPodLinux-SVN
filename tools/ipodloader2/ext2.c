#include "bootloader.h"
#include "ata2.h"
#include "vfs.h"
#include "ext2.h"
#include "minilibc.h"

#define MAX_HANDLES 10

static filesystem myfs;

typedef struct {
  uint32       lba_offset;
  superblock_t super;
  group_t      groups[512];
  uint32       block_size;

  ext2_file *filehandle[MAX_HANDLES];
  uint32     numHandles;
} ext2_t;

ext2_t ext2;

static void ext2_read_superblock(ext2_t *fs, uint32 offset) {
  ata_readblocks( &fs->super, offset + 2, 2 );
}



static uint32 ext2_finddirentry(uint8 *dirname,inode_t *inode) {
  dir_t dir;
  unsigned int diroff, dirlen;

  dirlen = mlc_strlen((char*)dirname);

  diroff = 0;
  while( diroff < inode->i_size ) {
    
    /* ext2_readdata(inode,&dir,diroff,sizeof(dir)); */

    if( dirlen == dir.name_len) {
      if( mlc_memcmp(dirname,dir.name,dirlen) == 0 ) {
        return(dir.inode);
      }
    }

    diroff += dir.rec_len;
  }

  return(0);
}


static void ext2_getblock(uint8 *buffer,uint32 block) {
  uint32 offset = ((block - ext2.super.s_first_data_block) << (1 + ext2.super.s_log_block_size)) + 2 + ext2.lba_offset;

  ata_readblocks(buffer,offset,1 << (ext2.super.s_log_block_size + 1));
}

static void ext2_getinode(inode_t *ptr,uint32 num) {
  uint32 block,off,group,group_offset;
  uint8 buff[1024];

  num--;

  group = num / ext2.super.s_inodes_per_group;
  num  %= ext2.super.s_inodes_per_group;

  group_offset = (num * sizeof(inode_t));
  block = ext2.groups[group].bg_inode_table + group_offset / ext2.block_size;
  off   = group_offset % (1024 << ext2.super.s_log_block_size);

  ext2_getblock(buff,block);
  mlc_memcpy(ptr,buff+off,sizeof(inode_t));
}

static ext2_file *ext2_findfile(char *fname) {
  ext2_file *ret;
  uint32     inode_num,nstr;
  uint8      dirname[1024];
  inode_t   *retnode;

  ret     = mlc_malloc( sizeof(ext2_file) );
  retnode = &ret->inode;

  inode_num = 0x2; /* ROOT_INODE */
  ext2_getinode(retnode,inode_num);
  while( mlc_strlen(fname) != 0 ) {
    if( fname[0] == '/' ) fname++;

    nstr = 0;
    while( (*fname != '/') && (*fname != 0) ) {
      dirname[nstr++] = *fname++;
    }
    dirname[nstr] = 0x0;

    inode_num = ext2_finddirentry(dirname,retnode);
    if(inode_num == 0) return(NULL);

    ext2_getinode(retnode,inode_num);
  }

  ret->length   = 0;
  ret->opened   = 0;
  ret->position = 0;
 
  return(ret);
}

int ext2_open(void *fsdata,char *fname) {
  ext2_t    *fs;
  ext2_file *file;

  fs = (ext2_t*)fsdata;

  file = ext2_findfile(fname);

  if( file == NULL ) {
    mlc_printf("Couldnt find file\n");
    return(-1);
  }

  if( fs->numHandles < MAX_HANDLES ) {
    fs->filehandle[fs->numHandles++] = file;
  }

  return(-1);
}

void ext2_newfs(uint32 offset) {
  ext2_read_superblock(&ext2,offset);

  if( ext2.super.s_magic == 0xEF53 ) {
    mlc_printf("ext2fs found\n");
  } else {
    mlc_printf("ext2fs NOT found\n");
    return;
  }

  ext2.numHandles = 0;
  ext2.lba_offset = offset;
  ext2.block_size = 1024 << ext2.super.s_log_block_size;

  myfs.fsdata     = (void*)&ext2;
  myfs.open       = ext2_open;
  
  vfs_registerfs(&myfs);
}
