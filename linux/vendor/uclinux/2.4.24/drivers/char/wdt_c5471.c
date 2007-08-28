/**************************************************************************
 * linux/drivers/char/wdt_c5471.c
 *
 * Industrial Computer Source WDT500/501 driver for Linux 2.4.x
 *
 * (c) Copyright (C) 2003 Cadenux, LLC. All rights reserved.
 *     todd.fischer@cadenux.com  <www.cadenux.com>
 * (c) Copyright 2001 RidgeRun, Inc <www.ridgerun.com>
 * (c) Copyright 1996-1997 Alan Cox <alan@redhat.com>, All Rights Reserved.
 *				http://www.redhat.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * THIS  SOFTWARE  IS  PROVIDED  ``AS  IS''  AND   ANY  EXPRESS  OR IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
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
 * Neither Alan Cox nor CymruNet Ltd. admit liability nor provide 
 * warranty for any of this software. This material is provided 
 * "AS-IS" and at no charge.	
 *
 * (c) Copyright 1995    Alan Cox <alan@lxorguk.ukuu.org.uk>
 *
 * Release 0.08.
 *
 * Fixes
 *	Dave Gregorich	:	Modularisation and minor bugs
 *	Alan Cox	:	Added the watchdog ioctl() stuff
 *	Alan Cox	:	Fixed the reboot problem (as noted by
 *				Matt Crocker).
 *	Alan Cox	:	Added wdt= boot option
 *	Alan Cox	:	Cleaned up copy/user stuff
 *	Tim Hockin	:	Added insmod parameters, comment cleanup
 *				Parameterized timeout
 *	Tigran Aivazian	:	Restructured wdt_init() to handle failures
 *      Gordon McNutt   :       Leveraged for TI watchdog timers
 *      Gregory Nutt    :	Leveraged to 2.4.x Linux; Made watchdog
 *				independent of timer module.
 **************************************************************************/

/**************************************************************************
 * Conditional Compilation
 **************************************************************************/

/* #define WDT_DEBUG 1 */

/**************************************************************************
 * Included Files
 **************************************************************************/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>

#include <asm/arch/sysdep.h>
#include <asm/arch/hardware.h>
#include <asm/arch/irqs.h>
#include <asm/arch/wdt_ioctl.h>

/* Use versioning if needed */

#if defined(CONFIG_MODVERSIONS)
# undef MODVERSIONS
# define MODVERSIONS
#endif

#if defined(MODVERSIONS)
# include <linux/modversions.h>
#endif

/**************************************************************************
 * Definitions
 **************************************************************************/

/* Common debug definitions */

#if defined(WDT_DEBUG)

 /* define MY_KERN_DEBUG KERN_DEBUG */
# define MY_KERN_DEBUG

