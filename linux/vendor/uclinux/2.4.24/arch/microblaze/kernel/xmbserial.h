/*
 * xmbserial.c -- serial driver for Microblaze UARTLite.
 *
 * Copyright (c) 2003 John Williams <jwilliams@itee.uq.edu.au>
 *
 * Based on code from mcfserial.c which was:
 * 
 * Copyright (c) 1999 Greg Ungerer <gerg@snapgear.com>
 * Copyright (c) 2000-2001 Lineo, Inc. <www.lineo.com>
 * Copyright (c) 2002 SnapGear Inc., <www.snapgear.com>
 *
 * Based on code from 68332serial.c which was:
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1998 TSHG
 * Copyright (c) 1999 Rt-Control Inc. <jeff@uclinux.org>
 */ 
#ifndef _XMB_SERIAL_H
#define _XMB_SERIAL_H

#include <linux/config.h>
#include <linux/serial.h>

#ifdef __KERNEL__

/*
 *	Define a local serial stats structure.
 */

struct xmb_stats {
	unsigned int	rx;
	unsigned int	tx;
	unsigned int	rxbreak;
	unsigned int	rxframing;
	unsigned int	rxparity;
	unsigned int	rxoverrun;
};


/*
 * This is our internal structure for each serial port's state.
 * Each serial port has one of these structures associated with it.
 */

struct xmb_serial {
	int			magic;
	unsigned int		addr;		/* UART memory address */
	int			irq;
	int			flags; 		/* defined in tty.h */
	int			type; 		/* UART type */
	struct tty_struct 	*tty;
	unsigned int		baud;
	int			sigs;
	int			custom_divisor;
	int			x_char;	/* xon/xoff character */
	int			baud_base;
	int			close_delay;
	unsigned short		closing_wait;
	unsigned short		closing_wait2;
	unsigned long		event;
	int			line;
	int			count;	    /* # of fd on device */
	int			blocked_open; /* # of blocked opens */
	long			session; /* Session of opening process */
	long			pgrp; /* pgrp of opening process */
	unsigned char 		*xmit_buf;
	int			xmit_head;
	int			xmit_tail;
	int			xmit_cnt;
	struct xmb_stats	stats;
	struct tq_struct	tqueue;
	struct tq_struct	tqueue_hangup;
	struct termios		normal_termios;
	struct termios		callout_termios;
#if LINUX_VERSION_CODE <= 0x020100
	struct wait_queue	*open_wait;
	struct wait_queue	*close_wait;
#else
	wait_queue_head_t	open_wait;
	wait_queue_head_t	close_wait;
#endif

};

#endif /* __KERNEL__ */

#endif /* _XMB_SERIAL_H */
