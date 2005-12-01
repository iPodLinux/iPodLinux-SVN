/*
 * Original code written by slowcoder as part of the yOS kernel
 * Distributed via IRC without license agreements
 * Original sourcecode lost over the years
 * Refound this code as part of "The Damn Small OS" kernel, without copyrights
 *
 * Reissued under the GNU Public License by original author
 * * James Jacobsson ( slowcoder@mac.com )  2005
 *
 */
#include "bootloader.h"
#include "ata2.h"
#include "vfs.h"
#include "ext2.h"
#include "minilibc.h"

#define EXT2_MAXBLOCKSIZE 8192

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

ext2_t *ext2;

static void ext2_read_superblock(ext2_t *fs, uint32 offset) {
  ata_readblocks( &fs->super, offset + 2, 2 );
}

static void ext2_getblock(uint8 *buffer,uint32 block);

void ext2_ReadDatablockFromInode(inode_t *inode, void *ptr,unsigned int num) {
  uint32 buff[EXT2_MAXBLOCKSIZE];
  uint32 buff2[EXT2_MAXBLOCKSIZE];

  if(        num < 12 ) { // Direct blocks
    //printf("Getting block %u <---\n",inode->i_block[num]);
    ext2_getblock(ptr,inode->i_block[num]);
  } else if( num < (12 + (ext2->block_size/4) ) ) { // Indirect block
    ext2_getblock((uint8*)buff,inode->i_block[12]);

    //printf("Getting block %u <---(%u)\n",buff[num-12],num-12);
    ext2_getblock((uint8*)ptr,buff[num-12]);
    //assert("Hej" == NULL);
  } else if( num < (12 + (ext2->block_size/4)*(ext2->block_size/4) ) ) { // Bi-indirect blocks
    uint32 block,offset;

    //printf("Num:%u ",num);
    num -= (12 + (ext2->block_size/4));

    ext2_getblock((uint8*)buff2,inode->i_block[13]);

    block  = num / (ext2->block_size/4);
    offset = num % (ext2->block_size/4);

    ext2_getblock((uint8*)buff,buff2[block]);

    //printf("Num = %u,  B=%u O=%u\n",num,block,offset); 
    //printf("Getting block %u <---\n",buff[offset]);
    ext2_getblock(ptr,buff[offset]);

    
    //assert("Check ERRORS" == NULL);
  } else {
    mlc_printf("Tri-indirects not supported");
    for(;;);
  }

		    
}

unsigned short ext2_readdata(inode_t *inode,void *ptr,unsigned int off,unsigned int size){
  static unsigned char buff[EXT2_MAXBLOCKSIZE];
  uint32 sblk,eblk,soff,eoff,read;

  read = 0;

  sblk = off          / (1024<<ext2->super.s_log_block_size);
  eblk = (off + size) / (1024<<ext2->super.s_log_block_size);
  soff = off          % (1024<<ext2->super.s_log_block_size);
  eoff = (off + size) % (1024<<ext2->super.s_log_block_size);

  //printf("Preparing to read %u blocks, %u bytes\n",eblk-sblk,eoff-soff);
  //printf("   off=%u  size=%u\n",off,size);
  //printf("   sb=%u, eb=%u, so=%u, eo=%u\n",sblk,eblk,soff,eoff);

  // Special case for reading less than a block
  if( sblk == eblk ) {
    ext2_ReadDatablockFromInode(inode,buff,sblk);
    mlc_memcpy(ptr,buff + soff,eoff-soff);
    read += eoff-soff;
    return(read);
  }

  // If we get here, we're reading cross block boundaries
  while(read < size) {
    ext2_ReadDatablockFromInode(inode,buff,sblk);

    if(sblk != eblk) {
      mlc_memcpy(ptr,buff + soff,(1024<<ext2->super.s_log_block_size)-soff);
      read += (1024<<ext2->super.s_log_block_size)-soff;
      ptr  += (1024<<ext2->super.s_log_block_size)-soff;
    } else {
      mlc_memcpy(ptr,buff,eoff);
      read += eoff;
      ptr  += eoff;
    }

    soff = 0;
    sblk++;
    //printf(" - Read: %u\n",read);

    // See if we're done (Yes, it might be 0 bytes to read in the next block)
    if(read==size)
      return(read);
  }

  //printf("READ=%u, SIZE=%u\n",read,size);
  //assert(read == size);

  return(read);
}

