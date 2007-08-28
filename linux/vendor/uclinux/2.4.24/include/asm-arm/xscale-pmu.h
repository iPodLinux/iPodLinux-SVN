/*
 * pmu.h
 *
 * This file contains the low level constants needed to access the 
 * performance monitoring unit on Intel's XScale Microarchitecture
 * processors.
 *
 * For details on how to use the perfmon unit from an application or 
 * from the kernel, see Documentation/arm/XScale/perfmon.txt
 *
 * Author: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright 2000-2001 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _PMU_H_
#define _PMU_H_

#include <linux/types.h>

/*
 * Different types of events that can be counted by the XScale PMU
 */

#define EVT_ICACHE_MISS			0x00
#define	EVT_ICACHE_NO_DELIVER		0x01
#define	EVT_DATA_STALL			0x02
#define	EVT_ITLB_MISS			0x03
#define	EVT_DTLB_MISS			0x04
#define	EVT_BRANCH			0x05
#define	EVT_BRANCH_MISS			0x06
#define	EVT_INSTRUCTION			0x07
#define	EVT_DCACHE_FULL_STALL		0x08
#define	EVT_DCACHE_FULL_STALL_CONTIG	0x09
#define	EVT_DCACHE_ACCESS		0x0A
#define	EVT_DCACHE_MISS			0x0B
#define	EVT_DCACE_WRITE_BACK		0x0C
#define	EVT_PC_CHANGED			0x0D
#define	EVT_BCU_REQUEST			0x10
#define	EVT_BCU_FULL			0x11
#define	EVT_BCU_DRAIN			0x12
#define	EVT_BCU_ECC_NO_ELOG		0x14
#define	EVT_BCU_1_BIT_ERR		0x15
#define	EVT_RMW				0x16


struct pmu_results
{
	u32	ccnt_of;
	u32	ccnt;		/* Clock Counter Register */
	u32	pmn0_of;
	u32	pmn0;		/* Performance Counter Register 0 */
	u32	pmn1_of;
	u32	pmn1;		/* Performance Counter Register 1 */
};

extern struct pmu_results results;

int pmu_claim(void);		/* Claim PMU for usage */
int pmu_start(u32, u32);		/* Start PMU execution */
int pmu_stop(struct pmu_results *);	/* Stop perfmon unit */
int pmu_release(int);		/* Release PMU */

#endif _PMU_H_


