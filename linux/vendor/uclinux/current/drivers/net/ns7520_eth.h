/***********************************************************************
 *
 *  Copyright 2003 by FS Forth-Systeme GmbH.
 *  All rights reserved.
 *
 *  $Id: ns7520_eth.h,v 1.1 2003/08/28 23:34:23 davidm Exp $
 *  @Author: Markus Pietrek
 *  @Descr: Defines the NS7520 ethernet registers.
 *          Stick with the old ETH prefix names instead going to the
 *          new EFE names in the manual.
 *          NS7520_ETH_* refer to NS7520 Hardware
 *           Reference/January 2003 [1]
 *          PHY_LXT971_* refer to Intel LXT971 Datasheet 
 *           #249414 Rev. 02 [2]
 *          Partly derived from netarm_eth_module.h
 *          
 *  
 ***********************************************************************/

#ifndef FS_NS7520_ETH_H
#define FS_NS7520_ETH_H

/* The port addresses */

#define	NS7520_ETH_MODULE_BASE	 (0xFF800000)
#define get_eth_reg_addr(c) \
     ((volatile unsigned int*) ( NS7520_ETH_MODULE_BASE + ( c ) ) )

#define NS7520_ETH_EGCR		 (0x0000)     // Ethernet Gen Control
#define NS7520_ETH_EGSR		 (0x0004)     // Ethernet Gen Status
#define NS7520_ETH_FIFO		 (0x0008)     // FIFO Data
#define NS7520_ETH_FIFOL	 (0x000C)     // FIFO Data Last
#define NS7520_ETH_ETSR		 (0x0010)     // Ethernet Transmit Status
#define NS7520_ETH_ERSR		 (0x0014)     // Ethernet Receive Status
#define NS7520_ETH_MAC1		 (0x0400)     // MAC Config 1
#define NS7520_ETH_MAC2		 (0x0404)     // MAC Config 2
#define NS7520_ETH_IPGT		 (0x0408)     // Back2Back InterPacket Gap
#define NS7520_ETH_IPGR		 (0x040C)     // non back2back InterPacket Gap
#define NS7520_ETH_CLRT		 (0x0410)     // Collision Window/Retry
#define NS7520_ETH_MAXF		 (0x0414)     // Maximum Frame Register
#define NS7520_ETH_SUPP		 (0x0418)     // PHY Support
#define NS7520_ETH_TEST		 (0x041C)     // Test Register
#define NS7520_ETH_MCFG		 (0x0420)     // MII Management Configuration
#define NS7520_ETH_MCMD		 (0x0424)     // MII Management Command
#define NS7520_ETH_MADR		 (0x0428)     // MII Management Address
#define NS7520_ETH_MWTD		 (0x042C)     // MII Management Write Data
#define NS7520_ETH_MRDD		 (0x0430)     // MII Management Read Data
#define NS7520_ETH_MIND		 (0x0434)     // MII Management Indicators
#define NS7520_ETH_SMII		 (0x0438)     // SMII Status Register
#define NS7520_ETH_SA1		 (0x0440)     // Station Address 1
#define NS7520_ETH_SA2		 (0x0444)     // Station Address 2
#define NS7520_ETH_SA3		 (0x0448)     // Station Address 3
#define NS7520_ETH_SAFR		 (0x05C0)     // Station Address Filter
#define NS7520_ETH_HT1		 (0x05D0)     // Hash Table 1
#define NS7520_ETH_HT2		 (0x05D4)     // Hash Table 2
#define NS7520_ETH_HT3		 (0x05D8)     // Hash Table 3
#define NS7520_ETH_HT4		 (0x05DC)     // Hash Table 4

// EGCR Ethernet General Control Register Bit Fields

