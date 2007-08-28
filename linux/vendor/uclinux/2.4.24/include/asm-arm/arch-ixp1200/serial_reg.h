/*
 * include/asm/arch/serial_reg.h
 *
 * Redistribution of this file is permitted under the terms of the GNU 
 * Public License (GPL)
 * 
 * These are the IXP1200 UART port assignments, expressed as long index 
 * of the base address.
 */

#ifndef _IXP1200_SERIAL_REG_H_
#define _IXP1200_SERIAL_REG_H_

#define UART_CR        CSR_UARTCR
#define UART_SR        CSR_UARTSR
#define UART_DR        CSR_UARTDR

/*
 * Register index. Since we will be using unsigned long* to access
 * registers, all byte offsets from the base must be divided by 4 
 */
#define UTSR 		0	        /* Status register */
#define UTCR 		(0x400/4)	/* Control register */
#define UTDR 		(0x800/4)	/* Data register */

#define UART_RX 	UTDR	/* Receive port, read only */
#define UART_TX 	UTDR	/* transmit port, write only */

#define UTCR_XIE        (1 << 8)     /* Xmit interrupt enable */
#define UTCR_RIE        (1 << 4)     /* Rcv interrupt enable */
#define UTCR_EN         (1 << 7)     /* Enable uart */

#define UTCR_9600_BAUD   (0x17 << 16) 
#define UTCR_19200_BAUD  (0xB  << 16) 
#define UTCR_38400_BAUD  (0x5  << 16) 
#define UTCR_57600_BAUD  (0x3  << 16) 
#define UTCR_115200_BAUD (0x1  << 16)

#define UTCR_BRK	(0x1)

#define UTCR_8_BIT      (0x3 << 5)   /* 8 bit */
#define UTCR_7_BIT      (0x2 << 5)   /* 7 bit */
#define UTCR_6_BIT      (0x1 << 5)   /* 7 bit */
#define UTCR_5_BIT      (0x0 << 5)   /* 7 bit */

#define UTCR_1_STOP_BIT (0 << 3)     /* 1 stop bit */
#define UTCR_2_STOP_BIT (1 << 3)     /* 2 stop bits  */

#define UTCR_PARENB     (1 << 1)     /* Parity Enable */
#define UTCR_ODD_PARITY (1 << 2)     /* Odd parity    */

#define UTCR_DSS_MASK   (0x3 << 5)    /* data size mask */
#define UTCR_SBS_MASK   (0x1 << 3)    /* stop bit select mask */
#define UTCR_BRD_MASK   (0x3ff << 16) /* Baud rate mask */

// #define CONFIG_A3_SILICON
#ifdef CONFIG_A3_SILICON
#define UTCR_INT_CLEAR  (1 << 9)      /* The !@#$ docs have a bug. This bit */
#endif

#define UTSR_RPE        (1 << 0) /* Parity error */
#define UTSR_RFE        (1 << 1) /* Framing error */
#define UTSR_ROR        (1 << 3) /* receiver overrun */
#define UTSR_RXR        (1 << 4) /* receiver not empty */
#define UTSR_TXR        (1 << 2) /* Transmit Fifo not full */
#define UTSR_TXE        (1 << 5) /* xmit fifo empty */
#define UTSR_RXF        (1 << 6) /* receive fifo full */
#define UTSR_TXF        (1 << 7) /* xmit fifo full */

#endif /* ASM_ARCH_SERIAL_REG_H */

