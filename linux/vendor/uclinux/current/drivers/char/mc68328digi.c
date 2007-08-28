/*----------------------------------------------------------------------------*/
/* mc68328digi.c,v 1.2 2001/02/12 11:14:10 pney Exp
 *
 * linux/drivers/char/mc68328digi.c - Touch screen driver.
 *
 * Author: Philippe Ney <philippe.ney@smartdata.ch>
 * Copyright (C) 2001 SMARTDATA <www.smartdata.ch>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Thanks to:
 *    Kenneth Albanowski for is first work on a touch screen driver.
 *    Alessandro Rubini for is "Linux device drivers" book.
 *    Ori Pomerantz for is "Linux Kernel Module Programming" guide.
 *
 * Updates:
 *   2001-03-07 Pascal bauermeister <pascal.bauermeister@smartdata.ch>
 *              - Adapted to work ok on xcopilot; yet untested on real Palm.
 *              - Added check for version in ioctl()
 *
 *   2001-03-21 Pascal bauermeister <pascal.bauermeister@smartdata.ch>
 *              - bypass real hw uninits for xcopilot. Now xcopilot is
 *                happy. It even no longer generates pen irq while this
 *                irq is disabled (I'd like to understand why, so that
 *                I can do the same in mc68328digi_init() !).
 *
 *   2001-10-23 Daniel Potts <danielp@cse.unsw.edu.au>
 *              - support for uCdimm/68VZ328. I use SPI 2 which maps to the
 *                same addresses as for 68EZ328 and requries less changes to
 *                the code. For CS I use PF_A22; CSD0/CAS0 is not made 
 *                available off the uCdimm.
 *                PF1 (/IRQ5) is already used on uCdimm by the ethernet
 *                controller. We use /IRQ6 instead (PD7).
 *              - kill_fasync and request_irq fixes for 2.4.x (only) support.
 *
 *
 * Hardware:      Motorola MC68EZ328 DragonBall-EZ
 *                Burr-Brown ADS7843 Touch Screen Controller
 *                Rikei REW-A1 Touch Screen
 *
 * OS:            uClinux version 2.0.38.1pre3
 *
 * Connectivity:  Burr-Brown        DragonBall
 *                PENIRQ     ---->  PF1  &  PD4 (with a 100k resistor)
 *                BUSY       ---->  PF5
 *                CS         ---->  PB4
 *                DIN        ---->  PE0
 *                DOUT       ---->  PE1
 *                DCLK       ---->  PE2
 *
 *     ADS7843: /PENIRQ ----+----[100k resistor]---- EZ328: PD4
 *                          |
 *                          +----------------------- EZ328: /IRQ5
 *
 * States graph:
 *
 *         ELAPSED & PEN_UP
 *         (re-init)             +------+
 *     +------------------------>| IDLE |<------+
 *     |                         +------+       |
 *     |                           |            | ELAPSED
 *     |  +------------------------+            | (re-init)
 *     |  |       PEN_IRQ                       |
 *     |  |       (disable pen_irq)             | 
 *     |  |       (enable & set timer)          | 
 *     |  |                                     |
 *     | \/                                     |
 *   +------+                                   |
 *   | WAIT |                                   |
 *   +------+                                   |
 *   /\   |                                     | 
 *   |    |             +---------------+-------+------+---------------+
 *   |    |             |               |              |               |
 *   |    |             |               |              |               |
 *   |    |        +--------+      +-------+      +--------+      +--------+
 *   |    +------->| ASK X  |      | ASK Y |      | READ X |      | READ Y |
 *   |  ELAPSED &  +--------+      +-------+      +--------+      +--------+
 *   |  PEN_DOWN       |           /\    |         /\    |         /\    |
 *   |  (init SPIM)    +-----------'     +---------'     +---------'     |
 *   |  (set timer)    XCH_COMPLETE      XCH_COMPLETE    XCH_COMPLETE    |
 *   |                                                                   |
 *   |                                                                   |
 *   |                                                                   |
 *   +-------------------------------------------------------------------+
 *                               XCH_COMPLETE
 *                               (release SPIM)
 *                               (set timer)
 *
 *
 * Remarks: I added some stuff for port on 2.2 and 2.4 kernels but currently
 *          this version works only on 2.0 because not tested on 2.4 yet.
 *          Someone interested?
 *
 */
/*----------------------------------------------------------------------------*/

#include <linux/kernel.h>     /* We're doing kernel work */
#include <linux/module.h>     /* Specifically, a module */
#include <linux/interrupt.h>  /* We want interrupts */

#include <linux/miscdevice.h> /* for misc_register() and misc_deregister() */

#include <linux/fs.h>         /* for struct 'file_operations' */

#include <linux/timer.h>     /* for timeout interrupts */
#include <linux/param.h>     /* for HZ. HZ = 100 and the timer step is 1/100 */
#include <linux/sched.h>     /* for jiffies definition. jiffies is incremented
                              * once for each clock tick; thus it's incremented
			      * HZ times per secondes.*/
#include <linux/mm.h>        /* for verify_area */
#include <linux/malloc.h>    /* for kmalloc */
#include <linux/init.h>

#include <asm/irq.h>         /* For IRQ_MACHSPEC */

/*----------------------------------------------------------------------------*/

#if defined(CONFIG_M68328)
/* These are missing in MC68328.h. I took them from MC68EZ328.h temporarily,
 * but did not dare modifying MC68328.h yet (don't want to break things and
 * have no time now to do regression testings)
 *   - Pascal Bauermeister
 */
#define ICR_ADDR        0xfffff302
#define ICR_POL5        0x0080  /* Polarity Control for IRQ5 */
#define IMR_MIRQ5       (1 << IRQ5_IRQ_NUM)     /* Mask IRQ5 */
#define IRQ5_IRQ_NUM    20      /* IRQ5 */
#define PBPUEN          BYTE_REF(PBPUEN_ADDR)
#define PBPUEN_ADDR     0xfffff40a              /* Port B Pull-Up enable reg */
#define PB_CSD0_CAS0    0x10    /* Use CSD0/CAS0 as PB[4] */
#define PDSEL           BYTE_REF(PDSEL_ADDR)
#define PDSEL_ADDR      0xfffff41b              /* Port D Select Register */
#define PD_IRQ1         0x10    /* Use IRQ1 as PD[4] */
#define PF_A22          0x20    /* Use A22       as PF[5] */
#define PF_IRQ5         0x02    /* Use IRQ5      as PF[1] */
#define SPIMCONT        WORD_REF(SPIMCONT_ADDR)
#define SPIMCONT_ADDR   0xfffff802
#define SPIMCONT_SPIMEN         SPIMCONT_RSPIMEN

#elif defined(CONFIG_M68EZ328)
# include <asm/MC68EZ328.h>  /* bus, port and irq definition of DragonBall EZ */
#elif defined(CONFIG_M68VZ328)
# include <asm/MC68VZ328.h>
#else
# error "mc68328digi: a known machine not defined."
#endif

