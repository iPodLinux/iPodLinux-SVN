/*
 * can_ioctl - can4linux CAN driver module
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
 * $Header: /var/cvs/uClinux-2.4.x/drivers/char/can4linux/can_ioctl.c,v 1.2 2003/08/28 00:38:31 gerg Exp $
 *
 *--------------------------------------------------------------------------
 *
 *
 * modification history
 * --------------------
 * $Log: can_ioctl.c,v $
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
* \file can_ioctl.c
* \author Heinz-Jürgen Oertel, port GmbH
* $Revision: 1.2 $
* $Date: 2003/08/28 00:38:31 $
*
*
*/

#include "can_defs.h"

int can_Command(struct inode *inode, int cmd);
int can_Send(struct inode *inode, canmsg_t *Tx);
int can_Receive(struct inode *inode, canmsg_t *Rx);
int can_GetStat(struct inode *inode, CanStatusPar_t *s);
int can_Config(struct inode *inode, int target, unsigned long val1,
					       unsigned long val2);

/***************************************************************************/
/**
*
\brief int ioctl(int fd, int request, ...);
the CAN controllers control interface
\param fd The descriptor to change properties
\param request special configuration request
\param ...  traditional a \a char *argp

The \a ioctl function manipulates the underlying device
parameters of the CAN special device.
In particular, many operating characteristics of
character CAN driver may be controlled with \a ioctl requests.
The argument \a fd must be an open file descriptor.

An ioctl request has encoded in it whether the argument is
an \b in parameter or \b out parameter,
and the size of the argument argp in bytes.
Macros and defines used in specifying an \a ioctl request
are located  in  the  file can4linux.h .

The following \a requests are defined:

\li \c COMMAND some commands for
start, stop and reset the CAN controller chip
\li \c CONFIG configure some of the device properties
like acceptance filtering, bit timings, mode of the output control register
or the optional software message filter configuration(not implemented yet).
\li \c STATUS request the CAN controllers status
\li \c SEND a single message over the \a ioctl interface 
\li \c RECEIVE poll a receive message
\li \c CONFIGURERTR configure automatic rtr responses(not implemented)

The third argument is a parameter structure depending on the request.
These are
\code
struct Command_par
struct Config_par
struct CanSja1000Status_par
struct ConfigureRTR_par
struct Receive_par
struct Send_par
\endcode
described in can4linux.h

\par Acceptance Filtering

\b Basic \b CAN.
In the case of using standard identifiers in Basic CAN mode
for receiving CAN messages
only the low bytes are used to set acceptance code and mask
for bits ID.10 ... ID.3

\par
\b PeliCAN.
For acceptance filtering the entries \c AccCode and \c AccMask are used
like specified in the controllers manual for
\b Single \b Filter \b Configuration .
Both are 4 byte entries.
In the case of using standard identifiers for receiving CAN messages
also all 4 bytes can be used.
In this case two bytes are used for acceptance code and mask
for all 11 identifier bits plus additional the first two data bytes.
The SJA1000 is working in the \b Single \b Filter \ Mode .
\code
       Bits
 mask  31 30 .....           4  3  2  1  0
 code
 -------------------------------------------
 ID    28 27 .....           1  0  R  +--+-> unused
                                   T
                                   R
\endcode

\returns
On success, zero is returned.
On error, -1 is returned, and errno is set appropriately.

\par Example
\code
Config_par_t  cfg;
volatile Command_par_t cmd;


    cmd.cmd = CMD_STOP;
    ioctl(can_fd, COMMAND, &cmd);

    cfg.target = CONF_ACCM; 
    cfg.val    = acc_mask;
    ioctl(can_fd, CONFIG, &cfg);
    cfg.target = CONF_ACCC; 
    cfg.val    = acc_code;
    ioctl(can_fd, CONFIG, &cfg);

    cmd.cmd = CMD_START;
    ioctl(can_fd, COMMAND, &cmd);

\endcode

*/
int can_ioctl( __LDDK_IOCTL_PARAM )
{
void *argp;
int retval = -EIO;
  
  DBGin("can_ioctl");
  DBGprint(DBG_DATA,("cmd=%d", cmd));
  Can_errno = 0;
  
  switch(cmd){

        case COMMAND:
	  if( verify_area(VERIFY_READ, (void *)arg, sizeof(Command_par_t))) {
	     DBGout(); return(retval); 
	  }
	  if( verify_area(VERIFY_WRITE, (void *)arg, sizeof(Command_par_t))) {
	     DBGout(); return(retval); 
	  }
	  argp = (void *) kmalloc( sizeof(Command_par_t) +1 , GFP_KERNEL );
	  __lddk_copy_from_user( (void *) argp,(Command_par_t *) arg,
	  					sizeof(Command_par_t));
	  ((Command_par_t *) argp)->retval =
	  		can_Command(inode, ((Command_par_t *) argp)->cmd );
	  ((Command_par_t *) argp)->error = Can_errno;
	  __lddk_copy_to_user( (Command_par_t *)arg, (void *)argp,
	  					sizeof(Command_par_t));
	  kfree(argp);
	  break;
      case CONFIG:
	  if( verify_area(VERIFY_READ, (void *) arg, sizeof(Config_par_t))) {
	     DBGout(); return(retval); 
	  }
	  if( verify_area(VERIFY_WRITE, (void *) arg, sizeof(Config_par_t))) {
	     DBGout(); return(retval); 
	  }
	  argp = (void *) kmalloc( sizeof(Config_par_t) +1 ,GFP_KERNEL);
	  __lddk_copy_from_user( (void *) argp,(Config_par_t *) arg,
	  					sizeof(Config_par_t));
	  ((Config_par_t *) argp)->retval =
	  		can_Config(inode, ((Config_par_t *) argp)->target, 
			     ((Config_par_t *) argp)->val1,
			     ((Config_par_t *) argp)->val2 );
	  ((Config_par_t *) argp)->error = Can_errno;
	  __lddk_copy_to_user( (Config_par_t *) arg, (void *) argp,
	  					sizeof(Config_par_t));
	  kfree(argp);
	  break;
      case SEND:
	  if( verify_area(VERIFY_READ, (void *) arg, sizeof(Send_par_t))) {
	     DBGout(); return(retval); 
	  }
	  if( verify_area(VERIFY_WRITE, (void *) arg, sizeof(Send_par_t))) {
	     DBGout(); return(retval); 
	  }
	  argp = (void *)kmalloc( sizeof(Send_par_t) +1 ,GFP_KERNEL );
	  __lddk_copy_from_user( (void *) argp, (Send_par_t *)arg,
	  				sizeof(Send_par_t));
	  ((Send_par_t *) argp)->retval =
	  		can_Send(inode, ((Send_par_t *) argp)->Tx );
	  ((Send_par_t *) argp)->error = Can_errno;
	  __lddk_copy_to_user( (Send_par_t *) arg, (void *)argp,
	  				sizeof(Send_par_t));
	  kfree(argp);
	  break;
      case RECEIVE:
	  if( verify_area(VERIFY_READ, (void *) arg, sizeof(Receive_par_t))) {
	     DBGout(); return(retval); 
	  }
	  if( verify_area(VERIFY_WRITE, (void *) arg, sizeof(Receive_par_t))) {
	     DBGout(); return(retval); 
	  }
	  argp = (void *)kmalloc( sizeof(Receive_par_t) +1 ,GFP_KERNEL );
	  __lddk_copy_from_user( (void *)argp, (Receive_par_t *)arg,
	  				sizeof(Receive_par_t));
	  ((Receive_par_t *) argp)->retval =
	  		can_Receive(inode, ((Receive_par_t *) argp)->Rx);
	  ((Receive_par_t *) argp)->error = Can_errno;
	  __lddk_copy_to_user( (Receive_par_t *)arg, (void *) argp,
	  				sizeof(Receive_par_t));
	  kfree(argp);
	  break;
      case STATUS:
	  if( verify_area(VERIFY_READ, (void *) arg,
	  				sizeof(CanStatusPar_t))) {
	     DBGout(); return(retval); 
	  }
	  if( verify_area(VERIFY_WRITE, (void *) arg,
	  			sizeof(CanStatusPar_t))) {
	     DBGout(); return(retval); 
	  }
	  argp = (void *)kmalloc( sizeof(CanStatusPar_t) +1 ,GFP_KERNEL );
	  ((CanStatusPar_t *) argp)->retval =
	  		can_GetStat(inode, ((CanStatusPar_t *)argp));
	  __lddk_copy_to_user( (CanStatusPar_t *)arg, (void *) argp,
	  				sizeof(CanStatusPar_t));
	  kfree(argp);
	  break;

#ifdef CAN_RTR_CONFIG
      case CONFIGURERTR:
	  if( verify_area(VERIFY_READ, (void *) arg,
	  			sizeof(ConfigureRTR_par_t))){
	     DBGout(); return(retval); 
	  }
	  if( verify_area(VERIFY_WRITE, (void *) arg,
	  			sizeof(ConfigureRTR_par_t))){
	     DBGout(); return(retval); 
	  }
	  argp = (void *)kmalloc( sizeof(ConfigureRTR_par_t) +1 ,GFP_KERNEL );
	  __lddk_copy_from_user( (void *) argp,(ConfigureRTR_par_t *) arg,
	  			sizeof(ConfigureRTR_par_t));
	  ((ConfigureRTR_par_t *) argp)->retval =
	  	can_ConfigureRTR(inode,
	  			((ConfigureRTR_par_t *) argp)->message, 
				((ConfigureRTR_par_t *) argp)->Tx );
	  ((ConfigureRTR_par_t *) argp)->error = Can_errno;
	  __lddk_copy_to_user( (ConfigureRTR_par_t *) arg, (void *) argp,
	  			sizeof(ConfigureRTR_par_t));
	  kfree(argp);
	  break;

#endif  	/* CAN_RTR_CONFIG */
  
      default:
        DBGout();
	return -EINVAL;
    }
    DBGout();
    return 1;
}



