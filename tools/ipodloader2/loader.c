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

static int is_pp5022 (ipod_t *ipod)
{
    if (ipod->hw_rev < 0x60000)
        return 0;

    if (((inl (0x70000000) << 8) >> 24) == 0x32)
        return 1;
    return 0;
}

static void *iram_get_end_ptr (ipod_t *ipod, int offset) 
{
    static unsigned long iram_end;
    if (!iram_end) {
        if (is_pp5022 (ipod))
            iram_end = 0x40020000;
        else
            iram_end = 0x40018000;
    }
    return (void *)(iram_end - 0x100 + offset);
}

static void set_boot_action (ipod_t *ipod, const char *str) {
  mlc_memcpy (iram_get_end_ptr (ipod, 0x0), str, 9);
  mlc_memcpy (iram_get_end_ptr (ipod, 0x8), "hotstuff", 9);
  outl (1, (unsigned long)iram_get_end_ptr (ipod, 0x10));
  if (ipod->hw_rev >= 0x40000) {
    outl(inl(0x60006004) | 0x4, 0x60006004);
  } else {
    outl(inl(0xcf005030) | 0x4, 0xcf005030);
  }
}

static void reboot_ipod (ipod_t *ipod) 
{
  if (ipod->hw_rev >= 0x40000) {
    outl(inl(0x60006004) | 0x4, 0x60006004);
  } else {
    outl(inl(0xcf005030) | 0x4, 0xcf005030);
  }
  /*
   * We never exit this function
   */
  for(;;);
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

static void load_rockbox(ipod_t *ipod,char *image) {
  int fd;
  unsigned char header[9];
  unsigned long chksum;
  unsigned long sum;
  int i;
  uint32 fsize,read;
  void  *entry;

  mlc_printf("File: %s\n",image);
  mlc_printf("Entry: %x\n",ipod->mem_base);

  /* Open the image-file */
  fd = vfs_open(image);
  if(fd == -1) return;

  /* Get the size of the image-file */
  vfs_seek(fd,0,VFS_SEEK_END);
  fsize = vfs_tell(fd);
  vfs_seek(fd,0,VFS_SEEK_SET);
  read = 0;

  mlc_printf("Size: %u\n",fsize);
  mlc_printf("iPod: 0x%08x (0x%x)\n",ipod->hw_rev,ipod->hw_rev>>16);
  fb_update(framebuffer);

  entry = (void*)ipod->mem_base;

  // Read Rockbox header
  vfs_read(header,8,1,fd);

  // The checksum is always stored in big-endian
  chksum=(header[0]<<24)|(header[1]<<16)|(header[2]<<8)|header[3];
  fsize -= 8;

  header[9]=0;
  mlc_printf("Model: %s\n",&header[4]);
  mlc_printf("Checksum: 0x%08x\n",chksum);
  fb_update(framebuffer);

  // Check that we are running the correct version of Rockbox for this
  // iPod.
  switch(ipod->hw_rev >> 16) {
    case 0x6: // Color/Photo
      if (mlc_memcmp(&header[4],"ipco",4)!=0) {
        mlc_printf("Invalid model.\n");
        fb_update(framebuffer);
        return;
      }
      sum=3;
      break;
    case 0xc: // Nano
      if (mlc_memcmp(&header[4],"nano",4)!=0) {
        mlc_printf("Invalid model.\n");
        fb_update(framebuffer);
        return;
      }
      sum=4;
      break;
    case 0xb: // 5g (Video)
      if (mlc_memcmp(&header[4],"ipvd",4)!=0) {
        mlc_printf("Invalid model.\n");
        fb_update(framebuffer);
        return;
      }
      sum=5;
      break;
    case 0x1:
    case 0x2:
    case 0x3:
      if (mlc_memcmp(&header[4],"ip3g",4)!=0) {
        mlc_printf("Invalid model.\n");
        fb_update(framebuffer);
        return;
      }
      sum=7;
      break;
    case 0x5:
      if (mlc_memcmp(&header[4],"ip4g",4)!=0) {
        mlc_printf("Invalid model.\n");
        fb_update(framebuffer);
        return;
      }
      sum=8;
      break;
    default: // Unsupported
      mlc_printf("Invalid model.\n");
      fb_update(framebuffer);
      return;
  }

  // Read the rest of Rockbox
  while(read < fsize) {
    if( (fsize-read) > (128*1024) ) { /* More than 128K to read */
      vfs_read( (void*)((uint8*)entry + read), 128*1024, 1, fd );
      read += 128 * 1024;
    } else { /* Last part of the file */
      vfs_read( (void*)((uint8*)entry + read), fsize-read, 1, fd );
      read = fsize;
    }

    menu_drawprogress(framebuffer,(read * 255) / fsize);
    fb_update(framebuffer);
  }

  // Calculate checksum for loaded firmware and compare with header 
  for(i = 0;i < fsize;i++) {
      sum += ((uint8*)entry)[i];
  }

  if (sum == chksum) {
    mlc_printf("Checksum OK - starting Rockbox.");
    fb_update(framebuffer);
  } else {
    mlc_printf("Checksum error!  Aborting.");
    return;
  }

  // Transfer execution directly to Rockbox - we don't want
  // to run the rest of the bootloader startup code.

  __asm__ volatile(
    "mov   r0, #0x10000000    \n"
    "mov   pc, r0             \n"
  );
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
    fb_cls(framebuffer,0x18);

    console_init(framebuffer);
    console_puts("iPL Bootloader 2.0\n");
    fb_update(framebuffer);

    /*<DEBUG>*/
    console_puts ("Another String\n");
    char ppver[9] = "????????";
    console_puts ("PPver set\n");
    mlc_printf ("inl()ing...\n");
    unsigned long pplo, pphi;
    pplo = inl (0x70000000);
    pphi = inl (0x70000004);
    mlc_printf ("memcpy()ing...\n");
    mlc_memcpy (ppver, &pplo, 4);
    mlc_memcpy (ppver + 4, &pphi, 4);
    mlc_printf ("Running on a <%s>\n", ppver);
    /*</DEBUG>*/

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

    menu_cls(framebuffer);
    console_home();
    
    if( conf->image[menuPos].type == CONFIG_IMAGE_BINARY ) {
      void *ret;
      ret = loader_startImage(ipod,conf->image[menuPos].path);

      if(ret==NULL)
	goto redoMenu;
      else
	return(ret);
    } else if( conf->image[menuPos].type == CONFIG_IMAGE_ROCKBOX ) {
      load_rockbox(ipod,conf->image[menuPos].path);
    } else if( conf->image[menuPos].type == CONFIG_IMAGE_SPECIAL ) {
        if (!mlc_strcmp (conf->image[menuPos].path, "diskmode"))
            set_boot_action (ipod, "diskmode");
        else if (!mlc_strcmp (conf->image[menuPos].path, "diskscan"))
            set_boot_action (ipod, "diskscan");
        else if (!mlc_strcmp (conf->image[menuPos].path, "retailOS"))
            set_boot_action (ipod, "retailOS");
        reboot_ipod (ipod);
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

