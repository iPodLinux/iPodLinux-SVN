/************************************************************************
 * linux/drivers/char/gio_c5471.c
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
 ***********************************************************************/

/************************************************************************
 * Included Files
 ************************************************************************/

/* Use versioning if needed */

#include <linux/autoconf.h>

#ifdef CONFIG_MODVERSIONS
# undef MODVERSIONS
# define MODVERSIONS
#endif

#ifdef MODVERSIONS
#  include <linux/modversions.h>
#endif

#include <linux/config.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mm.h>

#include <linux/ioport.h>
#include <asm/types.h>
#include <asm/semaphore.h>
#include <asm/segment.h>

#include <asm/arch/hardware.h>
#include <asm/arch/irqs.h>
#include <asm/arch/gio_ioctl.h>
#include <asm/arch/sysdep.h>

/************************************************************************
 * Definitions
 ************************************************************************/

/* #undef GIO_DEBUG */
/* #define GIO_DEBUG */

#ifdef GIO_DEBUG
# define dbg(format, arg...) \
  printk(KERN_DEBUG __FUNCTION__ ": " format "\n" , ##arg)
#else
# define dbg(x...)
#endif

#define info(format, arg...) \
  printk(KERN_INFO __FUNCTION__ ": " format "\n" , ##arg)
#define err(format, arg...) \
  printk(KERN_ERR __FUNCTION__ ": " format "\n" , ##arg)

#define NUM_GIO_INTERRUPTS 4

/************************************************************************
 * Private Type Definitions
 ************************************************************************/

/* GIO Overlay. This data structure represents each element in the
 * GIO registers.
 */

struct gio_s
{
  u32 io;
  u32 cio;
  u32 irqa;
  u32 irqb;
  u32 ddio;
  u32 en;
};
typedef struct gio_s gio_t;

/* Driver static data. */

struct driver_struct
{
#ifdef DECLARE_WAITQUEUE
  wait_queue_head_t  gio_waitq[NUM_GIO_INTERRUPTS];
#else
  struct wait_queue *gio_waitq[NUM_GIO_INTERRUPTS];
#endif
  char interrupt_occurred[NUM_GIO_INTERRUPTS];
  char interrupt_requested[NUM_GIO_INTERRUPTS];
  u64 driver_requested;	
  u64 direction;
  u32 interrupt;
};

/************************************************************************
 * Private Function Prototypes
 ************************************************************************/

static int 
gio_ioctl(struct inode *inode, struct file *filep,
	  unsigned int cmd, unsigned long arg);

static int
gio_open(struct inode * inode, struct file * filep);

#ifdef LINUX_20
static void
gio_release (struct inode *inode, struct file *filep);
#else
static int
gio_release (struct inode *inode, struct file *filep);
#endif /* LINUX_20 */

#ifdef LINUX_20
static int
gio_read(struct inode *inode, struct file *filep, char *buf, int count);
#else
static ssize_t
gio_read(struct file *filep, char *buf, size_t count, loff_t *f_pos);
#endif /* LINUX_20 */

static void
gio_interrupt(int irq, void *dev_id, struct pt_regs *regs);

/************************************************************************
 * Private Variable Data
 ************************************************************************/

static struct driver_struct gio;
static gio_t *pGIO = (gio_t *)GIO_REGISTER_BASE;
static gio_t *pKGIO = (gio_t *)KGIO_REGISTER_BASE;

/* Device File Operations */

static struct file_operations gio_fops =
{
  ioctl:     gio_ioctl,         /* ioctl */
  open:      gio_open,          /* open code */
  release:   gio_release,       /* close code */
  read:      gio_read,          /* Wait-for-interrupt code */
};

/* Give other modules access to the GIO ioctl calls without having to
 * search for the GIO module.
 */

#ifdef MODULE
struct file_operations *gio_get_fileops(void);
#ifdef __USE_OLD_SYMTAB__
static struct symbol_table gio_symbol_table =
{
# include <linux/symtab_begin.h>
  X(gio_get_fileops),
# include <linux/symtab_end.h>
};
#else
EXPORT_SYMBOL(gio_get_fileops);
#endif
#endif

/************************************************************************
 * Private Constant Data
 ************************************************************************/

static const char *int_names[] =
{
  "gio0", "gio1", "gio2", "gio3", "gio4", "gio5", "gio6", "gio7"
};

static const char desc[] = "GIO driver";
static const char author[] = "Copyright 2003 Cadenux, LLC";

/************************************************************************
 * Inline Functions
 ************************************************************************/

static inline int gio_2_irq_num(int val)
{
  switch (val)
    {
    case 0: return  3; break;
    case 1: return 12; break;
    case 2: return 10; break;
    case 3: return  9; break;
    default: return -1;
    }
}

static inline int irq_2_gio_num(int val)
{
  switch (val)
    {
    case 3:  return 0; break;
    case 9:  return 3; break;
    case 10: return 2; break;
    case 12: return 1; break;
    default: return -1;
    }
}

/************************************************************************
 * Private Functions
 ************************************************************************/

/************************************************************************
 * gio_ioctl
 ************************************************************************/

static int
gio_ioctl (struct inode *inode, struct file *filep,
	   unsigned int cmd, unsigned long arg)
{
  u32       *reg;
  u32        value;
  int        err     = 0;
  int        pin = MINOR(inode->i_rdev);
  signed int size    = _IOC_SIZE(cmd);

  dbg("cmd=0x%x arg=0x%lx", cmd, arg);
    
  /* Range check the IOCTL cmd */

  if ((_IOC_TYPE(cmd) != GIO_MAGIC_NUMBER) ||
      (_IOC_NR(cmd) > GIO_IOC_MAXNR)) 
    {
      dbg("Bad ioctl value");
      return -EINVAL;
    }

  /* Check address validity */

  if (_IOC_DIR(cmd) & _IOC_READ)
    {
      err = verify_area(VERIFY_READ, (void*)arg, size);
        
      if (err)
	dbg("verify_area(VERIFY_READ) error:%d", err);
    } 
  else if (_IOC_DIR(cmd) & _IOC_WRITE) 
    {
      err = verify_area(VERIFY_WRITE, (void*)arg, size);
          
      if (err)
	dbg("verify_area(VERIFY_WRITE) error:%d", err);
    }

  if (err) 
    {
      return err;
    }

  /* Command processing */

  switch (cmd) 
    {
      /******************************************************************
       * GIO_DIRECTION: The GIO_DIRECTION can be either input (GIO_INPUT) or
       * output (GIO_OUTPUT).  If unspecified, the GIO will default to input.
       */

    case GIO_DIRECTION:
      {
        dbg("GIO_DIRECTION: pin=%d arg=%ld", pin, arg);

        /* Pins 0-19 are controlled by bits in GIO; pins 20-35 are
         * controlled by bits in KGIO.
         */

	if (pin > 19)
	  {
	    reg = &pKGIO->cio;
	    value = pin - 20;
	  }
	else
	  {
	    reg = &pGIO->cio;
	    value = pin;
	  }

        /* Set or clear the direction bit associated with the pin */

	if (arg)
	  {
	    *reg |= (1 << value);
	    gio.direction |= (1 << pin);
	  }
	else
	  {
	    *reg &= ~(1 << value);
	    gio.direction &= ~(1 << pin);
	  }
      }
      break;

      /******************************************************************
       * GIO_BITSET: If the direction is set to output, this ioctl will
       * set the output.  If the direction is input, this ioctl will
       * return the value of the GIO pin (0, 1, or -1 on error).
       */

    case GIO_BITSET:
      {
	dbg("GIO_BITSET: pin=%d arg=%ld", pin, arg);

        /* Check if the pin has been programmed as an interrupt:  We
         * will not set a bit on an interrupt pin.
         */

	if (gio.interrupt & (1 << pin))
	  {
	    dbg("pin %d is programmed for interrupt", pin);
	    return -EINVAL;
	  }

	if (gio.direction & (1 << pin))
	  {
            /* Pin is programmed for input */

            /* Input state for pins 0-19 is provided in GIO; pin
             * 20-35 state is provided in KGIO.
             */

	    if (pin > 19)
	      {
                /* If the pin is set, return "1" otherwise the
                 * default behavior for this ioctl is to return
                 * zero.
                 */

		if (pKGIO->io & (1 << (pin - 20)))
		  {
		    return 1;
		  }
	      }
	    else
	      {
                /* If the pin is set, return "1" otherwise the
                 * default behavior for this ioctl is to return
                 * zero.
                 */

		if (pGIO->io & (1 << pin)) {
		  return 1;
		}
	      }
	  }
	else
	  {
            /* Pin is programmed for output */

            /* Output state for pins 0-19 is controlled in GIO; pin
             * 20-35 state is controlled in KGIO.
             */

	    if (pin > 19)
	      {
		pKGIO->io |= (1 << (pin - 20));
	      }
	    else
	      {
		pGIO->io |= (1 << pin);
	      }
	  }
      }
      break;

      /******************************************************************
       * GIO_BITCLR: If the direction is set to output, this ioctl will
       * clear the output.  If the direction is input, this ioctl will
       * return the value of the GIO pin (0, 1, or -1 on error).
       */

    case GIO_BITCLR:
      {
        dbg("GIO_BITCLR: pin=%d arg=%ld", pin, arg);

        /* Check if the pin has been programmed as an interrupt:  We
         * will not clear a bit on an interrupt pin.
         */

	if (gio.interrupt & (1 << pin))
	  {
            dbg("pin %d is programmed for interrupt", pin);
	    return -EINVAL;
	  }

	if (gio.direction & (1 << pin))
	  {
            /* Pin is programmed for input */

            /* Input state for pins 0-19 is provided in GIO; pin
             * 16-31 state is provided in KGIO.
             */

	    if (pin > 19)
	      {
                /* If the pin is set, return "1" otherwise the
                 * default behavior for this ioctl is to return
                 * zero.
                 */

		if (pKGIO->io & (1 << (pin - 20)))
		  {
		    return 1;
		  }
	      }
	    else
	      {
                /* If the pin is set, return "1" otherwise the
                 * default behavior for this ioctl is to return
                 * zero.
                 */

		if (pGIO->io & (1 << pin))
		  {
		    return 1;
		  }
	      }
	  }
	else
	  {
            /* Pin is programmed for output */

            /* Output state for pins 0-19 is controlled in GIO; pin
             * 16-31 state is controlled in KGIO.
             */

	    if (pin > 19)
	      {
		pKGIO->io &= ~(1 << (pin - 20));
	      }
	    else
	      {
		pGIO->io &= ~(1 << pin);
	      }
	  }
      }
      break;

      /******************************************************************
       * GIO_IRQPORT: Configure a GIO as an interrupt.  Only GIO0-GIO3 can
       * be configured as an interrupting source.
       */

    case GIO_IRQPORT:
      {
        dbg("GIO_IRQPORT: pin=%d arg=%ld", pin, arg);

        /* Verify that pin corresponds to one of GIO0-GIO3. */

	if (pin >= NUM_GIO_INTERRUPTS)
	  {
            dbg("pin=%d will not support interrupts", pin);
	    return -EINVAL;
	  }

        /* 0=detach from interrupt; 1=attach to interrupt */

	if (arg)
	  {
	    /* Attach to interrupt if it hasn't been done yet. */

	    if ((gio.interrupt & (1 << pin)) == 0)
	      {
		/* No, ... then do so now. */

		if (request_irq(gio_2_irq_num(pin), &gio_interrupt, 
				SA_INTERRUPT, int_names[pin], 
				&gio_interrupt))
		  {
		    err("unable to get IRQ%d for the GIO driver",
			(int)pin);
		    return -1;
		  }
		else
		  {
		    dbg("Attached to IRQ%d", pin);
		    gio.interrupt |= (1 << pin);
		  }                    
	      }
	    else
	      {
		dbg("IRQ%d already attached", pin);
	      }                    
	  }
	else
	  {
	    /* Detach from interrupt. */

	    if ((gio.interrupt & (1 << pin)) != 0)
	      {
		free_irq(gio_2_irq_num(pin), &gio_interrupt);
		gio.interrupt &= ~(1 << pin);    
	      }
            else
              {
                dbg("IRQ%d already detached", pin);
              }
	  }

	/* Then make it so */

	pGIO->irqa = pGIO->irqb = gio.interrupt;
      }
      break;

    default:
      dbg("Error, falling through switch");
      return -EINVAL;
      break;
    }
  return 0;
}

/************************************************************************
 * gio_open
 ************************************************************************/

static int gio_open(struct inode * inode, struct file * filep)
{
  int pin = MINOR(inode->i_rdev);

  dbg("pin=%d", pin);

  /* Verify that the GIO pin number is within the range of those supported
   * by this driver.
   */

  if (pin > MAX_GIO)
    {
      dbg("Bad minor (pin) %d", pin);
      return -EACCES;
    }

  /* Initialized the file structure */
    
  filep->private_data = 0;
  filep->f_pos        = 0;

  /* Check if this GIO has already been opened.  Sorry only one per
   * customer.
   */

  if (gio.driver_requested & (1 << pin))
    {
      err("GIO%d already opened", pin);
      return -EACCES;
    }

  /* Mark the driver as open */

  gio.driver_requested |= (1 << pin);
  MOD_INC_USE_COUNT;

  return 0;
}

/************************************************************************
 * gio_read
 ************************************************************************/

#ifdef LINUX_20
static void
gio_release (struct inode *inode, struct file *filep)
#else
static int
gio_release (struct inode *inode, struct file *filep)
#endif /* LINUX_20 */
{
  int pin = MINOR(inode->i_rdev);

  dbg("Closing inode %p, filep %p", inode, filep);

  /* Restore default pin settings */

  (void)gio_ioctl(inode, filep, GIO_DIRECTION, GIO_INPUT);
  (void)gio_ioctl(inode, filep, GIO_IRQPORT,   GIO_NOINTERRUPT);

  /* And mark the GIO un-opened */

  gio.driver_requested &= ~(1 << pin);

  MOD_DEC_USE_COUNT;

#ifndef LINUX_20
  return 0;
#endif /* LINUX_20 */
}

/************************************************************************
 * gio_read
 ************************************************************************/

#ifdef LINUX_20
static int
gio_read(struct inode *inode, struct file *filep, char *buf, int count)
#else
static ssize_t
gio_read(struct file *filep, char *buf, size_t count, loff_t *f_pos)
#endif /* LINUX_20 */
{
#ifdef LINUX_20
  int pin = MINOR(inode->i_rdev);
#else
  int pin = MINOR(filep->f_dentry->d_inode->i_rdev);
#endif /* LINUX_20 */
  int flags;

  dbg("Reading pin %d", pin);

  /* Make sure that this pin has been programmed for interrupts.
   * If not, we'll return success immediately.
   */

  if ((gio.interrupt & (1 << pin)) != 0)
    {
      /* If the open flag was noblock and there's no data, return -EAGAIN.
       * Implementation from Rubini, Blocking I/O, modified with save flags
       * because we're checking a static that is modified in a ISR, and we're
       * placing ourselves onto a wait queue that's checked in an ISR.
       */

      save_flags(flags); cli();

      /* Loop until we receive an interrupt on this pin. */

      while (gio.interrupt_occurred[pin] == 0)
	{
          /* If the open flag was O_NONBLOCK and and no interrupt has been
           * received, return -EAGAIN.
           */

	  if (filep->f_flags & O_NONBLOCK)
	    {
	      restore_flags(flags);
              dbg("Returing -EAGAIN");
	      return -EAGAIN;
	    }

          dbg("%s waiting on timer queue for GIO%d", current->comm, pin);
	  interruptible_sleep_on(&gio.gio_waitq[pin]);
          dbg("%s awakened from timer queue for GIO%d", current->comm, pin);

          /* Check signals */

#ifdef LINUX_20
          if (current->signal & ~current->blocked)
#else
          if (signal_pending(current))
#endif /* LINUX_20 */
	    {
              dbg("Returing -EINTR");
	      return -EINTR;
	    }
	  cli();
	}
      restore_flags(flags);

      /* Indicate that the interrupt has been received and processed. */

      gio.interrupt_occurred[pin] = 0;
    }
  return 0;
}

/************************************************************************
 * gio_read
 ************************************************************************/

static void gio_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
  int pin;

  dbg("irq%d received", irq);

  /* irq_2_gio_num translates the received irq to 0-3 for
   * both platforms.
   */

  pin = irq_2_gio_num(irq);
  if (pin >= 0)
    {
      gio.interrupt_occurred[pin] = 1;
      wake_up_interruptible(&gio.gio_waitq[pin]);
    }
}

/************************************************************************
 * Global Functions
 ************************************************************************/

struct file_operations *gio_get_fileops(void)
{
  return &gio_fops;
}

#ifdef MODULE
int init_module(void)
#else
int gio_c5471_init(void)
#endif
{
  /* Basic initialization of device nodes. */

  printk("%s module %s\n", desc, author);

  if (register_chrdev(GIO_MAJOR, "gio", &gio_fops) != 0)
    {
      info("GIO: can't get major number %d", GIO_MAJOR);
      return -1;
    }
  else
    {
      dbg("GIO major number : %d", GIO_MAJOR);
    }

  request_region((unsigned int)pGIO,sizeof(gio_t),"gio");
  request_region((unsigned int)pKGIO,sizeof(gio_t),"gio");

  /* Set all pins as input pins */

  pGIO->cio  = 0x000fffff;
  pKGIO->cio = 0x000fffff;

  /* Non-interrupting */

  pGIO->irqa  = 0;
  pGIO->irqb  = 0;
  pKGIO->irqa = 0;
  pKGIO->irqb = 0;

  /* Set delta detect registers */

  pGIO->ddio  = 0;
  pKGIO->ddio = 0;

  /* Non-muxed output */

  pGIO->en  = 0x0;
  pKGIO->en = 0x0;

#ifdef MODULE
  REGISTER_SYMTAB(&gio_syms);
#endif

  return 0;
}

#ifdef MODULE
void cleanup_module(void)
{
  unregister_chrdev(GIO_MAJOR, "gio");
  release_region((unsigned int)pGIO, sizeof(gio_t));
  release_region((unsigned int)pKGIO, sizeof(gio_t));
}
#endif
