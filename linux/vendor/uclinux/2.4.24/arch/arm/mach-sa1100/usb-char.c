/*
 * (C) Copyright 2000-2001 Extenex Corporation
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  usb-char.c
 *
 *  Miscellaneous character device interface for SA1100 USB function
 *	driver.
 *
 *  Background:
 *  The SA1100 function driver ported from the Compaq Itsy project
 *  has an interface, usb-eth.c, to feed network packets over the
 *  usb wire and into the Linux TCP/IP stack.
 *
 *  This file replaces that one with a simple character device
 *  interface that allows unstructured "byte pipe" style reads and
 *  writes over the USB bulk endpoints by userspace programs.
 *
 *  A new define, CONFIG_SA1100_USB_NETLINK, has been created that,
 *  when set, (the default) causes the ethernet interface to be used.
 *  When not set, this more pedestrian character interface is linked
 *  in instead.
 *
 *  Please see linux/Documentation/arm/SA1100/SA1100_USB for details.
 *
 *  ward.willats@extenex.com
 *
 *  To do:
 *  - Can't dma into ring buffer directly with pci_map/unmap usb_recv
 *    uses and get bytes out at the same time DMA is going on. Investigate:
 *    a) changing usb_recv to use alloc_consistent() at client request; or
 *    b) non-ring-buffer based data structures. In the meantime, I am using
 *    a bounce buffer. Simple, but wasteful.
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/cache.h>
#include <linux/poll.h>
#include <linux/circ_buf.h>
#include <linux/timer.h>

#include <asm/io.h>
#include <asm/semaphore.h>
#include <asm/proc/page.h>
#include <asm/mach-types.h>

#include "usb-char.h"
#include "sa1100_usb.h"



//////////////////////////////////////////////////////////////////////////////
// Driver  Options
//////////////////////////////////////////////////////////////////////////////

#define VERSION 	"0.4"


#define VERBOSITY 1

#if VERBOSITY
# define	PRINTK(x, a...)	printk (x, ## a)
#else
# define	PRINTK(x, a...)	/**/
#endif

//////////////////////////////////////////////////////////////////////////////
// Globals - Macros - Enums - Structures
//////////////////////////////////////////////////////////////////////////////
#ifndef MIN
#define MIN( a, b ) ((a)<(b)?(a):(b))
#endif

typedef int bool; enum { false = 0, true = 1 };

static const char pszMe[] = "usbchr: ";

static wait_queue_head_t wq_read;
static wait_queue_head_t wq_write;
static wait_queue_head_t wq_poll;

/* Serialze multiple writers onto the transmit hardware
.. since we sleep the writer during transmit to stay in
.. sync. (Multiple writers don't make much sense, but..) */
static DECLARE_MUTEX( xmit_sem );

// size of usb DATA0/1 packets. 64 is standard maximum
// for bulk transport, though most hosts seem to be able
// to handle larger.
#define TX_PACKET_SIZE 64
#define RX_PACKET_SIZE 64
#define RBUF_SIZE  (4*PAGE_SIZE)

static struct wcirc_buf {
  char *buf;
  int in;
  int out;
} rx_ring = { NULL, 0, 0 };

static struct {
	 unsigned long  cnt_rx_complete;
	 unsigned long  cnt_rx_errors;
	 unsigned long  bytes_rx;
	 unsigned long  cnt_tx_timeouts;
	 unsigned long  cnt_tx_errors;
	 unsigned long  bytes_tx;
} charstats;


static char * tx_buf = NULL;
static char * packet_buffer = NULL;
static int sending = 0;
static int usb_ref_count = 0;
static int last_tx_result = 0;
static int last_rx_result = 0;
static int last_tx_size = 0;
static struct timer_list tx_timer;

