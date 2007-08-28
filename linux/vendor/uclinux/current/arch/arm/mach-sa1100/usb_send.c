/*
 * Generic xmit layer for the SA1100 USB client function
 * Copyright (c) 2001 by Nicolas Pitre
 *
 * This code was loosely inspired by the original version which was
 * Copyright (c) Compaq Computer Corporation, 1998-1999
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This is still work in progress...
 *
 * Please see linux/Documentation/arm/SA1100/SA1100_USB for details.
 * 15/03/2001 - ep2_start now sets UDCAR to overcome something that is hardware
 * 		bug, I think. green@iXcelerator.com
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/errno.h>
#include <linux/delay.h> // for the massive_attack hack 28Feb01ww
#include <asm/hardware.h>
#include <asm/dma.h>
#include <asm/system.h>
#include <asm/byteorder.h>

#include "sa1100_usb.h"
#include "usb_ctl.h"


static char *ep2_buf;
static int ep2_len;
static usb_callback_t ep2_callback;
static dma_addr_t ep2_dma;
static dma_addr_t ep2_curdmapos;
static int ep2_curdmalen;
static int ep2_remain;
static int dmachn_tx;
static int tx_pktsize;

/* device state is changing, async */
void
ep2_state_change_notify( int new_state )
{
}

/* set feature stall executing, async */
void
ep2_stall( void )
{
	UDC_set( Ser0UDCCS2, UDCCS2_FST );  /* force stall at UDC */
}

static void
ep2_start(void)
{
	if (!ep2_len)
		return;

	ep2_curdmalen = tx_pktsize;
	if (ep2_curdmalen > ep2_remain)
		ep2_curdmalen = ep2_remain;

	/* must do this _before_ queue buffer.. */
	UDC_flip( Ser0UDCCS2,UDCCS2_TPC );  /* stop NAKing IN tokens */
	UDC_write( Ser0UDCIMP, ep2_curdmalen-1 );

	/* Remove if never seen...8Mar01ww */
	{
		 int massive_attack = 20;
		 while ( Ser0UDCIMP != ep2_curdmalen-1 && massive_attack-- ) {
			  printk( "usbsnd: Oh no you don't! Let me spin..." );
			  udelay( 500 );
			  printk( "and try again...\n" );
			  UDC_write( Ser0UDCIMP, ep2_curdmalen-1 );
		 }
		 if ( massive_attack != 20 ) {
			  if ( Ser0UDCIMP != ep2_curdmalen-1 )
				   printk( "usbsnd: Massive attack FAILED :-( %d\n",
						   20 - massive_attack );
			  else
				   printk( "usbsnd: Massive attack WORKED :-) %d\n",
						   20 - massive_attack );
		 }
	}
	/* End remove if never seen... 8Mar01ww */

	Ser0UDCAR = usbd_info.address; // fighting stupid silicon bug
	sa1100_dma_queue_buffer(dmachn_tx, NULL, ep2_curdmapos, ep2_curdmalen);
}

static void
ep2_done(int flag)
{
	int size = ep2_len - ep2_remain;
	if (ep2_len) {
		pci_unmap_single(NULL, ep2_dma, ep2_len, PCI_DMA_TODEVICE);
		ep2_len = 0;
		if (ep2_callback)
			ep2_callback(flag, size);
	}
}

int
ep2_init(int chn)
{
	desc_t * pd = sa1100_usb_get_descriptor_ptr();
	tx_pktsize = __le16_to_cpu( pd->b.ep2.wMaxPacketSize );
	dmachn_tx = chn;
	sa1100_dma_flush_all(dmachn_tx);
	ep2_done(-EAGAIN);
	return 0;
}

void
ep2_reset(void)
{
	desc_t * pd = sa1100_usb_get_descriptor_ptr();
	tx_pktsize = __le16_to_cpu( pd->b.ep2.wMaxPacketSize );
	UDC_clear(Ser0UDCCS2, UDCCS2_FST);
	sa1100_dma_flush_all(dmachn_tx);
	ep2_done(-EINTR);
}

void
ep2_int_hndlr(int udcsr)
{
	int status = Ser0UDCCS2;

	if (Ser0UDCAR != usbd_info.address) // check for stupid silicon bug.
		Ser0UDCAR = usbd_info.address;

	UDC_flip(Ser0UDCCS2, UDCCS2_SST);

	if (status & UDCCS2_TPC) {
		sa1100_dma_flush_all(dmachn_tx);

		if (status & (UDCCS2_TPE | UDCCS2_TUR)) {
			printk("usb_send: transmit error %x\n", status);
			ep2_done(-EIO);
		} else {
#if 1 // 22Feb01ww/Oleg
			ep2_curdmapos += ep2_curdmalen;
			ep2_remain -= ep2_curdmalen;
#else
			ep2_curdmapos += Ser0UDCIMP + 1; // this is workaround
			ep2_remain -= Ser0UDCIMP + 1;    // for case when setting of Ser0UDCIMP was failed
#endif

			if (ep2_remain != 0) {
				ep2_start();
			} else {
				ep2_done(0);
			}
		}
	} else {
		printk("usb_send: Not TPC: UDCCS2 = %x\n", status);
	}
}

int
sa1100_usb_send(char *buf, int len, usb_callback_t callback)
{
	int flags;

	if (usbd_info.state != USB_STATE_CONFIGURED)
		return -ENODEV;

	if (ep2_len)
		return -EBUSY;

	local_irq_save(flags);
	ep2_buf = buf;
	ep2_len = len;
	ep2_dma = pci_map_single(NULL, buf, len, PCI_DMA_TODEVICE);
	ep2_callback = callback;
	ep2_remain = len;
	ep2_curdmapos = ep2_dma;
	ep2_start();
	local_irq_restore(flags);

	return 0;
}


void
sa1100_usb_send_reset(void)
{
	ep2_reset();
}

int sa1100_usb_xmitter_avail( void )
{
	if (usbd_info.state != USB_STATE_CONFIGURED)
		return -ENODEV;
	if (ep2_len)
		return -EBUSY;
	return 0;
}


EXPORT_SYMBOL(sa1100_usb_xmitter_avail);
EXPORT_SYMBOL(sa1100_usb_send);
EXPORT_SYMBOL(sa1100_usb_send_reset);