#include <linux/mc68328digi.h>

/*----------------------------------------------------------------------------*/
/* Debug */

#if 0
#if 0
# define DPRINTK printk("%0: [%03d,%03d] ", __file__, \
                        current_pos.x, current_pos.y); \
                        printk
#else
# define DPRINTK __dummy__
  static void __dummy__(const char* fmt, ...) {}
#endif
#endif
/*----------------------------------------------------------------------------*/

static const char* __file__ = __FILE__;

/*----------------------------------------------------------------------------*//*
 * Kernel compatibility.
 * Kernel > 2.2.3, include/linux/version.h contain a macro for KERNEL_VERSION
 */
#include <linux/version.h>
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c)  (((a) << 16) + ((b) << 8) + (c))
#endif

/*
 * Conditional compilation. LINUX_VERSION_CODE is the code of this version
 * (as per KERNEL_VERSION)
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0))
#include <asm/uaccess.h>    /* for put_user     */
#include <linux/poll.h>     /* for polling fnct */
#endif

/*
 * time limit 
 * Used for a whole conversion (x & y) and to clock the Burr-Brown to fall the
 * BUSY signal down (otherwise, pen cannot generate irq anymore).
 * If this limit run out, an error is generated. In the first case, the driver
 * could recover his normal state. In the second, the BUSY signal cannot be
 * fallen down and the device have to be reseted.
 * This limit is defined as approximatively the double of the deglitch one.
 */
#define CONV_TIME_LIMIT    50      /* ms */

/* 
 * Length of the data structure
 */
#define DATA_LENGTH       sizeof(struct ts_pen_info)

/*
 * Size of the buffer for the event queue
 */
#define TS_BUF_SIZE        32*DATA_LENGTH

/*
 * Minor number for the touch screen device. Major is 10 because of the misc 
 * type of the driver.
 */
#define MC68328DIGI_MINOR    9

/*
 * SPIMCONT fields for this app (not defined in asm/MC68EZ328.h).
 */
#define SPIMCONT_DATARATE ( 7 <<SPIMCONT_DATA_RATE_SHIFT)/* SPICLK = CLK / 512 */
#define SPIMCONT_BITCOUNT (15<< 0)                      /* 16 bits transfert  */

/*
 * SPIMCONT used values.
 */
#define SPIMCONT_INIT (SPIMCONT_DATARATE    | \
                       SPIMCONT_ENABLE      | \
                       SPIMCONT_XCH      *0 | \
                       SPIMCONT_IRQ      *0 | \
                       SPIMCONT_IRQEN       | \
                       SPIMCONT_PHA         | \
                       SPIMCONT_POL         | \
                       SPIMCONT_BITCOUNT      )

/*
 * ADS7843 fields (BURR-BROWN Touch Screen Controller).
 */
#define ADS7843_START_BIT (1<<7)
#define ADS7843_A2        (1<<6)
#define ADS7843_A1        (1<<5)
#define ADS7843_A0        (1<<4)
#define ADS7843_MODE      (1<<3)  /* HIGH = 8, LOW = 12 bits conversion */
#define ADS7843_SER_DFR   (1<<2)  /* LOW = differential mode            */
#define ADS7843_PD1       (1<<1)  /* PD1,PD0 = 11  PENIRQ disable       */
#define ADS7843_PD0       (1<<0)  /* PD1,PD0 = 00  PENIRQ enable        */

/*
 * SPIMDATA used values.
 */
/*
 * Ask for X conversion. Disable PENIRQ.
 */
#define SPIMDATA_ASKX   (ADS7843_START_BIT   | \
                         ADS7843_A2          | \
			 ADS7843_A1        *0| \
			 ADS7843_A0          | \
                         ADS7843_MODE      *0| \
                         ADS7843_SER_DFR   *0| \
                         ADS7843_PD1         | \
                         ADS7843_PD0            )
/*
 * Ask for Y conversion. Disable PENIRQ.
 */
#define SPIMDATA_ASKY   (ADS7843_START_BIT   | \
                         ADS7843_A2        *0| \
 			 ADS7843_A1        *0| \
	 	 	 ADS7843_A0          | \
                         ADS7843_MODE      *0| \
                         ADS7843_SER_DFR   *0| \
                         ADS7843_PD1         | \
                         ADS7843_PD0            )
/*
 * Enable PENIRQ.
 */
#define SPIMDATA_NOP    (ADS7843_START_BIT   | \
                         ADS7843_A2        *0| \
 			 ADS7843_A1        *0| \
	 	 	 ADS7843_A0        *0| \
                         ADS7843_MODE      *0| \
                         ADS7843_SER_DFR   *0| \
                         ADS7843_PD1       *0| \
                         ADS7843_PD0       *0   )
/*
 * Generate clock to fall pull BUSY signal.
 * No start bit to avoid generating a BUSY at end of the transfert.
 */
#define SPIMDATA_NULL    0

/*
 * Useful masks.
 */
#if defined (CONFIG_UCDIMM)
#define PEN_MASK	PD_IRQ6  /* pen irq signal from the Burr-Brown is */
#define PEN_MASK_2	PD_IRQ1  /* connected to 2 pins of the DragonBall */
                             /* (bit 2 port F and bit 4 port D)       */
#define BUSY_MASK	PF_A22
#define CS_MASK		PF_CSA1
#define PESEL_MASK	0x07
#define CONV_MASK	0x0FFF   /* 12 bit conversion */

#define PEN_IRQ_NUM	IRQ6_IRQ_NUM

#define CS_DATA		PFDATA
#define CS_PUEN		PFPUEN
#define CS_DIR		PFDIR
#define CS_SEL		PFSEL

#define PENIRQ_DATA 	PDDATA
#define PENIRQ_PUEN	PDPUEN
#define PENIRQ_DIR	PDDIR
#define PENIRQ_SEL	PDSEL
#define ICR_PENPOL	ICR_POL6
#define IMR_MPEN	IMR_MIRQ6

#else
#error "unexpected"
#define USE_PENIRQ_PULUP
#define PEN_MASK	PF_IRQ5  /* pen irq signal from the Burr-Brown is */
#define PEN_MASK_2	PD_IRQ1  /* connected to 2 pins of the DragonBall */
                             /* (bit 2 port F and bit 4 port D)       */
#define BUSY_MASK	PF_A22
#define CS_MASK		PB_CSD0_CAS0
#define PESEL_MASK	0x07
#define CONV_MASK	0x0FFF   /* 12 bit conversion */

#define PEN_IRQ_NUM	IRQ5_IRQ_NUM

/* chip select register offset */
#define CS_DATA		PBDATA
#define CS_PUEN		PBPUEN
#define CS_DIR		PBDIR
#define CS_SEL		PBSEL

/* pen irq register offsets and data */
#define PENIRQ_DATA	PFDATA
#define PENIRQ_PUEN	PFPUEN
#define PENIRQ_DIR	PFDIR
#define PENIRQ_SEL	PFSEL
#define ICR_PENPOL	ICR_POL5
#define IMR_MPEN	IMR_MIRQ5
#endif