//////////////////////////////////////////////////////////////////////////////
// Prototypes
//////////////////////////////////////////////////////////////////////////////
static char * 	what_the_f( int e );
static void 	free_txrx_buffers( void );
static void     twiddle_descriptors( void );
static void     free_string_descriptors( void ) ;
static int      usbc_open( struct inode *pInode, struct file *pFile );
static void     rx_done_callback_packet_buffer( int flag, int size );

static void     tx_timeout( unsigned long );
static void     tx_done_callback( int flag, int size );

static ssize_t  usbc_read( struct file *, char *, size_t, loff_t * );
static ssize_t  usbc_write( struct file *, const char *, size_t, loff_t * );
static unsigned int usbc_poll( struct file *pFile, poll_table * pWait );
static int usbc_ioctl( struct inode *pInode, struct file *pFile,
                       unsigned int nCmd, unsigned long argument );
static int      usbc_close( struct inode *pInode, struct file *pFile );

#ifdef CONFIG_SA1100_EXTENEX1
static void     extenex_configured_notify_proc( void );
#endif
//////////////////////////////////////////////////////////////////////////////
// Private Helpers
//////////////////////////////////////////////////////////////////////////////

static char * what_the_f( int e )
{
	 char * p;
	 switch( e ) {
	 case 0:
		  p = "noErr";
		  break;
	 case -ENODEV:
		  p = "ENODEV - usb not in config state";
		  break;
	 case -EBUSY:
		  p = "EBUSY - another request on the hardware";
		  break;
	 case -EAGAIN:
		  p = "EAGAIN";
		  break;
	 case -EINTR:
		  p = "EINTR - interrupted\n";
		  break;
	 case -EPIPE:
		  p = "EPIPE - zero length xfer\n";
		  break;
	 default:
		  p = "????";
		  break;
	 }
	 return p;
}

static void free_txrx_buffers( void )
{
	 if ( rx_ring.buf != NULL ) {
		  kfree( rx_ring.buf );
		  rx_ring.buf = NULL;
	 }
	 if ( packet_buffer != NULL ) {
		  kfree( packet_buffer );
		  packet_buffer = NULL;
	 }
	 if ( tx_buf != NULL ) {
		  kfree( tx_buf );
		  tx_buf = NULL;
	 }
}

/* twiddle_descriptors()
 * It is between open() and start(). Setup descriptors.
 */
static void twiddle_descriptors( void )
{
	 desc_t * pDesc = sa1100_usb_get_descriptor_ptr();
	 string_desc_t * pString;

	 pDesc->b.ep1.wMaxPacketSize = make_word_c( RX_PACKET_SIZE );
	 pDesc->b.ep1.bmAttributes   = USB_EP_BULK;
	 pDesc->b.ep2.wMaxPacketSize = make_word_c( TX_PACKET_SIZE );
	 pDesc->b.ep2.bmAttributes   = USB_EP_BULK;

	 if ( machine_is_extenex1() ) {
#ifdef CONFIG_SA1100_EXTENEX1
		  pDesc->dev.idVendor = make_word_c( 0xC9F );
		  pDesc->dev.idProduct = 1;
		  pDesc->dev.bcdDevice = make_word_c( 0x0001 );
		  pDesc->b.cfg.bmAttributes = USB_CONFIG_SELFPOWERED;
		  pDesc->b.cfg.MaxPower = 0;

		  pString = sa1100_usb_kmalloc_string_descriptor( "Extenex" );
		  if ( pString ) {
			   sa1100_usb_set_string_descriptor( 1, pString );
			   pDesc->dev.iManufacturer = 1;
		  }

		  pString = sa1100_usb_kmalloc_string_descriptor( "Handheld Theater" );
		  if ( pString ) {
			   sa1100_usb_set_string_descriptor( 2, pString );
			   pDesc->dev.iProduct = 2;
		  }

		  pString = sa1100_usb_kmalloc_string_descriptor( "00000000" );
		  if ( pString ) {
			   sa1100_usb_set_string_descriptor( 3, pString );
			   pDesc->dev.iSerialNumber = 3;
		  }

		  pString = sa1100_usb_kmalloc_string_descriptor( "HHT Bulk Transfer" );
		  if ( pString ) {
			   sa1100_usb_set_string_descriptor( 4, pString );
			   pDesc->b.intf.iInterface = 4;
		  }
		  sa1100_set_configured_callback( extenex_configured_notify_proc );
#endif
	 }
}

