/****************************************************************************
*
*	Name:			cnxtserial.h
*
*	Description:	Provides serial port character I/O (16550) definitions
*
*	Copyright:		(c) 2002 Conexant Systems Inc.
*
*****************************************************************************

  This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option)
any later version.

  This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
more details.

  You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc., 59
Temple Place, Suite 330, Boston, MA 02111-1307 USA

****************************************************************************
*  $Author: davidm $
*  $Revision: 1.1 $
*  $Modtime: 10/25/02 10:52a $
****************************************************************************/

#ifndef _CNXTSERIAL_H
#define _CNXTSERIAL_H

#include <linux/config.h>
#include <asm/page.h>


struct uart_regs
{
   volatile unsigned short  fifo;
   volatile unsigned short  irqe;
   volatile unsigned short  iir;
   volatile unsigned short  line_ctrl;
   volatile unsigned short  modem_ctrl;
   volatile unsigned short  line_status_reg;
   volatile unsigned short  msr;
   volatile unsigned short  scratch;
};

//#define UART_GPIO_LINE  (1 << 10)
#define UART_GPIO_LINE  (1 << 9) // gpio25

/*******************************************************************************/
/* Interrupt Status Register - ISR                                             */
/*******************************************************************************/

/* ISR b0 indicates that an interrupt is pending when clear. b3 - b1 signal which interrupt as per:- */

#define ISR_LSR_Source      0x06   /* 0 1 1 - Receiver Line Status Register Priority 1 */
#define ISR_Rx_Rdy_Source   0x04   /* 0 1 0 - Received Data Ready           Priority 2 */
#define ISR_Rx_Rdy_TO_Src   0x0c   /* 1 1 0 - Received Data Ready Time Out  Priority 2 */
#define ISR_Tx_Rdy_Source   0x02   /* 0 0 1 - Transmitter Holding Reg Empty Priority 3 */
#define ISR_MODEM_Source    0x00   /* 0 0 0 - Modem Status Register         Priority 4 */

/* ISR b7 - b4 are not used - in st16c552 b7 - b6 are Set b5 - b4 are Clear   */

/*******************************************************************************/
/* Interrupt Enable Register - IER                                             */
/*******************************************************************************/
#define IER_Rx_Holding_Reg  0x01   /* b0 - Receive Holding Register Interrupt  - Enabled When Set   */
#define IER_Tx_Holding_Reg  0x02   /* b1 - Transmit Holding Register Interrupt - Enabled When Set   */
#define IER_Rx_Line_Stat    0x04   /* b2 - Receiver Line Status Interrupt      - Enabled When Set   */
#define IER_Modem_Status    0x08   /* b3 - Modem Status Register Interrupt     - Enabled When Set   */
                                   /* b7 - b4 are not used and are set to zero                    */

/*******************************************************************************/
/* Line Status Register - LSR                                                                              */
/*******************************************************************************/

#define LSR_Rx_Data_Ready   0x01   /* b0 - Data Received and Saved in Holding Reg - Set when Valid */
#define LSR_Overrun_Error   0x02   /* b1 - Overrun Error Occured                  - Set When Valid */
#define LSR_Parity_Error    0x04   /* b2 - Received Data has Incorrect Parity     - Set When Valid */
#define LSR_Framing_Error   0x08   /* b3 - Framing Error (No Stop Bit)            - Set When Valid */
#define LSR_Break_Interrupt 0x10   /* b4 - Break Signal Received                  - Set When Valid */
#define LSR_Tx_Hold_Empty   0x20   /* b5 - Tx Holding Register is empty and ready - Set When Valid */
#define LSR_Tx_Fifo_Empty   0x40   /* b6 - Tx Shift Registers and FIFO are Empty  - Set When Valid */
#define LSR_Fifo_Error      0x80   /* b7 - At Least one of b4 - b2 has occurred   - Set When Valid */

/*******************************************************************************/
/*  Latch Control Register - LCR                                               */
/*******************************************************************************/

/* LCR b2 defines the stop bits setup b1 - b0 define the Tx - Rx Word Length                        */
/* The following defines cover all of the available options                                         */

#define LCR_5_Bit_Word_1    0x00   /* 0 0 0  - 5 Bit Word - 1 Stop Bit   */
#define LCR_6_Bit_Word_1    0x01   /* 0 0 1  - 6 Bit Word - 1 Stop Bit   */ 
#define LCR_7_Bit_Word_1    0x02   /* 0 1 0  - 7 Bit Word - 1 Stop Bit   */
#define LCR_8_Bit_Word_1    0x03   /* 0 1 1  - 8 Bit Word - 1 Stop Bit   */
#define LCR_5_Bit_Word_1p5  0x04   /* 1 0 0  - 5 Bit Word - 1.5 Stop Bit */
#define LCR_6_Bit_Word_2    0x05   /* 1 0 1  - 6 Bit Word - 2 Stop Bit   */
#define LCR_7_Bit_Word_2    0x06   /* 1 1 0  - 6 Bit Word - 1 Stop Bit   */
#define LCR_8_Bit_Word_2    0x07   /* 1 1 1  - 6 Bit Word - 1 Stop Bit   */

