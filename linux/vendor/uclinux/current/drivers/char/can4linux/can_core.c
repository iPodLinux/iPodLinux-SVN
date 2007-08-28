/*
 * can_core - can4linux CAN driver module
 *
 * can4linux -- LINUX CAN device driver source
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 * 
 * Copyright (c) 2001 port GmbH Halle/Saale
 * (c) 2001 Heinz-Jürgen Oertel (oe@port.de)
 *          Claus Schroeter (clausi@chemie.fu-berlin.de)
 *------------------------------------------------------------------
 * $Header: /var/cvs/uClinux-2.4.x/drivers/char/can4linux/can_core.c,v 1.2 2003/08/28 00:38:31 gerg Exp $
 *
 *--------------------------------------------------------------------------
 *
 *
 * modification history
 * --------------------
 * $Log: can_core.c,v $
 * Revision 1.2  2003/08/28 00:38:31  gerg
 * I hope my patch doesn't come to late for the next uClinux distribution.
 * The new patch is against the latest CVS uClinux-2.4.x/drivers/char. The
 * FlexCAN driver is working but still needs some work. Phil Wilshire is
 * supporting me and we expect to have a complete driver in some weeks.
 *
 * commit text: added support for ColdFire FlexCAN
 *
 * Patch submitted by Heinz-Juergen Oertel <oe@port.de>.
 *
 * Revision 1.1  2003/07/18 00:11:46  gerg
 * I followed as much rules as possible (I hope) and generated a patch for the
 * uClinux distribution. It contains an additional driver, the CAN driver, first
 * for an SJA1000 CAN controller:
 *   uClinux-dist/linux-2.4.x/drivers/char/can4linux
 * In the "user" section two entries
 *   uClinux-dist/user/can4linux     some very simple test examples
 *   uClinux-dist/user/horch         more sophisticated CAN analyzer example
 *
 * Patch submitted by Heinz-Juergen Oertel <oe@port.de>.
 *
 *
 *
 *
 *
 *--------------------------------------------------------------------------
 */


/**
* \file can_core.c
* \author Heinz-Jürgen Oertel, port GmbH
* $Revision: 1.2 $
* $Date: 2003/08/28 00:38:31 $
*
* Contains the code for module initialization.
* The functions herein are never called directly by the user
* but when the driver module is loaded into the kernel space
* or unloaded.
*
* The driver is highly configurable using the \b sysctl interface.
* For a description see main page.

*/