static void free_string_descriptors( void )
{
	 if ( machine_is_extenex1() ) {
		  string_desc_t * pString;
		  int i;
		  for( i = 1 ; i <= 4 ; i++ ) {
			   pString = sa1100_usb_get_string_descriptor( i );
			   if ( pString )
					kfree( pString );
		  }
	 }
}

//////////////////////////////////////////////////////////////////////////////
// ASYNCHRONOUS
//////////////////////////////////////////////////////////////////////////////
static  void kick_start_rx( void )
{
	 if ( usb_ref_count ) {
		  int total_space  = CIRC_SPACE( rx_ring.in, rx_ring.out, RBUF_SIZE );
		  if ( total_space >= RX_PACKET_SIZE ) {
			   sa1100_usb_recv( packet_buffer,
								RX_PACKET_SIZE,
								rx_done_callback_packet_buffer
						      );
		  }
	 }
}
/*
 * rx_done_callback_packet_buffer()
 * We have completed a DMA xfer into the temp packet buffer.
 * Move to ring.
 *
 * flag values:
 * on init,  -EAGAIN
 * on reset, -EINTR
 * on RPE, -EIO
 * on short packet -EPIPE
 */
static void
rx_done_callback_packet_buffer( int flag, int size )
{
	 charstats.cnt_rx_complete++;

	 if ( flag == 0 || flag == -EPIPE ) {
		  size_t n;

		  charstats.bytes_rx += size;

		  n = CIRC_SPACE_TO_END( rx_ring.in, rx_ring.out, RBUF_SIZE );
		  n = MIN( n, size );
		  size -= n;

		  memcpy( &rx_ring.buf[ rx_ring.in ], packet_buffer, n );
		  rx_ring.in = (rx_ring.in + n) & (RBUF_SIZE-1);
		  memcpy( &rx_ring.buf[ rx_ring.in ], packet_buffer + n, size );
		  rx_ring.in = (rx_ring.in + size) & (RBUF_SIZE-1);

		  wake_up_interruptible( &wq_read );
		  wake_up_interruptible( &wq_poll );

		  last_rx_result = 0;

		  kick_start_rx();

	 } else if ( flag != -EAGAIN ) {
		  charstats.cnt_rx_errors++;
		  last_rx_result = flag;
		  wake_up_interruptible( &wq_read );
		  wake_up_interruptible( &wq_poll );
	 }
	 else  /* init, start a read */
		  kick_start_rx();
}


static void tx_timeout( unsigned long unused )
{
	printk( "%stx timeout\n", pszMe );
	sa1100_usb_send_reset();
	charstats.cnt_tx_timeouts++;
}


// on init, -EAGAIN
// on reset, -EINTR
// on TPE, -EIO
static void tx_done_callback( int flags, int size )
{
	 if ( flags == 0 )
		  charstats.bytes_tx += size;
	 else
		  charstats.cnt_tx_errors++;
	 last_tx_size = size;
	 last_tx_result = flags;
	 sending = 0;
	 wake_up_interruptible( &wq_write );
	 wake_up_interruptible( &wq_poll );
}


//////////////////////////////////////////////////////////////////////////////
// Workers
//////////////////////////////////////////////////////////////////////////////

