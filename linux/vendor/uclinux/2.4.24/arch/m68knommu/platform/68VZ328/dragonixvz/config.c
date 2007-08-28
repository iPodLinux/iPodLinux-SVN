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


#ifdef CONFIG_DRAGONIXVZ_JTAGFPGA
#include "jblaster.h"
	//extern int jbi_main();
	extern unsigned char rtccmosinvalid(void);
	extern void rtcchargebattery(void);
	extern unsigned char lcdsetcontrast(void);
        extern void gethwaddr(unsigned char dev, unsigned char *addr);
        extern void rtcgetintern(void);
        extern void rtcgettod(int *yearp, int *monp, int *dayp, int *hourp, int *minp, int *secp);
#ifdef CONFIG_DRAGONIX_SPI
	/* Include function specific to dragonix hardware */
	extern void dragonixrtc_gettod(int *year, int *mon, int *day, int *hour, int *min, int *sec);
	extern int dragonixrtc_set_clock_mmss(unsigned long nowtime);
#endif
#endif

#include <asm/MC68VZ328.h>

void BSP_sched_init(void (*timer_routine)(int, void *, struct pt_regs *))
{
#ifdef CONFIG_DRAGONBALL_USE_RTC
  /* Enable RTC */
  RTCCTL|=RTCCTL_EN;
  /* Enable 128Hz interrupt line */
  RTCIENR|=RTCIENR_SAM5;
  request_irq(SAM_IRQ_NUM, timer_routine, IRQ_FLG_LOCK, "timer", NULL);
#else
  /* Restart mode, Enable int, 32KHz, Enable timer */
  TCTL = TCTL_OM | TCTL_IRQEN | TCTL_CLKSOURCE_32KHZ | TCTL_TEN;
  /* Set prescaler (Divide 32.768KHz by 1)*/
  TPRER = 0;
  /* Set compare register  32.768Khz / 32 / 10 = 100 */
  TCMP = 327;
  request_irq(TMR_IRQ_NUM, timer_routine, IRQ_FLG_LOCK, "timer", NULL);
#endif
}

void BSP_tick(void)
{
#ifdef CONFIG_DRAGONBALL_USERTC
        RTCISR|=RTCISR_SAM5;
#else
	/* Reset Timer1 */
        TSTAT &= 0;
#endif

}

unsigned long BSP_gettimeoffset (void)
{
  return 0;
}

void BSP_gettod (int *yearp, int *monp, int *dayp,
		   int *hourp, int *minp, int *secp)
{
 rtcgettod(yearp, monp, dayp, hourp, minp, secp);
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
    moveal #0x02000000, %a0;
    moveb #0, 0xFFFFF300;
    moveal 0(%a0), %sp;
    moveal 4(%a0), %a0;
    jmp (%a0);
    ");
}

unsigned char cs8900a_hwaddr1[6],cs8900a_hwaddr2[6];
static int errno;

void config_BSP(char *command, int len)
{
  int year, mon, day, hour, min, sec;
  printk("\nDragonixVZ support by Daniel Haensse daniel.haensse@dragonix.net\n");
  #ifdef CONFIG_DRAGONIXVZ_JTAGFPGA
        jb_main();
        /* FIXME hack to mask fpga interrupt controller */
        *(volatile unsigned  char *)0x04000100 = 0x07; /* Mask RS232C Interrupts  */
        *(volatile unsigned  char *)0x04000104 = 0x01; /* Mask Pen IRQ  */

        /* Make serial port ready, looks like this is not done in the serial port driver, because people
           use to send characters over the first serial line right after start */
        #ifdef CONFIG_68328_SERIAL
                #ifdef CONFIG_68328_SERIAL_RTS_CTS
                        PESEL=(PESEL&0x0f);
                #else
                        PESEL=(PESEL&0x0f)|0xc0;
                #endif
                #ifdef CONFIG_68328_SERIAL_UART2
                        #ifdef CONFIG_68328_SERIAL_UART2_RTS_CTS
                                PJSEL=(PJSEL&0x0f);
                        #else
                                PJSEL=(PJSEL&0x0f)|0xc0;
                        #endif
                #endif
        #endif
        /* No we do some real initialisation */
        /* First let us check if CMOS content is valid, if not, validate it within the function */
        if(rtccmosinvalid())
        {
         printk("\nOops, CMOS is invalid, defaults set\n");
        }
        /* Get date & time */
        rtcgettod(&year, &mon, &day,&hour, &min, &sec);
        printk("\n%02d.%02d.20%02d %02d:%02d:%02d\n",day,mon,year,hour,min,sec);
        /* switch on cell charger because we want it to be valid in the future as well :-) */
        printk("Lithium cell trickle charger enabled\n");
        rtcchargebattery();
        /* Switch on the contrast of the display */
        printk("Contrast controller set to 0x%x\n",lcdsetcontrast());

        /* CS8900 MAC Address */
        gethwaddr(0,cs8900a_hwaddr1);
        printk("1st cs8900: hwaddr %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
        cs8900a_hwaddr1[0], cs8900a_hwaddr1[1], cs8900a_hwaddr1[2], cs8900a_hwaddr1[3], cs8900a_hwaddr1[4], cs8900a_hwaddr1[5]);
        /* We probe later in the cs8900 for it */
        gethwaddr(1,cs8900a_hwaddr2);
        printk("2st cs8900: hwaddr %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
        cs8900a_hwaddr2[0], cs8900a_hwaddr2[1], cs8900a_hwaddr2[2], cs8900a_hwaddr2[3], cs8900a_hwaddr2[4], cs8900a_hwaddr2[5]);

        /* Read date from external RTC */
        rtcgetintern();
  #endif

  printk("68VZ328 DragonBallVZ support (c) 2001 Lineo, Inc.\n");

  command[0] = 0;

  mach_sched_init      = BSP_sched_init;
  mach_tick            = BSP_tick;
#ifdef CONFIG_DRAGONIX_SPI
  mach_gettimeoffset   = BSP_gettimeoffset;
  mach_gettod          = dragonixrtc_gettod;
  mach_hwclk           = NULL;
  mach_set_clock_mmss  = dragonixrtc_set_clock_mmss;
#else
  mach_gettimeoffset   = BSP_gettimeoffset;
  mach_gettod          = BSP_gettod;
  mach_hwclk           = NULL;
  mach_set_clock_mmss  = NULL;
#endif
  // mach_mksound         = NULL;
  mach_reset           = BSP_reset;
  // mach_debug_init      = NULL;

  config_M68VZ328_irq();
}