#define NS7520_ETH_EGCR_ERX	 (0x80000000) // Enable Receive FIFO
#define NS7520_ETH_EGCR_ERXDMA	 (0x40000000) // Enable Receive DMA
#define NS7520_ETH_EGCR_ERXLNG	 (0x20000000) // Accept Long packets
#define NS7520_ETH_EGCR_ERXSHT	 (0x10000000) // Accept Short packets
#define NS7520_ETH_EGCR_ERXREG	 (0x08000000) // Enable Receive Data Interrupt
#define NS7520_ETH_EGCR_ERFIFOH	 (0x04000000) // Enable Receive Half-Full Int
#define NS7520_ETH_EGCR_ERXBR	 (0x02000000) // Enable Receive buffer ready
#define NS7520_ETH_EGCR_ERXBAD	 (0x01000000) // Accept bad receive packets
#define NS7520_ETH_EGCR_ETX	 (0x00800000) // Enable Transmit FIFO
#define NS7520_ETH_EGCR_ETXDMA	 (0x00400000) // Enable Transmit DMA
#define NS7520_ETH_EGCR_ETXWM_R  (0x00300000) // Enable Transmit FIFO mark Reserv
#define NS7520_ETH_EGCR_ETXWM_75 (0x00200000) // Enable Transmit FIFO mark 75%
#define NS7520_ETH_EGCR_ETXWM_50 (0x00100000) // Enable Transmit FIFO mark 50%
#define NS7520_ETH_EGCR_ETXWM_25 (0x00000000) // Enable Transmit FIFO mark 25%
#define NS7520_ETH_EGCR_ETXREG	 (0x00080000) // Enable Transmit Data Read Int
#define NS7520_ETH_EGCR_ETFIFOH	 (0x00040000) // Enable Transmit Fifo Half Int
#define NS7520_ETH_EGCR_ETXBC	 (0x00020000) // Enable Transmit Buffer Compl Int
#define NS7520_ETH_EGCR_EFULLD	 (0x00010000) // Enable Full Duplex Operation
#define NS7520_ETH_EGCR_MODE_MA  (0x0000C000) // Mask
#define NS7520_ETH_EGCR_MODE_SEE (0x0000C000) // 10 Mbps SEEQ ENDEC PHY
#define NS7520_ETH_EGCR_MODE_LEV (0x00008000) // 10 Mbps Level1 ENDEC PHY
#define NS7520_ETH_EGCR_RES1     (0x00002000) // Reserved
#define NS7520_ETH_EGCR_RXCINV	 (0x00001000) // Invert the receive clock input
#define NS7520_ETH_EGCR_TXCINV	 (0x00000800) // Invert the transmit clock input
#define NS7520_ETH_EGCR_PNA	 (0x00000400) // pSOS pNA buffer 
#define NS7520_ETH_EGCR_MAC_RES	 (0x00000200) // MAC Software reset
#define NS7520_ETH_EGCR_ITXA	 (0x00000100) // Insert Transmit Source Address
#define NS7520_ETH_EGCR_ENDEC_MA (0x000000FC) // ENDEC media control bits
#define NS7520_ETH_EGCR_EXINT_MA (0x00000003) // Mask
#define NS7520_ETH_EGCR_EXINT_RE (0x00000003) // Reserved
#define NS7520_ETH_EGCR_EXINT_TP (0x00000002) // TP-PMD Mode
#define NS7520_ETH_EGCR_EXINT_10 (0x00000001) // 10-MBit Mode
#define NS7520_ETH_EGCR_EXINT_NO (0x00000000) // MII normal operation

// EGSR Ethernet General Status Register Bit Fields

#define NS7520_ETH_EGSR_RES1	 (0xC0000000) // Reserved
#define NS7520_ETH_EGSR_RXFDB_MA (0x30000000) // Receive FIFO mask
#define NS7520_ETH_EGSR_RXFDB_3	 (0x30000000) // Receive FIFO 3 bytes available
#define NS7520_ETH_EGSR_RXFDB_2	 (0x20000000) // Receive FIFO 2 bytes available
#define NS7520_ETH_EGCR_RXFDB_1	 (0x10000000) // Receive FIFO 1 Bytes available
#define NS7520_ETH_EGCR_RXFDB_4	 (0x00000000) // Receive FIFO 4 Bytes available
#define NS7520_ETH_EGSR_RXREGR	 (0x08000000) // Receive Register Ready
#define NS7520_ETH_EGSR_RXFIFOH	 (0x04000000) // Receive FIFO Half Full
#define NS7520_ETH_EGSR_RXBR	 (0x02000000) // Receive Buffer Ready
#define NS7520_ETH_EGSR_RXSKIP	 (0x01000000) // Receive Buffer Skip
#define NS7520_ETH_EGSR_RES2	 (0x00F00000) // Reserved
#define NS7520_ETH_EGSR_TXREGE	 (0x00080000) // Transmit Register Empty
#define NS7520_ETH_EGSR_TXFIFOH	 (0x00040000) // Transmit FIFO half empty
#define NS7520_ETH_EGSR_TXBC	 (0x00020000) // Transmit buffer complete
#define NS7520_ETH_EGSR_TXFIFOE	 (0x00010000) // Transmit FIFO empty
#define NS7520_ETH_EGSR_RXPINS	 (0x0000FC00) // ENDEC Phy Status
#define NS7520_ETH_EGSR_RES3	 (0x000003FF) // Reserved

// ETSR Ethernet Transmit Status Register Bit Fields

#define NS7520_ETH_ETSR_RES1	 (0xFFFF0000) // Reserved
#define NS7520_ETH_ETSR_TXOK	 (0x00008000) // Packet transmitted OK
#define NS7520_ETH_ETSR_TXBR	 (0x00004000) // Broadcast packet transmitted
#define NS7520_ETH_ETSR_TXMC	 (0x00002000) // Multicast packet transmitted
#define NS7520_ETH_ETSR_TXAL	 (0x00001000) // Transmit abort - late collision
#define NS7520_ETH_ETSR_TXAED	 (0x00000800) // Transmit abort - deferral
#define NS7520_ETH_ETSR_TXAEC	 (0x00000400) // Transmit abort - exc collisions
#define NS7520_ETH_ETSR_TXAUR	 (0x00000200) // Transmit abort - underrun
#define NS7520_ETH_ETSR_TXAJ	 (0x00000100) // Transmit abort - jumbo
#define NS7520_ETH_ETSR_RES2	 (0x00000080) // Reserved
#define NS7520_ETH_ETSR_TXDEF	 (0x00000040) // Transmit Packet Deferred
#define NS7520_ETH_ETSR_TXCRC	 (0x00000020) // Transmit CRC error
#define NS7520_ETH_ETSR_RES3	 (0x00000010) // Reserved
#define NS7520_ETH_ETSR_TXCOLC   (0x0000000F) // Transmit Collision Count

// ERSR Ethernet Receive Status Register Bit Fields