/****************************************************************************/
/**
* \mainpage  can4linx - CAN network device driver
*
The LINUX CAN driver can be used to control the CAN bus
connected with the  AT-CAN-MINI  PC interface card.
This project was done in cooperation with the  LINUX LLP Project
to control laboratory or automation devices via CAN.

The full featured can4linux supports many different interface boards.
It is possible to use different kinds of boards at the same time.
Up to four boards can be placed in one computer.
With this feature it is possible to use /dev/can0 and
/dev/can2 for two boards AT-CAN-MINI with SJA1000
and /dev/can1 and /dev/can3 with two CPC-XT equipped with Intel 82527.
In all these configurations
the programmer sees the same driver interface with
open(), close(), read(), write() and ioctl() calls
( can_open(), can_close(), can_read(), can_write(), can_ioctl() ).

The driver itself is highly configurable
using the /proc interface of the LINUX kernel. 

The following listing shows a typical configuration with three boards: 

\code
$ grep . /proc/sys/Can/\*
/proc/sys/Can/AccCode:  -1       -1      -1      -1
/proc/sys/Can/AccMask:  -1       -1      -1      -1
/proc/sys/Can/Base:     800      672     832     896
/proc/sys/Can/Baud:     125      125     125     250
/proc/sys/Can/Chipset:  SJA1000
/proc/sys/Can/IOModel:  pppp
/proc/sys/Can/IRQ:      5     7       3       5
/proc/sys/Can/Outc:     250   250     250     0
/proc/sys/Can/Overrun:  0     0       0       0
/proc/sys/Can/RxErr:    0     0       0       0
/proc/sys/Can/Timeout:  100   100     100     100
/proc/sys/Can/TxErr:    0     0       0       0
/proc/sys/Can/dbgMask:  0
/proc/sys/Can/version:  3.0_ATCANMINI_PELICAN
\endcode


This above mentioned full flexibility
is not needed in embedded applications.
For this applications, a stripped-down version exists.
It uses the same programming interface
but does the most configurations at compile time.
That means especially that only one CAN controller support with
a special register access method is compiled into the driver.
Actually the only CAN controller supported by this version
is the Philips SJA 1000 in both the compatibility mode 
\b BasicCAN and the Philips \PeliCAN mode (compile time selectable).



The following sections are describing the \e sysctl entries.

\par AccCode/AccMask
contents of the message acceptance mask and acceptance code registers
of 82x200/SJA1000 compatible CAN controllers (see can_ioctl()).
\par Base
CAN controllers base address for each board.
Depending of the \e IOModel entry that can be a memory or I/O address.
(read-only for PCI boards)
\par Baud
used bit rate for this board
\par Chipset
name of the supported CAN chip used with this boards
Read only for this version.
\par IOModel
one letter for each port. Readonly.
Read the CAN register access model.
The following models are currently supported:
\li m - memory access, the registers are directly mapped into memory
\li f - fast register access, special mode for the 82527
     uses memory locations for register addresses 
     (ELIMA)
\li p - port I/O,  80x86 specific I/O address range
     (AT-CAN-MINI)
\li b - special mode for the B&R CAN card,
     two special I/O addresses for register addressing and access
Since version 2.4 set at compile time.
\par IRQ
used IRQ numbers, one value for each board.
(read-only for PCI boards)
\par Outc
value of the output control register of the CAN controller
Since version 2.4 set at compile time.
A board specific value is used when the module the first time is loaded.
This board specific value can be reloded by writing the value 0
to \e Outc .
\par Overrun
counter for overrun conditions in the CAN controller
\par RxErr
counter for CAN controller rx error conditions
\par Timeout
time out value for waiting for a successful transmission
\par TxErr
counter for CAN controller tx error conditions
\par dbgMask
if compiled with debugging support, writing a value greater then 0 
enables debugging to \b syslogd
\par version
read only entry containing the drivers version number


Please see also at can_ioctl() for some additional descriptions.

For initially writing these sysctl entries after loading the driver
(or at any time) a shell script utility does exist.
It uses a board configuration file that is written over \e /proc/sys/Can .
\code
utils/cansetup port.conf
\endcode
or, like used in the Makefile:
\code
CONFIG := $(shell uname -n)

# load host specific CAN configuration
load:
	@echo "Loading etc/$(CONFIG).conf CAN configuration"
	utils/cansetup etc/$(CONFIG).conf
	echo 0 >/proc/sys/Can/dbgMask
\endcode
Example *.conf files are located in the \e etc/ directory.

\note
This documentation was created using the wonderful tool
\b Doxygen http://www.doxygen.org/index.html .
Die Dokumentation wurde unter Verwendung von
\b Doxygen http://www.doxygen.org/index.html
erstellt

*/

#include "can_defs.h"
/* #include <linux/version.h> */


#ifndef DOXYGEN_SHOULD_SKIP_THIS



MODULE_LICENSE("GPL");
MODULE_AUTHOR("H.-J. Oertel <oe@port.de>");
MODULE_DESCRIPTION("CAN fieldbus driver");


char kernel_version[] = UTS_RELEASE;

int IRQ_requested[MAX_CHANNELS];
int Can_minors[MAX_CHANNELS];			/* used as IRQ dev_id */
#define LDDK_USE_REGISTER 1
#ifdef LDDK_USE_REGISTER
    int Can_major = Can_MAJOR; 
#endif

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

struct file_operations can_fops = { 
#if LINUX_VERSION_CODE >= 0x020400
				/* This started being used during 2.4.0-test */
    owner:		THIS_MODULE,
#endif
#if LINUX_VERSION_CODE >= 0x020201 
    llseek:	NULL, 
#else
    lseek:	NULL, 
#endif
    read:	can_read,
    write:	can_write,
    readdir:	NULL, 
#if LINUX_VERSION_CODE >= 0x020203 
    poll:	can_select,
#else
    select:	can_select,
#endif 
    ioctl:	can_ioctl,
    mmap:	NULL, 
    open:	can_open,
#if LINUX_VERSION_CODE >= 0x020203
    flush:	NULL, /* flush call */
#endif 
    release:	can_close,
    fsync:	NULL,
    fasync:	NULL,
};