static int usbc_open( struct inode *pInode, struct file *pFile )
{
	 int retval = 0;

	 PRINTK( KERN_DEBUG "%sopen()\n", pszMe );

	 /* start usb core */
	 retval = sa1100_usb_open( "usb-char" );
	 if ( retval ) return retval;

	 /* allocate memory */
	 if ( usb_ref_count == 0 ) {
		  tx_buf = (char*) kmalloc( TX_PACKET_SIZE, GFP_KERNEL | GFP_DMA );
		  if ( tx_buf == NULL ) {
			   printk( "%sARGHH! COULD NOT ALLOCATE TX BUFFER\n", pszMe );
			   goto malloc_fail;
		  }
		  rx_ring.buf =
			(char*) kmalloc( RBUF_SIZE, GFP_KERNEL );

		  if ( rx_ring.buf == NULL ) {
			   printk( "%sARGHH! COULD NOT ALLOCATE RX BUFFER\n", pszMe );
			   goto malloc_fail;
		  }

		  packet_buffer =
			(char*) kmalloc( RX_PACKET_SIZE, GFP_KERNEL | GFP_DMA  );

		  if ( packet_buffer == NULL ) {
			   printk( "%sARGHH! COULD NOT ALLOCATE RX PACKET BUFFER\n", pszMe );
			   goto malloc_fail;
		  }
		  rx_ring.in = rx_ring.out = 0;
		  memset( &charstats, 0, sizeof( charstats ) );
		  sending = 0;
		  last_tx_result = 0;
		  last_tx_size = 0;
	 }

	 /* modify default descriptors */
	 twiddle_descriptors();

	 retval = sa1100_usb_start();
	 if ( retval ) {
		  printk( "%sAGHH! Could not USB core\n", pszMe );
		  free_txrx_buffers();
		  return retval;
	 }
	 usb_ref_count++;   /* must do _before_ kick_start() */
	 MOD_INC_USE_COUNT;
	 kick_start_rx();
	 return 0;

 malloc_fail:
	 free_txrx_buffers();
	 return -ENOMEM;
}

/*
 * Read endpoint. Note that you can issue a read to an
 * unconfigured endpoint. Eventually, the host may come along
 * and configure underneath this module and data will appear.
 */
static ssize_t usbc_read( struct file *pFile, char *pUserBuffer,
                        size_t stCount, loff_t *pPos )
{
	 ssize_t retval;
	 int flags;
	 DECLARE_WAITQUEUE( wait, current );

	 PRINTK( KERN_DEBUG "%sread()\n", pszMe );

	 local_irq_save( flags );
	 if ( last_rx_result == 0 ) {
		  local_irq_restore( flags );
	 } else {  /* an error happended and receiver is paused */
		  local_irq_restore( flags );
		  last_rx_result = 0;
		  kick_start_rx();
	 }

	 add_wait_queue( &wq_read, &wait );
	 while( 1 ) {
		  ssize_t bytes_avail;
		  ssize_t bytes_to_end;

		  set_current_state( TASK_INTERRUPTIBLE );

		  /* snap ring buf state */
		  local_irq_save( flags );
		  bytes_avail  = CIRC_CNT( rx_ring.in, rx_ring.out, RBUF_SIZE );
		  bytes_to_end = CIRC_CNT_TO_END( rx_ring.in, rx_ring.out, RBUF_SIZE );
		  local_irq_restore( flags );

		  if ( bytes_avail != 0 ) {
			   ssize_t bytes_to_move = MIN( stCount, bytes_avail );
			   retval = 0;		// will be bytes transfered
			   if ( bytes_to_move != 0 ) {
					size_t n = MIN( bytes_to_end, bytes_to_move );
					if ( copy_to_user( pUserBuffer,
									   &rx_ring.buf[ rx_ring.out ],
									   n ) ) {
						 retval = -EFAULT;
						 break;
					}
					bytes_to_move -= n;
 					retval += n;
					// might go 1 char off end, so wrap
					rx_ring.out = ( rx_ring.out + n ) & (RBUF_SIZE-1);
					if ( copy_to_user( pUserBuffer + n,
									   &rx_ring.buf[ rx_ring.out ],
									   bytes_to_move )
						 ) {
						 retval = -EFAULT;
						 break;
					}
					rx_ring.out += bytes_to_move;		// cannot wrap
					retval += bytes_to_move;
					kick_start_rx();
			   }
			   break;
		  }
		  else if ( last_rx_result ) {
			   retval = last_rx_result;
			   break;
		  }
		  else if ( pFile->f_flags & O_NONBLOCK ) {  // no data, can't sleep
			   retval = -EAGAIN;
			   break;
		  }
		  else if ( signal_pending( current ) ) {   // no data, can sleep, but signal
			   retval = -ERESTARTSYS;
			   break;
		  }
		  schedule();					   			// no data, can sleep
	 }
	 set_current_state( TASK_RUNNING );
	 remove_wait_queue( &wq_read, &wait );

	 if ( retval < 0 )
		  printk( "%sread error %d - %s\n", pszMe, retval, what_the_f( retval ) );
	 return retval;
}

