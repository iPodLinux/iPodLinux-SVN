/*
 * linux/fec_ioctl.h: definitions for 5272 ColdFire Ethernet IOCTL calls
 *
 * Mostly a copy of linux/mii.h: definitions for MII-compatible transceivers
 * Originally drivers/net/sunhme.h.
 *
 * Copyright (C) 1996, 1999, 2001 David S. Miller (davem@redhat.com)
 *
 * Copyright (C) 2002 Arcturus Networks Inc. 
 *               by MaTed (Ted Ma - mated@ArcturusNetworks.comm)
 */

#ifndef __LINUX_MII_H__
#define __LINUX_MII_H__


#include <linux/types.h>
#include <linux/autoconf.h>

#ifdef CONFIG_FEC 
 
/*********
 * This structure is used in all SIOCxMIIxxx ioctl calls and
 * is provided only as a reference comment
struct mii_ioctl_data {
        u16             phy_id;
        u16             reg_num;
        u16             val_in;
        u16             val_out;
};
**********/

/************************
 * Defines for LXT972a
 *  Status Register 2
 ************************/
#define LXT972_STAT2		17	
    /* Reserved                            0x8000  */
#define PHY_STATUS_REG_TWO_MODE_100             0x4000
#define PHY_STATUS_REG_TWO_TX_STATUS            0x2000
#define PHY_STATUS_REG_TWO_RX_STATUS            0x1000
#define PHY_STATUS_REG_TWO_COLLISION_STATUS     0x0800
#define PHY_STATUS_REG_TWO_LINK                 0x0400
#define PHY_STATUS_REG_TWO_DUPLEX_MODE          0x0200
#define PHY_STATUS_REG_TWO_AUTO_NEGOTIATION     0x0100
#define PHY_STATUS_REG_TWO_AUTO_NEGO_COMPELETE  0x0080
    /* Reserved                                 0x0040  */
#define PHY_STATUS_REG_TWO_POLARITY             0x0020
#define PHY_STATUS_REG_TWO_PAUSE                0x0010
#define PHY_STATUS_REG_TWO_ERROR                0x0008
    /* Reserved                                 0x0004  */
    /* Reserved                                 0x0002  */
    /* Reserved                                 0x0001  */

// Defines for value_in for subcommands
#define SIOC_MIIREG_GET		0	// get register / data
#define SIOC_MIIREG_QUERY	1	// query previous "get" register /data

// Error return codes (in link_speed and duplex
#define SIOC_FEC_RC_RELINK		-6 	// link staus changed
#define SIOC_FEC_RC_INVALID_SUBCMND	-5	// invalid subcommand
#define SIOC_FEC_RC_WRONG_REG	-4	// wrong register
#define SIOC_FEC_RC_NO_REQUEST	-3	// no outstanding request or exists already
#define SIOC_FEC_RC_NOT_COMP	-2	// queued command not completed
#define SIOC_FEC_RC_NO_LINK		-1	// Link is down

/* This structure is used for special FEC only ioctl calls */
struct fec_ioctl_data
{
  unsigned short   phy_id;	/* For SIOMGMIIPHY
							   - returns phy addr / number associated with 
							   device
							   For SIOCSMIIREG and SIOCGMIIREG
							   - should be set to 1 for future uC5272 

							   compatibility
							*/
  unsigned short	reg; 	/* Which register to be read / set
							   - must be set to 17 for link status inquiry */
  unsigned short	value_in; 	/* for SIOCSMIIREG
								   - value to be written to the PHY register
								   for SIOCGMIIREG
								   - set to 0 for GET
								   - set to 1 for QUERY (gets results of
								   previous GET, if completed) 
								*/
  unsigned short   value_out;	/* returns the value of the previously 
								   requested register.
								   the value of Status register 2 (register 17
								   of Intel lxt972a PHY (see lxt972a datasheet
								   for explanation)) if register == 17, sets
								   the returnvalues for link_speed and duplex
								*/
  unsigned short   link_speed;	   /* returns current link speed
                           -5 => invalid subcommand
                           -4 => wrong register
                           -3 => no outstanding command request
                           -2 => command not yet completed
                           -1 => undetermined (ie no link established)
                            0 => 1 Mbps
                            1 => 10 Mbps
                            2 => 100 Mbps
                            3 => 1 Gbps
                            4 => 10 Gpbs
						*/
  unsigned short   duplex;   	    /* returns established duplex of the link
                           -5 => invalid subcommand
                           -4 => wrong register
                           -3 => no outstanding command request
                           -2 => command not yet completed
                           -1 => undetermined (ie no link established)
                            0 => half duplex
                            1 => full duplex
						*/
};

#endif	// CONFIG_FEC
#endif	// __LINUX_MII_H__
