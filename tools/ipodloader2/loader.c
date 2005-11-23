#include "bootloader.h"

#include "ata2.h"
#include "fb.h"
#include "console.h"
#include "minilibc.h"
#include "ipodhw.h"
#include "vfs.h"

#define PP5020_IDE_PRIMARY_BASE    0xC30001E0
#define PP5020_IDE_PRIMARY_CONTROL 0xC30003f8

uint16 *buff;

void *loader(void) {
    void *entry;
    int fd;
    uint32 ret;
    uint8 *buffer;

    // Backlight ON
    switch( ipod_get_hwrev() >> 16 ) {
    case 0xc:
      /* set port B03 */
      outl(((0x100 | 1) << 3), 0x6000d824);
      /* set port L07 */
      outl(((0x100 | 1) << 7), 0x6000d12c);
      break;
    case 0x6:
      outl(0x80000000 | (0xFF << 16),0x7000A010);
      outl( ((0x100 | 1) << 3),0x6000D824 );
      break;
    }

    //for(;;);

    // Make sure malloc() is initialized
    mlc_malloc_init();
    buff   = (uint16*)mlc_malloc( 320*240*2 ); // Room for the biggest display
    buffer = (uint8 *)mlc_malloc( 512 );

    //fb_init(0x60004);
    fb_init( ipod_get_hwrev() );

    fb_cls(buff,0x1F);
    //for(i=0;i<(220*176);i++) buff[i] = 0x1F; // cls()

    console_init(buff, ipod_get_hwrev() );

    console_puts("iPL Bootloader 2.0\n");

    //mlc_printf("hwVER: 0x%x\n",ipod_get_hwrev() );
    //for(;;);

    ret = ata_init(PP5020_IDE_PRIMARY_BASE);
    if( ret ) {
      mlc_printf("ATAinit: %i\n",ret);
      for(;;);
    }

    ata_identify();

    vfs_init();
    fd = vfs_open("NOTES/KERNEL.BIN");
    //fd = vfs_open("NOTES      /KERNEL  BIN");
    //fd = vfs_open("NOTES      /DIAG    BIN");
    vfs_seek(fd,0,VFS_SEEK_END);
    ret = vfs_tell(fd);
    vfs_seek(fd,0,VFS_SEEK_SET);

    mlc_printf("FD=%i (Len %u)\n",fd,ret);

    console_puts("Trying to load kernel\n");
    fb_update(buff);
    //for(;;);

    entry = (void*)0x10000000;
    vfs_read( entry, ret, 1, fd );

    mlc_printf("Trying to start.\n");
    fb_update(buff);
    //for(;;);

    return entry;
}

