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

#define LOADERNAME "iPL Loader 2.3"

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
  mlc_memcpy (iram_get_end_ptr (ipod, 0x0), str, 8);
  mlc_memcpy (iram_get_end_ptr (ipod, 0x8), "hotstuff", 8);
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
  for(;;) {}
}

static short calc_checksum2 (char* dest, int size) {
  short csum = 0;
  while (size-- > 0) {
    char b = *dest++;
    csum = ((csum << 1) & 0xffff) + ((csum<0)? 1 : 0) + b; // csum-rotation plus b
  }
  return csum;
}

static char* getArgs (char* baseAddr) {
  // fetch the args
  if (mlc_strncmp (baseAddr, "Args", 4) == 0) {
    int strlen = *(short*)(baseAddr+6);
    if (*(short*)(baseAddr+4) == calc_checksum2 (baseAddr+6, strlen+2)) {
      return baseAddr + 8;
    }
  }
  return 0;
}

static void setArgs (char* baseAddr, int size, char* args) {
  int strlen = mlc_strlen (args);
  // first, make sure the space we want to use is empty:
  int n = size;
  char* p = baseAddr;
  while (n-- > 0) {
    if (*p++) {
      mlc_printf ("Err: setArgs mem ~zero");
      return;
    }
  }
  // now fill it up:
  size -= 9;
  if (strlen > size) strlen = size;
  // offset 0: "Args", ofs 4: 2-byte checksum of strlen+string, ofs 6: 2-byte strlen, ofs 8: 0-terminated string
  mlc_strncpy (baseAddr, "Args", 4);
  *(short*)(baseAddr+6) = strlen;
  mlc_strncpy (baseAddr+8, args, strlen);
  baseAddr[8+strlen] = 0;
  *(short*)(baseAddr+4) = calc_checksum2 (baseAddr+6, strlen+2);
  if (!getArgs (baseAddr)) {
    mlc_printf ("Internal err: getArgs");
  }
}

//
// this function is to be called before the screen gets cleared so that
// the user can, in debug mode, confirm to continue by a keypress
//
static int userconfirm () {
  int shown = 0;
  if (console_printcount) {
    if (config_get()->debug & 2) {
      mlc_printf ("-Press a key-\n");
      do { } while (!keypad_getkey());
      shown = 1;
    } else if (config_get()->debug) {
      // do this always in debug mode, not just if bit 0 is set
      mlc_delay_ms (3000); // 3s
      shown = 1;
    }
    console_printcount = 0;
  }
  return shown;
}

// -----------------------------
//   image file type detection
// -----------------------------

static char* rockboxIDs[] = { "ipco","nano","ipvd","ip3g","ip4g","mini", "mn2g", 0 };

static int is_rockbox_img (char *firstblock) {
  long **ids = (long **) rockboxIDs;
  long cmpval = *(long*)(firstblock+4);
  while (*ids) {
    if (cmpval == **ids) {
      // we have a match - seems to be a rockbox image
      return 1;
    }
    ++ids;
  }
  return 0;
}

static int is_appleos_img (char *firstblock) {
  return (mlc_memcmp (firstblock, "!ATAsoso", 8) == 0);
}

int is_applefw_img (char *firstblock); // we call this in config.c
int is_applefw_img (char *firstblock) {
  return (mlc_memcmp (firstblock+0x20, "portalpl", 8) == 0);
}

static int is_linux_img (char *firstblock) {
  return (mlc_memcmp (firstblock, "\xfe\x1f\x00\xea", 4) == 0);
}

uint32 calc_checksum_fw (char* dest, int size); // we call this in config.c
uint32 calc_checksum_fw (char* dest, int size) {
  // Calculate checksum the way the firmware images are doing it
  uint32 sum = 0;
  long i;
  for (i = 0; i < size; i++) {
      sum += (uint8) *dest++;
  }
  return sum;
}


// ----------------------
//    Rockbox loading
// ----------------------

