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
    uint32 ret,val;
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
#if 0
    outl( 0x10000000, 0x70003828 );
    outl( 0x10800000, 0x7000382C );

    val = (861<<16) | (3);
    outl( val, 0x70003804 ); // 26:16 horizEnd            10:0  horizStart
    val = (863<<16) | 1;
    outl( val, 0x70003808 ); // 26:16 horizSyncPeriod (863=Pal) 10:0  horizSyncWidth
    val = (2<<16) | 4;
    outl( val, 0x7000380C ); // 26:16 hblankStart (> hsyncWidth)  10:0 hblankStop (hblankStart+2)
    val = (3<<16);
    outl( val, 0x70003810 ); // 24:16 vsyncStart
    val = (309<<16) | 3;
    outl( val, 0x70003814 ); // 26:16 vertEnd  10:0 vertStart
    val = (311<<16) | 1;
    outl( val, 0x70003818 ); // 26:16 vsyncPeriod (311=Pal)  7:0  vsyncWidth
    val = (310<<16) | 2;
    outl( val, 0x7000381C ); // 26:16 vblankStart  10:0 vblankStop
    
    

    outl( (inl(0x70003800)|(1<<7)), 0x70003800 ); // Activate DTV controller
    outl( (inl(0x70003824)|(1<<7)), 0x70003824 ); // Activate output
    outl( (inl(0x70003800)|(1<<7)), 0x70003800 ); // Test-pattern    
    

    fb_bitblt(buff,0,0,220,176);
    for(;;);
#endif
    ret = ata_init(PP5020_IDE_PRIMARY_BASE);
    if( ret ) {
      mlc_printf("ATAinit: %i\n",ret);
      for(;;);
    }



    ata_identify();

    vfs_init();
    fd = vfs_open("NOTES      /KERNEL  BIN");
    //fd = vfs_open("NOTES      /DIAG    BIN");
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
    fb_bitblt(buff,0,0,220,176);
    //for(;;);

    return entry;
}