/*
 * Touch screen driver states.
 */
#define TS_DRV_ERROR       -1
#define TS_DRV_IDLE         0
#define TS_DRV_WAIT         1
#define TS_DRV_ASKX         2
#define TS_DRV_ASKY         3
#define TS_DRV_READX        4
#define TS_DRV_READY        5

/*
 * Conversion status.
 */
#define CONV_ERROR      -1   /* error during conversion flow        */    
#define CONV_END         0   /* conversion ended (= pen is up)      */
#define CONV_LOOP        1   /* conversion continue (= pen is down) */

/*----------------------------------------------------------------------------*/
/* macros --------------------------------------------------------------------*/

#define TICKS(nbms)        ((HZ*(nbms))/1000)

/*----------------------------------------------------------------------------*/

/*
 * Look in the PF register if it is on interrupt state.
 */
#ifdef CONFIG_XCOPILOT_BUGS
# define IS_PEN_DOWN    ((IPR&IPR_PEN)!=0)
#else
# define IS_PEN_DOWN	((PENIRQ_DATA & PEN_MASK)==ST_PEN_DOWN)
#endif

/*
 * State of the BUSY signal of the SPIM.
 */
#define IS_BUSY_ENDED   ((PFDATA & BUSY_MASK)==0)

/*
 * Write the SPIM (data and control).
 */
#define SET_SPIMDATA(x) { SPIMDATA = x; }
#define SET_SPIMCONT(x) { SPIMCONT = x; }

/*----------------------------------------------------------------------------*/

#define SPIM_ENABLE  { SET_SPIMCONT(SPIMCONT_ENABLE); }  /* enable SPIM       */
#define SPIM_INIT    { SPIMCONT |= SPIMCONT_INIT; }      /* init SPIM         */
#define SPIM_DISABLE { SET_SPIMCONT(0x0000); }           /* disable SPIM      */

/*----------------------------------------------------------------------------*/
#ifdef CONFIG_XCOPILOT_BUGS
# define CARD_SELECT   
# define CARD_DESELECT 
#else
# define CARD_SELECT   { CS_DATA &= ~CS_MASK; }     /* PB4 active (=low)       */
# define CARD_DESELECT { CS_DATA |=  CS_MASK; }     /* PB4 inactive (=high)    */
#endif
/*----------------------------------------------------------------------------*/

#define ENABLE_PEN_IRQ    { IMR &= ~IMR_MPEN; }
#define ENABLE_SPIM_IRQ   { IMR &= ~IMR_MSPI; SPIMCONT |= SPIMCONT_IRQEN; }

#define DISABLE_PEN_IRQ   { IMR |= IMR_MPEN; }
#define DISABLE_SPIM_IRQ  { SPIMCONT &= ~SPIMCONT_IRQEN; IMR |= IMR_MSPI; }

#define CLEAR_SPIM_IRQ    { SPIMCONT &= ~SPIMCONT_IRQ; }

/*----------------------------------------------------------------------------*/
/* predefinitions ------------------------------------------------------------*/

static void handle_timeout(void);
static void handle_pen_irq(int irq, void *dev_id, struct pt_regs *regs);
static void handle_spi_irq(void);

/*----------------------------------------------------------------------------*/
/* structure -----------------------------------------------------------------*/

struct ts_pen_position { int x,y; };

struct ts_queue {
  unsigned long head;
  unsigned long tail;
  wait_queue_head_t proc_list;
  struct fasync_struct *fasync;
  unsigned char buf[TS_BUF_SIZE];
};

/*----------------------------------------------------------------------------*/
/* Variables -----------------------------------------------------------------*/

/*
 * driver state variable.
 */
static int ts_drv_state;

/*
 * first tick initiated at ts_open.
 */
static struct timeval   first_tick;

/*
 * pen state.
 */
static int new_pen_state;

/*
 * event counter.
 */
static int ev_counter;

/*
 * counter to differentiate a click from a move.
 */
static int state_counter;

static struct timer_list         ts_wake_time;

/*
 * drv parameters.
 */
static struct ts_drv_params      current_params;
static int                       sample_ticks;
/*
 * pen info variables.
 */
static struct ts_pen_info        ts_pen;
static struct ts_pen_info        ts_pen_prev;
static struct ts_pen_position    current_pos;

/*
 * driver management.
 */
static struct ts_queue *queue;
static int    device_open = 0;   /* number of open device to prevent concurrent
                                  * access to the same device
				  */


/*----------------------------------------------------------------------------*/
/* Init & release functions --------------------------------------------------*/

static void init_ts_state(void) {
  DISABLE_SPIM_IRQ;      /* Disable SPIM interrupt */

  ts_drv_state = TS_DRV_IDLE;
  state_counter = 0;

  ENABLE_PEN_IRQ;        /* Enable interrupt IRQ5  */
}

static void init_ts_pen_values(void) {
  ts_pen.x = 0;ts_pen.y = 0;
  ts_pen.dx = 0;ts_pen.dy = 0;
  ts_pen.event = EV_PEN_UP;
  ts_pen.state &= 0;ts_pen.state |= ST_PEN_UP;

  ts_pen_prev.x = 0;ts_pen_prev.y = 0;
  ts_pen_prev.dx = 0;ts_pen_prev.dy = 0;
  ts_pen_prev.event = EV_PEN_UP;
  ts_pen_prev.state &= 0;ts_pen_prev.state |= ST_PEN_UP;
  
  new_pen_state = 0;
  ev_counter = 0;
  do_gettimeofday(&first_tick);
}

static void init_ts_timer(struct timer_list *timer) {
  init_timer(timer);
  timer->function = (void *)handle_timeout;
}

static void release_ts_timer(struct timer_list *timer) {
  del_timer(timer);
}

/*
 * Set default values for the params of the driver.
 */
static void init_ts_settings(void) {
  current_params.version = MC68328DIGI_VERSION;
  current_params.version_req = 0;
  current_params.x_ratio_num = 1;
  current_params.x_ratio_den = 1;
  current_params.y_ratio_num = 1;
  current_params.y_ratio_den = 1;
  current_params.x_offset = 0;
  current_params.y_offset = 0;
  current_params.xy_swap = 0;
  current_params.x_min = 0;
  current_params.x_max = 4095;
  current_params.y_min = 0;
  current_params.y_max = 4095;
  current_params.mv_thrs = 50;
  current_params.follow_thrs = 5;    /* to eliminate the conversion noise */
  current_params.sample_ms = 20;
  current_params.deglitch_on = 1;
  current_params.event_queue_on = 1;
  sample_ticks = TICKS(current_params.sample_ms);
}

