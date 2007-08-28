/* can_select
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
 * $Header: /var/cvs/uClinux-2.4.x/drivers/char/can4linux/can_select.c,v 1.1 2003/07/18 00:11:46 gerg Exp $
 *
 *--------------------------------------------------------------------------
 *
 *
 * modification history
 * --------------------
 * $Log: can_select.c,v $
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
 */
#include "can_defs.h"

#if  LINUX_VERSION_CODE >= 0x020200
unsigned int can_select( __LDDK_SELECT_PARAM )
#else
int can_select( __LDDK_SELECT_PARAM )
#endif
{

unsigned int minor = __LDDK_MINOR;
msg_fifo_t *RxFifo = &Rx_Buf[minor];
    DBGin("can_select");
	    DBGprint(DBG_DATA,("minor = %d", minor));
#if 0
	    DBGprint(DBG_DATA,("file = %p", file));
	    ;
	    DBGprint(DBG_DATA,("s(CanWait[]) = %2d", sizeof(CanWait[0])));
	    ;
	    /* DBGprint(DBG_DATA,("s(CanWait)   = %2d", sizeof(CanWait))); */
	    ;
	    DBGprint(DBG_DATA,("&CanWait[] = %p", &CanWait[0]));
	    DBGprint(DBG_DATA,("CanWait[minor].task_list.n = %p", CanWait[minor].task_list.next));
	    DBGprint(DBG_DATA,("CanWait[minor].task_list.p = %p", CanWait[minor].task_list.prev));
	    DBGprint(DBG_DATA,("&CanWait[minor]->task_list.p = %p", (&CanWait[minor])->task_list.prev));

	    DBGprint(DBG_DATA,("wait = %p", wait));
	    if(wait) {
	    DBGprint(DBG_DATA,("wait->error = %d", wait->error));
	    DBGprint(DBG_DATA,("wait->table = %p", wait->table));
	    } else {
	    DBGprint(DBG_DATA,("can not dereference wait components"));
	    }
#endif
#ifdef DEBUG
    CAN_ShowStat(minor);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,0)
    DBGprint(DBG_BRANCH,("POLL: fifo empty,poll waiting...\n"));

    /* every event queue that could wake up the process
     * and change the status of the poll operation
     * can be added to the poll_table structure by
     * calling the function poll_wait:  
     */
                /*     _select para, wait queue, _select para */
/* X */		poll_wait(file, &CanWait[minor] , wait);

    DBGprint(DBG_BRANCH,("POLL: wait returned \n"));
    if( RxFifo->head != RxFifo->tail ) {
	/* fifo has some telegrams */
	/* Return a bit mask
	 * describing operations that could be immediately performed
	 * without blocking.
	 */
	DBGout();
	/*
	 * POLLIN This bit must be set
	 *        if the device can be read without blocking. 
	 * POLLRDNORM This bit must be set
	 * if "normal'' data is available for reading.
	 * A readable device returns (POLLIN | POLLRDNORM)
	 *
	 *
	 *
	 */
	return POLLIN | POLLRDNORM;
    }
    DBGout();return 0;

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,3)
    DBGprint(DBG_BRANCH,("POLL: fifo empty,poll waiting...\n"));
    poll_wait(file, &CanWait[minor] , wait);
    DBGprint(DBG_BRANCH,("POLL: wait returned \n"));
    if( RxFifo->head != RxFifo->tail ) {
	/* fifo has some telegrams */
	DBGout();
	return POLLIN | POLLRDNORM;
    }
    DBGout();return 0;

#else
    switch (sel_type) {
	case SEL_IN:
	    DBGprint(DBG_BRANCH,("sel_in \n"));
	    if( RxFifo->head == RxFifo->tail ) {
		DBGprint(DBG_BRANCH,("fifo empty \n"));
		select_wait(&CanWait[minor],wait);
		DBGout();return 0;
	    }
	    break;
	case SEL_OUT:
	    DBGprint(DBG_BRANCH,("sel_out \n"));
	    /* ready for write ? */
	    select_wait(&CanWait[minor],wait);
	    DBGout();return 0;
    }
    DBGout();return 1;
#endif

        
    DBGout();
    return 0;
}
