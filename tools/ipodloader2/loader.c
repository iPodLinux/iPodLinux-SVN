#include "bootloader.h"

#include "ata2.h"
#include "fb.h"
#include "console.h"
#include "keypad.h"
#include "minilibc.h"
#include "ipodhw.h"
#include "vfs.h"
#include "menu.h"
#include "config.h"

static uint16 *framebuffer;

static void loader_startDiskmode(ipod_t *ipod) {
  unsigned char * storage_ptr = (unsigned char *)0x40017F00;
  char * diskmode = "diskmode\0";
  char * hotstuff = "hotstuff\0";
  
  mlc_memcpy(storage_ptr, diskmode, 9);
  storage_ptr = (unsigned char *)0x40017f08;
  mlc_memcpy(storage_ptr, hotstuff, 9);
  outl(1, 0x40017F10);
  
  if (ipod->hw_rev >= 0x40000) {
    outl(inl(0x60006004) | 0x4, 0x60006004);
  } else {
    outl(inl(0xcf005030) | 0x4, 0xcf005030);
  }
  /*
   * We never exit this function
   */
}

static void *loader_startImage(ipod_t *ipod,char *image) {
  int fd;
  uint32 fsize,read;
  void  *entry;

  mlc_printf("File: %s\n",image);
  mlc_printf("Entry: %x\n",ipod->mem_base);

  /* Open the image-file */
  fd = vfs_open(image);
  if(fd == -1) return(NULL);

  /* Get the size of the image-file */
  vfs_seek(fd,0,VFS_SEEK_END);
  fsize = vfs_tell(fd);
  vfs_seek(fd,0,VFS_SEEK_SET);
  read = 0;

  mlc_printf("Size: %u\n",fsize);
  fb_update(framebuffer);

  entry = (void*)ipod->mem_base;

  //vfs_read( entry, fsize, 1, fd );

  //return(entry);

  while(read < fsize) {
    if( (fsize-read) > (128*1024) ) { /* More than 128K to read */
      vfs_read( (void*)((uint8*)entry + read), 128*1024, 1, fd );
      read += 128 * 1024;
    } else { /* Last part of the file */
      vfs_read( (void*)((uint8*)entry + read), fsize-read, 1, fd );
      read = fsize;
    }

    //mlc_printf("Read: %u\r",read);
    //fb_update(framebuffer);
    menu_drawprogress(framebuffer,(read * 255) / fsize);
    fb_update(framebuffer);
  }
  //for(;;);

  return(entry);
}

void *loader(void) {
  //void *entry;
    int menuPos,done,redraw;
    uint32 ret;
    ipod_t *ipod;
    config_t *conf;
    //config_image_t *img;

    ipod_init_hardware();
    ipod = ipod_get_hwinfo();

    /* Make sure malloc() is initialized */
    mlc_malloc_init();

    keypad_init();

    framebuffer = (uint16*)mlc_malloc( ipod->lcd_width * ipod->lcd_height * 2 );

    fb_init();
    fb_cls(framebuffer,0x1F);

    console_init(framebuffer);
    console_puts("iPL Bootloader 2.0\n");
    fb_update(framebuffer);

    ret = ata_init();
    if( ret ) {
      mlc_printf("ATAinit: %i\n",ret);
      fb_update(framebuffer);
      for(;;);
    }

    ata_identify();
    fb_update(framebuffer);
    vfs_init();

    /*fd = vfs_open("(hd0,1)/NOTES/KERNEL.BIN");
    fd = vfs_open("(hd0,2)/kernel.ext");
    if(fd == -1) {
      mlc_printf("Unable to open kernel\n",ret);
      fb_update(framebuffer);
      for(;;);
      }

    vfs_seek(fd,0,VFS_SEEK_END);
    ret = vfs_tell(fd);
    vfs_seek(fd,0,VFS_SEEK_SET);
 
    mlc_printf("FD=%i (Len %u)\n",fd,ret);
    fb_update(framebuffer);*/

    menu_init();
    config_init();
    conf = config_get();

    for(menuPos=0;menuPos<conf->items;menuPos++) {
      menu_additem( conf->image[menuPos].title );
    }

    /*
     * This is the "event loop" for the menu
     */
 redoMenu:
    menuPos = conf->def;
    done    = 0;
    redraw  = 1;
    while(!done ) {
      int key;

      key = keypad_getstate();
      if( key == IPOD_KEYPAD_UP ) {
	if(menuPos>0)
	  menuPos--;
	redraw = 1;
      } else if( key == IPOD_KEYPAD_DOWN ) {
	if(menuPos<(conf->items-1))
	  menuPos++;
	redraw = 1;
      } else if( key == IPOD_KEYPAD_OK ) {
	done = 1;
      }

      if(redraw) {
	menu_redraw(framebuffer,menuPos);
	fb_update(framebuffer);
	redraw = 0;
      }
    }

    if( conf->image[menuPos].type == CONFIG_IMAGE_BINARY ) {
      void *ret;
      ret = loader_startImage(ipod,conf->image[menuPos].path);

      if(ret==NULL)
	goto redoMenu;
      else
	return(ret);
    } else if( conf->image[menuPos].type == CONFIG_IMAGE_SPECIAL ) {
      loader_startDiskmode(ipod);
    } else {
      goto redoMenu;
    }

    goto redoMenu;
#if 0
    entry = (void*)ipod->mem_base;

    if( menuPos == 0 ) return( entry ); /* RetailOS */
    if( menuPos == 2 ) { /* Diskmode */
      unsigned char * storage_ptr = (unsigned char *)0x40017F00;
      char * diskmode = "diskmode\0";
      char * hotstuff = "hotstuff\0";
      
      mlc_memcpy(storage_ptr, diskmode, 9);
      storage_ptr = (unsigned char *)0x40017f08;
      mlc_memcpy(storage_ptr, hotstuff, 9);
      outl(1, 0x40017F10);

      if (ipod->hw_rev >= 0x40000) {
	      outl(inl(0x60006004) | 0x4, 0x60006004);
      } else {
	      outl(inl(0xcf005030) | 0x4, 0xcf005030);
      }

    } else { /* Linux kernel */
      mlc_printf("Loading kernel\n");
      vfs_read( entry, ret, 1, fd );

      mlc_printf("Trying to start.\n");
      fb_update(framebuffer);
    }
#endif
    return(NULL);
}