static void load_rockbox(ipod_t *ipod, int fd, uint32 fsize, uint32 read, void *entry) {
  char header[12];
  unsigned long chksum;
  unsigned long sum;
  int i;

  // since the first block is already read to memory, we need to move it a bit around
  mlc_memcpy (header, entry, 8);
  header[8]=0;
  mlc_memcpy (entry, (char*)entry+8, read-8);
  fsize -= 8;
  read -= 8;

  // The checksum is always stored in big-endian
  chksum = (header[0]<<24)|(header[1]<<16)|(header[2]<<8)|header[3];

  mlc_printf("Model: %s\n",&header[4]);
  mlc_printf("Checksum: 0x%08x\n",chksum);

  // Check that we are running the correct version of Rockbox for this
  // iPod.
  switch(ipod->hw_rev >> 16) {
    case 0x6: // Color/Photo
      if (mlc_memcmp(&header[4],rockboxIDs[0],4)!=0) {
        mlc_printf("Invalid model.\n");
        return;
      }
      sum=3;
      break;
    case 0xc: // Nano
      if (mlc_memcmp(&header[4],rockboxIDs[1],4)!=0) {
        mlc_printf("Invalid model.\n");
        return;
      }
      sum=4;
      break;
    case 0xb: // 5g (Video)
      if (mlc_memcmp(&header[4],rockboxIDs[2],4)!=0) {
        mlc_printf("Invalid model.\n");
        return;
      }
      sum=5;
      break;
    case 0x1:
    case 0x2:
    case 0x3: // 3g
      if (mlc_memcmp(&header[4],rockboxIDs[3],4)!=0) {
        mlc_printf("Invalid model.\n");
        return;
      }
      sum=7;
      break;
    case 0x5: // 4g
      if (mlc_memcmp(&header[4],rockboxIDs[4],4)!=0) {
        mlc_printf("Invalid model.\n");
        return;
      }
      sum=8;
      break;
    case 0x4: // 1st Gen mini
      if (mlc_memcmp(&header[4],rockboxIDs[5],4)!=0) {
        mlc_printf("Invalid model.\n");
        return;
      }
      sum=9;
      break;
    case 0x7: // 2nd Gen mini
      if (mlc_memcmp(&header[4],rockboxIDs[6],4)!=0) {
        mlc_printf("Invalid model.\n");
        return;
      }
      sum=11;
      break;


    // Note: if you add more ids here, please use the rockboxIDs[] array
    // so that the auto-detection of rb files keeps working in is_rockbox_img()

    default: // Unsupported
      mlc_printf("Invalid model.\n");
      return;
  }

  userconfirm ();

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
  } else {
    mlc_printf("Checksum error! Aborting.");
    return;
  }

  // Store the IPOD hw revision in last 4 bytes of DRAM for use by Rockbox
  // and transfer execution directly to Rockbox - we don't want to run
  // the rest of the bootloader startup code.
  if (ipod->hw_rev < 0x40000) {  // PP5002
    outl(ipod->hw_rev,0x29fffffc);

    __asm__ volatile(
      "mov   r0, #0x28000000    \n"
      "mov   pc, r0             \n"
    );
  } else {                        // PP502x

    // (Note by TT: as of Mar2006, the extra RAM in 60GB iPod is ignored by
    // rockbox, so that this still works with this address at end of 32MB RAM.
    // "linuxstb" from their dev team expressed the desire to change this
    // eventually, though):
    outl(ipod->hw_rev,0x11fffffc);

    __asm__ volatile(
      "mov   r0, #0x10000000    \n"
      "mov   pc, r0             \n"
    );
  }
}


// ------------------------------
//   generic image file loading
// ------------------------------