#ifndef DOXYGEN_SHOULD_SKIP_THIS


/* 
* ioctl functions are following here 
*/


int can_Command(struct inode *inode, int cmd)
{
unsigned int minor = MINOR(inode->i_rdev);

    DBGprint(DBG_DATA,("cmd=%d", cmd));
    switch (cmd) {
      case CMD_START:
	    CAN_StartChip(minor);
	    break;
      case CMD_STOP:
	    CAN_StopChip(minor);
	    break;
      case CMD_RESET:
	    CAN_ChipReset(minor);
	    break;
      default:
	    DBGout();
	    return(-EINVAL);
    }
    return 0;
}

/* is not very useful! use it if you are sure the tx queu is empty */
int can_Send(struct inode *inode, canmsg_t *Tx)
{
unsigned int minor = MINOR(inode->i_rdev);	
canmsg_t tx;

    if( verify_area(VERIFY_READ,Tx,sizeof(canmsg_t)) ) {
	    return -EINVAL;
    }
    __lddk_copy_from_user((canmsg_t *) &tx, (canmsg_t *) Tx,sizeof(canmsg_t));
    return CAN_SendMessage(minor, &tx);
}


int can_Receive(
	struct inode *inode,
	canmsg_t *Rx
	)
{

unsigned int minor = MINOR(inode->i_rdev);
canmsg_t rx;
int len;

    len = CAN_GetMessage(minor,&rx);

    if( len > 0 ){
       /* printk("\nrx[ ] got id 0x%x len %d adr00x%lx\n",rx.id,rx.length,Rx); */
       if( verify_area(VERIFY_WRITE,Rx,sizeof(canmsg_t) ) ) {
	    return -EINVAL;
       }
       __lddk_copy_to_user((canmsg_t *) Rx, (canmsg_t *) &rx,
       						sizeof(canmsg_t));
       return rx.length;
    } else {
     /* no message availiable */
     return 0;
    }
}