# define dbg(format, arg...) \
    printk(MY_KERN_DEBUG __FUNCTION__ ": " format "\n" , ## arg)

# if defined(VERBOSE)
#  define verbose(format, arg...) \
    printk(MY_KERN_DEBUG __FUNCTION__ ": " format "\n" , ## arg)
# else /* VERBOSE */
#  define verbose(format, arg...) do {} while (0)
# endif /* VERBOSE */

#else /* WDT_DEBUG */

# undef  VERBOSE
# define dbg(format, arg...) do {} while (0)
# define verbose(format, arg...) do {} while (0)

#endif /* WDT_DEBUG */

#define err(format, arg...) \
  printk(KERN_ERR __FUNCTION__ ": " format "\n" , ## arg)
#define info(format, arg...) \
  printk(KERN_INFO __FUNCTION__ ": " format "\n" , ## arg)
#define warn(format, arg...) \
  printk(KERN_WARNING __FUNCTION__ ": " format "\n" , ## arg)
#define critical(format, arg...) \
  printk(KERN_CRIT __FUNCTION__ ": " format "\n" , ## arg)

/* Major device number (see linux/Documentation/devices.txt) */

#ifdef LINUX_20
  /* register_chrdev() will not except character device major
   * numbers greater than 127 in Linux 2.0.  Use the first number
   * in the range 60-64 of Local/Experimental numbers.
   */

# define WATCHDOG_MAJOR 60
#else
  /* Use the first number in the range 240-254 of Local/Experimental
   * numbers.
   */

# define WATCHDOG_MAJOR 240
#endif /* LINUX_20 */

#define MAX_WDT_USEC            353200
#define RESET_MODE              0
#define INTERRUPT_MODE          1

#define WDOG_IRQ               0
#define MAX_PRESCALER          256
#define C5471_TIMER_STOP       0

#define C5471_TIMER_PRESCALER  0x07          /* Bits 0-2: Prescale value */
#define C5471_TIMER_STARTBIT   (1 << 3)      /* Bit 3: Start timer bit */
#define C5471_TIMER_AUTORELOAD (1 << 4)      /* Bit 4: Auto-reload timer */
#define C5471_TIMER_LOADTIM    (0xffff << 5) /* Bits 20-5: Load timer value */
#define C5471_TIMER_MODE       (1 << 21)     /* Bit 21: Timer mode */
#define C5471_DISABLE_VALUE1   (0xf5 << 22)  /* Bits 29-22: WD disable */
#define C5471_DISABLE_VALUE2   (0xa0 << 22)

#define CLOCK_KHZ              47500
#define CLOCK_MHZx2            95

#define RESET_MODE             0
#define INTERRUPT_MODE         1

/**************************************************************************
 * Private Types
 **************************************************************************/

/* This structure represents each register in a block of timer
 * registers
 */

struct c5471_timer_s
{
  volatile unsigned long cntl_timer;  /* Timer control register */
  volatile unsigned long read_tim;    /* Current value register */
};
typedef struct c5471_timer_s c5471_timer_t;

/* We will use timer 0 for watchdog support */

#define c5471_timer ((c5471_timer_t*)C5471_TIMER0_CTRL)

/**************************************************************************
 * Global Function Prototypes
 **************************************************************************/

/**************************************************************************
 * Private Function Prototypes
 **************************************************************************/

/* Local implementation of timer interface */

static void
wdt_unregister_cb(void (*handler)(int, void*, struct pt_regs*));

static int
wdt_register_cb(void (*handler)(int, void*, struct pt_regs*));

static inline int
prescale_to_ptv(int in);

static int
wdt_set_watchdog(int mode);

static int
wdt_set_usec(unsigned long usec_value);

static void
wdt_timer_init(void);

static void
wdt_timer_exit(void);

#if !defined(MODULE)
static int
wdt_setup(char *str) __init;
#endif

static void
wdt_interrupt(int timer, void *dev_id, struct pt_regs *regs);

static void
wdt_ping(void);

#if defined(WDT_DEBUG)
static ssize_t
wdt_read(struct file *filep, char *buf, size_t count, loff_t *f_pos);
#endif /* WDT_DEBUG */

static ssize_t
wdt_write(struct file *file, const char *buf, size_t count, loff_t *ppos);

static int
wdt_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	  unsigned long arg);

static int
wdt_open(struct inode *inode, struct file *file);

static int
wdt_release(struct inode *inode, struct file *file);

static int
wdt_notify_sys(struct notifier_block *this, unsigned long code, void *unused);

/**************************************************************************
 * Private Data
 **************************************************************************/

static int wdt_is_open;

static const char myname[] = "watchdog";

/* Kernel Interfaces */

static struct file_operations wdt_fops =
{
  owner:    THIS_MODULE,
  llseek:   no_llseek,
#if defined(WDT_DEBUG)
  read:     wdt_read,
#endif /* WDT_DEBUG */
  write:    wdt_write,
  ioctl:    wdt_ioctl,
  open:     wdt_open,
  release:  wdt_release,
};

/* The WDT card needs to learn about soft shutdowns in order to
 * turn the timebomb registers off.
 */

static struct notifier_block wdt_notifier =
{
  wdt_notify_sys,
  NULL,
  0
};

/* ioctl info */

static struct watchdog_info ident =
{
  options: 0,
  firmware_version: 0,
  identity: "C5471 WDT"
};

/**************************************************************************
 * Inline Functions
 **************************************************************************/

/**************************************************************************
 * Local Timer Logic
 **************************************************************************/

static void
wdt_unregister_cb(void (*handler)(int, void*, struct pt_regs*))
{
  /* Stop the timer */

  c5471_timer->cntl_timer = C5471_TIMER_STOP;

  /* And release the IRQ */

  free_irq(WDOG_IRQ, handler);
}

static int
wdt_register_cb(void (*handler)(int, void*, struct pt_regs*))
{
  dbg("Attaching to Watcdog Timer IRQ(%d)", WDOG_IRQ);

  /* Make sure that the timer is stopped (We had better be the only user!
   * We had better be successful when we request the IRQ!) */

  c5471_timer->cntl_timer = C5471_TIMER_STOP;

  /* Get the interrupt. */

  if (request_irq(WDOG_IRQ, handler, SA_INTERRUPT, myname, handler)) 
    {
      err("Unable to get IRQ for the Watchdog driver\n");
      return -EPERM;
    }

  return 0;
}

static inline int
prescale_to_ptv(int in)
{
  if (in <= 2)
    return 0;
  if (in <= 4)
    return 1;
  if (in <= 8)
    return 2;
  if (in <= 16)
    return 3;
  if (in <= 32)
    return 4;
  if (in <= 64)
    return 5;
  if (in <= 128)
    return 6;
  /* > 128 to <= 256 */
  return 7;
}

static int
wdt_set_watchdog(int mode)
{
  dbg("Setting watchdog mode");

  if (mode == RESET_MODE)
    {
      c5471_timer->cntl_timer = C5471_DISABLE_VALUE1;
      c5471_timer->cntl_timer = C5471_DISABLE_VALUE2;
    } 
  else if (mode == INTERRUPT_MODE)
    {
      c5471_timer->cntl_timer = C5471_TIMER_MODE;
    } 
  else
    return -1;
         
  return 0;
}

static int
wdt_set_usec(unsigned long usec_value)
{
  /* prescaler: clock / prescaler = #clock ticks per counter in ptv */
  /* divisor:   #counts until the interrupt comes. */

  unsigned long prescaler  = 2;     
  unsigned long divisor    = 1;
  unsigned long timer_mode;

  prescaler = MAX_PRESCALER;

  dbg("Watchdog timer usec_value=%ld", usec_value);
     
  /* Calculate a value of prescaler and divisor that will be able
   * to count to the usec_value.  It may not be exact or the best
   * possible set, but it's a quick and simple algorithm.
   *
   *    divisor max = 0x10000
   *    prescaler max = MAX_PRESCALER
   */

  do
    {
      divisor = (CLOCK_MHZx2 * usec_value) / (prescaler * 2);
      dbg("divisor=0x%lx prescaler=0x%lx", divisor, prescaler);
      if (divisor >= 0x10000)
	{
	  if (prescaler == MAX_PRESCALER)
	    { 
	      /* this is the max possible ~2.5 seconds. */

	      dbg("prescaler=0x%lx too big!", prescaler);
	      return -EINVAL;
	    }
	  prescaler *= 2;
	  if (prescaler > MAX_PRESCALER)
	    {
	      prescaler = MAX_PRESCALER;
	    }
	}
    }
  while (divisor >= 0x10000);

  dbg("prescaler=0x%lx divisor=0x%lx", prescaler, divisor);

  timer_mode = prescale_to_ptv(prescaler);
  dbg("timer_mode=0x%lx", timer_mode);

  timer_mode &= ~C5471_TIMER_AUTORELOAD; /* One shot mode. */
  dbg("timer_mode=0x%lx", timer_mode);

  timer_mode |= divisor << 5;
  dbg("timer_mode=0x%lx", timer_mode);

  /* Program the mode, but don't set the start bit yet. */

  c5471_timer->cntl_timer = timer_mode;
 
  /* Now hit the start bit. */

  c5471_timer->cntl_timer |= C5471_TIMER_STARTBIT;
  dbg("cntl_timer=0x%lx", c5471_timer->cntl_timer);

  return 0;
}

static void
wdt_timer_init(void)
{
  /* Disable watdog timer. */

  dbg("&cntl_timer=0x%p", &c5471_timer->cntl_timer);

  c5471_timer->cntl_timer = C5471_TIMER_STOP;
  request_region((unsigned int)&c5471_timer->cntl_timer,
		 sizeof(c5471_timer_t),
		 myname);
}

static void
wdt_timer_exit(void)
{
  release_region((unsigned int)&c5471_timer->cntl_timer,
		 sizeof(c5471_timer_t));
}

/**************************************************************************
 * Private Functions
 **************************************************************************/

#if !defined(MODULE)

/**************************************************************************
 * wdt_setup:
 * @str: command line string
 *
 * Setup options. N/A.
 **************************************************************************/
 
static int __init
wdt_setup(char *str)
{
  return 1;
}

__setup("wdt=", wdt_setup);

#endif /* !MODULE */

/**************************************************************************
 * wdt_interrupt:
 * @timer:		Timer number
 * @dev_id:	Unused as we don't allow multiple devices.
 *
 * Handle an interrupt from the timer. This only happens when the timer
 *      is configured in 'interrupt' instead of 'reset' mode (which is never
 *      by this driver). I include this here as an example of how we could
 *      do something else besides h/w reset (Dan mentioned some realtime
 *      applications that might want to shutdown equipment or something 
 *      instead of just blindly resetting the MPU).
 **************************************************************************/
 
static void
wdt_interrupt(int timer, void *dev_id, struct pt_regs *regs)
{
  critical("WDT expired!");
	
#if defined(SOFTWARE_REBOOT)
#if defined(ONLY_TESTING)
  critical("Would Reboot");
#else		
  critical("Initiating system reboot");
  machine_restart(NULL);
#endif		
#else
  critical("But since the WDT is configued in interrupt "\
	   "mode -- without software reboot -- nothing will happen");
#endif		
}

/**************************************************************************
 * wdt_ping:
 *
 * Reset the counter. 
 **************************************************************************/

static void
wdt_ping(void)
{
  dbg("wdt_ping");

  wdt_set_usec(MAX_WDT_USEC); /* Max usec seconds possible */
}

/**************************************************************************
 * wdt_read:
 * @file: file handle to the watchdog board
 * @buf: buffer to write 1 byte into
 * @count: length of buffer
 * @ptr: offset (no seek allowed)
 *
 * Read reports the temperature in degrees Fahrenheit. The API is in
 * farenheit. It was designed by an imperial measurement luddite.
 **************************************************************************/
 
#if defined(WDT_DEBUG)
static ssize_t
wdt_read(struct file *filep, char *buf, size_t count, loff_t *f_pos)
{
  static int not_end = 0;
  char buffer[256];
  int len;

  dbg("filep=0x%p size=%d", filep, count);

  /* Have we returned the end of file yet? */

  if (not_end) 
    {
      /* No... next time we will. */

      not_end = 0;

      /* Return debug information. */

      dbg("cntl_timer(0x%p)=0x%08lx\nread_tim(0x%p)=0x%08lx",
	  &c5471_timer->cntl_timer,  c5471_timer->cntl_timer,
	  &c5471_timer->read_tim, c5471_timer->read_tim);

      sprintf(buffer, "cntl_timer(0x%p)=0x%08lx\nread_tim(0x%p)=0x%08lx\n",
	  &c5471_timer->cntl_timer,  c5471_timer->cntl_timer,
	  &c5471_timer->read_tim, c5471_timer->read_tim);

      len = strlen(buffer);
      if (len > count) len = count;

      copy_to_user(buf, buffer, len);
    }
  else
    {
      /* Return end of file */

      not_end = 1;
      len     = 0;
    }
  return len;
}
#endif /* WDT_DEBUG */

/**************************************************************************
 * wdt_write:
 * @file: file handle to the watchdog
 * @buf: buffer to write (unused as data does not matter here 
 * @count: count of bytes
 * @ppos: pointer to the position to write. No seeks allowed
 *
 * A write to a watchdog device is defined as a keepalive signal. Any
 * write of data will do, as we we don't define content meaning.
 **************************************************************************/
 
static ssize_t
wdt_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
  /*  Can't seek (pwrite) on this device  */

  if (ppos != &file->f_pos)
    return -ESPIPE;

  dbg("Write call");
  if(count)
    {
      wdt_ping();
      return 1;
    }
  return 0;
}

/**************************************************************************
 * wdt_ioctl:
 * @inode: inode of the device
 * @file: file handle to the device
 * @cmd: watchdog command
 * @arg: argument pointer
 *
 * The watchdog API defines a common set of functions for all watchdogs
 * according to their available features. We only actually usefully support
 * querying capabilities and current status. 
 **************************************************************************/
 
static int
wdt_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	  unsigned long arg)
{
  int retval = 0;

  dbg("ioctl Call: inode=%p file=%p cmd=0x%x arg=0x%lx",
      inode, file, cmd, arg);

  /* Range check the IOCTL cmd */

  if (_IOC_TYPE(cmd) != WATCHDOG_IOCTL_BASE)
    {
      err("Invalid ioctl base value");
      return -EINVAL;
    }

  /* Process the the IOCTL command (see include/linux/watchdog.h) */

  switch(cmd)
    {
    case WDIOC_GETSUPPORT:
      {
	dbg("WDIOC_GETSUPPORT");
	retval = copy_to_user((struct watchdog_info*)arg, &ident, 
			      sizeof(ident));
	if (retval)
	  retval = -EFAULT;
      }
      break;
                
    case WDIOC_GETSTATUS:
      {
	dbg("WDIOC_GETSTATUS");
	retval = PUT_USER(0, (int*)arg);
      }
      break;

    case WDIOC_GETBOOTSTATUS:
      {
	dbg("WDIOC_GETBOOTSTATUS");
	retval = PUT_USER(0, (int*)arg);
      }
      break;

    case WDIOC_KEEPALIVE:
      {
	dbg("WDIOC_KEEPALIVE");
	wdt_ping();
      }
      break;

      /* These commands are defined in watchdog.h, but were not
       * supported in the original implemenation of this
       * watchdog driver.
       */
    case WDIOC_GETTEMP:
    case WDIOC_SETOPTIONS:
    case WDIOC_SETTIMEOUT:
      {
	err("Unsupported ioctl command");
	retval = -ENOSYS;
      }
      break;

    default:
      {
	err("Invalid ioctl command");
	retval = -ENOIOCTLCMD;
      }
      break;
    }
  return retval;
}

/**************************************************************************
 * wdt_open:
 * @inode: inode of device
 * @file: file handle to device
 *
 * Our misc device has been opened. It's single open and on open we load
 *      the counter and start the countdown.
 **************************************************************************/
 
static int
wdt_open(struct inode *inode, struct file *file)
{
  int ret;

  dbg("Open Module");

  if(wdt_is_open)
    return -EBUSY;

  /* Activate */

  wdt_is_open=1;

  /* This will automatically load the timer with its max
   * count and start it running.
   */

  if ((ret = wdt_set_watchdog(RESET_MODE)))
    {
      wdt_is_open = 0;
      return ret;
    }
  return 0;
}

/**************************************************************************
 * wdt_release:
 * @inode: inode to board
 * @file: file handle to board
 *
 * The watchdog has a configurable API. There is a religious dispute 
 * between people who want their watchdog to be able to shut down and 
 * those who want to be sure if the watchdog manager dies the machine
 * reboots. In the former case we disable the counters, in the latter
 * case you have to open it again very soon.
 **************************************************************************/
 
static int
wdt_release(struct inode *inode, struct file *file)
{
  dbg("Release Module");

  /* lock_kernel(); */

#if !defined(CONFIG_WATCHDOG_NOWAYOUT)
  wdt_set_watchdog(INTERRUPT_MODE); /* reset off */
#endif		
  wdt_is_open=0;

  /* unlock_kernel(); */

  return 0;
}

/**************************************************************************
 *      wdt_notify_sys:
 *      @this: our notifier block
 *      @code: the event being reported
 *      @unused: unused
 *
 *      Our notifier is called on system shutdowns. We want to turn the card
 *      off at reboot otherwise the machine will reboot again during memory
 *      test or worse yet during the following fsck. This would suck, in fact
 *      trust me - if it happens it does suck.
 **************************************************************************/

static int
wdt_notify_sys(struct notifier_block *this, unsigned long code, void *unused)
{
  if(code==SYS_DOWN || code==SYS_HALT)
    {
      /* Turn the card off */
    }
  return NOTIFY_DONE;
}

/**************************************************************************
 * Public Functions
 **************************************************************************/

/**************************************************************************
 * wdt_exit
 *
 * Unload the watchdog. You cannot do this with any file handles open.
 * If your watchdog is set to continue ticking on close and you unload
 * it, well it keeps ticking. We won't get the interrupt but the board
 * will not touch PC memory so all is fine. You just have to load a new
 * module in 19 seconds or reboot.
 **************************************************************************/
 
void __exit wdt_exit(void)
{
  dbg("Cleanup Module");

  wdt_timer_exit();
  unregister_reboot_notifier(&wdt_notifier);
  unregister_chrdev(WATCHDOG_MAJOR, myname);
  wdt_unregister_cb(&wdt_interrupt);
}

/**************************************************************************
 * wdt_init:
 *
 * Set up the WDT watchdog board. All we have to do is grab the
 * resources we require and bitch if anyone beat us to them.
 * The open() function will actually kick the board off.
 **************************************************************************/
 
int __init wdt_init(void)
{
  int ret;

  /* Announce that we are installing */

  printk("C5471 Watchdog Driver\n");

  /* Register ourselves as the watchdog device driver */

  ret = register_chrdev(WATCHDOG_MAJOR, myname, &wdt_fops);
  if (ret)
    {
      err("Can't register on major=%d ret=%d\n", 
	  WATCHDOG_MAJOR, ret);
      goto out;
    }

  /* Initialize the watchdog timer */

  wdt_timer_init();

  /* Register for an interrupt level callback through wdt_interrupt */

  ret = wdt_register_cb(wdt_interrupt);
  if (ret)
    {
      err("Watchdog timer is not free");
      goto outmisc;
    }

  ret = register_reboot_notifier(&wdt_notifier);
  if(ret)
    {
      err("Can't register reboot notifier (err=%d)", ret);
      goto outtimer;
    }

  ret = 0;

 out:
  return ret;

 outtimer:
  wdt_unregister_cb(wdt_interrupt);

 outmisc:
  unregister_chrdev(WATCHDOG_MAJOR, myname);
  goto out;
}

module_init(wdt_init);
module_exit(wdt_exit);
EXPORT_NO_SYMBOLS;