static void init_ts_hardware(void) {
  /* Set bit 2 of port F for interrupt (IRQ5 for pen_irq)                 */
#if 0
  PENIRQ_PUEN |= PEN_MASK;     /* Port F as pull-up			*/
#endif
  PENIRQ_DIR &= ~PEN_MASK;    /* PF1 as input                                 */
  PENIRQ_SEL &= ~PEN_MASK;    /* PF1 internal chip function is connected
			       */
#if 0
  /* Set polarity of IRQ5 as low (interrupt occure when signal is low)    */
  ICR &= ~ICR_PENPOL;
#endif

  /* Init stuff for port E. The 3 LSB are multiplexed with the SPIM       */
  PESEL &= ~PESEL_MASK;
  
  /* Set bit 4 of port B for the CARD_SELECT signal of ADS7843 as output. */
#ifndef CONFIG_XCOPILOT_BUGS
  CS_PUEN |= CS_MASK;       /* Port B as pull-up                           */
  CS_DIR  |= CS_MASK;       /* PB4 as output                               */
  CS_DATA |= CS_MASK;       /* PB4 inactive (=high)                        */
  CS_SEL  |= CS_MASK;       /* PB4 as I/O not dedicated                    */
#endif
  
  /* Set bit 5 of port F for the BUSY signal of ADS7843 as input          */
  PFPUEN |= BUSY_MASK;     /* Port F as pull-up                           */
  PFDIR  &= ~BUSY_MASK;    /* PF5 as input                                */
  PFSEL  |= BUSY_MASK;     /* PF5 I/O port function pin is connected      */

#ifdef USE_PENIRQ_PULLUP
  /* Set bit 4 of port D for the pen irq pull-up of ADS7843 as output.    */
  PDDIR  |= PEN_MASK_2;    /* PD4 as output                               */
  PDDATA |= PEN_MASK_2;    /* PD4 up (pull-up)                            */
  PDPUEN |= PEN_MASK_2;    /* PD4 as pull-up                              */
#ifndef CONFIG_XCOPILOT_BUGS
  PDSEL  |= PEN_MASK_2;    /* PD4 I/O port function pin is connected      */
#endif
#endif
}


/*
 * Reset bits used in each register to their default value.
 */
static void release_ts_hardware(void) {
                          /* Register   default value */
#ifndef CONFIG_XCOPILOT_BUGS
  PESEL  |= PESEL_MASK;   /* PESEL      0xFF          */
  CS_DIR  &= ~CS_MASK;    /* PBDIR      0x00          */
  CS_DATA &= ~CS_MASK;    /* PBDATA     0x00          */
  CS_PUEN |= CS_MASK;     /* PBPUEN     0xFF          */
  CS_SEL  |= CS_MASK;     /* PBSEL      0xFF          */
#ifdef USE_PENIRQ_PULLUP
  PDDIR  |= PEN_MASK_2;   /* PDDIR      0xFF          */
  PDDATA |= PEN_MASK_2;   /* PDDATA     0xF0          */
  PDPUEN |= PEN_MASK_2;   /* PDPUEN     0xFF          */
  PDSEL  |= PEN_MASK_2;   /* PDSEL      0xF0          */
#endif
  PENIRQ_PUEN |= PEN_MASK;     /* PFPUEN     0xFF          */
  PENIRQ_DIR  &= ~PEN_MASK;    /* PFDIR      0x00          */
  PENIRQ_SEL  |= PEN_MASK;     /* PFSEL      0xFF          */
#endif
  IMR    |= IMR_MPEN;     /* IMR        0xFFFFFFFF    */
                          /* I release it anyway!     */ 
}

static void init_ts_drv(void) {
  printk("a\n");
  init_ts_state();
  printk("b\n");
  init_ts_pen_values();
  printk("c\n");
  init_ts_timer(&ts_wake_time);
  printk("d\n");
  init_ts_hardware();
  printk("e\n");
}

static void release_ts_drv(void) {
  release_ts_timer(&ts_wake_time);
  release_ts_hardware();
}


/*----------------------------------------------------------------------------*/
/* scaling functions ---------------------------------------------------------*/

static inline void rescale_xpos(int *x) {
  *x &= CONV_MASK;

  *x *= current_params.x_ratio_num;
  *x /= current_params.x_ratio_den;
  *x += current_params.x_offset;
  *x = (*x > current_params.x_max ? current_params.x_max :
	(*x < current_params.x_min ? current_params.x_min :
	 *x));
}

static inline void rescale_ypos(int *y) {
  *y &= CONV_MASK;

  *y *= current_params.y_ratio_num;
  *y /= current_params.y_ratio_den;
  *y += current_params.y_offset;
  *y = (*y > current_params.y_max ? current_params.y_max :
	(*y < current_params.y_min ? current_params.y_min :
	 *y));
}

static inline void swap_xy(int *x, int *y)
{
  if(current_params.xy_swap) {
    int t = *x;
    *x = *y;
    *y = t;
  }
}

/*----------------------------------------------------------------------------*/
/* xcopilot compatibility hacks ----------------------------------------------*/
#ifdef CONFIG_XCOPILOT_BUGS

/* xcopilot has the following bugs:
 *
 * - Disabling the penirq has no effect; we keep on getting irqs even when
 *   penirq is disabled; this is not too annoying: we just trash pen irq events
 *   that come when disabled.
 *
 * - SPIM interrupt is not simulated; this is not too annoying: we just read
 *   SPI data immediately and bypass a couple of states related to SPI irq.
 *
 * - We do not get mouse drag events; this is a known xcopilot bug.
 *   This is annoying: we miss all moves ! You should patch and recompile
 *   your xcopilot.
 *
 *   This has been reported as Debian bug #64367, and there is a patch there:
 *     http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=64367&repeatmerged=yes
 *   which is:
 *   +-----------------------------------------------------------------------+
 *   | --- display.c   Tue Aug 25 14:56:02 1998
 *   | +++ display2.c  Fri May 19 16:07:52 2000
 *   | @@ -439,7 +439,7 @@
 *   | static void HandleDigitizer(Widget w, XtPointer client_data, XEvent\
 *   | *event, Boolean *continue_to_dispatch)
 *   | {
 *   | -  Time start;
 *   | +  static Time start;
 *   | 
 *   | *continue_to_dispatch = True;
 *   +-----------------------------------------------------------------------+
 *   In short, add 'static' to the declaration of 'start' in HandleDigitizer()
 *   in display.c
 *
 *   With this patch, all works perfectly !
 *
 * - The pen state can be read only in IPR, not in port D; again, we're doing
 *   the workaround.
 *
 * With all these workarounds in this file, and with the patch on xcopilot,
 * things go perfectly smoothly.
 *
 */

/*
 * Ugly stuff to read the mouse position on xcopilot
 *
 * We get the position of the last mouse up/down only, we get no moves !!! :-(
 */
