/*
 * linux/arch/m68knommu/platform/MC68VZ328/de2/traps.c -- general exception handling code
 *
 * Cloned from Linux/m68k.
 *
 * No original Copyright holder listed,
 * Probabily original (C) Roman Zippel (assigned DJD, 1999)
 *
 * Copyright 1999-2000 D. Jeff Dionne, <jeff@uclinux.org>
 * Copyright 2000-2001 Lineo, Inc. D. Jeff Dionne <jeff@uClinux.org>
 * Copyright 2002      Georges Menie
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/errno.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/page.h>
#include <asm/machdep.h>
#include <asm/MC68VZ328.h>

#include "traps_proto.h"

extern e_vector *_ramvec;

void __init dragen2_trap_init(void)
{
	int i;

	/* set up the vectors */
	for (i=2; i < 32; ++i)
		_ramvec[i] = bad_interrupt;

	_ramvec[2] = buserr;
	_ramvec[3] = exception3;
	_ramvec[4] = exception4;
	_ramvec[5] = exception5;
	_ramvec[6] = exception6;
	_ramvec[7] = exception7;
	_ramvec[8] = exception8;
	_ramvec[9] = exception9;
	_ramvec[10] = exception10;
	_ramvec[11] = exception11;

	_ramvec[14] = exception14;
	_ramvec[15] = exception15;

	_ramvec[32] = system_call;
	_ramvec[33] = trap1;

	_ramvec[47] = trap15;

	_ramvec[64] = bad_interrupt;
	_ramvec[65] = inthandler1;
	_ramvec[66] = inthandler2;
	_ramvec[67] = inthandler3;
	_ramvec[68] = inthandler4;
	_ramvec[69] = inthandler5;
	_ramvec[70] = inthandler6;
	_ramvec[71] = inthandler7;
 
	IVR = 0x40; /* Set interrupt base to 64 */
}