/*
 * Write endpoint. This routine attempts to break the passed in buffer
 * into usb DATA0/1 packet size chunks and send them to the host.
 * (The lower-level driver tries to do this too, but easier for us
 * to manage things here.)
 *
 * We are at the mercy of the host here, in that it must send an IN
 * token to us to pull this data back, so hopefully some higher level
 * protocol is expecting traffic to flow in that direction so the host
 * is actually polling us. To guard against hangs, a 5 second timeout
 * is used.
 *
 * This routine takes some care to only report bytes sent that have
 * actually made it across the wire. Thus we try to stay in lockstep
 * with the completion routine and only have one packet on the xmit
 * hardware at a time. Multiple simultaneous writers will get
 * "undefined" results.
 *
  */
static ssize_t  usbc_write( struct file *pFile, const char * pUserBuffer,
							 size_t stCount, loff_t *pPos )
{
	 ssize_t retval = 0;
	 ssize_t stSent = 0;

	 DECLARE_WAITQUEUE( wait, current );

	 PRINTK( KERN_DEBUG "%swrite() %d bytes\n", pszMe, stCount );

	 down( &xmit_sem );  // only one thread onto the hardware at a time

	 while( stCount != 0 && retval == 0 ) {
		  int nThisTime  = MIN( TX_PACKET_SIZE, stCount );
		  copy_from_user( tx_buf, pUserBuffer, nThisTime );
		  sending = nThisTime;
 		  retval = sa1100_usb_send( tx_buf, nThisTime, tx_done_callback );
		  if ( retval < 0 ) {
			   char * p = what_the_f( retval );
			   printk( "%sCould not queue xmission. rc=%d - %s\n",
					   pszMe, retval, p );
			   sending = 0;
			   break;
		  }
		  /* now have something on the diving board */
		  add_wait_queue( &wq_write, &wait );
		  tx_timer.expires = jiffies + ( HZ * 5 );
		  add_timer( &tx_timer );
		  while( 1 ) {
			   set_current_state( TASK_INTERRUPTIBLE );
			   if ( sending == 0 ) {  /* it jumped into the pool */
					del_timer( &tx_timer );
					retval = last_tx_result;
					if ( retval == 0 ) {
						 stSent		 += last_tx_size;
						 pUserBuffer += last_tx_size;
						 stCount     -= last_tx_size;
					}
					else
						 printk( "%sxmission error rc=%d - %s\n",
								 pszMe, retval, what_the_f(retval) );
					break;
			   }
			   else if ( signal_pending( current ) ) {
					del_timer( &tx_timer );
					printk( "%ssignal\n", pszMe  );
					retval = -ERESTARTSYS;
					break;
			   }
			   schedule();
		  }
		  set_current_state( TASK_RUNNING );
		  remove_wait_queue( &wq_write, &wait );
	 }

	 up( &xmit_sem );

	 if ( 0 == retval )
		  retval = stSent;
	 return retval;
}

