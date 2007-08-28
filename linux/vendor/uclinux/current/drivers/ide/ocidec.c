/*
 *  linux/drivers/ide/ocidec.c		Version 0.06	Aug 3, 2000
 *
 */

/*
 * Open Core IDE Controller (OCIDEC) support
 *
 */

#undef REALLY_SLOW_IO		/* most systems can safely undef this */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/nios.h>

#include "ide_modes.h"
#include "ocidec.h"



#define WAIT_LOOP_COUNT  6

unsigned long ATA_reset( np_OCIDEC* np_ptr )
{
    int i;
    unsigned long val;

//vic FIXME - maybe wait a bit longer to make sure the devices see the reset
    np_ptr->np_ctrl = ATA_RESET;
	udelay (25);		/* Assert RESET for at least 25us */

    for ( i = WAIT_LOOP_COUNT; i; i-- )
    {
        val = np_ptr->np_ctrl;
        if ( val == ATA_RESET )
            break;
    }

    if ( !i )
        return 1;      // timed out

    np_ptr->np_ctrl = 0;  // Clear reset op
	udelay (2000);		/* Negate RESET for at least 2ms */

    for ( i = WAIT_LOOP_COUNT; i; i-- )
    {
        val = np_ptr->np_ctrl;
        if ( !val )
            break;
    }
    return val;
}


unsigned long ATA_initialize_control( np_OCIDEC* np_ptr )
{
    unsigned int stat;
    unsigned int ctrlVal = IDE_ENABLE | COMPATIBLE_TIMING_IORDY_ENABLE | (IS_PP_ENABLED ? WRITE_PING_PONG_ENABLE : 0);

    if ( ATA_reset( np_ptr ) )
        return 1;

    if ( (stat = np_ptr->np_stat) != VERSION3_REV1 )
    {
        printk( "OCIDEC status = 0x%08lX, should be 0x%08lX\n", VERSION3_REV1, stat );
        return 1;
    }
    else
    {
        stat >>= 24;
        printk( "OCIDEC V%d.%d\n", stat >> 4, stat & 0x0F );
    }

    np_ptr->np_ctrl = ctrlVal;
    if ( np_ptr->np_ctrl != ctrlVal )
        return 1;

    np_ptr->np_pctr  = MODE0_TIMING;
    np_ptr->np_pftr0 = MODE0_DTIMING;
    np_ptr->np_pftr1 = MODE0_DTIMING;

    np_ptr->np_stat = 0;		/* clear pending interrupt */

    return 0;
}


int Reset( np_OCIDEC* ideptr )
{
    if ( ATA_initialize_control( ideptr ) )
        return -1;
    return 0;
}


/* FIXME - the use of IORDY and fast timing for modes 0, 1, and 2 needs tbd */
void ocidec_set_mode(  np_OCIDEC *ideptr, int mode, int device )
{
    if ( mode < 0 || mode > 4 )
        mode = 0;

	if ( device ) {
		/* device = 1; */
		switch ( mode )
		{
			case 0:
				ideptr->np_pftr1 = MODE0_DTIMING;
				ideptr->np_ctrl &= ~(FAST_TIMING_DEV1_ENABLE | FAST_TIMING_DEV1_IORDY_ENABLE);
				break;

			case 1:
				ideptr->np_pftr1 = MODE1_DTIMING;
				ideptr->np_ctrl |= (FAST_TIMING_DEV1_ENABLE | FAST_TIMING_DEV1_IORDY_ENABLE);
				break;

			case 2:
				ideptr->np_pftr1 = MODE2_DTIMING;
				ideptr->np_ctrl &= ~(FAST_TIMING_DEV1_ENABLE | FAST_TIMING_DEV1_IORDY_ENABLE);
				break;

			case 3:
				ideptr->np_pftr1 = MODE3_DTIMING;
				ideptr->np_ctrl |= (FAST_TIMING_DEV1_ENABLE | FAST_TIMING_DEV1_IORDY_ENABLE);
				break;

			case 4:
				ideptr->np_pftr1 = MODE4_DTIMING;
				ideptr->np_ctrl |= (FAST_TIMING_DEV1_ENABLE | FAST_TIMING_DEV1_IORDY_ENABLE);
				break;

			default:
				break;
		}
	} else /* device = 0 */ {
		switch ( mode )
		{
			case 0:
				ideptr->np_pftr0 = MODE0_DTIMING;
				ideptr->np_ctrl &= ~(FAST_TIMING_DEV0_ENABLE | FAST_TIMING_DEV0_IORDY_ENABLE);
				break;

			case 1:
				ideptr->np_pftr0 = MODE1_DTIMING;
				ideptr->np_ctrl |= (FAST_TIMING_DEV0_ENABLE | FAST_TIMING_DEV0_IORDY_ENABLE);
				break;

			case 2:
				ideptr->np_pftr0 = MODE2_DTIMING;
				ideptr->np_ctrl &= ~(FAST_TIMING_DEV0_ENABLE | FAST_TIMING_DEV0_IORDY_ENABLE);
				break;

			case 3:
				ideptr->np_pftr0 = MODE3_DTIMING;
				ideptr->np_ctrl |= (FAST_TIMING_DEV0_ENABLE | FAST_TIMING_DEV0_IORDY_ENABLE);
				break;

			case 4:
				ideptr->np_pftr0 = MODE4_DTIMING;
				ideptr->np_ctrl |= (FAST_TIMING_DEV0_ENABLE | FAST_TIMING_DEV0_IORDY_ENABLE);
				break;

	        default:
				break;
		}
	}
}


static void ocidec_tune_drive(ide_drive_t *drive, byte pio)
{
    np_OCIDEC *ideptr = (np_OCIDEC *)na_ide_interface;
	ide_pio_data_t d;

	pio = ide_get_best_pio_mode(drive, pio, OCIDEC_MAX_PIO, &d);
	printk ("%s: selected PIO mode%d (%dns) %s/IORDY%s%s\n",
		drive->name,
		d.pio_mode,
		d.cycle_time,
		d.use_iordy ? "w" : "wo",
		d.overridden ? " (overriding vendor mode)" : "",
		d.blacklisted ? " (blacklisted)" : "");

    ocidec_set_mode( ideptr, pio, drive->select.b.unit );
}


static void delay_2s (void)
{
	unsigned long timer = jiffies + (HZ*2);
	while (timer > jiffies);
}

void __init ocidec_init(void)		/* called from ide.c */
{
    np_OCIDEC *ideptr = (np_OCIDEC *)na_ide_interface;
    int i;

    if ( Reset( ideptr ) == -1 )
    {
        printk( "Error in resetting OCIDEC.\n");
        return;
    }

	delay_2s();		/* give drives time to spinup and settle */

    ide_hwifs[0].chipset  = ide_ocidec;
    ide_hwifs[0].tuneproc = &ocidec_tune_drive;
	ide_hwifs[0].drives[0].autotune = 1;
	ide_hwifs[0].drives[1].autotune = 1;
}