#define NS7520_ETH_ERSR_RXSIZE	 (0xFFFF0000) // Receive Buffer Size
#define NS7520_ETH_ERSR_RXCE	 (0x00008000) // Receive Carrier Event 
#define NS7520_ETH_ERSR_RXDV	 (0x00004000) // Receive Data Violation Event
#define NS7520_ETH_ERSR_RXOK	 (0x00002000) // Receive Packet OK
#define NS7520_ETH_ERSR_RXBR	 (0x00001000) // Receive Broadcast Packet
#define NS7520_ETH_ERSR_RXMC	 (0x00000800) // Receive Multicast Packet
#define NS7520_ETH_ERSR_RXCRC	 (0x00000400) // Receive Packet has CRC error
#define NS7520_ETH_ERSR_RXDR	 (0x00000200) // Receive Packet has dribble error
#define NS7520_ETH_ERSR_RXCV	 (0x00000100) // Receive Packet code violation
#define NS7520_ETH_ERSR_RXLNG	 (0x00000080) // Receive Packet too long
#define NS7520_ETH_ERSR_RXSHT	 (0x00000040) // Receive Packet too short
#define NS7520_ETH_ERSR_ROVER	 (0x00000020) // Recive overflow
#define NS7520_ETH_ERSR_RES	 (0x0000001F) // Reserved

// MAC1 MAC Configuration Register 1 Bit Fields

#define NS7520_ETH_MAC1_RES1 	 (0xFFFF0000) // Reserved
#define NS7520_ETH_MAC1_SRST	 (0x00008000) // Soft Reset
#define NS7520_ETH_MAC1_SIMMRST	 (0x00004000) // Simulation Reset
#define NS7520_ETH_MAC1_RES2	 (0x00003000) // Reserved
#define NS7520_ETH_MAC1_RPEMCSR	 (0x00000800) // Reset PEMCS/RX
#define NS7520_ETH_MAC1_RPERFUN	 (0x00000400) // Reset PERFUN
#define NS7520_ETH_MAC1_RPEMCST	 (0x00000200) // Reset PEMCS/TX
#define NS7520_ETH_MAC1_RPETFUN	 (0x00000100) // Reset PETFUN
#define NS7520_ETH_MAC1_RES3	 (0x000000E0) // Reserved
#define NS7520_ETH_MAC1_LOOPBK	 (0x00000010) // Internal Loopback
#define NS7520_ETH_MAC1_TXFLOW	 (0x00000008) // TX flow control
#define NS7520_ETH_MAC1_RXFLOW	 (0x00000004) // RX flow control
#define NS7520_ETH_MAC1_PALLRX	 (0x00000002) // Pass ALL receive frames
#define NS7520_ETH_MAC1_RXEN	 (0x00000001) // Receive enable

// MAC Configuration Register 2 Bit Fields

#define NS7520_ETH_MAC2_RES1 	 (0xFFFF8000) // Reserved
#define NS7520_ETH_MAC2_EDEFER	 (0x00004000) // Excess Deferral
#define NS7520_ETH_MAC2_BACKP	 (0x00002000) // Backpressure/NO back off
#define NS7520_ETH_MAC2_NOBO	 (0x00001000) // No back off
#define NS7520_ETH_MAC2_RES2	 (0x00000C00) // Reserved
#define NS7520_ETH_MAC2_LONGP	 (0x00000200) // Long Preable enforcement
#define NS7520_ETH_MAC2_PUREP	 (0x00000100) // Pure preamble enforcement
#define NS7520_ETH_MAC2_AUTOP	 (0x00000080) // Auto detect PAD enable
#define NS7520_ETH_MAC2_VLANP	 (0x00000040) // VLAN pad enable
#define NS7520_ETH_MAC2_PADEN  	 (0x00000020) // PAD/CRC enable
#define NS7520_ETH_MAC2_CRCEN	 (0x00000010) // CRC enable
#define NS7520_ETH_MAC2_DELCRC	 (0x00000008) // Delayed CRC
#define NS7520_ETH_MAC2_HUGE	 (0x00000004) // Huge frame enable
#define NS7520_ETH_MAC2_FLENC	 (0x00000002) // Frame length checking
#define NS7520_ETH_MAC2_FULLD	 (0x00000001) // Full duplex

// IPGT Back-to-Back Inter-Packet-Gap Register Bit Fields

#define NS7520_ETH_IPGT_RES	 (0xFFFFFF80) // Reserved
#define NS7520_ETH_IPGT_IPGT	 (0x0000007F) // Back-to-Back Interpacket Gap

// IPGR Non Back-to-Back Inter-Packet-Gap Register Bit Fields

#define NS7520_ETH_IPGR_RES1	 (0xFFFF8000) // Reserved
#define NS7520_ETH_IPGR_IPGR1	 (0x00007F00) // Non Back-to-back Interpacket Gap
#define NS7520_ETH_IPGR_RES2	 (0x00000080) // Reserved
#define NS7520_ETH_IPGR_IPGR2	 (0x0000007F) // Non back-to-back Interpacket Gap

// CLRT Collision Windows/Collision Retry Register Bit Fields

#define NS7520_ETH_CLRT_RES1	 (0xFFFFC000) // Reserved
#define NS7520_ETH_CLRT_CWIN	 (0x00003F00) // Collision Windows
#define NS7520_ETH_CLRT_RES2	 (0x000000F0) // Reserved
#define	NS7520_ETH_CLRT_RETX	 (0x0000000F) // Retransmission maximum

// MAXF Maximum Frame Register Bit Fields

