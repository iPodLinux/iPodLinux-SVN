/****************************************************************************/

/*
 *	serial_ks8695.h -- Kendin/Micrel KS8695 UART support defines.
 *
 *	(C) Copyright 2003, Greg Ungerer <gerg@snapgear.com>
 */

/****************************************************************************/
#ifndef	serial_ks8695_h
#define	serial_ks8695_h
/****************************************************************************/

/*
 *	The internal UART of the Kendin/Micrel KS8696 is a mutant
 *	relation of the 8250/16550. Sort of similar and osrt of different.
 *	Some common defines could be used, but I have not bothered here.
 */

/*
 *	Define the UART register access structure.
 */
struct ksuart {
	unsigned int	RX;		/* 0x00	- Receive data (r) */
	unsigned int	TX;		/* 0x04	- Transmit data (w) */
	unsigned int	FCR;		/* 0x08	- Fifo Control (r/w) */
	unsigned int	LCR;		/* 0x0c	- Line Control (r/w) */
	unsigned int	MCR;		/* 0x10	- Modem Control (r/w) */
	unsigned int	LSR;		/* 0x14	- Line Status (r/w) */
	unsigned int	MSR;		/* 0x18	- Modem Status (r/w) */
	unsigned int	BD;		/* 0x1c	- Baud Rate (r/w) */
	unsigned int	SR;		/* 0x20	- Status (r/w) */
};

struct ksintctrl {
	unsigned int	CONTL;		/* 0x00 - Interrupt control (r/w) */
	unsigned int	ENABLE;		/* 0x04 - Interrupt enable (r/w) */
	unsigned int	STATUS;		/* 0x08 - Interrupt status (r/w) */
};

/****************************************************************************/
#endif	/* serial_ks8695_h */
