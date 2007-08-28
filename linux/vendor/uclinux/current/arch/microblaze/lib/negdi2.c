/*
 * arch/v850/lib/negdi2.c -- 64-bit negation
 *
 *  Copyright (C) 2001  NEC Corporation
 *  Copyright (C) 2001  Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Written by Miles Bader <miles@gnu.org>
 */

typedef		 int DItype	__attribute__ ((mode (DI)));

DItype __negdi2 (DItype x)
{
#if 0
	__asm__ __volatile__
		("not	r6, r10;"	/* r10 = ~r6 */
		 "add	1, r10;"	/* r10=r10+1 */
		 "setf	c, r6;"		/* r6 = c.f. */
		 "not	r7, r11;"	/* r11 = ~r7 */
		 "add	r6, r11"	/* r11 = r11+r6 */
		 ::: "r6", "r7", "r10", "r11");
#endif
	__asm__ __volatile__ ("	xori	r3, r5, 0xFFFFFFFF;	/* r3 = ~r5 */
				addi	r3, r3, 1;	/* r3 = r3+1 */
				xori	r6, r4, 0xFFFFFFFF;	/* r6 = ~r6 */
				addc	r4, r4, r0"	/* r4 = r4 + c.f. */
				::: "r3", "r4", "r6");

}
