 /*
 *	Copyright (C) Compaq Computer Corporation, 1998, 1999
 *  Copyright (C) Extenex Corporation, 2001
 *
 *  usb_ctl.c
 *
 *  SA1100 USB controller core driver.
 *
 *  This file provides interrupt routing and overall coordination
 *  of the three endpoints in usb_ep0, usb_receive (1),  and usb_send (2).
 *
 *  Please see linux/Documentation/arm/SA1100/SA1100_USB for details.
 *
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/tqueue.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include "sa1100_usb.h"
#include "usb_ctl.h"

//////////////////////////////////////////////////////////////////////////////
// Prototypes
//////////////////////////////////////////////////////////////////////////////

int usbctl_next_state_on_event( int event );
static void udc_int_hndlr(int, void *, struct pt_regs *);
static void initialize_descriptors( void );
static void soft_connect_hook( int enable );
static void udc_disable(void);
static void udc_enable(void);

#if CONFIG_PROC_FS
#define PROC_NODE_NAME "sausb"
static int usbctl_read_proc(char *page, char **start, off_t off,
							int count, int *eof, void *data);
#endif

//////////////////////////////////////////////////////////////////////////////
// Globals
//////////////////////////////////////////////////////////////////////////////
static const char pszMe[] = "usbctl: ";
struct usb_info_t usbd_info;  /* global to ep0, usb_recv, usb_send */

/* device descriptors */
static desc_t desc;

#define MAX_STRING_DESC 8
static string_desc_t * string_desc_array[ MAX_STRING_DESC ];
static string_desc_t sd_zero;  /* special sd_zero holds language codes */

// called when configured
static usb_notify_t configured_callback = NULL;

enum {	kStateZombie  = 0,  kStateZombieSuspend  = 1,
		kStateDefault = 2,  kStateDefaultSuspend = 3,
		kStateAddr    = 4,  kStateAddrSuspend    = 5,
		kStateConfig  = 6,  kStateConfigSuspend  = 7
};

static int device_state_machine[8][6] = {
//                suspend               reset          resume     adddr config deconfig
/* zombie */  {  kStateZombieSuspend,  kStateDefault, kError,    kError, kError, kError },
/* zom sus */ {  kError, kStateDefault, kStateZombie, kError, kError, kError },
/* default */ {  kStateDefaultSuspend, kError, kStateDefault, kStateAddr, kError, kError },
/* def sus */ {  kError, kStateDefault, kStateDefault, kError, kError, kError },
/* addr */    {  kStateAddrSuspend, kStateDefault, kError, kError, kStateConfig, kError },
/* addr sus */{  kError, kStateDefault, kStateAddr, kError, kError, kError },
/* config */  {  kStateConfigSuspend, kStateDefault, kError, kError, kError, kStateAddr },
/* cfg sus */ {  kError, kStateDefault, kStateConfig, kError, kError, kError }
};

/* "device state" is the usb device framework state, as opposed to the
   "state machine state" which is whatever the driver needs and is much
   more fine grained
*/
static int sm_state_to_device_state[8] =
//  zombie           zom suspend          default            default sus
{ USB_STATE_POWERED, USB_STATE_SUSPENDED, USB_STATE_DEFAULT, USB_STATE_SUSPENDED,
// addr              addr sus             config                config sus
  USB_STATE_ADDRESS, USB_STATE_SUSPENDED, USB_STATE_CONFIGURED, USB_STATE_SUSPENDED
};

static char * state_names[8] =
{ "zombie", "zombie suspended", "default", "default suspended",
  "address", "address suspended", "configured", "config suspended"
};

static char * event_names[6] =
{ "suspend", "reset", "resume",
  "address assigned", "configure", "de-configure"
};

static char * device_state_names[] =
{ "not attached", "attached", "powered", "default",
  "address", "configured", "suspended" };

static int sm_state = kStateZombie;

//////////////////////////////////////////////////////////////////////////////
// Async
//////////////////////////////////////////////////////////////////////////////
static void core_kicker(void);

static inline void enable_resume_mask_suspend( void );
static inline void enable_suspend_mask_resume(void);

