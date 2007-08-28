/*
 *  arch/e1nommu/lib/delay.c
 *
 *  Copyrights: (C) 2002 GDT,  George Thanos<george.thanos@gdt.gr>
 *  			       Yannis Mitsos<yannis.mitsos@gdt.gr>
 *
 *  Derived from the S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com),
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/delay.h>

void __delay(unsigned long loops)
{
		int tmp=1;
        /*
         * To end the bloody studid and useless discussion about the
         * BogoMips number I took the liberty to define the __delay
         * function in a way that that resulting BogoMips number will
         * yield the megahertz number of the cpu. The important function
         * is udelay and that is done using the tod clock. -- martin.
         */
	asm volatile("p0:
					dbne p0 
					sub %1, %0"
					:  /* no outputs */
					: "l" (tmp), "l" (loops) );
}

extern unsigned long f;

/*
 * Waits for 'usecs' microseconds
 * FIXME I consoder that the Prescelar is set to 10MHz.
 */
void __udelay(unsigned long usecs)
{
	long start_cc, end_cc;

	if (usecs == 0)
		return;
		
	asm volatile ("
					ORI SR, 0x20
					MOV %0, TR" 
					: "=l" (start_cc)
					: /* no inputs */);

	do {
		asm volatile ("
						ORI SR, 0x20
						MOV %0, TR" 
						: "=l" (end_cc)
						: /* no inputs */);
	} while (((end_cc - start_cc)) < (usecs*10));
}