static void xcopilot_read_now(void) {
    PFDATA &= ~0xf; PFDATA |= 6;
    SPIMCONT |= SPIMCONT_XCH | SPIMCONT_IRQEN;
    current_pos.x = SPIMDATA;
    rescale_xpos(&current_pos.x);

    PFDATA &= ~0xf; PFDATA |= 9;
    SPIMCONT |= SPIMCONT_XCH | SPIMCONT_IRQEN;
    current_pos.y = SPIMDATA;
    rescale_ypos(&current_pos.y);

    swap_xy(&current_pos.x, &current_pos.y);
}


#endif
/*----------------------------------------------------------------------------*/
/* flow functions ------------------------------------------------------------*/

/*
 * Set timer irq to now + delay ticks.
 */
static void set_timer_irq(struct timer_list *timer,int delay) {
  del_timer(timer);
  timer->expires = jiffies + delay;
  add_timer(timer);
}

/*
 * Set ADS7843 and SPIM parameters for transfer.
 */
static void set_SPIM_transfert(void) {
  /* DragonBall drive I/O 1 and I/O 2 LOW to reverse bias the PENIRQ diode
   * of the ADS7843
   */
#ifdef USE_PENIRQ_PULLUP
  PDDATA &= ~PEN_MASK_2;        /* Pull down I/0 2 of PENIRQ */
#endif
  PENIRQ_DIR  |= PEN_MASK;      /* I/O 1 of PENIRQ as output */
  PENIRQ_DATA &= ~PEN_MASK;     /* Pull down I/O 2 of PENIRQ */

  /* Initiate selection and parameters for using the Burr-Brown ADS7843
   * and the DragonBall SPIM.
   */
  CARD_SELECT;          /* Select ADS7843.     */
  SPIM_ENABLE;          /* Enable SPIM         */
  SPIM_INIT;            /* Set SPIM parameters */
}

/*
 * Clock the Burr-Brown to fall the AD_BUSY. 
 * With a 'start' bit and PD1,PD0 = 00 to enable PENIRQ.
 * Used for the first pen irq after booting. And when error occures during
 * conversion that need an initialisation.
 */
static void fall_BUSY_enable_PENIRQ(void) {
  SET_SPIMDATA(SPIMDATA_NOP);
  SPIMCONT |= SPIMCONT_XCH;     /* initiate exchange */
}

/*
 * Release transfer settings.
 */
static void release_SPIM_transfert(void) {
  SPIM_DISABLE;
  CARD_DESELECT;

#ifdef USE_PENIRQ_PULLUP
  PDDATA |= PEN_MASK_2;       /* Pull up I/0 2 of PENIRQ   */
#endif
  PENIRQ_DIR  &= ~PEN_MASK;   /* I/O 1 of PENIRQ as input  */
  PENIRQ_DATA |= PEN_MASK;    /* Pull down I/O 2 of PENIRQ */
}

/*
 * Ask for an X conversion.
 */
static void ask_x_conv(void) {
  SET_SPIMDATA(SPIMDATA_ASKX);
  SPIMCONT |= SPIMCONT_XCH;     /* initiate exchange */
}

/*
 * Ask for an Y conversion.
 */
static void ask_y_conv(void) {
  SET_SPIMDATA(SPIMDATA_ASKY);
  SPIMCONT |= SPIMCONT_XCH;     /* initiate exchange */
}

/*
 * Get the X conversion. Re-enable the PENIRQ.
 */
static void read_x_conv(void) {
  current_pos.x = (SPIMDATA & 0x7FF8) >> 3;
  /*
   * The first clock is used to fall the BUSY line and the following 15 clks
   * to transfert the 12 bits of the conversion, MSB first.
   * The result will then be in the SPIM register in the bits 14 to 3.
   */
  SET_SPIMDATA(SPIMDATA_NOP);   /* nop with a start bit to re-enable */
  SPIMCONT |= SPIMCONT_XCH;     /* the pen irq.                      */

  rescale_xpos(&current_pos.x);
}

/*
 * Get the Y result. Clock the Burr-Brown to fall the BUSY.
 */
static void read_y_conv(void) {
  current_pos.y = (SPIMDATA & 0x7FF8) >> 3;
  /* Same remark as upper. */
  SET_SPIMDATA(SPIMDATA_NULL);  /* No start bit to fall the BUSY without */
  SPIMCONT |= SPIMCONT_XCH;     /* initiating another BUSY...            */

  rescale_ypos(&current_pos.y);
}

/*
 * Get one char from the queue buffer.
 * AND the head with 'TS_BUF_SIZE -1' to have the loopback
 */
static unsigned char get_char_from_queue(void) {
  unsigned int result;

  result = queue->buf[queue->tail];
  queue->tail = (queue->tail + 1) & (TS_BUF_SIZE - 1);
  return result;
}

/*
 * Write one event in the queue buffer.
 * Test if there is place for an event = the head cannot be just one event
 * length after the queue.
 */
static void put_in_queue(char *in, int len) {
  unsigned long head    = queue->head;
  unsigned long maxhead = (queue->tail - len) & (TS_BUF_SIZE - 1);
  int i;
  
  if(head != maxhead)              /* There is place for the event */
    for(i=0;i<len;i++) {
      queue->buf[head] = in[i];
      head++;
      head &= (TS_BUF_SIZE - 1);
    }
  //else printk("%0: Queue is full !!!!\n", __file__);
  queue->head = head;
}

/*
 * Test if queue is empty.
 */
static inline int queue_empty(void) {
  return queue->head == queue->tail;
}

/*
 * Test if the delta move of the pen is enough big to say that this is a really
 * move and not due to noise.
 */
static int is_moving(void) {
  int threshold;
  int dx, dy;
  
  threshold=((ts_pen_prev.event & EV_PEN_MOVE) > 0 ?
             current_params.follow_thrs : current_params.mv_thrs);
  dx = current_pos.x-ts_pen_prev.x;
  dy = current_pos.y-ts_pen_prev.y;
  if(dx < 0) dx = -dx; /* abs() */
  if(dy < 0) dy = -dy;
  return (dx > threshold ? 1 :
	  (dy > threshold ? 1 : 0));
}

static void copy_info(void) {
  ts_pen_prev.x = ts_pen.x;
  ts_pen_prev.y = ts_pen.y;
  ts_pen_prev.dx = ts_pen.dx;
  ts_pen_prev.dy = ts_pen.dy;
  ts_pen_prev.event = ts_pen.event;
  ts_pen_prev.state = ts_pen.state;
  ts_pen_prev.ev_no = ts_pen.ev_no;
  ts_pen_prev.ev_time = ts_pen.ev_time;
}

unsigned long set_ev_time(void) {
  struct timeval now;

  do_gettimeofday(&now);
  return (now.tv_sec -first_tick.tv_sec )*1000 +
         (now.tv_usec-first_tick.tv_usec)/1000;
}