#define NS7520_ETH_MAXF_RES1	 (0xFFFF0000) // Reserved
#define NS7520_ETH_MAXF_MAXF	 (0x0000FFFF) // Maximum frame length

// SUPP PHY Support Register Bit Fields

#define NS7520_ETH_SUPP_RES1	 (0xFFFFFF00) // Reserved
#define NS7520_ETH_SUPP_RPE100X	 (0x00000080) // Reset PE100X module
#define NS7520_ETH_SUPP_FORCEQ	 (0x00000040) // Force Quit
#define NS7520_ETH_SUPP_NOCIPH	 (0x00000020) // No Cipher
#define NS7520_ETH_SUPP_DLINKF	 (0x00000010) // Disable link fail
#define NS7520_ETH_SUPP_RPE10T	 (0x00000008) // Reset PE10T module
#define NS7520_ETH_SUPP_RES2	 (0x00000004) // Reserved
#define NS7520_ETH_SUPP_JABBER	 (0x00000002) // Enable Jabber protection
#define NS7520_ETH_SUPP_BITMODE	 (0x00000001) // Bit Mode

// TEST Register Bit Fields

#define NS7520_ETH_TEST_RES1	 (0xFFFFFFF8) // Reserved
#define NS7520_ETH_TEST_TBACK	 (0x00000004) // Test backpressure
#define NS7520_ETH_TEST_TPAUSE	 (0x00000002) // Test Pause
#define NS7520_ETH_TEST_SPQ	 (0x00000001) // Shortcut pause quanta

// MCFG MII Management Configuration Register Bit Fields

#define NS7520_ETH_MCFG_RES1	 (0xFFFF0000) // Reserved
#define NS7520_ETH_MCFG_RMIIM	 (0x00008000) // Reset MII management
#define NS7520_ETH_MCFG_RES2	 (0x00007FE0) // Reserved
#define NS7520_ETH_MCFG_CLKS_MA	 (0x0000001C) // Clock Select
#define NS7520_ETH_MCFG_CLKS_4	 (0x00000004) // Sysclk / 4
#define NS7520_ETH_MCFG_CLKS_6	 (0x00000008) // Sysclk / 6
#define NS7520_ETH_MCFG_CLKS_8	 (0x0000000C) // Sysclk / 8
#define NS7520_ETH_MCFG_CLKS_10	 (0x00000010) // Sysclk / 10
#define NS7520_ETH_MCFG_CLKS_14	 (0x00000014) // Sysclk / 14
#define NS7520_ETH_MCFG_CLKS_20	 (0x00000018) // Sysclk / 20
#define NS7520_ETH_MCFG_CLKS_28	 (0x0000001C) // Sysclk / 28
#define NS7520_ETH_MCFG_SPRE	 (0x00000002) // Suppress preamble
#define NS7520_ETH_MCFG_SCANI	 (0x00000001) // Scan increment

// MCMD MII Management Command Register Bit Fields

#define NS7520_ETH_MCMD_RES1	 (0xFFFFFFFC) // Reserved
#define NS7520_ETH_MCMD_SCAN	 (0x00000002) // Automatically Scan for Read Data
#define NS7520_ETH_MCMD_READ	 (0x00000001) // Single scan for Read Data

// MCMD MII Management Address Register Bit Fields

#define NS7520_ETH_MADR_RES1	 (0xFFFFE000) // Reserved
#define NS7520_ETH_MADR_DADR	 (0x00001F00) // MII PHY device address
#define NS7520_ETH_MADR_RES2	 (0x000000E0) // Reserved
#define NS7520_ETH_MADR_RADR	 (0x0000001F) // MII PHY register address

// MWTD MII Management Write Data Register Bit Fields

#define NS7520_ETH_MWTD_RES1	 (0xFFFF0000) // Reserved
#define NS7520_ETH_MWTD_MWTD	 (0x0000FFFF) // MII Write Data

// MRRD MII Management Read Data Register Bit Fields

#define NS7520_ETH_MRRD_RES1	 (0xFFFF0000) // Reserved
#define NS7520_ETH_MRRD_MRDD	 (0x0000FFFF) // MII Read Data

// MIND MII Management Indicators Register Bit Fields

#define NS7520_ETH_MIND_RES1	 (0xFFFFFFF8) // Reserved
#define NS7520_ETH_MIND_NVALID	 (0x00000004) // Read Data not valid
#define NS7520_ETH_MIND_SCAN	 (0x00000002) // Automatically scan for read data
#define NS7520_ETH_MIND_BUSY	 (0x00000001) // MII interface busy

// SMII Status Register Bit Fields

#define NS7520_ETH_SMII_RES1	 (0xFFFFFFE0) // Reserved
#define NS7520_ETH_SMII_CLASH	 (0x00000010) // MAC-to-MAC with PHY
#define NS7520_ETH_SMII_JABBER	 (0x00000008) // Jabber condition present
#define NS7520_ETH_SMII_LINK	 (0x00000004) // Link OK
#define NS7520_ETH_SMII_DUPLEX	 (0x00000002) // Full-duplex operation
#define NS7520_ETH_SMII_SPEED	 (0x00000001) // 100 Mbps

// SA1 Station Address 1 Register Bit Fields

#define NS7520_ETH_SA1_RES1	 (0xFFFF0000) // Reserved
#define NS7520_ETH_SA1_OCTET1	 (0x0000FF00) // Station Address octet 1
#define NS7520_ETH_SA1_OCTET2	 (0x000000FF) // Station Address octet 2

