/*
 *  linux/arch/$(ARCH)/platform/$(PLATFORM)/config.c
 *
 *  Copyright (C) 1993 Hamish Macdonald
 *  Copyright (C) 1999 D. Jeff Dionne
 *  Copyright (C) 2003 Metrowerks (Blackfin support)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <stdarg.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <asm/current.h>

#include <asm/setup.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/irq.h>
#include <asm/machdep.h>
#include <asm/blackfin.h>

/*
 * By setting TSCALE such that TCOUNT counts a binary fraction
 * of microseconds, we can read TCOUNT directly and then with
 * a logical shift trivially calculate how many microseconds 
 * since the last tick, allowing do_gettimeofday() to yield
 * far better time resolution for real time benchmarking.
 */

#define CCLK_MHZ 300	/* would be nice to derive using the rtc */
#define TSCALE_SHIFT 2	/* 0.25 microseconds */
#define TSCALE_COUNT (CCLK_MHZ >> TSCALE_SHIFT)
#define CLOCKS_PER_JIFFY ((1000*1000/HZ) << TSCALE_SHIFT)

void config_FRIO_irq(void);

void BSP_sched_init(void (*timer_routine)(int, void *, struct pt_regs *))
{

  /* power up the timer, but don't enable it just yet */

  TCNTL = 1;

  /* make TCOUNT a binary fraction of microseconds using
   * the TSCALE prescaler counter.
   */

  TSCALE = TSCALE_COUNT - 1;
  TCOUNT = TPERIOD = CLOCKS_PER_JIFFY - 1;

  /* now enable the timer */

  TCNTL = 7;

  /* set up the timer irq */

  request_irq(IRQ_CORETMR, timer_routine, IRQ_FLG_LOCK, "timer", NULL);
  enable_irq(IRQ_CORETMR);
}

void BSP_tick(void)
{
#if 0
  /* Reset Timer2 */
  TSTAT2 &= 0;
#endif
}

unsigned long BSP_gettimeoffset (void)
{
  return (CLOCKS_PER_JIFFY - TCOUNT) >> TSCALE_SHIFT;
}

void BSP_gettod (int *yearp, int *monp, int *dayp,
		   int *hourp, int *minp, int *secp)
{
}

int BSP_hwclk(int op, struct hwclk_time *t)
{
  if (!op) {
    /* read */
  } else {
    /* write */
  }
  return 0;
}

int BSP_set_clock_mmss (unsigned long nowtime)
{
#if 0
  short real_seconds = nowtime % 60, real_minutes = (nowtime / 60) % 60;

  tod->second1 = real_seconds / 10;
  tod->second2 = real_seconds % 10;
  tod->minute1 = real_minutes / 10;
  tod->minute2 = real_minutes % 10;
#endif
  return 0;
}

void BSP_reset (void)
{
}

void config_BSP(char *command, int len)
{
  mach_sched_init      = BSP_sched_init;
  mach_tick            = BSP_tick;
  mach_gettimeoffset   = BSP_gettimeoffset;
  mach_gettod          = BSP_gettod;
  mach_hwclk           = NULL;
  mach_set_clock_mmss  = NULL;
  // mach_mksound         = NULL;
  mach_reset           = BSP_reset;
  // mach_debug_init      = NULL;

  config_FRIO_irq();
}