static unsigned int usbc_poll( struct file *pFile, poll_table * pWait )
{
	 unsigned int retval = 0;

	 PRINTK( KERN_DEBUG "%poll()\n", pszMe );

	 poll_wait( pFile, &wq_poll, pWait );

	 if ( CIRC_CNT( rx_ring.in, rx_ring.out, RBUF_SIZE ) )
		  retval |= POLLIN | POLLRDNORM;
	 if ( sa1100_usb_xmitter_avail() )
		  retval |= POLLOUT | POLLWRNORM;
	 return retval;
}

static int usbc_ioctl( struct inode *pInode, struct file *pFile,
                       unsigned int nCmd, unsigned long argument )
{
	 int retval = 0;

	 switch( nCmd ) {

	 case USBC_IOC_FLUSH_RECEIVER:
		  sa1100_usb_recv_reset();
		  rx_ring.in = rx_ring.out = 0;
		  break;

	 case USBC_IOC_FLUSH_TRANSMITTER:
		  sa1100_usb_send_reset();
		  break;

	 case USBC_IOC_FLUSH_ALL:
		  sa1100_usb_recv_reset();
		  rx_ring.in = rx_ring.out = 0;
		  sa1100_usb_send_reset();
		  break;

	 default:
		  retval = -ENOIOCTLCMD;
		  break;

	 }
	 return retval;
}


static int usbc_close( struct inode *pInode, struct file * pFile )
{
	PRINTK( KERN_DEBUG "%sclose()\n", pszMe );
	if ( --usb_ref_count == 0 ) {
		 down( &xmit_sem );
		 sa1100_usb_stop();
		 free_txrx_buffers();
		 free_string_descriptors();
		 del_timer( &tx_timer );
		 sa1100_usb_close();
		 up( &xmit_sem );
	}
    MOD_DEC_USE_COUNT;
    return 0;
}

#ifdef CONFIG_SA1100_EXTENEX1
#include "../../../drivers/char/ex_gpio.h"
void extenex_configured_notify_proc( void )
{
	 if ( exgpio_play_string( "440,1:698,1" ) == -EAGAIN )
		  printk( "%sWanted to BEEP but ex_gpio not open\n", pszMe );
}
#endif
//////////////////////////////////////////////////////////////////////////////
// Initialization
//////////////////////////////////////////////////////////////////////////////

static struct file_operations usbc_fops = {
		owner:      THIS_MODULE,
		open:		usbc_open,
		read:		usbc_read,
		write:		usbc_write,
		poll:		usbc_poll,
		ioctl:		usbc_ioctl,
		release:	usbc_close,
};

static struct miscdevice usbc_misc_device = {
    USBC_MINOR, "usb_char", &usbc_fops
};

/*
 * usbc_init()
 */

int __init usbc_init( void )
{
	 int rc;

#if !defined( CONFIG_ARCH_SA1100 )
	 return -ENODEV;
#endif

	 if ( (rc = misc_register( &usbc_misc_device )) != 0 ) {
		  printk( KERN_WARNING "%sCould not register device 10, "
				  "%d. (%d)\n", pszMe, USBC_MINOR, rc );
		  return -EBUSY;
	 }

	 // initialize wait queues
	 init_waitqueue_head( &wq_read );
	 init_waitqueue_head( &wq_write );
	 init_waitqueue_head( &wq_poll );

	 // initialize tx timeout timer
	 init_timer( &tx_timer );
	 tx_timer.function = tx_timeout;

	  printk( KERN_INFO "USB Function Character Driver Interface"
				  " - %s, (C) 2001, Extenex Corp.\n", VERSION
		   );

	 return rc;
}

void __exit usbc_exit( void )
{
}

EXPORT_NO_SYMBOLS;

module_init(usbc_init);
module_exit(usbc_exit);



// end: usb-char.c



