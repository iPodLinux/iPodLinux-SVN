/*
 * can_write - can4linux CAN driver module
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
 * $Header: /var/cvs/uClinux-2.4.x/drivers/char/can4linux/can_write.c,v 1.2 2003/08/28 00:38:31 gerg Exp $
 *
 *--------------------------------------------------------------------------
 *
 *
 * modification history
 * --------------------
 * $Log: can_write.c,v $
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
* \file can_write.c
* \author Heinz-Jürgen Oertel, port GmbH
* $Revision: 1.2 $
* $Date: 2003/08/28 00:38:31 $
*
*/

#include "can_defs.h"

/* \fn size_t can_write( __LDDK_WRITE_PARAM) */
/***************************************************************************/
/**

\brief size_t write(int fd, const char *buf, size_t count);
write CAN messages to the network
\param fd The descriptor to write to.
\param buf The data buffer to write (array of CAN canmsg_t).
\param count The number of bytes to write.

write  writes  up to \a count CAN messages to the CAN controller
referenced by the file descriptor fd from the buffer
starting at buf.



\par Errors

the following errors can occur

\li \c EBADF  fd is not a valid file descriptor or is not open
              for writing.

\li \c EINVAL fd is attached to an object which is unsuitable for
              writing.

\li \c EFAULT buf is outside your accessible address space.

\li \c EINTR  The call was interrupted by a signal before any
              data was written.



\returns
On success, the number of CAN messages written are returned
(zero indicates nothing was written).
On error, -1 is returned, and errno is set appropriately.

\internal
*/

__LDDK_WRITE_TYPE can_write( __LDDK_WRITE_PARAM )
{
unsigned int minor = __LDDK_MINOR;
msg_fifo_t *TxFifo = &Tx_Buf[minor];
canmsg_t *addr;
canmsg_t tx;
unsigned long flags;
int written        = 0;

    DBGin("can_write");
#ifdef DEBUG_COUNTER
    Cnt1[minor] = Cnt1[minor] + 1;
#endif /* DEBUG_COUNTER */

/* DEBUG_TTY(1, "write: %d", count); */
    DBGprint(DBG_DATA,(" -- write %d msg\n", count));
    /* printk("w[%d/%d]", minor, TxFifo->active); */
    addr = (canmsg_t *)buffer;

    if(verify_area(VERIFY_READ, (canmsg_t *)addr, count * sizeof(canmsg_t))) { 
	    DBGout();return -EINVAL;
    }
    while( written < count ) {
	/* enter critical section */
	save_flags(flags);cli();
	/* there are data to write to the network */
	if(TxFifo->free[TxFifo->head] == BUF_FULL) {
/* DEBUG_TTY(1, "ret buffer full"); */
	    /* there is already one message at this place */;
	    DBGout();
	    /* return -ENOSPC; */
	    return written;
	}
	if( TxFifo->active ) {
	    /* more than one data and actual data in queue */
	    __lddk_copy_from_user(	/* copy one message to FIFO */
		    (canmsg_t *) &(TxFifo->data[TxFifo->head]), 
		    (canmsg_t *) &addr[written],
		    sizeof(canmsg_t) );
/* DEBUG_TTY(1, " fifo active: id %d/%x; head %d/tail %d", */
/* TxFifo->data[TxFifo->head].id, */
/* TxFifo->data[TxFifo->head].id, */
/* TxFifo->head, */
/* TxFifo->tail); */
	    TxFifo->free[TxFifo->head] = BUF_FULL; /* now this entry is FULL */
	    TxFifo->head = ++(TxFifo->head) % MAX_BUFSIZE;
	} else {
	    __lddk_copy_from_user(
		    (canmsg_t *) &tx, 
		    (canmsg_t *) &addr[written],
		    sizeof(canmsg_t) );
/* DEBUG_TTY(1, " fifo in-active: id %d/%x; head %d/tail %d", */
		/* tx.id, tx.id, */
/* TxFifo->head, */
/* TxFifo->tail ); */
	  /* f - fast -- use interrupts */
	  if( count >= 1 ) {
	    /* !!! CHIP abh. !!! */
	    TxFifo->active = 1;
	  }
	  CAN_SendMessage( minor, &tx);  /* Send, no wait */
	}
        written++;
	/* leave critical section */
	restore_flags(flags);
    }
    /* printk("W[%d/%d]", minor, TxFifo->active); */
    DBGout();
    return written;
}