#define LCR_Parity_Enable   0x08   /* b3 - Enable Parity Bit Generation and Check - Enabled When Set */
#define LCR_Parity_Even     0x10   /* b4 - Odd/Even Parity Generation and Check   - Even When Set    */
#define LCR_Parity_Set      0x20   /* b5 - Toggle Generated Parity Bit 0/1        - 0 When Set       */
#define LCR_Break_Set       0x40   /* b6 - Force Break Control ( Tx o/p low)      - Forced When Set  */
#define LCR_Divisor_Latch   0x80   /* b7 - Enable Internal Baud Rate Latch        - Enabled When Set */


#if DEBUG_CONSOLE_ON_UART_2
	#ifdef CONFIG_BD_RUSHMORE
		#define UART0_Base_Addr    0x002C0010
	#endif
	#ifdef CONFIG_BD_HASBANI
		#define UART0_Base_Addr    0x002C0010
	#endif
	#ifdef CONFIG_BD_MACKINAC
		#define UART0_Base_Addr    0x00290000
	#endif
#else
	#if CONFIG_CHIP_CX82100 && PLD_CS4_UART_v100
		// Needed to accomodate a programming error in the
		// current PLD code of the Oakland board that maps
		// the UART to 0x2C1000.  This error should go away
		// someday, hopefully.
		#define UART0_Base_Addr    0x002C1000
	#else
		#define UART0_Base_Addr    0x002C0000
	#endif
#endif

/*  We use a 1.8432Mhz clk */
#define UART_CLK    1843200
#define UART_2400   ((UART_CLK)/(16*2400))
#define UART_4800   ((UART_CLK)/(16*4800))
#define UART_9600   ((UART_CLK)/(16*9600))
#define UART_19200  ((UART_CLK)/(16*19200))
#define UART_38400  ((UART_CLK)/(16*38400))
#define UART_57600  ((UART_CLK)/(16*57600))
#define UART_115200  ((UART_CLK)/(16*115200))

#define UART_CTRL       (UART0_Base_Addr+ )
#define UART_BRD        (UART0_Base_Addr+ )
#define UART_DBUF       (UART0_Base_Addr+ )
#define UART_ISR        (UART0_Base_Addr+ )

/*******************************************************************************/
/* FIFO Control Register - FCR                                                 */
/*******************************************************************************/
#define FCR_Fifo_Enable     0x01   /* b0 - Tx and Rx FIFO Enable               - Enabled When Set   */
#define FCR_Rx_Fifo_Reset   0x02   /* b1 - Clear Rx FIFO and reset its counter - Clears When Set    */
#define FCR_Tx_Fifo_Reset   0x04   /* b2 - Clear Tx FIFO and reset its counter - Clears When Set    */
#define FCR_DMA_Mode_Select 0x08   /* b3 - Change DMA Mode State from m0 to m1 - Mode 1 When Set    */

/* FCR b7 - b6 FIFO Trigger Level  */
 
#define FCR_Rx_Trig_Lvl_01  0x00   /* 0 0 - FIFO Rx Trigger Level 01 */              
#define FCR_Rx_Trig_Lvl_04  0x40   /* 0 1 - FIFO Rx Trigger Level 04 */          
#define FCR_Rx_Trig_Lvl_08  0x80   /* 1 0 - FIFO Rx Trigger Level 08 */
#define FCR_Rx_Trig_Lvl_16  0xc0   /* 1 1 - FIFO Rx Trigger Level 16 */


#define BIT24            0x01000000
#define CNXT_INT_LVL_GPIO  24       /* GPIO interrupts */

struct serial_struct {
	int	type;
	int	line;
	int	port;
	int	irq;
	int	flags;
	int	xmit_fifo_size;
	int	custom_divisor;
	int	baud_base;
	unsigned short	close_delay;
	char	reserved_char[2];
	int	hub6;  /* FIXME: We don't have AT&T Hub6 boards! */
	unsigned short	closing_wait; /* time to wait before closing */
	unsigned short	closing_wait2; /* no longer used... */
	int	reserved[4];
};

/*
 * For the close wait times, 0 means wait forever for serial port to
 * flush its output.  65535 means don't wait at all.
 */
