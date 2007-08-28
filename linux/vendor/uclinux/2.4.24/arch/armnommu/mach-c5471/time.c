/***********************************************************************
 * linux/arch/armnommu/mach-c5471/time.c
 *
 *   Copyright (C) 2003 Cadenux, LLC. All rights reserved.
 *   todd.fischer@cadenux.com  <www.cadenux.com>
 *
 *   Copyright (C) 2001 RidgeRun, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 * USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ***********************************************************************/

/* Included Files */

#include <linux/spinlock.h>  /* spinlock_t needed by linux/interrupt.h */
#include <asm/system.h>      /* smp_mb() needed by linux/interrupt.h */
#include <linux/interrupt.h> /* For struct irqaction */

#include <linux/ioport.h>    /* For request_region */
#include <linux/time.h>      /* struct timeval needed by linux/timex.h */
#include <linux/timex.h>     /* For tick */
#include <linux/sched.h>     /* For do_timer() */

#include <asm/io.h>          /* For IO macros */
#include <asm/hardware.h>    /* For c547x "Orion" register definitions */
#include <asm/irq.h>         /* For IRQ_TIMER */

#include <asm/mach/irq.h>    /* For setup_arm_irq */

/* Definitions */

#define COUNTS_PER_INTERRUPT 29687
#define CNTL_TIMER2 ((volatile unsigned long *)0xffff2c00)
#define COUNTS_PER_INTERRUPT_field_shift 5
#define AR  0x00000010
#define ST  0x00000008
#define PTV 0x00000003

unsigned int test_bss = 0x12345;

/* Global data */

extern unsigned long (*gettimeoffset)(void);

static unsigned long c5471_gettimeoffset(void)
{
  unsigned long elapsed;

  /* Compute the elapsed count since the last interrupt.
   * Our counter counts down starting at COUNTS_PER_INTERRUPT.
   */

  elapsed = COUNTS_PER_INTERRUPT - inl(C5471_TIMER2_CNT);
  
   /* Convert the elapsed count to usecs. 'tick' is assigned in sched.c.
    * Normally it's 10,000 -- the # of usecs/interrupt.
    */

  return (unsigned long)((elapsed * tick) / COUNTS_PER_INTERRUPT);
}

static void c5471_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
  do_timer(regs);
}

void c5471_setup_timer(struct irqaction *timer_irq)
{
  /* Configure timer.c so that c5471_gettimeoffset will be used
   * to get the elapsed microseconds between interrupts.
   */

  gettimeoffset = c5471_gettimeoffset;

  /* Capture the interrupt through c5471_timer_interrupt */

  timer_irq->handler = c5471_timer_interrupt;
  setup_arm_irq(IRQ_TIMER, timer_irq);

  /* Start the general purpose timer running in auto-reload mode
   * so that an interrupt is generated at the rate of HZ (param.h)
   * which typically is set to 100HZ.
   * When HZ is set to 100 then COUNTS_PER_INTERRUPT should be
   * established to reflect timer counts that occur every 10ms
   * (giving you 100 of them per second -- 100HZ). Orion's clock
   * is 47.5MHz and we're using a timer PTV value of 3 (3 == divide
   * incoming frequency by 16) which then yields a 16 bit
   * COUNTS_PER_INTERRUPT value of 29687.
   */

  *CNTL_TIMER2 = (((COUNTS_PER_INTERRUPT-1)<<COUNTS_PER_INTERRUPT_field_shift) | AR | ST | PTV);

  /* Finally, be a good citizen and do what it takes to insure
   * the user gets accurate info if they type "cat /proc/ioports"
   */

  request_region((unsigned long)CNTL_TIMER2, 0x8, "system timer (TIMER2)");
}