static void
udc_int_hndlr(int irq, void *dev_id, struct pt_regs *regs)
{
  	__u32 status = Ser0UDCSR;

	/* ReSeT Interrupt Request - UDC has been reset */
	if ( status & UDCSR_RSTIR )
	{
		if ( usbctl_next_state_on_event( kEvReset ) != kError )
		{
			/* starting 20ms or so reset sequence now... */
			printk("%sResetting\n", pszMe);
			ep0_reset();  // just set state to idle
			ep1_reset();  // flush dma, clear false stall
			ep2_reset();  // flush dma, clear false stall
		}
		// mask reset ints, they flood during sequence, enable
		// suspend and resume
		Ser0UDCCR |= UDCCR_REM;    // mask reset
		Ser0UDCCR &= ~(UDCCR_SUSIM | UDCCR_RESIM); // enable suspend and resume
		UDC_flip(  Ser0UDCSR, status );	// clear all pending sources
		return;		// <-- no reason to continue if resetting
	}
	// else we have done something other than reset, so be sure reset enabled
	UDC_clear( Ser0UDCCR, UDCCR_REM );

	/* RESume Interrupt Request */
	if ( status & UDCSR_RESIR )
	{
		usbctl_next_state_on_event( kEvResume );
		core_kicker();
		enable_suspend_mask_resume();
	}

	/* SUSpend Interrupt Request */
	if ( status & UDCSR_SUSIR )
	{
		usbctl_next_state_on_event( kEvSuspend );
		enable_resume_mask_suspend();
	}

	UDC_flip(Ser0UDCSR, status); // clear all pending sources

	if (status & UDCSR_EIR)
		 ep0_int_hndlr();

	if (status & UDCSR_RIR)
		ep1_int_hndlr(status);

	if (status & UDCSR_TIR)
		ep2_int_hndlr(status);
}

static inline void enable_resume_mask_suspend( void )
{
	 int i = 0;

	 while( 1 ) {
		  Ser0UDCCR |= UDCCR_SUSIM; // mask future suspend events
		  udelay( i );
		  if ( (Ser0UDCCR & UDCCR_SUSIM) || (Ser0UDCSR & UDCSR_RSTIR) )
			   break;
		  if ( ++i == 50 ) {
			   printk( "%senable_resume(): Could not set SUSIM %8.8X\n",
					   pszMe, Ser0UDCCR );
			   break;
		  }
	 }

	 i = 0;
	 while( 1 ) {
		  Ser0UDCCR &= ~UDCCR_RESIM;
		  udelay( i );
		  if ( ( Ser0UDCCR & UDCCR_RESIM ) == 0
			   ||
			   (Ser0UDCSR & UDCSR_RSTIR)
			 )
			   break;
		  if ( ++i == 50 ) {
			   printk( "%senable_resume(): Could not clear RESIM %8.8X\n",
					   pszMe, Ser0UDCCR );
			   break;
		  }
	 }
}

static inline void enable_suspend_mask_resume(void)
{
	 int i = 0;
	 while( 1 ) {
		  Ser0UDCCR |= UDCCR_RESIM; // mask future resume events
		  udelay( i );
		  if ( Ser0UDCCR & UDCCR_RESIM || (Ser0UDCSR & UDCSR_RSTIR) )
			   break;
		  if ( ++i == 50 ) {
			   printk( "%senable_suspend(): Could not set RESIM %8.8X\n",
					   pszMe, Ser0UDCCR );
			   break;
		  }
	 }
	 i = 0;
	 while( 1 ) {
		  Ser0UDCCR &= ~UDCCR_SUSIM;
		  udelay( i );
		  if ( ( Ser0UDCCR & UDCCR_SUSIM ) == 0
			   ||
			   (Ser0UDCSR & UDCSR_RSTIR)
			 )
			   break;
		  if ( ++i == 50 ) {
			   printk( "%senable_suspend(): Could not clear SUSIM %8.8X\n",
					   pszMe, Ser0UDCCR );
			   break;
		  }
	 }
}


//////////////////////////////////////////////////////////////////////////////
// Public Interface
//////////////////////////////////////////////////////////////////////////////

/* Open SA usb core on behalf of a client, but don't start running */

