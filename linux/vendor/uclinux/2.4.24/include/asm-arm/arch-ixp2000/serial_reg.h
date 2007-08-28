/*
 * include/asm-arm/arch-ixp2000/serial_reg.h
 *
 * Author: Naeem M Afzal <naeem.m.afzal@intel.com>
 *
 * Copyright (c) 2002 Intel Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#ifndef _ARCH_SERIAL_H_
#define _ARCH_SERIAL_H_

#define IXP2000_CSR(x)	(volatile unsinged long *)(IXP2000_UART_BASE + (x))
#define CSR_UARTCR     IXP2000_CSR(0x0c)
#define CSR_UARTSR     IXP2000_CSR(0x14)
#define CSR_UARTDR     IXP2000_CSR(0x00)
#define CSR_UARTIER    IXP2000_CSR(0x04)
#define CSR_UARTIIR    IXP2000_CSR(0x04)
#define CSR_UARTFCR    IXP2000_CSR(0x08)

#define UART_CR        CSR_UARTCR
#define UART_SR        CSR_UARTSR
#define UART_DR        CSR_UARTDR
#define UART_IER       CSR_UARTIER
#define UART_IIR       CSR_UARTIIR
#define UART_FCR       CSR_UARTFCR

#define UTDR        0x00  /* DLAB = 0 */
#define UTDLL       0x00  /* DLAB = 1 */
#define UTIER       0x04  /* DLAB = 0 */
#define UTDLM       0x04  /* DLAB = 1 */
#define UTFCR       0x08
#define UTIIR       0x08
#define UTCR        0x0c
#define UTSR        0x14

#define UART_RX     UTDR	/* Receive port, read only */
#define UART_TX     UTDR	/* transmit port, write only */

/* bit definition on IER */
#define UTCR_XIE    (1 << 1)     /* Xmit interrupt enable */
#define UTCR_TIE    UTCR_XIE     /* Xmit interrupt enable */
#define UTCR_RIE    (1 << 0)     /* Rcv interrupt enable */
#define UTCR_RLSE   (1 << 2)     /* Rcv line status interrupt enable */
#define UTCR_RTOIE  (1 << 4)     /* Rcv timeout status interrupt enable */
#define UTCR_EN     (1 << 6)     /* Enable uart */

/* dont need these below */
#define UTCR_9600_BAUD   (0x17 << 16) 
#define UTCR_19200_BAUD  (0xB  << 16) 
#define UTCR_38400_BAUD  (0x5  << 16) 
#define UTCR_57600_BAUD  (0x3  << 16) 
#define UTCR_115200_BAUD (0x1  << 16)

/* LCR def */
#define UTCR_PARENB     (1 << 3)     /* Parity Enable */
#define UTCR_EVNEN_PARITY (1 << 4)   /* Even parity    */

#define UTCR_8_BIT      (0x3 << 0)   /* 8 bit */
#define UTCR_7_BIT      (0x2 << 0)   /* 7 bit */
#define UTCR_6_BIT      (0x1 << 0)   /* 6 bit */
#define UTCR_5_BIT      (0x0 << 0)   /* 5 bit */

#define UTCR_1_STOP_BIT (0 << 2)     /* 1 stop bit */
#define UTCR_2_STOP_BIT (1 << 2)     /* 2 stop bits  */
#define UTCR_BRK	(1<<6)
#define UTCR_DLAB	(1<<7)


#define UTCR_DSS_MASK   (0x3 << 0)    /* data size mask */
#define UTCR_SBS_MASK   (0x1 << 2)    /* stop bit select mask */

#define UTCR_INT_CLEAR  (1 << 2)      /* UART_INT_MASK */

/* LSR */
#define UTSR_RPE        (1<<2)  /* Parity error */
#define UTSR_RFE        (1<<3) /* Framing error */
#define UTSR_ROR        (1<<1) /* receiver overrun */
#define UTSR_RXR        (1<<0)  /* receiver not empty */
#define UTSR_TXR        (1<<5) /* Transmit Fifo not full */
#define UTSR_TXE        (1<<6) /* xmit fifo empty */
#define UTSR_RXF        (0<<0) /* receive fifo full */
#define UTSR_TXF        (0<<5) /* xmit fifo full */

#define UART_LCR_DLAB	(1<<7)  /* DLAB bit */
#define UART_FCR_ENABLE_FIFO  (1<<0)  /* FIFOs enable */
#define UART_FCR_RESET  (3<<1)  /* RSET FIFOs */
#define UART_FCR_TRIGGER_8  (1<<7) /* 6:7=01 trigger on 8bits*/

#endif /* _ARCH_SERIAL_H */