#define S_CLOSING_WAIT_INF	0
#define S_CLOSING_WAIT_NONE	65535

/*
 * Definitions for S_struct (and serial_struct) flags field
 */
#define S_HUP_NOTIFY 0x0001 /* Notify getty on hangups and closes 
				   on the callout port */
#define S_FOURPORT  0x0002	/* Set OU1, OUT2 per AST Fourport settings */
#define S_SAK	0x0004	/* Secure Attention Key (Orange book) */
#define S_SPLIT_TERMIOS 0x0008 /* Separate termios for dialin/callout */

#define S_SPD_MASK	0x0030
#define S_SPD_HI	0x0010	/* Use 56000 instead of 38400 bps */

#define S_SPD_VHI	0x0020  /* Use 115200 instead of 38400 bps */
#define S_SPD_CUST	0x0030  /* Use user-specified divisor */

#define S_SKIP_TEST	0x0040 /* Skip UART test during autoconfiguration */
#define S_AUTO_IRQ  0x0080 /* Do automatic IRQ during autoconfiguration */
#define S_SESSION_LOCKOUT 0x0100 /* Lock out cua opens based on session */
#define S_PGRP_LOCKOUT    0x0200 /* Lock out cua opens based on pgrp */
#define S_CALLOUT_NOHUP   0x0400 /* Don't do hangups for cua device */

#define S_FLAGS	0x0FFF	/* Possible legal S flags */
#define S_USR_MASK 0x0430	/* Legal flags that non-privileged
				 * users can set or reset */

/* Internal flags used only by kernel/chr_drv/serial.c */
#define S_INITIALIZED	0x80000000 /* Serial port was initialized */
#define S_CALLOUT_ACTIVE	0x40000000 /* Call out device is active */
#define S_NORMAL_ACTIVE	0x20000000 /* Normal device is active */
#define S_BOOT_AUTOCONF	0x10000000 /* Autoconfigure port on bootup */
#define S_CLOSING		0x08000000 /* Serial port is closing */
#define S_CTS_FLOW		0x04000000 /* Do CTS flow control */
#define S_CHECK_CD		0x02000000 /* i.e., CLOCAL */

/* Software state per channel */

#ifdef __KERNEL__
/*
 * This is our internal structure for each serial port's state.
 * 
 * Many fields are paralleled by the structure used by the serial_struct
 * structure.
 *
 * For definitions of the flags field, see tty.h
 */

struct cnxt_serial {
	char soft_carrier;  /* Use soft carrier on this channel */
	char break_abort;   /* Is serial console in, so process brk/abrt */
	char is_cons;       /* Is this our console. */

	/* We need to know the current clock divisor
	 * to read the bps rate the chip has currently
	 * loaded.
	 */
	unsigned char clk_divisor;  /* May be 1, 16, 32, or 64 */
	int baud;
	int			magic;
	int			baud_base;
	int			port;
	int			irq;
	int			irqmask;
	int			flags; 		/* defined in tty.h */
	int			type; 		/* UART type */
	int			use_ints;
	struct uart_regs	*uart;
	struct tty_struct 	*tty;
	int			read_status_mask;
	int			ignore_status_mask;
	int			timeout;
	int			xmit_fifo_size;
	int			custom_divisor;
	int			x_char;	/* xon/xoff character */
	int			close_delay;
	unsigned short		closing_wait;
	unsigned short		closing_wait2;
	unsigned long		event;
	unsigned long		last_active;
	int			line;
	int			count;	    /* # of fd on device */
	int			blocked_open; /* # of blocked opens */
	long			session; /* Session of opening process */
	long			pgrp; /* pgrp of opening process */
	unsigned char 		*xmit_buf;
	unsigned char 		*rx_buf;
	int			xmit_head;
	int			xmit_tail;
	int			xmit_cnt;
	struct tq_struct	tqueue;
	struct tq_struct	tqueue_hangup;
	struct termios		normal_termios;
	struct termios		callout_termios;
  //struct wait_queue	*open_wait;
  //struct wait_queue	*close_wait;
	wait_queue_head_t	open_wait;
	wait_queue_head_t	close_wait;
};


#define SERIAL_MAGIC 0x5301

/*
 * The size of the serial xmit buffer is 1 page, or 4096 bytes
 */
#define SERIAL_XMIT_SIZE	PAGE_SIZE
#define SERIAL_RX_SIZE		PAGE_SIZE

/*
 * Events are used to schedule things to happen at timer-interrupt
 * time, instead of at rs interrupt time.
 */
#define RS_EVENT_WRITE_WAKEUP	0

#endif /* __KERNEL__ */



#endif 
