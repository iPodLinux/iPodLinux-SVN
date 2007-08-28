/******************************************************************/
/*                                                                */
/* Module:       jb_io.c                                          */
/*                                                                */
/* Descriptions: Manages I/O related routines, for the DraginixVZ */
/*                                                                */
/* Revisions:    0.1 02/12/07                                     */
/*                                                                */
/******************************************************************/
/* $Id: jb_io.c,v 1.1 2002/09/09 15:02:53 gerg Exp $ */

//#include <stdio.h>
#include "jb_io.h"
#include <asm/MC68VZ328.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>			/* For in_interrupt() */
#include <linux/config.h>


/* SIGNALS */
#define SIG_TCK  0 /* TCK */
#define SIG_TMS  1 /* TMS */
#define SIG_TDI  2 /* TDI */
#define SIG_TDO  3 /* TDO */

/* serial port interface available on all platforms */
/* we need a direct interface to the serial port, because
   the serial port driver is not working that early at boottime*/
extern void putC (char ch);
extern void putS (char * str);


int low_printf(const char *fmt, ...)
{
	va_list args;
	int printed_len;

	static char printk_buf[1024];

	va_start(args, fmt);
	printed_len = vsnprintf(printk_buf, sizeof(printk_buf), fmt, args);
	va_end(args);

	putS(printk_buf);

	return printed_len;
}

void initialize_jtag_hardware()
{
        // Inititialize ports B,D and M
        *(volatile unsigned char *) PBSEL_ADDR |= 0x20;
        *(volatile unsigned char *) PDSEL_ADDR |= 0x03;
        *(volatile unsigned char *) PMSEL_ADDR  |= 0x20;
        *(volatile unsigned char *) PBPUEN_ADDR &= ~0x20;
        *(volatile unsigned char *) PDPUEN_ADDR &= ~0x03;
        *(volatile unsigned char *) PMPUEN_ADDR &= ~0x20;
        *(volatile unsigned char *) PBDIR_ADDR  |= 0x20;
        *(volatile unsigned char *) PDDIR_ADDR  |= 0x03;
        *(volatile unsigned char *) PMDIR_ADDR  &= ~0x20;
        *(volatile unsigned char *) PBDATA_ADDR |= 0x20;
        *(volatile unsigned char *) PDDATA_ADDR |= 0x03;
        *(volatile unsigned char *) PMDATA_ADDR |= 0x20;
        //printk("JTAG Hardware Initilized\r\n");
}

void close_jtag_hardware()
{
        *(volatile unsigned char *) PBDIR_ADDR  &= ~0x20;
        *(volatile unsigned char *) PDDIR_ADDR  &= ~0x03;
        *(volatile unsigned char *) PMDIR_ADDR  &= ~0x20;
        //printk("JTAG Hardware Closed\r\n");
}


/******************************************************************/
/* Name:         ReadTDO                                          */
/*                                                                */
/* Parameters:   bit_count,data,inst                              */
/*               -bit_count is the number of bits to shift out.   */
/*               -data is the value to shift in from lsb to msb.  */
/*               -inst determines if the data is an instruction.  */
/*                if inst=1,the number of bits shifted in/out     */
/*                equals to bit_count-1;if not,the number of bits */
/*                does not change.                                */
/*                                                                */
/* Return Value: The data shifted out from TDO. The first bit     */
/*               shifted out is placed at the lsb of the returned */
/*               integer.                                         */
/*               		                                          */
/* Descriptions: Shift out bit_count bits from TDO while shift in */
/*               data to TDI. During instruction loading, the     */
/*               number of shifting equals to the instruction     */
/*               length minus 1                                   */
/*                                                                */
/******************************************************************/
__inline__ int ReadTDO(int bit_count,int data,int inst)
{

	unsigned int tdi=0,tdo=0,record=0;
	unsigned int i,max = inst? (bit_count-1):bit_count;

	for(i=0;i<max;i++)
	{
		unsigned int mask=1;

		tdo = ((PMDATA&0x20) == 0x20) ? (1<<i):0;
		record = record | tdo;

		mask = mask << i;
		tdi = data & mask;
		tdi = tdi >> i;
		PDDATA = tdi ? (PDDATA | 0x01) : (PDDATA & 0xfe);
		PDDATA |= 0x02;
		PDDATA &= 0xfd;
		//DriveSignal(SIG_TDI,tdi,1,1);
	}

	return record;
}

/******************************************************************/
/* Name:         DriveSignal                                      */
/*                                                                */
/* Parameters:   signal,data,clk,test                             */
/*               -the name of the signal (SIG_*).                 */
/*               -the value to be dumped to the signal,'1' or '0' */
/*               -driving a LOW to HIGH transition to SIG_TCK     */
/*                together with signal.                           */
/*               -test is used by WritePort function.             */
/*                                                                */
/* Return Value: None.                                            */
/*                                                                */
/* Descriptions: Dump data to signal. If clk is '1', a clock pulse*/
/*               is driven after the data is dumped to signal.    */
/*                                                                */
/******************************************************************/
__inline__ void DriveSignal(int signal,int data,int clk,int test)
{

	if(signal == SIG_TCK)
		PDDATA = data ? (PDDATA | 0x02) : (PDDATA & 0xfd);
	else
		if(signal == SIG_TMS)
			PBDATA = data ? (PBDATA | 0x20) : (PBDATA & 0xdf);
		else
			if(signal == SIG_TDI)
				PDDATA = data ? (PDDATA | 0x01) : (PDDATA & 0xfe);

	if(clk)
	{
		PDDATA |= 0x02;
		PDDATA &= 0xfd;
	}
}