static void *loader_handleImage (ipod_t *ipod, char *imagename, int forceRockbox) {
  int fd, isLinux = 0;
  char *txt, *args;
  uint32 fsize, n;
  uint32 read = 0;
  int shown, showWarning = 0;
  void *entry = (void*)ipod->mem_base;

  mlc_printf("Addr: %08lx\n", entry);

  args = mlc_strchr (imagename, ' ');
  if (args) {
    *args++ = 0;  // the file name ends with the first blank - then come the args
    while (*args == ' ' || *args == '\t') args++; 
  }

  mlc_printf("File: %s\n", imagename);
  fd = vfs_open(imagename);
  if(fd < 0) {
    mlc_printf("Err: open failed\n");
    return 0;
  }
  
  /* Get the size of the image-file */
  vfs_seek(fd,0,VFS_SEEK_END);
  fsize = vfs_tell(fd);
  vfs_seek(fd,0,VFS_SEEK_SET);
  mlc_printf("Size: %u\n",fsize);

  // read the first block of the image and see what type it is
  n = vfs_read( entry, 1, 512, fd );
  if (n != 512) {
      mlc_printf("Err: couldn't read 512 bytes\n");
      return 0;
  }
  read = n;
  if (is_appleos_img (entry)) {
    // we've got the raw apple_os.bin - skip the first 512 bytes
    read = 0;
    txt = "Apple OS";
  } else if (is_applefw_img (entry)) {
    // we've got the apple_os without the 512-byte-header
    txt = "Apple firmware";
  } else if (is_linux_img (entry)) {
    // we've got the linux kernel
    txt = "Linux kernel";
    isLinux = 1;
  } else if (is_rockbox_img (entry)) {
    // we've got a rockbox file
    txt = "Rockbox";
    forceRockbox = 1;
  } else if (forceRockbox) {
    // the user told us in the config to handle this as a rockbox file
    txt = "Rockbox (forced)";
  } else {
    // we've got something else - just launch it blindly
    txt = "Unknown!";
    showWarning = 1;
  }

  mlc_printf("Type: %s\n",txt);
  if (isLinux && args) {
    mlc_printf("Args: %s\n", args);
  } else {
    args = 0;
  }

  if (forceRockbox) {
    // pass this on to the rockbox loader now
    load_rockbox (ipod, fd, fsize, read, entry);
    return 0;
  }

  if (args) {
    // pass a string to linux by storing it at offset 0x80-0xFF
    // note: if you change this mem area, then also update the "getLoader2Args" tool!
    setArgs (entry+0x80, 0x80, args);
  }

  shown = userconfirm ();
  if (showWarning && !shown) {
    mlc_show_critical_error ();
  }

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


  console_home (); // if we get here, then there were no errors, so we can safely reset the output cursor
  mlc_printf("Load succeeded\n");
  mlc_printf("Starting now...\n");
  return entry;
}


// -----------------------
//  main entry of loader2
// -----------------------

extern void hd_demo();
static void testcontrast (config_t *conf);

static int orig_contrast;