int
sa1100_usb_open( const char * client )
{
	 if ( usbd_info.client_name != NULL )
		  return -EBUSY;

	 usbd_info.client_name = (char*) client;
	 memset(&usbd_info.stats, 0, sizeof(struct usb_stats_t));
	 memset(string_desc_array, 0, sizeof(string_desc_array));

	 /* hack to start in zombie suspended state */
	 sm_state = kStateZombieSuspend;
	 usbd_info.state = USB_STATE_SUSPENDED;

	 /* create descriptors for enumeration */
	 initialize_descriptors();

	 printk( "%sOpened for %s\n", pszMe, client );
	 return 0;
}

/* Start running. Must have called usb_open (above) first */
int
sa1100_usb_start( void )
{
	 if ( usbd_info.client_name == NULL ) {
		  printk( "%s%s - no client registered\n",
				  pszMe, __FUNCTION__ );
		  return -EPERM;
	 }

	 /* start UDC internal machinery running */
	 udc_enable();
	 udelay( 100 );

	 /* clear stall - receiver seems to start stalled? 19Jan01ww */
	 /* also clear other stuff just to be thurough 22Feb01ww */
	 UDC_clear(Ser0UDCCS1, UDCCS1_FST | UDCCS1_RPE | UDCCS1_RPC );
	 UDC_clear(Ser0UDCCS2, UDCCS2_FST | UDCCS2_TPE | UDCCS2_TPC );

	 /* mask everything */
	 Ser0UDCCR = 0xFC;

	 /* flush DMA and fire through some -EAGAINs */
	 ep1_init( usbd_info.dmach_rx );
	 ep2_init( usbd_info.dmach_tx );

	 /* give endpoint notification we are starting */
	 ep1_state_change_notify( USB_STATE_SUSPENDED );
	 ep2_state_change_notify( USB_STATE_SUSPENDED );

	 /* enable any platform specific hardware */
	 soft_connect_hook( 1 );

	/* clear all top-level sources */
	 Ser0UDCSR = UDCSR_RSTIR | UDCSR_RESIR | UDCSR_EIR   |
		         UDCSR_RIR   | UDCSR_TIR   | UDCSR_SUSIR ;

	 /* EXERIMENT - a short line in the spec says toggling this
	  ..bit diddles the internal state machine in the udc to
	  ..expect a suspend */
	 Ser0UDCCR  |= UDCCR_RESIM;
	 /* END EXPERIMENT 10Feb01ww */

	 /* enable any platform specific hardware */
	 soft_connect_hook( 1 );

	 /* enable interrupts. If you are unplugged you will
	    immediately get a suspend interrupt. If you are plugged
	    and have a soft connect-circuit, you will get a reset
        If you are plugged without a soft-connect, I think you
	    also get suspend. In short, start with suspend masked
	    and everything else enabled */
	 UDC_write( Ser0UDCCR, UDCCR_SUSIM );

	 printk( "%sStarted for %s\n", pszMe, usbd_info.client_name );
	 return 0;
}

/* Stop USB core from running */
int
sa1100_usb_stop( void )
{
	 if ( usbd_info.client_name == NULL ) {
		  printk( "%s%s - no client registered\n",
				  pszMe, __FUNCTION__ );
		  return -EPERM;
	 }
	 /* mask everything */
	 Ser0UDCCR = 0xFC;
	 ep1_reset();
	 ep2_reset();
	 udc_disable();
	 printk( "%sStopped\n", pszMe );
	 return 0;
}

/* Tell SA core client is through using it */
int
sa1100_usb_close( void )
{
	 if ( usbd_info.client_name == NULL ) {
		  printk( "%s%s - no client registered\n",
				  pszMe, __FUNCTION__ );
		  return -EPERM;
	 }
	 usbd_info.client_name = NULL;
	 printk( "%sClosed\n", pszMe );
	 return 0;
}

/* set a proc to be called when device is configured */
usb_notify_t sa1100_set_configured_callback( usb_notify_t func )
{
	 usb_notify_t retval = configured_callback;
	 configured_callback = func;
	 return retval;
}

/*====================================================
 * Descriptor Manipulation.
 * Use these between open() and start() above to setup
 * the descriptors for your device.
 *
 */

/* get pointer to static default descriptor */
desc_t *
sa1100_usb_get_descriptor_ptr( void ) { return &desc; }

