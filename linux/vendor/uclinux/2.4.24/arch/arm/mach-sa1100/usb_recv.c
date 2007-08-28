/*
 * Generic receive layer for the SA1100 USB client function
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
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/errno.h>
#include <asm/dma.h>
#include <asm/system.h>

#include "sa1100_usb.h"
#include "usb_ctl.h"


static char *ep1_buf;
static int ep1_len;
static usb_callback_t ep1_callback;
static char *ep1_curdmabuf;
static dma_addr_t ep1_curdmapos;
static int ep1_curdmalen;
static int ep1_remain;
static int dmachn_rx;
static int rx_pktsize;

static int naking;

static void
ep1_start(void)
{
	sa1100_dma_flush_all(dmachn_rx);
	if (!ep1_curdmalen) {
	  	ep1_curdmalen = rx_pktsize;
		if (ep1_curdmalen > ep1_remain)
			ep1_curdmalen = ep1_remain;
		ep1_curdmapos = pci_map_single(NULL, ep1_curdmabuf, ep1_curdmalen,
					       PCI_DMA_FROMDEVICE);
	}
	sa1100_dma_queue_buffer(dmachn_rx, NULL, ep1_curdmapos, ep1_curdmalen);
	if ( naking ) {
		/* turn off NAK of OUT packets, if set */
		UDC_flip( Ser0UDCCS1, UDCCS1_RPC );
		naking = 0;
	}
}

static void
ep1_done(int flag)
{
	int size = ep1_len - ep1_remain;

	if (!ep1_len)
		return;
	if (ep1_curdmalen)
		pci_unmap_single(NULL, ep1_curdmapos, ep1_curdmalen,
				 PCI_DMA_FROMDEVICE);
	ep1_len = ep1_curdmalen = 0;
	if (ep1_callback) {
		ep1_callback(flag, size);
	}
}

void
ep1_state_change_notify( int new_state )
{

}

void
ep1_stall( void )
{
	/* SET_FEATURE force stall at UDC */
	UDC_set( Ser0UDCCS1, UDCCS1_FST );
}

int
ep1_init(int chn)
{
	desc_t * pd = sa1100_usb_get_descriptor_ptr();
	rx_pktsize = __le16_to_cpu( pd->b.ep1.wMaxPacketSize );
	dmachn_rx = chn;
	sa1100_dma_flush_all(dmachn_rx);
	ep1_done(-EAGAIN);
	return 0;
}

void
ep1_reset(void)
{
	desc_t * pd = sa1100_usb_get_descriptor_ptr();
	rx_pktsize = __le16_to_cpu( pd->b.ep1.wMaxPacketSize );
	sa1100_dma_flush_all(dmachn_rx);
	UDC_clear(Ser0UDCCS1, UDCCS1_FST);
	ep1_done(-EINTR);
}

void
ep1_int_hndlr(int udcsr)
{
	dma_addr_t dma_addr;
	unsigned int len;
	int status = Ser0UDCCS1;

	if ( naking ) printk( "%sEh? in ISR but naking = %d\n", "usbrx: ", naking );

	if (status & UDCCS1_RPC) {

		if (!ep1_curdmalen) {
			printk("usb_recv: RPC for non-existent buffer\n");
			naking=1;
			return;
		}

		sa1100_dma_stop(dmachn_rx);

		if (status & UDCCS1_SST) {
			printk("usb_recv: stall sent OMP=%d\n",Ser0UDCOMP);
			UDC_flip(Ser0UDCCS1, UDCCS1_SST);
			ep1_done(-EIO); // UDC aborted current transfer, so we do
			return;
		}

		if (status & UDCCS1_RPE) {
		    printk("usb_recv: RPError %x\n", status);
			UDC_flip(Ser0UDCCS1, UDCCS1_RPC);
			ep1_done(-EIO);
			return;
		}

		sa1100_dma_get_current(dmachn_rx, NULL, &dma_addr);
		pci_unmap_single(NULL, ep1_curdmapos, ep1_curdmalen,
				 PCI_DMA_FROMDEVICE);
		len = dma_addr - ep1_curdmapos;
		if (len < ep1_curdmalen) {
			char *buf = ep1_curdmabuf + len;
			while (Ser0UDCCS1 & UDCCS1_RNE) {
				if (len >= ep1_curdmalen) {
					printk("usb_recv: too much data in fifo\n");
					break;
				}
				*buf++ = Ser0UDCDR;
				len++;
			}
		} else if (Ser0UDCCS1 & UDCCS1_RNE) {
			printk("usb_recv: fifo screwed, shouldn't contain data\n");
			len = 0;
		}
		ep1_curdmalen = 0;  /* dma unmap already done */
		ep1_remain -= len;
		naking = 1;
		ep1_done((len) ? 0 : -EPIPE);
	}
	/* else, you can get here if we are holding NAK */
}

int
sa1100_usb_recv(char *buf, int len, usb_callback_t callback)
{
	int flags;

	if (ep1_len)
		return -EBUSY;

	local_irq_save(flags);
	ep1_buf = buf;
	ep1_len = len;
	ep1_callback = callback;
	ep1_remain = len;
	ep1_curdmabuf = buf;
	ep1_curdmalen = 0;
	ep1_start();
	local_irq_restore(flags);

	return 0;
}

EXPORT_SYMBOL(sa1100_usb_recv);

void
sa1100_usb_recv_reset(void)
{
	ep1_reset();
}

EXPORT_SYMBOL(sa1100_usb_recv_reset);

void
sa1100_usb_recv_stall(void)
{
	ep1_stall();
}

EXPORT_SYMBOL(sa1100_usb_recv_stall);