int can_Config(
	struct inode *inode,
	int target,
	unsigned long val1,
	unsigned long val2
	)
{
unsigned int minor = MINOR(inode->i_rdev);	

    switch(target) {
      case CONF_ACC:
    DBGprint(DBG_DATA,("mask = 0x%lx, code = 0x%lx", val1, val2));
	   AccMask[minor] = val1;
	   AccCode[minor] = val2;
	   CAN_SetMask(minor, AccCode[minor], AccMask[minor]);		
	   break;
      case CONF_ACCM:
    DBGprint(DBG_DATA,("acc_mask=0x%lx", val1));
	   AccMask[minor] = val1;
	   CAN_SetMask(minor, AccCode[minor], AccMask[minor]);		
	   break;
      case CONF_ACCC:
    DBGprint(DBG_DATA,("acc_code=0x%lx", val1));
	   AccCode[minor] = val1;
	   CAN_SetMask(minor, AccCode[minor], AccMask[minor]);		
	   break;
      case CONF_TIMING:
	   Baud[minor] = val1;
	   CAN_SetTiming(minor,(int) val1);   
	   break;                    
      case CONF_OMODE:
	   CAN_SetOMode( minor, (int) val1);
	   break;			
#if CAN_USE_FILTER
      case CONF_FILTER:
	   Can_FilterOnOff( minor, (int) val1 );
	   break;
      case CONF_FENABLE:
	   Can_FilterMessage( minor, (int) val1, 1);
	   break;
      case CONF_FDISABLE:
	   Can_FilterMessage( minor, (int) val1, 0);
	   break;
#endif
      default:
	    DBGout();
	    return(-EINVAL);
    }
    return 0;
}
#endif /* DOXYGEN_SHOULD_SKIP_THIS */
