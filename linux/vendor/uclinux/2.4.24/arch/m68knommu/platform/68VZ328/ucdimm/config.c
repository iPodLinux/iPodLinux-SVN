/*
 *  linux/arch/$(ARCH)/platform/$(PLATFORM)/config.c
 *
 *  Copyright (C) 1993 Hamish Macdonald
 *  Copyright (C) 1999 D. Jeff Dionne
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

#include <asm/setup.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/irq.h>
#include <asm/machdep.h>
#include <asm/uCbootstrap.h>
#include <asm/MC68VZ328.h>

#define CLOCK_SOURCE TCTL_CLKSOURCE_SYSCLK
#define CLOCK_PRE 7

/* sysclock / HZ / TPRER+1
   The PLL frequency is 32768 with a default multiplier of 2024
   which results in a PLL clock of 66.322 MHz, resulting in
   a clock freq of 33.161 MHz */
#define TICKS_PER_JIFFY 41451

static void BSP_sched_init(void (*timer_routine)(int, void *, struct pt_regs *))
{
#ifdef CONFIG_DRAGONBALL_USE_RTC
	/* Enable RTC in RTC control register (defined in asm/MC68VZ328.h) */
	RTCCTL |= RTCCTL_ENABLE;
	/* Enable 128 Hz interrupt line (defined in asm/MC68VZ328.h) */
	/* SAM5 == 0x2000 or 128Hz / 150.0000Hz interrupt enable */
	RTCIENR |= RTCIENR_SAM5;
	/* SAM_IRQ_NUM = Sampling timer for RTC */
	request_irq(SAM_IRQ_NUM, timer_routine, IRQ_FLG_LOCK, "timer", NULL);
#else
	TCTL = 0; /* disable timer 1 */
	/* set ISR */
	request_irq(TMR_IRQ_NUM, timer_routine, IRQ_FLG_LOCK, "timer", NULL);
	/* Restart mode, Enable int, Set clock source */
	TCTL = TCTL_OM | TCTL_IRQEN | CLOCK_SOURCE;
	TPRER = CLOCK_PRE;
	TCMP = TICKS_PER_JIFFY;
	TCTL |= TCTL_TEN;               /* Enable timer 1 */
#endif
}

static void BSP_tick(void)
{
#ifdef CONFIG_DRAGONBALL_USE_RTC
	RTCISR |= RTCISR_SAM5;
#else
	/* Reset Timer1 */
	TSTAT &= 0;
#endif
}

static unsigned long BSP_gettimeoffset(void)
{
#ifdef CONFIG_DRAGONBALL_USE_RTC
	return 0;
#else
	unsigned long ticks = TCN, offset = 0;
	
	/* check for pending interrupt */
	if (ticks < (TICKS_PER_JIFFY>>1) && (ISR & (1<<TMR_IRQ_NUM)))
		offset = 1000000/HZ;
	ticks = (ticks * 1000000/HZ) / TICKS_PER_JIFFY;
	return ticks + offset;
#endif
}

void BSP_gettod (int *yearp, int *monp, int *dayp,
		   int *hourp, int *minp, int *secp)
{
	*yearp = *monp = *dayp = *hourp = *minp = *secp = 0;
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
  cli();
  asm volatile ("
    moveal #0x10c00000, %a0;
    moveb #0, 0xFFFFF300;
    moveal 0(%a0), %sp;
    moveal 4(%a0), %a0;
    jmp (%a0);
    ");
}

unsigned char *cs8900a_hwaddr;
static int errno;

_bsc0(char *, getserialnum)
_bsc1(unsigned char *, gethwaddr, int, a)
_bsc1(char *, getbenv, char *, a)

void config_BSP(char *command, int len)
{
  unsigned char *p;

  printk("\n68VZ328 DragonBallVZ support (c) 2003 Arcturus Networks, Inc.\n");

  printk("uCdimm serial string [%s]\n",getserialnum());
  p = cs8900a_hwaddr = gethwaddr(0);
  printk("uCdimm hwaddr %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
         p[0], p[1], p[2], p[3], p[4], p[5]);
  p = getbenv("APPEND");
  if (p)
	  strcpy(command, p);
  else
	  command[0] = 0;
 
  mach_sched_init      = BSP_sched_init;
  mach_tick            = BSP_tick;
  mach_gettimeoffset   = BSP_gettimeoffset;
  mach_gettod          = BSP_gettod;
  mach_hwclk           = NULL;
  mach_set_clock_mmss  = NULL;
  // mach_mksound         = NULL;
  mach_reset           = BSP_reset;
  // mach_debug_init      = NULL;

  config_M68VZ328_irq();
}