// SA2 Station Address 2 Register Bit Fields

#define NS7520_ETH_SA2_RES1	 (0xFFFF0000) // Reserved
#define NS7520_ETH_SA2_OCTET3	 (0x0000FF00) // Station Address octet 3
#define NS7520_ETH_SA2_OCTET4	 (0x000000FF) // Station Address octet 4

// SA3 Station Address 3 Register Bit Fields

#define NS7520_ETH_SA3_RES1	 (0xFFFF0000) // Reserved
#define NS7520_ETH_SA3_OCTET5	 (0x0000FF00) // Station Address octet 5
#define NS7520_ETH_SA3_OCTET6	 (0x000000FF) // Station Address octet 6

// SAFR Station Address Filter Register Bit Fields

#define NS7520_ETH_SAFR_RES1	 (0xFFFFFFF0) // Reserved
#define NS7520_ETH_SAFR_PRO	 (0x00000008) // Enable Promiscuous mode
#define NS7520_ETH_SAFR_PRM	 (0x00000004) // Accept ALL multicast packets
#define NS7520_ETH_SAFR_PRA	 (0x00000002) // Accept multicast packets table
#define NS7520_ETH_SAFR_BROAD	 (0x00000001) // Accept ALL Broadcast packets

// HT1 Hash Table 1 Register Bit Fields

#define NS7520_ETH_HT1_RES1	 (0xFFFF0000) // Reserved
#define NS7520_ETH_HT1_HT1	 (0x0000FFFF) // CRC value 15-0

// HT2 Hash Table 2 Register Bit Fields

#define NS7520_ETH_HT2_RES1	 (0xFFFF0000) // Reserved
#define NS7520_ETH_HT2_HT2	 (0x0000FFFF) // CRC value 31-16

// HT3 Hash Table 3 Register Bit Fields

#define NS7520_ETH_HT3_RES1	 (0xFFFF0000) // Reserved
#define NS7520_ETH_HT3_HT3	 (0x0000FFFF) // CRC value 47-32

// HT4 Hash Table 4 Register Bit Fields

#define NS7520_ETH_HT4_RES1	 (0xFFFF0000) // Reserved
#define NS7520_ETH_HT4_HT4	 (0x0000FFFF) // CRC value 63-48

// PHY definitions (LXT971A)
// TODO move it later to a file of it's own with all the code to handle the
// registers

#define PHY_COMMON_CTRL    	 (0x00)	      // Control Register
#define PHY_COMMON_STAT    	 (0x01)	      // Status Register
#define PHY_COMMON_ID1    	 (0x02)	      // PHY Identification Register 1
#define PHY_COMMON_ID2           (0x03)       // PHY Identification Register 2
#define PHY_COMMON_AUTO_ADV      (0x04)	      // Auto-neg Advert Reg
#define PHY_COMMON_AUTO_LNKB     (0x05)	      // Auto-neg Link Ability Reg
#define PHY_COMMON_AUTO_EXP      (0x06)       // Auto-neg expansion Reg
#define PHY_COMMON_AUTO_NEXT     (0x07)       // Auto-neg Next Page Tx Reg
#define PHY_COMMON_AUTO_LNKN     (0x08)       // Auto-neg Link Partner Rx Reg
#define PHY_LXT971_PORT_CFG      (0x10)       // Port Configuration Register
#define PHY_LXT971_STAT2         (0x11)       // Status Register #2
#define PHY_LXT971_INT_ENABLE    (0x12)       // Interrupt Enable Register
#define PHY_LXT971_INT_STATUS    (0x13)       // Interrupt Status Register
#define PHY_LXT971_LED_CFG       (0x14)       // Interrupt LED Configuration
#define PHY_LXT971_DIG_CFG       (0x1A)       // Digital Config Register
#define PHY_LXT971_TX_CTRL       (0x1E)       // Transmit Control Register

// CTRL PHY Control Register Bit Fields
#define PHY_COMMON_CTRL_RESET  	 (0x8000)     // Reset PHY
#define PHY_COMMON_CTRL_LOOPBACK (0x4000)     // Loopback mode
#define PHY_COMMON_CTRL_SPD_MA   (0x2040)     // Speed Mask
#define PHY_COMMON_CTRL_SPD_10   (0x0000)     // Speed 10 Mbps
#define PHY_COMMON_CTRL_SPD_100  (0x2000)     // Speed 100 Mbps
#define PHY_COMMON_CTRL_SPD_1000 (0x0040)     // Speed 1000 Mbps
#define PHY_COMMON_CTRL_SPD_RES  (0x2040)     // Speed 1000 Mbps
#define PHY_COMMON_CTRL_AUTO_NEG (0x1000)     // Auto Negotiation enable
#define PHY_COMMON_CTRL_POWER_DN (0x0800)     // Power Down
#define PHY_COMMON_CTRL_ISOLATE	 (0x0400)     // Isolate
#define PHY_COMMON_CTRL_RES_AUTO (0x0200)     // Restart Auto-Negotation
#define PHY_COMMON_CTRL_DUPLEX	 (0x0100)     // Full Duplex Mode  
#define PHY_COMMON_CTRL_COL_TEST (0x0080)     // Collision Test
#define PHY_COMMON_CTRL_RES1     (0x003F)     // Reserved

// STAT Status Register Bit Fields

