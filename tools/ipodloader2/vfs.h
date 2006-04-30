#ifndef _VFS_H_
#define _VFS_H_

#define VFS_SEEK_CUR 0
#define VFS_SEEK_SET 1
#define VFS_SEEK_END 2

typedef struct {
  int    (*open)(void *fsdata,char *fname);
  void   (*close)(void *fsdata, int fd);
  long   (*tell)(void *fsdata,int fd);
  int    (*seek)(void *fsdata,int fd,long offset,int whence);
  size_t (*read)(void *fsdata,void *ptr,size_t size,size_t nmemb,int fd);
  int    (*getinfo)(void *fsdata, int fd, long *out_chksum);

  void *fsdata;
  uint8 partnum;
} filesystem;

void vfs_init(void);
void vfs_registerfs( filesystem *fs );
int  vfs_open(char *fname);
int  vfs_seek(int fd,long offset,int whence);
long vfs_tell(int fd);
size_t vfs_read(void *ptr,size_t size, size_t nmemb,int fd);
int vfs_getinfo(int fd, long *out_chksum);
void vfs_close(int fd);

#endif