static void cause_event(int conv) {

  switch(conv) {

  case CONV_ERROR: /* error occure during conversion */
    ts_pen.state &= 0;            /* clear */
    ts_pen.state |= ST_PEN_UP;    /* recover a stable state for the drv.      */
    ts_pen.state |= ST_PEN_ERROR; /* tell that the state is due to an error   */
    ts_pen.event = EV_PEN_UP;     /* event = pen go to up */
    ts_pen.x = ts_pen_prev.x;     /* get previous coord as current to by-pass */
    ts_pen.y = ts_pen_prev.y;     /* possible errors                          */
    ts_pen.dx = 0;
    ts_pen.dy = 0;
    ts_pen.ev_no = ev_counter++;    /* get no of event   */
    ts_pen.ev_time = set_ev_time(); /* get time of event */
    copy_info();                    /* remember info */
    if(current_params.event_queue_on)
      put_in_queue((char *)&ts_pen,DATA_LENGTH);  /* queue event */

    /* tell user space progs that a new state occure */
    new_pen_state = 1;
    /* signal asynchronous readers that data arrives */
    if(queue->fasync)
      kill_fasync(&queue->fasync, SIGIO, POLL_IN);
    wake_up_interruptible(&queue->proc_list);  /* tell user space progs */
    break;
    
  case CONV_LOOP:  /* pen is down, conversion continue */    
    ts_pen.state &= 0;          /* clear */
    ts_pen.state &= ST_PEN_DOWN;

    switch(state_counter) {       /* It could be a move   */
	    
    case 1:    /* the pen just went down, it's a new state */
      ts_pen.x = current_pos.x;
      ts_pen.y = current_pos.y;
      ts_pen.dx = 0;
      ts_pen.dy = 0;
      ts_pen.event = EV_PEN_DOWN;      /* event = pen go to down */
      ts_pen.ev_no = ev_counter++;     /* get no of event        */
      ts_pen.ev_time = set_ev_time();  /* get time of event */
      copy_info();                     /* remember info */
      if(current_params.event_queue_on)
        put_in_queue((char *)&ts_pen,DATA_LENGTH);  /* queue event */

      /* tell user space progs that a new state occure */
      new_pen_state = 1;
      /* signal asynchronous readers that data arrives */
      if(queue->fasync)
        kill_fasync(&queue->fasync, SIGIO, POLL_IN);
      wake_up_interruptible(&queue->proc_list);  /* tell user space progs */
      break;

    case 2:    /* the pen is down for at least 2 loop of the state machine */

      if(is_moving()) {
        ts_pen.event = EV_PEN_MOVE;
        ts_pen.x = current_pos.x;
        ts_pen.y = current_pos.y;
        ts_pen.dx = ts_pen.x - ts_pen_prev.x;
        ts_pen.dy = ts_pen.y - ts_pen_prev.y;
        ts_pen.ev_no = ev_counter++;    /* get no of event   */
        ts_pen.ev_time = set_ev_time(); /* get time of event */
        copy_info();                    /* remember info */
        if(current_params.event_queue_on)
          put_in_queue((char *)&ts_pen,DATA_LENGTH);  /* queue event */

        /*
	 * pen is moving, then it's anyway a good reason to tell it to
	 * user space progs
	 */
        new_pen_state = 1;
        /* signal asynchronous readers that data arrives */
        if(queue->fasync)
          kill_fasync(&queue->fasync, SIGIO, POLL_IN);
        wake_up_interruptible(&queue->proc_list);
      }
      else {
        if(ts_pen_prev.event == EV_PEN_MOVE)  /* if previous state was moving */
	  ts_pen.event = EV_PEN_MOVE;         /* -> keep it!                  */
        ts_pen.x = ts_pen_prev.x;       /* No coord passing to     */
        ts_pen.y = ts_pen_prev.y;       /* avoid Parkinson effects */
        ts_pen.dx = 0;
	ts_pen.dy = 0;  /* no wake-up interruptible because nothing new */
        copy_info();    /* remember info */
      }
      break;
    }
    break;

  case CONV_END:   /* pen is up, conversion ends */
    ts_pen.state &= 0;         /* clear */
    ts_pen.state |= ST_PEN_UP;
    ts_pen.event = EV_PEN_UP;
    ts_pen.x = current_pos.x;
    ts_pen.y = current_pos.y;
    ts_pen.dx = ts_pen.x - ts_pen_prev.x;
    ts_pen.dy = ts_pen.y - ts_pen_prev.y;
    ts_pen.ev_time = set_ev_time();  /* get time of event */
    ts_pen.ev_no = ev_counter++;     /* get no of event   */
    copy_info();                     /* remember info */
    if(current_params.event_queue_on)
      put_in_queue((char *)&ts_pen,DATA_LENGTH);  /* queue event */

    /* tell user space progs that a new state occure */
    new_pen_state = 1;
    /* signal asynchronous readers that data arrives */
    if(queue->fasync)
      kill_fasync(&queue->fasync, SIGIO, POLL_IN);
    wake_up_interruptible(&queue->proc_list);
    break;
  }
}

/*----------------------------------------------------------------------------*/
/* Interrupt functions -------------------------------------------------------*/

/*
 * pen irq.
 */
static void handle_pen_irq(int irq, void *dev_id, struct pt_regs *regs) {
  unsigned long flags;
  int bypass_initial_timer = 0;

  /* if unwanted interrupts come, trash them */ 
  if(!device_open)
    return;

  PENIRQ_DATA &= ~PEN_MASK;

#ifdef CONFIG_XCOPILOT_BUGS
  if(IMR&IMR_MPEN) {
    return;
  }
#endif
  save_flags(flags);     /* disable interrupts */
  cli();

  switch(ts_drv_state) {

  case TS_DRV_IDLE:  
    DISABLE_PEN_IRQ;
    ts_drv_state++;      /* update driver state */
    if(current_params.deglitch_on)
      set_timer_irq(&ts_wake_time,sample_ticks);
    else
      bypass_initial_timer = 1;
    break;

  default: /* Error */
    /* PENIRQ is enable then just init the driver 
     * Its not necessary to pull down the busy signal
     */
    init_ts_state();
    break;
  }    
  restore_flags(flags);  /* Enable interrupts */

  /* if deglitching is off, we haven't started the deglitching timeout
   * so we do the inital sampling immediately:
   */
  if(bypass_initial_timer)
    handle_timeout();
}

/*
 * timer irq.
 */