#define PHY_COMMON_STAT_100BT4	 (0x8000)     // able to perform 100BaseT4
#define PHY_COMMON_STAT_100BXFD	 (0x4000)     // able to perform 100BaseX full du
#define PHY_COMMON_STAT_100BXHD	 (0x2000)     // able to perform 100BaseX half du
#define PHY_COMMON_STAT_10BTFD	 (0x1000)     // able to perform 10BaseT full dup
#define PHY_COMMON_STAT_10BTHD	 (0x0800)     // able to perform 10BaseT half dup
#define PHY_COMMON_STAT_100BT2FD (0x0400)     // able to perform 100BaseT2 full d
#define PHY_COMMON_STAT_100BT2HD (0x0200)     // able to perform 100BaseT2 half d
#define PHY_COMMON_STAT_EXT_STAT (0x0100)     // extendes status register #15
#define PHY_COMMON_STAT_RES1	 (0x0080)     // Reserved
#define PHY_COMMON_STAT_MF_PSUP	 (0x0040)     // mangement frames preamble suppre
#define PHY_COMMON_STAT_AN_COMP  (0x0020)     // Auto-negotation complete
#define PHY_COMMON_STAT_RMT_FLT	 (0x0010)     // Remote Fault
#define PHY_COMMON_STAT_AN_CAP	 (0x0008)     // Auto-negotiation capable
#define PHY_COMMON_STAT_LNK_STAT (0x0004)     // Link Status (Up)
#define PHY_COMMON_STAT_JAB_DTCT (0x0002)     // Jabber Condition detected
#define PHY_COMMON_STAT_EXT_CAP	 (0x0001)     // Extendes Register Capabilities


// AUTO_ADV Auto-neg Advert Register Bit Fields

#define PHY_COMMON_AUTO_ADV_NP       	(0x8000) // send multiple pages
#define PHY_COMMON_AUTO_ADV_RES1        (0x4000) // Reserved
#define PHY_COMMON_AUTO_ADV_RMT_FLT     (0x2000) // Remote Fault
#define PHY_COMMON_AUTO_ADV_RES2        (0x1000) // Reserved
#define PHY_COMMON_AUTO_ADV_AS_PAUSE    (0x0800) // Pause operation
#define PHY_COMMON_AUTO_ADV_PAUSE       (0x0400) // Pause
#define PHY_COMMON_AUTO_ADV_100BT4      (0x0200) // 100 BaseT4
#define PHY_COMMON_AUTO_ADV_100BTXFD   	(0x0100) // 100 BaseTX Full Duplex
#define PHY_COMMON_AUTO_ADV_100BTX      (0x0080) // 100 Base TX capable
#define PHY_COMMON_AUTO_ADV_10BTFD   	(0x0040) // 10 Base T Full Duplex
#define PHY_COMMON_AUTO_ADV_10BT     	(0x0020) // 10 Base T Full Duplex
#define PHY_COMMON_AUTO_ADV_SEL_FLD_MA  (0x001F) // Selector Field
#define PHY_COMMON_AUTO_ADV_802_9       (0x0002) // 802.9
#define PHY_COMMON_AUTO_ADV_802_3       (0x0001) // 802.3

// AUTO_LNKB Auto-neg Link Ability Register Bit Fields

#define PHY_COMMON_AUTO_LNKB_NP       	(0x8000) // send multiple pages
#define PHY_COMMON_AUTO_LNKB_ACK        (0x4000) // Acknowledge
#define PHY_COMMON_AUTO_LNKB_RMT_FLT    (0x2000) // Remote Fault
#define PHY_COMMON_AUTO_LNKB_RES2       (0x1000) // Reserved
#define PHY_COMMON_AUTO_LNKB_AS_PAUSE   (0x0800) // Pause operation
#define PHY_COMMON_AUTO_LNKB_PAUSE      (0x0400) // Pause
#define PHY_COMMON_AUTO_LNKB_100BT4     (0x0200) // 100 BaseT4
#define PHY_COMMON_AUTO_LNKB_100BTXFD   (0x0100) // 100 BaseTX Full Duplex
#define PHY_COMMON_AUTO_LNKB_100BTX     (0x0080) // 100 Base TX capable
#define PHY_COMMON_AUTO_LNKB_10BTFD   	(0x0040) // 10 Base T Full Duplex
#define PHY_COMMON_AUTO_LNKB_10BT     	(0x0020) // 10 Base T Full Duplex
#define PHY_COMMON_AUTO_LNKB_SEL_FLD_MA (0x001F) // Selector Field
#define PHY_COMMON_AUTO_LNKB_802_9      (0x0002) // 802.9
#define PHY_COMMON_AUTO_LNKB_802_3      (0x0001) // 802.3

// AUTO_EXP Auto-neg Expansion Register Bit Fields

#define PHY_COMMON_AUTO_EXP_RES1        (0xFFC0) // Reserved
#define PHY_COMMON_AUTO_EXP_BASE_PAGE   (0x0020) // Base Page
#define PHY_COMMON_AUTO_EXP_PAR_DT_FLT  (0x0010) // Parallel detection fault
#define PHY_COMMON_AUTO_EXP_LNK_NP_CAP  (0x0008) // Link Partner Nxt Page Cap
#define PHY_COMMON_AUTO_EXP_NP_CAP      (0x0004) // Next Page Capable
#define PHY_COMMON_AUTO_EXP_PAGE_REC    (0x0002) // Page Received
#define PHY_COMMON_AUTO_EXP_LNK_AN_CAP  (0x0001) // Link Partner Auto Neg Cap