/* optional: set a string descriptor */
int
sa1100_usb_set_string_descriptor( int i, string_desc_t * p )
{
	 int retval;
	 if ( i < MAX_STRING_DESC ) {
		  string_desc_array[i] = p;
		  retval = 0;
	 } else {
		  retval = -EINVAL;
	 }
	 return retval;
}

/* optional: get a previously set string descriptor */
string_desc_t *
sa1100_usb_get_string_descriptor( int i )
{
	 return ( i < MAX_STRING_DESC )
		    ? string_desc_array[i]
		    : NULL;
}


/* optional: kmalloc and unicode up a string descriptor */
string_desc_t *
sa1100_usb_kmalloc_string_descriptor( const char * p )
{
	 string_desc_t * pResult = NULL;

	 if ( p ) {
		  int len = strlen( p );
		  int uni_len = len * sizeof( __u16 );
		  pResult = (string_desc_t*) kmalloc( uni_len + 2, GFP_KERNEL ); /* ugh! */
		  if ( pResult != NULL ) {
			   int i;
			   pResult->bLength = uni_len + 2;
			   pResult->bDescriptorType = USB_DESC_STRING;
			   for( i = 0; i < len ; i++ ) {
					pResult->bString[i] = make_word( (__u16) p[i] );
			   }
		  }
	 }
	 return pResult;
}

//////////////////////////////////////////////////////////////////////////////
// Exports to rest of driver
//////////////////////////////////////////////////////////////////////////////

/* called by the int handler here and the two endpoint files when interesting
   .."events" happen */

int
usbctl_next_state_on_event( int event )
{
	int next_state = device_state_machine[ sm_state ][ event ];
	if ( next_state != kError )
	{
		int next_device_state = sm_state_to_device_state[ next_state ];
		printk( "%s%s --> [%s] --> %s. Device in %s state.\n",
				pszMe, state_names[ sm_state ], event_names[ event ],
				state_names[ next_state ], device_state_names[ next_device_state ] );

		sm_state = next_state;
		if ( usbd_info.state != next_device_state )
		{
			if ( configured_callback != NULL
				 &&
				 next_device_state == USB_STATE_CONFIGURED
				 &&
				 usbd_info.state != USB_STATE_SUSPENDED
			   ) {
			  configured_callback();
			}
			usbd_info.state = next_device_state;
			ep1_state_change_notify( next_device_state );
			ep2_state_change_notify( next_device_state );
		}
	}
#if 0
	else
		printk( "%s%s --> [%s] --> ??? is an error.\n",
				pszMe, state_names[ sm_state ], event_names[ event ] );
#endif
	return next_state;
}

//////////////////////////////////////////////////////////////////////////////
// Private Helpers
//////////////////////////////////////////////////////////////////////////////

/* setup default descriptors */