void *loader(void) {
  int menuPos, done;
  uint32 ret;
  ipod_t *ipod;
  config_t *conf;

  // Delaying mlc_printf output - here's the deal (by TT 31Mar06):
  //  The goal is not to print out text if the user prefers to have a "clean" screen
  //  with just the graphics (menu and progress bar) shown. The idea is to let the
  //  user choose this "no text" option in the config file. Problem is that we can
  //  only know what's in the config file after we've read it, and that requires a
  //  lot of code run, and that code might want to print useful information in case
  //  something goes wrong.
  //  That's why there is now a "buffered" printf mode: When enabled (which it will
  //  be from startup on), mlc_printf output will be buffered and not appear on
  //  screen. Once the buffered output is turned off, the buffered text will be
  //  printed.
  //  But what if there's a "emergency", i.e. a fatal problem that prevents us to
  //  ever read the config file? Then the buffer will never be shown.
  //  For that reason, we have another 2 functions called mlc_show_critical_error()
  //  and mlc_show_fatal_error(), which, when called, will not only flush the text
  //  to screen but might to other things that are generally useful in such a case.
  //
  mlc_set_output_options (1, 0);  // this caches screen text output for now

  ipod_init_hardware();
  ipod = ipod_get_hwinfo();

  /* Make sure malloc() is initialized */
  mlc_malloc_init();

  framebuffer = (uint16*)mlc_malloc( ipod->lcd_width * ipod->lcd_height * 2 );
  fb_init();
  fb_cls(framebuffer, ipod->lcd_is_grayscale ? WHITE : BLUEISH);
  fb_update (framebuffer);

  orig_contrast = lcd_curr_contrast();
  if (ipod->lcd_is_grayscale && (ipod->hw_rev>>16) >= 3) {
    // increase the contrast a little on 3G, 4G and Minis because of their crappy LCDs
    // whose contrast weakens with certain patterns (e.g. horizontal lines as they appear
    // in the menu's frame)
    lcd_set_contrast (orig_contrast + 4);
  }

  console_init(framebuffer);

  mlc_printf(LOADERNAME"\niPod: %08lx\n", ipod->hw_rev);

  #ifdef DEBUG
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
  #endif

  keypad_init();

  ret = ata_init();
  if( ret ) {
    mlc_printf("ATAinit: %i\n",ret);
    mlc_show_fatal_error ();
  }

  ata_identify();
  vfs_init();

  config_init();
  conf = config_get();

  if (conf->debug) {
    // any non-zero debug value turns on printf console output
    // furthermore, the debug value's bits have the following meaning:
    //  0 or any other bit set: enable printf output
    //  1: confirmation: if set, wait for a keypress before text may be vanishing,
    //       otherwise just pause for 3 seconds before continuing
    //  2: slow output: make a delay after each printf()
    //  3: scrolling test (it's not clear if scrolling still crashes on some iPods; TT 31Mar06)
    mlc_set_output_options (0, conf->debug & 4);
  }

  {
    int contrast;
    if (conf->contrast < 64) {
      contrast = lcd_curr_contrast() + conf->contrast;
    } else {
      contrast = conf->contrast;
    }
    lcd_set_contrast (contrast);
  }

  ipod_set_backlight (conf->backlight);

  if (conf->debug & 8) { // test scrolling
    int i;
    for (i = 1; i <= 15; ++i) mlc_printf ("%i\n",i);
    userconfirm ();
  }

  if (conf->debug & 16) { // test contrast, mainly for grayscale ipods
    testcontrast (conf);
  }

  menu_init();
  for(menuPos=0;menuPos<conf->items;menuPos++) {
    menu_additem( conf->image[menuPos].title );
  }

  /*
   * This is the "event loop" for the menu
   */
redoMenu:
  menuPos = conf->def - 1;
  if (menuPos < 0) menuPos = 0;
  done    = 0;

  userconfirm ();
  mlc_clear_screen ();

  int startTime = timer_get_current ();
  char needsupdate = 1;
  int last_menuPos = menuPos;
  int last_second = 0;
  while(!done) {
    int key = keypad_getkey();
    
    if( key == IPOD_KEY_REW || key == IPOD_KEY_MENU ) {
      if (menuPos>0) menuPos--;
    } else if( key == IPOD_KEY_FWD || key == IPOD_KEY_PLAY ) {
      if (menuPos<(conf->items-1)) menuPos++;
    } else if( key == IPOD_KEY_SELECT ) {
      done = 1;
    }
    if (last_menuPos != menuPos) {
      last_menuPos = menuPos;
      needsupdate = 1;
    }
    
    if (key) {
      conf->timeout = 0; // user has pressed a key -> stop auto-selection timer
    }
    char timeLeft[4];
    timeLeft[0] = 0;
    if (conf->timeout) {
      int t = conf->timeout - (timer_get_current() - startTime) / 1000000;
      if (t < 0) t = 0;
      if (t != last_second) {
        last_second = t;
        needsupdate = 1;
      }
      // show two digits
      timeLeft[1] = (t % 10) + '0';
      t /= 10;
      timeLeft[0] = t ? t+'0' : ' ';
      timeLeft[2] = 0;
      if (timer_check (startTime, 1000000*(conf->timeout))) {
        // timed out
        done = 1;
      }
    }

    if (needsupdate) {
      needsupdate = 0;
      menu_redraw(framebuffer, menuPos, LOADERNAME, timeLeft);
      fb_update(framebuffer);
    }
  }

  menu_cls(framebuffer);
  fb_update(framebuffer);
  
  int forceRockbox = conf->image[menuPos].type == CONFIG_IMAGE_ROCKBOX;
  if( conf->image[menuPos].type == CONFIG_IMAGE_BINARY || forceRockbox ) {
    void *ret;
    ret = loader_handleImage (ipod, conf->image[menuPos].path, forceRockbox);
    if(ret==NULL) {
      // load failed - wait 5 seconds so that we can read the error msgs
      mlc_show_critical_error();
      goto redoMenu;
    } else {
      return ret;
    }
  } else if( conf->image[menuPos].type == CONFIG_IMAGE_SPECIAL ) {
    char *cmd = conf->image[menuPos].path;
    if (mlc_strcmp ("standby", cmd) == 0 || mlc_strcmp ("sleep", cmd) == 0) {
      mlc_printf("Going into standby mode\n", cmd);
      userconfirm ();
      ipod_set_backlight (0);
      mlc_delay_ms (1000);
      pcf_standby_mode ();
      mlc_show_fatal_error();
    } else if (mlc_strcmp ("osos", cmd) == 0 || mlc_strcmp ("ramimg", cmd) == 0) {
      if (is_applefw_img ((void*)ipod->mem_base)) {
        mlc_printf("Launching Apple OS\n");
      } else {
        mlc_printf("Launching from RAM\n");
      }
      lcd_set_contrast (orig_contrast); // restore contrast in case launch fails and loader() is entered again
      ipod_set_backlight (0);
      return (void*)ipod->mem_base;
    } else if (mlc_strcmp ("reboot", cmd) == 0 || mlc_strcmp ("diskmode", cmd) == 0) {
      mlc_printf("Boot command:\n%s\n", cmd);
      userconfirm ();
      lcd_set_contrast (orig_contrast); // restore contrast in case action fails and loader() is entered again
      set_boot_action (ipod, cmd);
      reboot_ipod (ipod);
    } else {
      mlc_printf("Unknown command:\n%s\n", cmd);
      mlc_show_critical_error();
    }
  }

  goto redoMenu;
}