static void handle_timeout(void) {
  unsigned long flags;

  /* if unwanted interrupts come, trash them */ 
  if(!device_open)
    return;
  
  save_flags(flags);     /* disable interrupts */
  cli();
  
  switch(ts_drv_state) {

  case TS_DRV_IDLE:  /* Error in the idle state of the driver */
    printk("%s: Error in the idle state of the driver\n", __file__);
    goto treat_error;
  case TS_DRV_ASKX:  /* Error in release SPIM   */
    printk("%s: Error in release SPIM\n", __file__);
    goto treat_error;
  case TS_DRV_ASKY:  /* Error in ask Y coord    */
    printk("%s: Error in ask Y coord\n", __file__);
    goto treat_error;
  case TS_DRV_READX: /* Error in read X coord   */
    printk("%s: Error in read X coord\n", __file__);
    goto treat_error;
  case TS_DRV_READY: /* Error in read Y coord   */
    printk("%s: Error in read Y coord\n", __file__);
    goto treat_error;
  treat_error:
    /* Force re-enable of PENIRQ because of an abnormal termination. Limit this
     * initialization with a timer to define a more FATAL error ...
     */
    set_timer_irq(&ts_wake_time,TICKS(CONV_TIME_LIMIT));
    ts_drv_state = TS_DRV_ERROR;
    CLEAR_SPIM_IRQ;              /* Consume residual irq from spim */
    ENABLE_SPIM_IRQ;
    fall_BUSY_enable_PENIRQ();
    break;
    
  case TS_DRV_WAIT:
    if(state_counter) {
      cause_event(CONV_LOOP);  /* only after one loop */
    }
    if(IS_PEN_DOWN) {
      if(state_counter < 2) state_counter++;
      set_timer_irq(&ts_wake_time,TICKS(CONV_TIME_LIMIT));
      set_SPIM_transfert();
      ts_drv_state++;
      CLEAR_SPIM_IRQ;            /* Consume residual irq from spim */
      ENABLE_SPIM_IRQ;
      fall_BUSY_enable_PENIRQ();
    }
    else {
      if(state_counter) {
#ifdef CONFIG_XCOPILOT_BUGS
	/* on xcopilot, we read the last mouse position now, because we
	 * missed the moves, during which the position should have been
	 * read
	 */
	xcopilot_read_now();
#endif
	cause_event(CONV_END);
      }
      init_ts_state();
    }
    break;
    
  case TS_DRV_ERROR:  /* Busy doesn't want to fall down -> reboot? */
    panic(__FILE__": cannot fall pull BUSY signal\n");

  default:
    init_ts_state();
  }
  restore_flags(flags);  /* Enable interrupts */

#ifdef CONFIG_XCOPILOT_BUGS
  if (ts_drv_state==TS_DRV_ASKX) {
    handle_spi_irq();
  }
  PENIRQ_DATA |= PEN_MASK;
#endif
}

/*
 * spi irq.
 */
static void handle_spi_irq(void) {
  unsigned long flags;

  /* if unwanted interrupts come, trash them */ 
  if(!device_open)
    return;
  
  save_flags(flags);     /* disable interrupts */
  cli();
  
  CLEAR_SPIM_IRQ;       /* clear source of interrupt */

  switch(ts_drv_state) {


  case TS_DRV_ASKX:
#ifdef CONFIG_XCOPILOT_BUGS
    /* for xcopilot we bypass all SPI interrupt-related stuff
     * and read the pos immediately
     */
    xcopilot_read_now();
    ts_drv_state = TS_DRV_WAIT;
    DISABLE_SPIM_IRQ;
    release_SPIM_transfert();
    set_timer_irq(&ts_wake_time,sample_ticks);
    break;
#endif
    if(IS_BUSY_ENDED) {     /* If first BUSY down, then */
      ask_x_conv();         /* go on with conversion    */
      ts_drv_state++;
    }
    else  fall_BUSY_enable_PENIRQ();  /* else, re-loop  */
    break;

  case TS_DRV_ASKY:
    ask_y_conv();
    ts_drv_state++;
    break;

  case TS_DRV_READX:
    read_x_conv();
    ts_drv_state++;
    break;

  case TS_DRV_READY:
    read_y_conv();
    swap_xy(&current_pos.x, &current_pos.y);
    ts_drv_state = TS_DRV_WAIT;
    break;

  case TS_DRV_WAIT:
    DISABLE_SPIM_IRQ;
    release_SPIM_transfert();
    set_timer_irq(&ts_wake_time,sample_ticks);
    break;

  case TS_DRV_ERROR:
    if(IS_BUSY_ENDED) {              /* If BUSY down... */
      release_SPIM_transfert();
      cause_event(CONV_ERROR);
      init_ts_state();
    }
    else fall_BUSY_enable_PENIRQ();  /* else, re-loop */
    break;

  default:
    init_ts_state();
  }
  restore_flags(flags);  /* Enable interrupts */

#ifdef CONFIG_XCOPILOT_BUGS
  PENIRQ_DATA |= PEN_MASK;
#endif
}

/*----------------------------------------------------------------------------*/
/* file_operations functions -------------------------------------------------*/

/*
 * Called whenever a process attempts to read the device it has already open.
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0))
static ssize_t ts_read (struct file *file,
                        char *buff,       /* the buffer to fill with data */
			size_t len,       /* the length of the buffer.    */
			loff_t *offset) { /* Our offset in the file       */
#else
static int ts_read(struct inode *inode, struct file *file,
		   char *buff,            /* the buffer to fill with data */
		   int len) {             /* the length of the buffer.    */
		                          /* NO WRITE beyond that !!!     */
#endif

  char *p_buffer;
  char c;
  int  i;
  int  err;

  while(!new_pen_state) {    /* noting to read */
    if(file->f_flags & O_NONBLOCK)
      return -EAGAIN;
    interruptible_sleep_on(&queue->proc_list);

#if (LINUX_VERSION_CODE >= 0x020100)
    if(signal_pending(current))
#else
    if(current->signal & ~current->blocked)  /* a signal arrived */
#endif
      return -ERESTARTSYS;    /* tell the fs layer to handle it  */
    /* otherwise loop */
  }

  /* Force length to the one available */
  len = DATA_LENGTH;

  /* verify that the user space address to write is valid */
  err = verify_area(VERIFY_WRITE,buff,len);
  if(err) return err;

  /*
   * The events are anyway put in the queue.
   * But the read could choose to get the last event or the next in queue.
   * This decision is controlled through the ioctl function.
   * When the read doesn't flush the queue, this last is always full.
   */
  if(current_params.event_queue_on) {
    i = len;
    while((i>0) && !queue_empty()) {
      c = get_char_from_queue();
      put_user(c,buff++);
      i--;
    }
  }
  else {
    p_buffer = (char *)&ts_pen;
    for(i=len-1;i>=0;i--)   put_user(p_buffer[i],buff+i);
/*    for(i=0;i<len;i++)   put_user(p_buffer[i],buff+i);*/
  }

  if(queue_empty() || !current_params.event_queue_on)
    new_pen_state = 0;  /* queue events fully consumned */
  else
    new_pen_state = 1;  /* queue not empty              */

  if(len-i) {
      file->f_dentry->d_inode->i_atime = CURRENT_TIME; /* nfi */
      return len-i;
  }

  return 0;

}

/*
 * select file operation seems to become poll since version 2.2 of the kernel
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0))
static unsigned int ts_poll(struct file *file, poll_table *table) {

  poll_wait(file,&queue->proc_list,table);
  if (new_pen_state)
    return POLLIN | POLLRDNORM;
  return 0;

}
#else
static int ts_select(struct inode *inode, struct file *file,
                     int mode, select_table *table) {
  if(mode != SEL_IN)
    return 0;                   /* the device is readable only */
  if(new_pen_state)
    return 1;                   /* there is some new stuff to read */
  select_wait(&queue->proc_list,table);    /* wait for data */
  return 0;
}
#endif