static void
initialize_descriptors(void)
{
	desc.dev.bLength               = sizeof( device_desc_t );
	desc.dev.bDescriptorType       = USB_DESC_DEVICE;
	desc.dev.bcdUSB                = 0x100; /* 1.0 */
	desc.dev.bDeviceClass          = 0xFF;	/* vendor specific */
	desc.dev.bDeviceSubClass       = 0;
	desc.dev.bDeviceProtocol       = 0;
	desc.dev.bMaxPacketSize0       = 8;	/* ep0 max fifo size */
	desc.dev.idVendor              = 0;	/* vendor ID undefined */
	desc.dev.idProduct             = 0; /* product */
	desc.dev.bcdDevice             = 0; /* vendor assigned device release num */
	desc.dev.iManufacturer         = 0;	/* index of manufacturer string */
	desc.dev.iProduct              = 0; /* index of product description string */
	desc.dev.iSerialNumber         = 0;	/* index of string holding product s/n */
	desc.dev.bNumConfigurations    = 1;

	desc.b.cfg.bLength             = sizeof( config_desc_t );
	desc.b.cfg.bDescriptorType     = USB_DESC_CONFIG;
	desc.b.cfg.wTotalLength        = make_word_c( sizeof(struct cdb) );
	desc.b.cfg.bNumInterfaces      = 1;
	desc.b.cfg.bConfigurationValue = 1;
	desc.b.cfg.iConfiguration      = 0;
	desc.b.cfg.bmAttributes        = USB_CONFIG_BUSPOWERED;
	desc.b.cfg.MaxPower            = USB_POWER( 500 );

	desc.b.intf.bLength            = sizeof( intf_desc_t );
	desc.b.intf.bDescriptorType    = USB_DESC_INTERFACE;
	desc.b.intf.bInterfaceNumber   = 0; /* unique intf index*/
	desc.b.intf.bAlternateSetting  = 0;
	desc.b.intf.bNumEndpoints      = 2;
	desc.b.intf.bInterfaceClass    = 0xFF; /* vendor specific */
	desc.b.intf.bInterfaceSubClass = 0;
	desc.b.intf.bInterfaceProtocol = 0;
	desc.b.intf.iInterface         = 0;

	desc.b.ep1.bLength             = sizeof( ep_desc_t );
	desc.b.ep1.bDescriptorType     = USB_DESC_ENDPOINT;
	desc.b.ep1.bEndpointAddress    = USB_EP_ADDRESS( 1, USB_OUT );
	desc.b.ep1.bmAttributes        = USB_EP_BULK;
	desc.b.ep1.wMaxPacketSize      = make_word_c( 64 );
	desc.b.ep1.bInterval           = 0;

	desc.b.ep2.bLength             = sizeof( ep_desc_t );
	desc.b.ep2.bDescriptorType     = USB_DESC_ENDPOINT;
	desc.b.ep2.bEndpointAddress    = USB_EP_ADDRESS( 2, USB_IN );
	desc.b.ep2.bmAttributes        = USB_EP_BULK;
	desc.b.ep2.wMaxPacketSize      = make_word_c( 64 );
	desc.b.ep2.bInterval           = 0;

	/* set language */
	/* See: http://www.usb.org/developers/data/USB_LANGIDs.pdf */
	sd_zero.bDescriptorType = USB_DESC_STRING;
	sd_zero.bLength         = sizeof( string_desc_t );
	sd_zero.bString[0]      = make_word_c( 0x409 ); /* American English */
	sa1100_usb_set_string_descriptor( 0, &sd_zero );
}

/* soft_connect_hook()
 * Some devices have platform-specific circuitry to make USB
 * not seem to be plugged in, even when it is. This allows
 * software to control when a device 'appears' on the USB bus
 * (after Linux has booted and this driver has loaded, for
 * example). If you have such a circuit, control it here.
 */
static void
soft_connect_hook( int enable )
{
#ifdef CONFIG_SA1100_EXTENEX1
	 if (machine_is_extenex1() ) {
		  if ( enable ) {
			   PPDR |= PPC_USB_SOFT_CON;
			   PPSR |= PPC_USB_SOFT_CON;
		  } else {
			   PPSR &= ~PPC_USB_SOFT_CON;
			   PPDR &= ~PPC_USB_SOFT_CON;
		  }
	 }
#endif
}

/* disable the UDC at the source */
static void
udc_disable(void)
{
	soft_connect_hook( 0 );
	UDC_set( Ser0UDCCR, UDCCR_UDD );
}


/*  enable the udc at the source */
static void
udc_enable(void)
{
	UDC_clear(Ser0UDCCR, UDCCR_UDD);
}

// HACK DEBUG  3Mar01ww
// Well, maybe not, it really seems to help!  08Mar01ww
static void
core_kicker( void )
{
	 __u32 car = Ser0UDCAR;
	 __u32 imp = Ser0UDCIMP;
	 __u32 omp = Ser0UDCOMP;

	 UDC_set( Ser0UDCCR, UDCCR_UDD );
	 udelay( 300 );
	 UDC_clear(Ser0UDCCR, UDCCR_UDD);

	 Ser0UDCAR = car;
	 Ser0UDCIMP = imp;
	 Ser0UDCOMP = omp;
}

//////////////////////////////////////////////////////////////////////////////
// Proc Filesystem Support
//////////////////////////////////////////////////////////////////////////////

#if CONFIG_PROC_FS

#define SAY( fmt, args... )  p += sprintf(p, fmt, ## args )
#define SAYV(  num )         p += sprintf(p, num_fmt, "Value", num )
#define SAYC( label, yn )    p += sprintf(p, yn_fmt, label, yn )
#define SAYS( label, v )     p += sprintf(p, cnt_fmt, label, v )

