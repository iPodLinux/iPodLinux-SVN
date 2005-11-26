#include "bootloader.h"

#include "ata2.h"
#include "fb.h"
#include "console.h"
#include "keypad.h"
#include "minilibc.h"
#include "ipodhw.h"
#include "vfs.h"
#include "menu.h"

void *loader(void) {
    void *entry;
    int fd,menuPos,done,redraw;
    uint32 ret;
    uint16 *framebuffer;
    ipod_t *ipod;

    ipod_init_hardware();
    ipod = ipod_get_hwinfo();

    /* Make sure malloc() is initialized */
    mlc_malloc_init();

    keypad_init();

    framebuffer = (uint16*)mlc_malloc( ipod->lcd_width*ipod->lcd_height * 2 ); // !!! Allocates 16 times too much memory for 2BPP displays

    fb_init();
    fb_cls(framebuffer,0x1F);

    console_init(framebuffer);
    console_puts("iPL Bootloader 2.0\n");
    fb_update(framebuffer);
    //for(;;);

    ret = ata_init();
    if( ret ) {
      mlc_printf("ATAinit: %i\n",ret);
      fb_update(framebuffer);
      for(;;);
    }

    ata_identify();
    fb_update(framebuffer);

    vfs_init();
    fd = vfs_open("NOTES/KERNEL.BIN");
    vfs_seek(fd,0,VFS_SEEK_END);
    ret = vfs_tell(fd);
    vfs_seek(fd,0,VFS_SEEK_SET);

    mlc_printf("FD=%i (Len %u)\n",fd,ret);
    fb_update(framebuffer);
    //for(;;);

    menu_init();
    menu_additem("Retail OS");
    menu_additem("uCLinux");
    menu_additem("Disk mode");

    menuPos = 0;
    done    = 0;
    redraw  = 1;
    while(!done ) {
      int key;

      key = keypad_getstate();
      if( key == 0x10 ) { // Up
	if(menuPos>0)
	  menuPos--;
	redraw = 1;
      } else if( key == 0x8 ) { // Down
	if(menuPos<2)
	  menuPos++;
	redraw = 1;
      } else if( key == 0x1 ) { // Center
	done = 1;
      }

      if(redraw) {
	menu_redraw(framebuffer,menuPos);
	fb_update(framebuffer);
	redraw = 0;
      }
    }

    entry = (void*)ipod->mem_base;

    if( menuPos == 0 ) return( entry ); // RetailOS
    if( menuPos == 2 ) { // Diskmode
      unsigned char * storage_ptr = (unsigned char *)0x40017F00;
      char * diskmode = "diskmode\0";
      char * hotstuff = "hotstuff\0";
      
      mlc_memcpy(storage_ptr, diskmode, 9);
      storage_ptr = (unsigned char *)0x40017f08;
      mlc_memcpy(storage_ptr, hotstuff, 9);
      outl(1, 0x40017F10);
      outl(inl(0x60006004) | 0x4, 0x60006004);
    } else { // Linux kernel

      vfs_read( entry, ret, 1, fd );
      
      mlc_printf("Trying to start.\n");
      fb_update(framebuffer);
    }

    return entry;
}