static uint32 ext2_finddirentry(uint8 *dirname,inode_t *inode) {
  dir_t dir;
  unsigned int diroff, dirlen;

  dirlen = mlc_strlen((char*)dirname);
  
  //printf("[EXT2] FindDirEntry(%s,?) (inode->i_size=%u)\n",dirname,inode->i_size);

  diroff = 0;
  while( diroff < inode->i_size ) {

    ext2_readdata(inode,&dir,diroff,sizeof(dir));
    //assert(0);
    //printf("[EXT2]  Comparing \"%s\" and \"%s\" (SIze=%u)\n",dir.name,dirname,inode->i_size);
    
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
  uint32 offset = ((block - ext2->super.s_first_data_block) << (1 + ext2->super.s_log_block_size)) + 2 + ext2->lba_offset;

  //printf("[EXT2] GetBlock reading from LBA %u (Block %u, 512B blocks %u)\n",offset,block,1 << (ext2->super.s_log_block_size + 1));
  ata_readblocks(buffer,offset,1 << (ext2->super.s_log_block_size + 1));
}

static void ext2_getinode(inode_t *ptr,uint32 num) {
  uint32 block,off,group,group_offset;
  uint8 buff[1024];

  num--;
  //num--;

  //printf("PREWIERD: num=%u  ipg=%u\n",num,ext2->super.s_inodes_per_group);

  group = num / ext2->super.s_inodes_per_group;
  num  %= ext2->super.s_inodes_per_group;

  group_offset = (num * sizeof(inode_t));
  block = ext2->groups[group].bg_inode_table + group_offset / (1024 << ext2->super.s_log_block_size);
  off   = group_offset % (1024 << ext2->super.s_log_block_size);

  //printf("[EXT2] GetInode(?,%u)  block=%u off=%u group=%u num=%u\n",num,block,off,group,num);

  ext2_getblock(buff,block);
  mlc_memcpy(ptr,buff+off,sizeof(inode_t));
}

void ext2_getblockgroup(){  /* gets our groups of blocks descriptor */
  inline int min(int x, int y) { return (x < y) ? x : y; } 
  unsigned char buff[512];
  unsigned char *dest = (unsigned char *)ext2->groups;
  int block;// = get_deviceblock(ext2->super.s_first_data_block + 1);
  int numgroups = ext2->super.s_inodes_count / ext2->super.s_inodes_per_group;
  int read = 0;

  block = (((ext2->super.s_first_data_block+1) - ext2->super.s_first_data_block) << (1 + ext2->super.s_log_block_size)) +2 + ext2->lba_offset;

  while(read < numgroups * sizeof(group_t))
    {
      ata_readblocks(buff, block++, 1);
      mlc_memcpy(dest + read, buff, min(512, (numgroups * sizeof(group_t)) - read));
      read += 512;
    }
}

static ext2_file *ext2_findfile(char *fname) {
  ext2_file *ret;
  uint32     inode_num,nstr;
  uint8      dirname[1024];
  inode_t   *retnode;

  ret     = mlc_malloc( sizeof(ext2_file) );
  retnode = &ret->inode;

  //printf("[EXT2] In FindFile(%s)\n",fname);

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
    if(inode_num == 0) {
      //printf("[EXT2] FindDirEntry returned NULL\n");
      return(NULL);
    }
    //printf("Matched inode num %u (strlen=%u,i_size=%u)\n",inode_num,strlen(fname),retnode->i_size);

    //if( strlen(fname) ) 
      ext2_getinode(retnode,inode_num);
  }
  //printf("Exiting FINDFILE\n");

  ret->inodeNum = inode_num;
  ret->length   = retnode->i_size;
  ret->opened   = 1;
  ret->position = 0;
 
  return(ret);
}

int ext2_open(void *fsdata,char *fname) {
  ext2_t    *fs;
  ext2_file *file;

  fs = (ext2_t*)fsdata;

  file = ext2_findfile(fname);

  if( file == NULL ) {
    //printf("Couldnt find file\n");
    return(-1);
  }

  if( fs->numHandles < MAX_HANDLES ) {
    fs->filehandle[fs->numHandles++] = file;
  }

  return(0);
}

int ext2_seek(void *fsdata,int fd,long offset,int whence) {
  ext2_t *fs;

  fs = (ext2_t*)fsdata;

  switch(whence) {
  case VFS_SEEK_SET:
    if( fs->filehandle[fd]->length > offset ) {
      fs->filehandle[fd]->position = offset;
      return(0);
    } else {
      return(-1);
    }
    break;
  case VFS_SEEK_END:
    if( fs->filehandle[fd]->length > offset ) {
      fs->filehandle[fd]->position = fs->filehandle[fd]->length - offset;
      return(0);
    } else {
      return(-1);
    }
    break;
  default:
    mlc_printf("NOOOTT IMPLEMENTED\n");

    return(-1);
  }
  //printf("ext2_seek()\n");

  return(0);
}

long ext2_tell(void *fsdata,int fd) {
  ext2_t *fs;

  fs = (ext2_t*)fsdata;

  return( fs->filehandle[fd]->position );
}

size_t ext2_read(void *fsdata,void *ptr,size_t size,size_t nmemb,int fd) {
  uint32 read,toRead;
  ext2_t *fs;

  fs = (ext2_t*)fsdata;

  read   = 0;
  toRead = size*nmemb;
  if( toRead > (fs->filehandle[fd]->length + fs->filehandle[fd]->position) ) {
    toRead = fs->filehandle[fd]->length + fs->filehandle[fd]->position;
  }

  //printf("ToRead: %u\n",toRead);

  ext2_readdata(&fs->filehandle[fd]->inode, ptr, fs->filehandle[fd]->position ,toRead);

  return(read);
}

void ext2_newfs(uint8 part,uint32 offset) {
  ext2 = (ext2_t*)mlc_malloc( sizeof(ext2_t) );

  ext2_read_superblock(ext2,offset);

  if( ext2->super.s_magic == 0xEF53 ) {
    mlc_printf("ext2fs found\n");
  } else {
    mlc_printf("ext2fs NOT found\n");
    return;
  }



  ext2->numHandles = 0;
  ext2->lba_offset = offset;
  ext2->block_size = 1024 << ext2->super.s_log_block_size;

  ext2_getblockgroup();

  myfs.fsdata     = (void*)ext2;
  myfs.open       = ext2_open;
  myfs.seek       = ext2_seek;
  myfs.tell       = ext2_tell;
  myfs.read       = ext2_read;
  myfs.partnum    = part;

  vfs_registerfs(&myfs);
}