static int usbctl_read_proc(char *page, char **start, off_t off,
			    int count, int *eof, void *data)
{
	 const char * num_fmt   = "%25.25s: %8.8lX\n";
	 const char * cnt_fmt   = "%25.25s: %lu\n";
	 const char * yn_fmt    = "%25.25s: %s\n";
	 const char * yes       = "YES";
	 const char * no        = "NO";
	 unsigned long v;
	 char * p = page;
	 int len;

 	 SAY( "SA1100 USB Controller Core\n" );
	 SAY( "USB state: %s (%s) %d\n",
		  device_state_names[ sm_state_to_device_state[ sm_state ] ],
		  state_names[ sm_state ],
		  sm_state );

	 SAYS( "ep0 bytes read", usbd_info.stats.ep0_bytes_read );
	 SAYS( "ep0 bytes written", usbd_info.stats.ep0_bytes_written );
	 SAYS( "ep0 FIFO read failures", usbd_info.stats.ep0_fifo_write_failures );
	 SAYS( "ep0 FIFO write failures", usbd_info.stats.ep0_fifo_write_failures );

	 SAY( "\n" );

	 v = Ser0UDCAR;
	 SAY( "%25.25s: 0x%8.8lX - %ld\n", "Address Register", v, v );
	 v = Ser0UDCIMP;
	 SAY( "%25.25s: %ld (%8.8lX)\n", "IN  max packet size", v+1, v );
	 v = Ser0UDCOMP;
	 SAY( "%25.25s: %ld (%8.8lX)\n", "OUT max packet size", v+1, v );

	 v = Ser0UDCCR;
	 SAY( "\nUDC Mask Register\n" );
	 SAYV( v );
	 SAYC( "UDC Active",                ( v & UDCCR_UDA ) ? yes : no );
	 SAYC( "Suspend interrupts masked", ( v & UDCCR_SUSIM ) ? yes : no );
	 SAYC( "Resume interrupts masked",  ( v & UDCCR_RESIM ) ? yes : no );
	 SAYC( "Reset interrupts masked",   ( v & UDCCR_REM ) ? yes : no );

	 v = Ser0UDCSR;
	 SAY( "\nUDC Interrupt Request Register\n" );
	 SAYV( v );
	 SAYC( "Reset pending",      ( v & UDCSR_RSTIR ) ? yes : no );
	 SAYC( "Suspend pending",    ( v & UDCSR_SUSIR ) ? yes : no );
	 SAYC( "Resume pending",     ( v & UDCSR_RESIR ) ? yes : no );
	 SAYC( "ep0 pending",        ( v & UDCSR_EIR )   ? yes : no );
	 SAYC( "receiver pending",   ( v & UDCSR_RIR )   ? yes : no );
	 SAYC( "tramsitter pending", ( v & UDCSR_TIR )   ? yes : no );

#ifdef CONFIG_SA1100_EXTENEX1
	 SAYC( "\nSoft connect", (PPSR & PPC_USB_SOFT_CON) ? "Visible" : "Hidden" );
#endif

#if 0
	 v = Ser0UDCCS0;
	 SAY( "\nUDC Endpoint Zero Status Register\n" );
	 SAYV( v );
	 SAYC( "Out Packet Ready", ( v & UDCCS0_OPR ) ? yes : no );
	 SAYC( "In Packet Ready",  ( v & UDCCS0_IPR ) ? yes : no );
	 SAYC( "Sent Stall",       ( v & UDCCS0_SST ) ? yes : no );
	 SAYC( "Force Stall",      ( v & UDCCS0_FST ) ? yes : no );
	 SAYC( "Data End",         ( v & UDCCS0_DE )  ? yes : no );
	 SAYC( "Data Setup End",   ( v & UDCCS0_SE )  ? yes : no );
	 SAYC( "Serviced (SO)",    ( v & UDCCS0_SO )  ? yes : no );

	 v = Ser0UDCCS1;
	 SAY( "\nUDC Receiver Status Register\n" );
	 SAYV( v );
	 SAYC( "Receive Packet Complete", ( v & UDCCS1_RPC ) ? yes : no );
	 SAYC( "Sent Stall",              ( v & UDCCS1_SST ) ? yes : no );
	 SAYC( "Force Stall",             ( v & UDCCS1_FST ) ? yes : no );
	 SAYC( "Receive Packet Error",    ( v & UDCCS1_RPE ) ? yes : no );
	 SAYC( "Receive FIFO not empty",  ( v & UDCCS1_RNE ) ? yes : no );

	 v = Ser0UDCCS2;
	 SAY( "\nUDC Transmitter Status Register\n" );
	 SAYV( v );
	 SAYC( "FIFO has < 8 of 16 chars", ( v & UDCCS2_TFS ) ? yes : no );
	 SAYC( "Transmit Packet Complete", ( v & UDCCS2_TPC ) ? yes : no );
	 SAYC( "Transmit FIFO underrun",   ( v & UDCCS2_TUR ) ? yes : no );
	 SAYC( "Transmit Packet Error",    ( v & UDCCS2_TPE ) ? yes : no );
	 SAYC( "Sent Stall",               ( v & UDCCS2_SST ) ? yes : no );
	 SAYC( "Force Stall",              ( v & UDCCS2_FST ) ? yes : no );
#endif

	 len = ( p - page ) - off;
	 if ( len < 0 )
		  len = 0;
	 *eof = ( len <=count ) ? 1 : 0;
	 *start = page + off;
	 return len;
}