// AUTO_NEXT Aut-neg Next Page Tx Register Bit Fields

#define PHY_COMMON_AUTO_NEXT_NP         (0x8000) // Next Page
#define PHY_COMMON_AUTO_NEXT_RES1       (0x4000) // Reserved
#define PHY_COMMON_AUTO_NEXT_MSG_PAGE   (0x2000) // Message Page
#define PHY_COMMON_AUTO_NEXT_ACK_2      (0x1000) // Acknowledge 2
#define PHY_COMMON_AUTO_NEXT_TOGGLE     (0x0800) // Toggle
#define PHY_COMMON_AUTO_NEXT_MSG        (0x07FF) // Message

// AUTO_LNKN Auto-neg Link Partner Rx Reg Bit Fields

#define PHY_COMMON_AUTO_LNKN_NP         (0x8000) // Next Page
#define PHY_COMMON_AUTO_LNKN_ACK        (0x4000) // Acknowledge
#define PHY_COMMON_AUTO_LNKN_MSG_PAGE   (0x2000) // Message Page
#define PHY_COMMON_AUTO_LNKN_ACK_2      (0x1000) // Acknowledge 2
#define PHY_COMMON_AUTO_LNKN_TOGGLE     (0x0800) // Toggle
#define PHY_COMMON_AUTO_LNKN_MSG        (0x07FF) // Message

// PORT_CFG Port Configuration Register Bit Fields

#define PHY_LXT971_PORT_CFG_RES1        (0x8000) // Reserved
#define PHY_LXT971_PORT_CFG_FORCE_LNK   (0x4000) // Force Link Pass
#define PHY_LXT971_PORT_CFG_TX_DISABLE  (0x2000) // Transmit Disable
#define PHY_LXT971_PORT_CFG_BYPASS_SCR  (0x1000) // Bypass Scrambler
#define PHY_LXT971_PORT_CFG_RES2        (0x0800) // Reserved
#define PHY_LXT971_PORT_CFG_JABBER      (0x0400) // Disable Jabber Correction
#define PHY_LXT971_PORT_CFG_SQE	        (0x0200) // Enable Heart Beat
#define PHY_LXT971_PORT_CFG_TP_LOOPBACK (0x0100) // Disable TP Loopack (10BT)
#define PHY_LXT971_PORT_CFG_CRS_SEL     (0x0080) // CRS deasserts extends
#define PHY_LXT971_PORT_CFG_SLEEP_MODE  (0x0040) // Enable Sleep Mode
#define PHY_LXT971_PORT_CFG_PRE_EN      (0x0020) // Preamble Enable
#define PHY_LXT971_PORT_CFG_SLEEP_T_MA  (0x0018) // Sleep Timer Mask
#define PHY_LXT971_PORT_CFG_SLEEP_T_104 (0x0010) // Sleep Timer 1.04 seconds
#define PHY_LXT971_PORT_CFG_SLEEP_T_200 (0x0001) // Sleep Timer 2 seconds
#define PHY_LXT971_PORT_CFG_SLEEP_T_304 (0x0000) // Sleep Timer 3.04 seconds
#define PHY_LXT971_PORT_CFG_FLT_CODE_EN (0x0004) // Fault Code Enable
#define PHY_LXT971_PORT_CFG_ALT_NP      (0x0002) // Alternate NP
#define PHY_LXT971_PORT_CFG_FIBER_SEL   (0x0001) // Fiber Select Mode

// STAT2 Status Register #2 Bit Fields

#define PHY_LXT971_STAT2_RES1   	(0x8000) // Reserved
#define PHY_LXT971_STAT2_100BTX 	(0x4000) // operating in 100BaseTX
#define PHY_LXT971_STAT2_TX_STATUS	(0x2000) // is transmitting a packet
#define PHY_LXT971_STAT2_RX_STATUS	(0x1000) // is receiveing a packet
#define PHY_LXT971_STAT2_COL_STATUS	(0x0800) // Collision is occuring
#define PHY_LXT971_STAT2_LINK   	(0x0400) // Link is up
#define PHY_LXT971_STAT2_DUPLEX_MODE	(0x0200) // Duplex Mode
#define PHY_LXT971_STAT2_AUTO_NEG	(0x0100) // Auto negotiation process
#define PHY_LXT971_STAT2_AUTO_NEG_COMP 	(0x0080) // Auto negotation complete
#define PHY_LXT971_STAT2_RES2   	(0x0040) // Reserved
#define PHY_LXT971_STAT2_POLARITY	(0x0020) // Polarity is reversed
#define PHY_LXT971_STAT2_PAUSE  	(0x0010) // Device Pause capable
#define PHY_LXT971_STAT2_ERROR  	(0x0008) // Error occured
#define PHY_LXT971_STAT2_RES3   	(0x0007) // Reserved

// INT_ENABLE Interrupt Enable Register Bit Fields

#define PHY_LXT971_INT_ENABLE_RES1      (0xFF00) // Reserved
#define PHY_LXT971_INT_ENABLE_ANMSK     (0x0080) // Mask For Auto Negotiate Compl
#define PHY_LXT971_INT_ENABLE_SPEEDMSK  (0x0040) // Mask for Speed
#define PHY_LXT971_INT_ENABLE_DUPLEXMSK (0x0020) // Mask for Duplex Mode
#define PHY_LXT971_INT_ENABLE_LINKMSK   (0x0010) // Mask for Link Status
#define PHY_LXT971_INT_ENABLE_RES2      (0x000C) // Reserved
#define PHY_LXT971_INT_ENABLE_INTEN     (0x0002) // Interrupts Enable
#define PHY_LXT971_INT_ENABLE_TINT      (0x0001) // Force Interrupt on MDINT

