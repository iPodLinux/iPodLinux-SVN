/*
 * can_open - can4linux CAN driver module
 *
 * can4linux -- LINUX CAN device driver source
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * 
 * Copyright (c) 2001 port GmbH Halle/Saale
 * (c) 2001 Heinz-Jürgen Oertel (oe@port.de)
 *          Claus Schroeter (clausi@chemie.fu-berlin.de)
 *------------------------------------------------------------------
 * $Header: /var/cvs/uClinux-2.4.x/drivers/char/can4linux/can_open.c,v 1.2 2003/08/28 00:38:31 gerg Exp $
 *
 *--------------------------------------------------------------------------
 *
 *
 * modification history
 * --------------------
 * $Log: can_open.c,v $
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
 *--------------------------------------------------------------------------
 */


/**
* \file can_open.c
* \author Heinz-Jürgen Oertel, port GmbH
* $Revision: 1.2 $
* $Date: 2003/08/28 00:38:31 $
*
*
*/


/* header of standard C - libraries */
/* #include <linux/module.h>			 */

/* header of common types */

/* shared common header */

/* header of project specific types */

/* project headers */
#include "can_defs.h"

/* local header */

/* constant definitions
---------------------------------------------------------------------------*/

/* local defined data types
---------------------------------------------------------------------------*/

/* list of external used functions, if not in headers
---------------------------------------------------------------------------*/

/* list of global defined functions
---------------------------------------------------------------------------*/

/* list of local defined functions
---------------------------------------------------------------------------*/

/* external variables
---------------------------------------------------------------------------*/

/* global variables
---------------------------------------------------------------------------*/
int Can_isopen[4] = { 0, 0, 0, 0};   /* device minor already opened */

/* local defined variables
---------------------------------------------------------------------------*/
/* static char _rcsid[] = "$Id: can_open.c,v 1.2 2003/08/28 00:38:31 gerg Exp $"; */


/***************************************************************************/
/**
*
* \brief int open(const char *pathname, int flags);
* opens the CAN device for following operations
* \param pathname device pathname, usual /dev/can?
* \param flags is one of \c O_RDONLY, \c O_WRONLY or \c O_RDWR which request
*       opening  the  file  read-only,  write-only  or read/write,
*       respectively.
*
*
* The open call is used to "open" the device.
* Doing a first initialization according the to values in the /proc/sys/Can
* file system.
* Additional an ISR function is assigned to the IRQ.
*
* The CLK OUT pin is configured for creating the same frequency
* like the chips input frequency fclk (XTAL). 
*
* If Vendor Option \a VendOpt is set to 's' the driver performs
* an hardware reset befor initializing the chip.
*
* \returns
* open return the new file descriptor,
* or -1 if an error occurred (in which case, errno is set appropriately).
*
* \par ERRORS
* the following errors can occur
* \arg \c ENXIO  the file is a device special file
* and no corresponding device exists.
* \arg \c EINVAL illegal \b minor device number
* \arg \c EINVAL wrong IO-model format in /proc/sys/Can/IOmodel
* \arg \c EBUSY  IRQ for hardware is not available
* \arg \c EBUSY  I/O region for hardware is not available

*/
int can_open( __LDDK_OPEN_PARAM )
{
int retval = 0;

    DBGin("can_open");
    {

	int lasterr;	

	unsigned int minor = __LDDK_MINOR;

	if( minor > MAX_CHANNELS )
	{
	    printk("CAN: Illegal minor number %d\n", minor);
	    DBGout();
	    return -EINVAL;
	}
	/* check if device is already open, should be used only by one process */

#ifdef UNSAFE
	/* if(Can_isopen[minor]) { */
	    /* MOD_DEC_USE_COUNT; */
	    /* DBGout(); */
	    /* return -ENXIO; */
	/* } */
	++Can_isopen[minor];		/* flag device in use */
#else
	if(Can_isopen[minor] == 1) {
	    DBGout();
	    return -ENXIO;
	}
	Can_isopen[minor] = 1;		/* flag device in use */
#endif
	if( Base[minor] == 0x00) {
	    /* No device available */
	    printk("CAN[%d]: no device available\n", minor);
	    DBGout();
	    return -ENXIO;
	}

	/* the following does all the board specific things
	   also memory remapping if necessary */
	if( (lasterr = CAN_VendorInit(minor)) < 0 ){
	    DBGout();
	    return lasterr;
	}
	/* Access macros based in can_base[] should work now */
	/* CAN_ShowStat(minor); */

/* controller_available(curr + 0x400, 4); */
	Can_WaitInit(minor);	/* initialize wait queue for select() */
	Can_FifoInit(minor);
#if CAN_USE_FILTER
	Can_FilterInit(minor);
#endif

	if( CAN_ChipReset(minor) < 0 ) {
	    DBGout();
	    return -EINVAL;
	}
	CAN_StartChip(minor);
#if DEBUG
    CAN_ShowStat(minor);
#endif
    }

    MOD_INC_USE_COUNT;
    DBGout();
    return retval;
}

