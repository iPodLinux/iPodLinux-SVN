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
    int i,fd;
    uint32 ret;
    uint8 *buffer;

    // Backlight ON
    outl(0x80000000 | (0xFF << 16),0x7000A010);
    outl( ((0x100 | 1) << 3),0x6000D824 );

    // Make sure malloc() is initialized
    mlc_malloc_init();
    buff   = (uint16*)mlc_malloc( 220*176*2 );
    buffer = (uint8 *)mlc_malloc( 512 );

    ipod_get_hwrev();

    fb_init(0x6);

    for(i=0;i<(220*176);i++) buff[i] = 0x1F; // cls()

    console_init(buff);

    console_puts("iPL Bootloader 2.0\n");
    
    ret = ata_init(PP5020_IDE_PRIMARY_BASE);
    if( ret ) {
      mlc_printf("ATAinit: %i\n",ret);
      for(;;);
    }

    ata_identify();

    vfs_init();
    fd = vfs_open("NOTES      /KERNEL  BIN");
    vfs_seek(fd,0,VFS_SEEK_END);
    ret = vfs_tell(fd);
    vfs_seek(fd,0,VFS_SEEK_SET);

    mlc_printf("FD=%i (Len %u)\n",fd,ret);

    console_puts("Trying to load kernel\n");
    fb_bitblt(buff,0,0,220,176);
    //for(;;);

    entry = (void*)0x10000000;
    vfs_read( entry, ret, 1, fd );

    mlc_printf("Trying to start.\n");
    //for(;;);

    return entry;
}