// INT_STATUS Interrupt Status Register Bit Fields

#define PHY_LXT971_INT_STATUS_RES1      (0xFF00) // Reserved
#define PHY_LXT971_INT_STATUS_ANDONE    (0x0080) // Auto Negotiate Complete
#define PHY_LXT971_INT_STATUS_SPEEDCHG  (0x0040) // Speed Change
#define PHY_LXT971_INT_STATUS_DUPLEXCHG (0x0020) // Duplex Change
#define PHY_LXT971_INT_STATUS_LINKCHG   (0x0010) // Link Status Change
#define PHY_LXT971_INT_STATUS_RES2      (0x0008) // Reserved
#define PHY_LXT971_INT_STATUS_MDINT     (0x0004) // MDINT pending
#define PHY_LXT971_INT_STATUS_RES3      (0x0003) // Reserved

// LED_CFG Interrupt LED Configuration Register Bit Fields

#define PHY_LXT971_LED_CFG_SHIFT_LED1   (0x000C) // Shift LED1 Config left
#define PHY_LXT971_LED_CFG_SHIFT_LED2   (0x0008) // Shift LED2 Config left
#define PHY_LXT971_LED_CFG_SHIFT_LED3   (0x0004) // Shift LED3 Config left
#define PHY_LXT971_LED_CFG_LEDFREQ_MA	(0x000C) // LED Frequency
#define PHY_LXT971_LED_CFG_LEDFREQ_RES	(0x000C) // Reserved
#define PHY_LXT971_LED_CFG_LEDFREQ_100	(0x0008) // Stretch 100ms
#define PHY_LXT971_LED_CFG_LEDFREQ_60	(0x0004) // Stretch 60ms
#define PHY_LXT971_LED_CFG_LEDFREQ_30	(0x0000) // Stretch 30ms
#define PHY_LXT971_LED_CFG_PULSE_STR    (0x0002) // Disable Pulse Stretching
#define PHY_LXT971_LED_CFG_RES1         (0x0001) // Reserved

// only one of these values must be shifted for each SHIFT_LED? 

#define PHY_LXT971_LED_CFG_UNUSED1      (0x000F) // Unused
#define PHY_LXT971_LED_CFG_DUPLEX_COL   (0x000E) // Duplex and Collision
#define PHY_LXT971_LED_CFG_LINK_ACT     (0x000D) // Link and Activiity
#define PHY_LXT971_LED_CFG_LINK_RX      (0x000C) // Link and Receive
#define PHY_LXT971_LED_CFG_TEST_BLK_SLW (0x000B) // Test Mode blink slow
#define PHY_LXT971_LED_CFG_TEST_BLK_FST (0x000A) // Test Mode blink fast
#define PHY_LXT971_LED_CFG_TEST_OFF     (0x0009) // Test Mode LED OFF
#define PHY_LXT971_LED_CFG_TEST_ON      (0x0008) // Test Mode LED ON
#define PHY_LXT971_LED_CFG_RX_OR_TX     (0x0007) // Receive or Transmit Status
#define PHY_LXT971_LED_CFG_UNUSED2      (0x0006) // Unused
#define PHY_LXT971_LED_CFG_DUPLEX       (0x0005) // Duplex Status
#define PHY_LXT971_LED_CFG_LINK	        (0x0004) // Link Status
#define PHY_LXT971_LED_CFG_COLLISION    (0x0003) // Collision Status
#define PHY_LXT971_LED_CFG_RECEIVE      (0x0002) // Receive Status
#define PHY_LXT971_LED_CFG_TRANSMIT     (0x0001) // Transmit Status
#define PHY_LXT971_LED_CFG_SPEED        (0x0000) // Speed Status

// DIG_CFG Digitial Configuration Register Bit Fields

#define PHY_LXT971_DIG_CFG_RES1 	(0xF000) // Reserved
#define PHY_LXT971_DIG_CFG_MII_DRIVE	(0x0800) // MII drive strength increase
#define PHY_LXT971_DIG_CFG_RES2 	(0x0400) // Reserved
#define PHY_LXT971_DIG_CFG_SHOW_SYMBOL	(0x0200) // Map Symbol Error to RXER
#define PHY_LXT971_DIG_CFG_RES3 	(0x01FF) // Reserved

#define PHY_LXT971_MDIO_MAX_CLK		8000000  // it is capable of 8 MHz

// TX_CTRL Transmit Control Register Bit Fields
// documentation is buggy for this register, therefore setting not included

typedef enum 
{
    PHY_NONE    = 0x0000, // no PHY detected yet
    PHY_LXT971A = 0x0013
} PhyType;

#define PHY_MDIO_MAX_CLK		2500000  // default is 2.5 MHz

#define NS7520_ETH_PHY_ADDRESS   (0x0000)     // For Orthos we exploit the reserved address
#ifndef NS7520_ETH_PHY_ADDRESS
# define NS7520_ETH_PHY_ADDRESS	 (0x0100)     // suitable for UNC20
#endif // NETARM_ETH_PHY_ADDRESS

#endif // FS_NS7520_ETH_H