#endif  /* CONFIG_PROC_FS */

//////////////////////////////////////////////////////////////////////////////
// Module Initialization and Shutdown
//////////////////////////////////////////////////////////////////////////////
/*
 * usbctl_init()
 * Module load time. Allocate dma and interrupt resources. Setup /proc fs
 * entry. Leave UDC disabled.
 */
int __init usbctl_init( void )
{
	int retval = 0;

	udc_disable();

	memset( &usbd_info, 0, sizeof( usbd_info ) );

#if CONFIG_PROC_FS
	create_proc_read_entry ( PROC_NODE_NAME, 0, NULL, usbctl_read_proc, NULL);
#endif

	/* setup rx dma */
	retval = sa1100_request_dma(&usbd_info.dmach_rx, "USB receive", DMA_Ser0UDCRd);
	if (retval) {
		printk("%sunable to register for rx dma rc=%d\n", pszMe, retval );
		goto err_rx_dma;
	}

	/* setup tx dma */
	retval = sa1100_request_dma(&usbd_info.dmach_tx, "USB transmit", DMA_Ser0UDCWr);
	if (retval) {
		printk("%sunable to register for tx dma rc=%d\n",pszMe,retval);
		goto err_tx_dma;
	}

	/* now allocate the IRQ. */
	retval = request_irq(IRQ_Ser0UDC, udc_int_hndlr, SA_INTERRUPT,
			  "SA USB core", NULL);
	if (retval) {
		printk("%sCouldn't request USB irq rc=%d\n",pszMe, retval);
		goto err_irq;
	}

	printk( "SA1100 USB Controller Core Initialized\n");
	return 0;

err_irq:
	sa1100_free_dma(usbd_info.dmach_tx);
	usbd_info.dmach_tx = 0;
err_tx_dma:
	sa1100_free_dma(usbd_info.dmach_rx);
	usbd_info.dmach_rx = 0;
err_rx_dma:
	return retval;
}
/*
 * usbctl_exit()
 * Release DMA and interrupt resources
 */
void __exit usbctl_exit( void )
{
    printk("Unloading SA1100 USB Controller\n");

	udc_disable();

#if CONFIG_PROC_FS
    remove_proc_entry ( PROC_NODE_NAME, NULL);
#endif

    sa1100_free_dma(usbd_info.dmach_rx);
    sa1100_free_dma(usbd_info.dmach_tx);
    free_irq(IRQ_Ser0UDC, NULL);
}

EXPORT_SYMBOL( sa1100_usb_open );
EXPORT_SYMBOL( sa1100_usb_start );
EXPORT_SYMBOL( sa1100_usb_stop );
EXPORT_SYMBOL( sa1100_usb_close );


EXPORT_SYMBOL( sa1100_usb_get_descriptor_ptr );
EXPORT_SYMBOL( sa1100_usb_set_string_descriptor );
EXPORT_SYMBOL( sa1100_usb_get_string_descriptor );
EXPORT_SYMBOL( sa1100_usb_kmalloc_string_descriptor );


module_init( usbctl_init );
module_exit( usbctl_exit );