#ifndef DOXYGEN_SHOULD_SKIP_THIS


/* int init_module(void) */
int can_init(void)
{
int i;
#if LDDK_USE_BLKREQUEST
  extern int Can_request ();
#endif

  DBGin("init_module");
#ifdef LDDK_USE_REGISTER
    if( register_chrdev(Can_major, "Can", &can_fops) ) {
	  printk("can't get Major %d\n", Can_major);
      return(-EIO);
    }
  

#endif
    {
	printk(__CAN_TYPE__ "CAN Driver " VERSION " (c) " __DATE__  " " __TIME__ "\n");
	printk(" H.J. Oertel (oe@port.de)\n");
	/* printk(" C.Schroeter (clausi@chemie.fu-berlin.de), H.D. Stich\n");  */
	    
    }

    /*
    initialize the variables layed down in /proc/sys/Can
    */
    for (i = 0; i < MAX_CHANNELS; i++) {
	IOModel[i]       = IO_MODEL;
	Baud[i]          = 125;

	AccCode[i]       = AccMask[i] =  STD_MASK;
	Timeout[i]       = 100;
	Outc[i]          = CAN_OUTC_VAL;
	IRQ_requested[i] = 0;
	Can_minors[i]    = i;		/* used as IRQ dev_id */

#if defined(MCF5282)
	/* we have a really fixed address here */
	Base[i] = (MCF_MBAR + 0x1c0000);
	/* Because the MCF FlexCAN is using more then 1 Interrupt vector,
	 * what should be specified here ?
	 * For information purpose let's only specify  the first used here
	 */
	IRQ[i] = 136;
#endif

#if defined(CCPC104)
        pc104_irqsetup();
        IRQ[i]           = 67;          /* The only possible vector on CTRLink's 5282 CPU */
        Base[i]          = 0x40000280;
#endif
    }
    /* after initializing channel based parameters
     * finisch some entries 
     * and do drivers specific initialization
     */
    IOModel[i] = '\0';

    /* CAN_register_dump(); */

#if CAN4LINUX_PCI
    /* make some sysctl entries read only
     * IRQ number
     * Base address
     * and access mode
     * are fixed and provided by the PCI BIOS
     */
    Can_sysctl_table[SYSCTL_IRQ - 1].mode = 0444;
    Can_sysctl_table[SYSCTL_BASE - 1].mode = 0444;
    /* printk("CAN pci test loaded\n"); */
    /* dbgMask = 0; */
    pcimod_scan();
#endif
#if defined(CCPC104)
    /* The only possible interrupt could be IRQ4 on the PC104 Board */
    Can_sysctl_table[SYSCTL_IRQ - 1].mode = 0444;
#endif
#if defined(MCF5282)
    Can_sysctl_table[SYSCTL_BASE - 1].mode = 0444;
#endif

/* #if LDDK_USE_PROCINFO */
/* #error procinfo */
    /* register_procinfo(); */
/* #endif */
/* #if LDDK_USE_SYSCTL */
    register_systables();
/* #endif */

#if LDDK_USE_BLKREQUEST
    blk_dev[Can_major].request_fn = Can_request ;
#endif

    DBGout();
    return 0;
}

/* void cleanup_module(void) */
void can_exit(void)
{
    DBGin("cleanup_module");
#ifndef MODULE
    if (MOD_IN_USE) {
      printk("Can : device busy, remove delayed\n");
    }
    /* printk("CAN: removed successfully \n"); */
#endif
	

#if LDDK_USE_BLKREQUEST
    blk_dev[Can_major].request_fn = NULL ;
#endif
#ifdef LDDK_USE_REGISTER
    if( unregister_chrdev(Can_major, "Can") != 0 ){
        printk("can't unregister Can, device busy \n");
    } else {
        printk("Can successfully removed\n");
    }

#endif
#if LDDK_USE_PROCINFO
    unregister_procinfo();
#endif
#if LDDK_USE_SYSCTL
    unregister_systables();
#endif
    DBGout();
}


module_init(can_init);
module_exit(can_exit);
EXPORT_NO_SYMBOLS;


#endif /* DOXYGEN_SHOULD_SKIP_THIS */