/*
 * Called whenever a process attempts to do an ioctl to the device it has
 * already open.
 */
static int ts_ioctl(struct inode *inode, struct file *file,
		    unsigned int cmd,     /* the command to the ioctl */
		    unsigned long arg) {  /* the parameter to it      */

  struct ts_drv_params  new_params;
  int  err;
  int  i;
  char *p_in;
  char *p_out;

  switch(cmd) {
  
  case TS_PARAMS_GET:  /* Get internal params. First check if user space area
                        * to write is valid.
			*/
    err = verify_area(VERIFY_WRITE,(char *)arg,sizeof(current_params));
    if(err) return err;
    p_in  = (char *)&current_params;
    p_out = (char *)arg;
    for(i=0;i<sizeof(current_params);i++)
      put_user(p_in[i],p_out+i);
    return 0;

  case TS_PARAMS_SET:  /* Set internal params. First check if user space area
                        * to read is valid.
			*/
    err = verify_area(VERIFY_READ,(char *)arg,sizeof(new_params));
    if(err) {
      return err;
    }

    /* ok */
    p_in  = (char *)&new_params;
    p_out = (char *)arg;
    for(i=0;i<sizeof(new_params);i++) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0))
      get_user(p_in[i],p_out+i);
#else
      p_in[i] = get_user(p_out+i);
#endif
    }
    
    /* All the params are set with minimal test. */
    memcpy((void*)&current_params, (void*)&new_params, sizeof(current_params));
    sample_ticks = TICKS(current_params.sample_ms);

    /* check version */
    if(new_params.version_req!=-1) {
      if(new_params.version_req!=MC68328DIGI_VERSION) {
	printk("%s: error: driver version is %d, requested version is %d\n",
	       __file__, MC68328DIGI_VERSION, new_params.version_req);
	return -EBADRQC;
      }
    }

    return 0;

  default:  /* cmd is not an ioctl command */
    return -ENOIOCTLCMD;
  }
}
		    
/*
 * Called whenever a process attempts to open the device file.
 */
static int ts_open(struct inode *inode, struct file *file) {
  int err;
  /* To talk to only one device at a time */
  if(device_open) return -EBUSY;

  /* IRQ registration have to be done before the hardware is instructed to
   * generate interruptions.
   */
  /* IRQ for pen */
  err = request_irq(PEN_IRQ_NUM, handle_pen_irq,
		    IRQ_FLG_STD,"touch_screen",NULL);
  if(err) {
    printk("%s: Error. Cannot attach IRQ %d to touch screen device\n",
	   __file__, PEN_IRQ_NUM);
    return err;
  }
  printk("%s: IRQ %d attached to touch screen\n",
	 __file__, PEN_IRQ_NUM);

  /* IRQ for SPIM */
  err = request_irq(SPI_IRQ_NUM, handle_spi_irq,
		    IRQ_FLG_STD,"spi_irq",NULL);
  if(err) {
    printk("%s: Error. Cannot attach IRQ %d to SPIM device\n",
           __file__, SPI_IRQ_NUM);
    return err;
  }
  printk("%s: IRQ %d attached to SPIM\n",
	 __file__, SPI_IRQ_NUM);

  /* Init hardware */
  init_ts_drv();
  
  printk("digi: init hardware done..\n");

  /* Init the queue */
  queue = (struct ts_queue *) kmalloc(sizeof(*queue),GFP_KERNEL);
  memset(queue,0,sizeof(*queue));
  queue->head = queue->tail = 0;
  init_waitqueue_head(&queue->proc_list);

  /* Increment the usage count (number of opened references to the module)
   * to be sure the module couldn't be removed while the file is open.
   */
  MOD_INC_USE_COUNT;

  /* And my own counter too */
  device_open++;

  return 0;  /* open succeed */
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0))
static int ts_fasync(int inode, struct file *file, int mode) {
#else
static int ts_fasync(struct inode *inode, struct file *file, int mode) {
#endif

  int retval;
  
  retval = fasync_helper(inode,file,mode,&queue->fasync);
  if(retval < 0)
    return retval;
  return 0;
}

/*
 * Called whenever a process attempts to close the device file.
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0))
static int ts_release(struct inode *inode, struct file *file) {
#else
static void ts_release(struct inode *inode, struct file *file) {
#endif

  /* Remove this file from the asynchronously notified file's */
  ts_fasync(inode,file,0);

  /* Release hardware */
  release_ts_drv();

  /* Free the allocated memory for the queue */
  kfree(queue);

  /* IRQ have to be freed after the hardware is instructed not to interrupt
   * the processor any more.
   */
  free_irq(SPI_IRQ_NUM,NULL);
  free_irq(PEN_IRQ_NUM,NULL);

  /* Decrement the usage count, otherwise once the file is open we'll never
   * get rid of the module.
   */
  MOD_DEC_USE_COUNT;

  /* And my own counter too */
  device_open--;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0))
  return 0;
#endif
}


static struct file_operations ts_fops = {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
  owner:          THIS_MODULE,
  read:           ts_read,
  poll:           ts_poll,
  ioctl:          ts_ioctl,
  open:           ts_open,
  release:        ts_release,
  fasync:         ts_fasync,
#else

  NULL,       /* lseek              */
  ts_read,    /* read               */
  NULL,       /* write              */
  NULL,       /* readdir            */
#  if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0))
  ts_poll,    /* poll               */
#  else
  ts_select,  /* select             */
#  endif
  ts_ioctl,   /* ioctl              */
  NULL,       /* mmap               */
  ts_open,    /* open               */
#  if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0))
  NULL,       /* flush              */
#  endif
  ts_release, /* release (close)    */
  NULL,       /* fsync              */
  ts_fasync   /* async notification */

#endif
};

/*
 * miscdevice structure for misc driver registration.
 */
static struct miscdevice mc68328_digi = {
  MC68328DIGI_MINOR,"mc68328 digitizer",&ts_fops
};

/*
 * Initialize the driver.
 */
static int __init mc68328digi_init(void) {
  int err;

  printk("%s: MC68328DIGI touch screen driver\n", __file__);

  /* Register the misc device driver */
  err = misc_register(&mc68328_digi);
  if(err<0)
    printk("%s: Error registering the device\n", __file__);
  else
    printk("%s: Device register with name: %s and number: %d %d\n",
	   __file__, mc68328_digi.name, MC68328DIGI_MAJOR, mc68328_digi.minor);

  /* Init prameters settings at boot time */
  init_ts_settings();

  return err;     /* A non zero value means that init_module failed */
}

/*
 * Cleanup - undid whatever mc68328digi_init did.
 */
void mc68328digi_cleanup(void) {
  /* Unregister device driver */
  misc_deregister(&mc68328_digi);
}  

module_init(mc68328digi_init);
module_exit(mc68328digi_cleanup);