static void testcontrast (config_t *conf)
{
  int linemode = 0;
  int contrast = orig_contrast;
  int redraw = 1;
  int kbdstate = 0, lastkbd = 0;
  int backlight = conf->backlight;
  uint16 linecolor = 0xffff;
  ipod_t *ipod = ipod_get_hwinfo();

  menu_init();
  console_setcolor(BLACK, WHITE, 0);

  while (1) {
    int key;

    if (redraw) {
      redraw = 0;
      lcd_set_contrast (contrast);
      console_clear();
      console_suppress_fbupdate (1); // suppresses fb_update calls for now
      mlc_printf ("Contrast test screen\n");
      mlc_printf ("Key state: %x\n", kbdstate);
      mlc_printf ("<< >>: contrast %d\n", (int)lcd_curr_contrast());
      mlc_printf ("Menu: linemode %d\n", linemode);
      mlc_printf ("Play: backlight %d\n", backlight);
      mlc_printf ("Select: exit\n");
      linecolor = fb_rgb(linemode << 6,linemode << 6,linemode << 6);
      {
        int w = ipod->lcd_width;
        menu_hline (framebuffer, 0, w-1, 78, linecolor);
        menu_drawrect (framebuffer, 111, 82, w-1, 95, linecolor);
        menu_drawrect (framebuffer, 0, 96, 110, 109, linecolor);
      }
      console_suppress_fbupdate (-1); // calls fb_update now
    }

    key = keypad_getkey();
    redraw = 1;
    if (key == IPOD_KEY_REW) {
      contrast -= 1;
    } else if (key == IPOD_KEY_FWD) {
      contrast += 1;
    } else if (key == IPOD_KEY_MENU) {
      if (++linemode > 3) linemode = 0;
    } else if (key == IPOD_KEY_PLAY) {
      backlight = !backlight;
      ipod_set_backlight (backlight);
      redraw = 0;
    } else if (key == IPOD_KEY_SELECT) {
      console_printcount = 0; // prevents userconfirm() from doing something
      return;
    } else {
      redraw = 0;
    }
    
    kbdstate = keypad_getstate();
    if (kbdstate != lastkbd) {
      lastkbd = kbdstate;
      redraw = 1;
    }
  } // while

}
