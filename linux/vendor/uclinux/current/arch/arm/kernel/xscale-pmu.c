/*
 * pmu.c
 *
 * This function provides the implementation of the access functions to 
 * the Performance Monitoring Unit on all CPUs based on the XScale core.
 *
 * See Documentation/arm/XScale/pmu.txt for example usage
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

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <asm/atomic.h>
#include <asm/xscale-pmu.h>
#include <asm/arch/irqs.h>

static atomic_t usage = ATOMIC_INIT(0);
static unsigned long id = 0;
static u32 pmnc;
struct pmu_results results;

#define	PMU_ENABLE	0x001	/* Enable counters */
#define PMN_RESET	0x002	/* Reset event counters */
#define	CCNT_RESET	0x004	/* Reset clock counter */
#define	PMU_RESET	CCNT_RESET | PMN_RESET
#define	CLK_DIV		0x008	/* Clock divide enbable */
#define INT_ENABLE	0x070	/* Interrupt enable */

#define	PMN0_OVERFLOW	0x100	/* Perfromance counter 0 overflow */
#define PMN1_OVERFLOW	0x200	/* Performance counter 1 overflow */
#define CCNT_OVERFLOW	0x400	/* Clock counter overflow */

static void pmu_irq_handler(int, void *, struct pt_regs *);


int pmu_claim(void)
{
	int err = 0;

	if(atomic_read(&usage))
		return -EBUSY;

	err = request_irq(XSCALE_PMU_IRQ, pmu_irq_handler, SA_INTERRUPT,
					  NULL, (void *)&results);
	if(err < 0)
	{
		printk(KERN_ERR "unable to request IRQ %d for 80200 PMU: %d\n",
			   XSCALE_PMU_IRQ, err);
		return err;
	}
	
	atomic_inc(&usage);
	pmnc = 0;
	asm ("mcr p14, 0, %0, c0, c0, 0" : : "r" (pmnc));
	
	return ++id;
}

int pmu_release(int claim_id)
{
	if(!atomic_read(&usage))
		return 0;

	if(claim_id != id)
		return -EPERM;

	free_irq(XSCALE_PMU_IRQ, (void *)&results);
	atomic_dec(&usage);

	return 0;
}

int pmu_start(u32 pmn0, u32 pmn1)
{
	results.ccnt_of = 0;
	results.ccnt = 0;
	results.pmn0_of = 0;
	results.pmn0 = 0;
	results.pmn1_of = 0;
	results.pmn1 = 0;

	pmnc = (pmn1 << 20) | (pmn0 << 12);
	pmnc |= PMU_ENABLE | PMU_RESET | INT_ENABLE;

	asm ("mcr p14, 0, %0, c0, c0, 0" : : "r" (pmnc));

	/* event 0x5 branch instr exec work around */
	/* fixed in B stepping of the chip */
	/* A stepping requires two writes to pmnc to execute correctly */
	if((pmn0 == 0x5) || (pmn1 == 0x5))
	{
		asm ("mcr p14, 0, %0, c0, c0, 0" : : "r" (pmnc));
	}

	return 0;
}

int pmu_stop(struct pmu_results *results)
{
	u32 ccnt;
	u32 pmn0;
	u32 pmn1;

	if(!pmnc)
		return -ENOSYS;

	asm ("mrc p14, 0, %0, c1, c0, 0" : "=r" (ccnt));
	asm ("mrc p14, 0, %0, c2, c0, 0" : "=r" (pmn0));
	asm ("mrc p14, 0, %0, c3, c0, 0" : "=r" (pmn1));

	pmnc = 0;

	asm ("mcr p14, 0, %0, c0, c0, 0" : : "r" (pmnc));

	results->ccnt = ccnt;
	results->pmn0 = pmn0;
	results->pmn1 = pmn1;
	return 0;
}

static void pmu_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	struct pmu_results *results = (struct pmu_results *)dev_id;

	/* NOTE: there's an A stepping errata that states if an overflow */
	/*       bit already exists and another occurs, the previous     */
	/*       Overflow bit gets cleared. There's no workaround.	 */
	/*	 Fixed in B stepping or later			 	 */

	/* read the status */
	asm ("mrc p14, 0, %0, c0, c0, 0" : "=r" (pmnc));

	if(pmnc & PMN0_OVERFLOW)
	{
		results->pmn0_of++;
	}

	if(pmnc & PMN1_OVERFLOW)
	{
		results->pmn1_of++;
	}

	if(pmnc & CCNT_OVERFLOW)
	{
		results->ccnt_of++;
	}

	asm ("mcr p14, 0, %0, c0, c0, 0" : : "r" (pmnc));
}

