/*
 * adapter.c
 *
 * Xilinx System ACE Adapter component to interface System ACE to Linux
 *
 * Author: MontaVista Software, Inc.
 *         source@mvista.com
 *
 * 2002 (c) MontaVista, Software, Inc.  This file is licensed under the terms
 * of the GNU General Public License version 2.  This program is licensed
 * "as is" without any warranty of any kind, whether express or implied.
 */

/*
 * Through System ACE, the processor can access the CompactFlash and the
 * JTAG chain.  In addition, the System ACE controls system reset and
 * which configuration will be loaded into the JTAG chain at that time.
 * This driver provides two different interfaces.  The first is handling
 * reset by tying into the system's reset code as well as providing a
 * /proc interface to read and write which configuration should be used
 * when the system is reset.  The second is to expose a block interface
 * to the CompactFlash.
 * 
 * This driver is a bit unusual in that it is composed of two logical
 * parts where one part is the OS independent code and the other part is
 * the OS dependent code.  Xilinx provides their drivers split in this
 * fashion.  This file represents the Linux OS dependent part known as
 * the Linux adapter.  The other files in this directory are the OS
 * independent files as provided by Xilinx with no changes made to them.
 * The names exported by those files begin with XSysAce_.  All functions
 * in this file that are called by Linux have names that begin with
 * xsysace_.  Any other functions are static helper functions.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/hdreg.h>
#include <linux/slab.h>
#include <linux/blkpg.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#define NR_HD		1	/* System ACE only handles one CompactFlash */
#define PARTN_BITS 4		/* Only allow 15 partitions. */

/* Dynamically allocate a major number. */
static int xsa_major = 0;
#define MAJOR_NR (xsa_major)
#define MAJOR_NAME "xsysace"

#define DEVICE_NAME "System ACE"
#define DEVICE_REQUEST xsysace_do_request
#define DEVICE_NR(device) (MINOR(device) >> PARTN_BITS)
#include <linux/blk.h>
#include <linux/blkdev.h>

#include <xbasic_types.h>
#include "xsysace.h"
#include <asm/xparameters.h>

MODULE_AUTHOR("MontaVista Software, Inc. <source@mvista.com>");
MODULE_DESCRIPTION("Xilinx System ACE block driver");
MODULE_LICENSE("GPL");

/*
 * We have to wait for a lock and for the CompactFlash to not be busy
 * via polling.  If dont_spin is non-zero, we will use schedule_timeout
 * in a loop to check for these conditions.  If dont_spin is zero, we
 * will tight loop without a schedule_timeout as long as need_resched is
 * false.  If need_resched is true, we'll fall back to using
 * schedule_timeout.  This could obviously be made run-time settable.
 */
static const int dont_spin = 0;

static u32 save_BaseAddress;	/* Saved physical base address */
// FIXME this was used in the ppc version. Sometime it has to
//       be adopted to the microblaze
//static void (*old_restart) (char *cmd) = NULL;	/* old ppc_md.restart *
static unsigned char heads;
static unsigned char sectors;
static unsigned short cylinders;

static int access_count = 0;
static char revalidating = 0;
static DECLARE_WAIT_QUEUE_HEAD(revalidate_wait);

/*
 * The following variables are used to keep track of what all has been
 * done to make error handling easier.
 */
static char reqirq = 0;		/* Has request_irq() been called? */
static char registered = 0;	/* Has devfs_register_blkdev() been called? */

/*
 * The underlying OS independent code needs space as well.  A pointer to
 * the following XSysAce structure will be passed to any XSysAce_
 * function that requires it.  However, we treat the data as an opaque
 * object in this file (meaning that we never reference any of the
 * fields inside of the structure).
 */
static XSysAce SysAce;

/* These tables are indexed by major and minor numbers. */
static int xsa_sizes[NR_HD << PARTN_BITS];	/* Size of the device, kb */
static int xsa_blocksizes[NR_HD << PARTN_BITS];	/* Block size, bytes */
static int xsa_hardsectsizes[NR_HD << PARTN_BITS];	/* Sector size, bytes */
static int xsa_maxsect[NR_HD << PARTN_BITS];	/* Max request size, sectors */
static struct hd_struct xsa_hd[NR_HD << PARTN_BITS];	/* Partition Table */

/*
 * In general, requests enter this driver through xsysace_do_request()
 * which then schedules the xsa_thread.  The xsa_thread sends the
 * request down to the Xilinx OS independent layer.  When the request
 * completes, EventHandler() will get called as a result of an
 * interrupt.  The following variables support this flow.
 */
static struct task_struct *xsa_task = NULL;	/* xsa_thread pointer */
static struct completion task_sync;	/* xsa_thread start/stop syncing */
static char task_shutdown = 0;	/* Set to non-zero when task should quit.  */
/*
 * req_fnc will be either NULL or a pointer to XSysAce_SectorRead or
 * XSysAce_SectorWrite.  It will be set to point to one of the XSysAce*
 * functions by xsysace_do_request and then xsa_thread will be scheduled.
 * The xsa_thread will then NULL req_fnc after it has copied the pointer.
 */
static XStatus(*req_fnc) (XSysAce * InstancePtr, u32 StartSector,
			  int NumSectors, u8 * BufferPtr);
/* xsa_thread waits on req_wait until xsysace_do_request does wake_up on it */
static DECLARE_WAIT_QUEUE_HEAD(req_wait);
/* req_active is a simple flag that says a request is being serviced. */
static char req_active = 0;
/* req_str will be used for errors and will be either "reading" or "writing" */
static char *req_str;


/* SAATODO: Xilinx is going to add this function.  Nuke when they do. */
unsigned int
XSysAce_GetCfgAddr(XSysAce * InstancePtr)
{
	u32 Status;

	XASSERT_NONVOID(InstancePtr != NULL);
	XASSERT_NONVOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);

	Status = XSysAce_mGetControlReg(InstancePtr->BaseAddress);
	if (!(Status & XSA_CR_FORCECFGADDR_MASK))
		Status = XSysAce_mGetStatusReg(InstancePtr->BaseAddress);

	return (unsigned int) ((Status & XSA_SR_CFGADDR_MASK) >>
			       XSA_CR_CFGADDR_SHIFT);
}

/* SAATODO: Nuke the following line when Config stuff moved out. */
extern XSysAce_Config XSysAce_ConfigTable[];
/* SAATODO: This function will be moved into the Xilinx code. */
/*****************************************************************************/
/**
*
* Lookup the device configuration based on the sysace instance.  The table
* XSysAce_ConfigTable contains the configuration info for each device in the system.
*
* @param Instance is the index of the interface being looked up.
*
* @return
*
* A pointer to the configuration table entry corresponding to the given
* device ID, or NULL if no match is found.
*
* @note
*
* None.
*
******************************************************************************/
XSysAce_Config *
XSysAce_GetConfig(int Instance)
{
	if (Instance < 0 || Instance >= XPAR_XSYSACE_NUM_INSTANCES) {
		return NULL;
	}

	return &XSysAce_ConfigTable[Instance];
}

/*
 * The following block of code implements the reset handling.  The first
 * part implements /proc/xsysace/cfgaddr.  When read, it will yield a
 * number from 0 to 7 that represents which configuration will be used
 * next (the configuration address).  Writing a number to it will change
 * the configuration address.  After that is the function that is hooked
 * into the system's reset handler.
 */
#ifndef CONFIG_PROC_FS
#define proc_init() 0
#define proc_cleanup()
#else
#define CFGADDR_NAME "cfgaddr"
static struct proc_dir_entry *xsysace_dir = NULL;
static struct proc_dir_entry *cfgaddr_file = NULL;

static int
cfgaddr_read(char *page, char **start,
	     off_t off, int count, int *eof, void *data)
{
	unsigned int cfgaddr;

	/* Make sure we have room for a digit (0-7), a newline and a NULL */
	if (count < 3)
		return -EINVAL;

	MOD_INC_USE_COUNT;

	cfgaddr = XSysAce_GetCfgAddr(&SysAce);

	count = sprintf(page + off, "%d\n", cfgaddr);
	*eof = 1;

	MOD_DEC_USE_COUNT;

	return count;
}

static int
cfgaddr_write(struct file *file,
	      const char *buffer, unsigned long count, void *data)
{
	char val[2];

	if (count < 1 || count > 2)
		return -EINVAL;

	MOD_INC_USE_COUNT;

	if (copy_from_user(val, buffer, count)) {
		MOD_DEC_USE_COUNT;
		return -EFAULT;
	}

	if (val[0] < '0' || val[0] > '7' || (count == 2 && !(val[1] == '\n' ||
							     val[1] == '\0'))) {
		MOD_DEC_USE_COUNT;
		return -EINVAL;
	}

	XSysAce_SetCfgAddr(&SysAce, val[0] - '0');

	MOD_DEC_USE_COUNT;

	return count;
}

static int
proc_init(void)
{
	xsysace_dir = proc_mkdir(MAJOR_NAME, NULL);
	if (!xsysace_dir)
		return -ENOMEM;
	xsysace_dir->owner = THIS_MODULE;

	cfgaddr_file = create_proc_entry(CFGADDR_NAME, 0644, xsysace_dir);
	if (!cfgaddr_file) {
		remove_proc_entry(MAJOR_NAME, NULL);
		return -ENOMEM;
	}
	cfgaddr_file->read_proc = cfgaddr_read;
	cfgaddr_file->write_proc = cfgaddr_write;
	cfgaddr_file->owner = THIS_MODULE;
	return 0;
}

static void
proc_cleanup(void)
{
	if (cfgaddr_file)
		remove_proc_entry(CFGADDR_NAME, xsysace_dir);
	if (xsysace_dir)
		remove_proc_entry(MAJOR_NAME, NULL);
}
#endif

// FIXME At the moment this funtion is not used in 
//       the microblaze port
static void 
xsysace_restart(char *cmd) 
{ 
	XSysAce_ResetCfg(&SysAce);

	/* Wait for reset. */ 
	for (;;) ; 
} 

/*
 * The code to handle the block device starts here.
 */

/*
 * This is just a small helper function to clean up a request that has
 * been given to the xsa_thread.
 */
static void
xsa_complete_request(int uptodate)
{
	unsigned long flags;

	XSysAce_Unlock(&SysAce);
	spin_lock_irqsave(&io_request_lock, flags);
	end_request(uptodate);
	req_active = 0;
	/* If there's something in the queue, */
	if (!QUEUE_EMPTY)
		xsysace_do_request(NULL);	/* handle it. */
	spin_unlock_irqrestore(&io_request_lock, flags);
}

/* Small helper function used inside polling loops. */
static inline void
xsa_short_delay(void)
{
	/* If someone else needs the CPU, go to sleep. */
	if (dont_spin || current->need_resched) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ / 100);
	}
}

/* Simple function that hands an interrupt to the Xilinx code. */
static void
xsysace_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	XSysAce_InterruptHandler(&SysAce);
}

/* Called by the Xilinx interrupt handler to give us an event. */
static void
EventHandler(void *CallbackRef, int Event)
{
	u32 ErrorMask;

	switch (Event) {
	case XSA_EVENT_DATA_DONE:
		xsa_complete_request(1);	/* The request succeeded. */
		break;

	case XSA_EVENT_ERROR:
		ErrorMask = XSysAce_GetErrors(&SysAce);

		/* Print out what went wrong. */
		if (ErrorMask & XSA_ER_CARD_RESET)
			printk(KERN_ERR "CompactFlash failed to reset\n");
		if (ErrorMask & XSA_ER_CARD_READY)
			printk(KERN_ERR "CompactFlash failed to ready\n");
		if (ErrorMask & XSA_ER_CARD_READ)
			printk(KERN_ERR "CompactFlash read command failed\n");
		if (ErrorMask & XSA_ER_CARD_WRITE)
			printk(KERN_ERR "CompactFlash write command failed\n");
		if (ErrorMask & XSA_ER_SECTOR_READY)
			printk(KERN_ERR
			       "CompactFlash sector failed to ready\n");
		if (ErrorMask & XSA_ER_BAD_BLOCK)
			printk(KERN_ERR "CompactFlash bad block detected\n");
		if (ErrorMask & XSA_ER_UNCORRECTABLE)
			printk(KERN_ERR "CompactFlash uncorrectable error\n");
		if (ErrorMask & XSA_ER_SECTOR_ID)
			printk(KERN_ERR "CompactFlash sector ID not found\n");
		if (ErrorMask & XSA_ER_ABORT)
			printk(KERN_ERR "CompactFlash command aborted\n");
		if (ErrorMask & XSA_ER_GENERAL)
			printk(KERN_ERR "CompactFlash general error\n");

		if (ErrorMask & XSA_ER_CFG_READ)
			printk(KERN_ERR
			       "JTAG controller couldn't read configuration from the CompactFlash\n");
		if (ErrorMask & XSA_ER_CFG_ADDR)
			printk(KERN_ERR
			       "Invalid address given to JTAG controller\n");
		if (ErrorMask & XSA_ER_CFG_FAIL)
			printk(KERN_ERR
			       "JTAG controller failed to configure a device\n");
		if (ErrorMask & XSA_ER_CFG_INSTR)
			printk(KERN_ERR
			       "Invalid instruction during JTAG configuration\n");
		if (ErrorMask & XSA_ER_CFG_INIT)
			printk(KERN_ERR "JTAG CFGINIT pin error\n");

		/* Check for errors that should reset the CompactFlash */
		if (ErrorMask & (XSA_ER_CARD_RESET |
				 XSA_ER_CARD_READY |
				 XSA_ER_CARD_READ |
				 XSA_ER_CARD_WRITE |
				 XSA_ER_SECTOR_READY |
				 XSA_ER_BAD_BLOCK |
				 XSA_ER_UNCORRECTABLE |
				 XSA_ER_SECTOR_ID | XSA_ER_ABORT |
				 XSA_ER_GENERAL)) {
			if (XSysAce_ResetCF(&SysAce) != XST_SUCCESS)
				printk(KERN_ERR
				       "Could not reset CompactFlash\n");
			xsa_complete_request(0); /* The request failed. */
		}
		break;
	case XSA_EVENT_CFG_DONE:
		printk(KERN_WARNING "XSA_EVENT_CFG_DONE not handled yet.\n");
		break;
	default:
		printk(KERN_ERR "%s: unrecognized event %d\n",
		       DEVICE_NAME, Event);
		break;
	}
}

/*
 * This task takes care of sending requests down to the Xilinx OS independent
 * code.  A task is necessary because there isn't an interrupt telling us
 * when the hardware is ready to accept another request, so we have to poll.
 */
static int
xsa_thread(void *arg)
{
	XStatus stat;
	unsigned long sector;

	struct task_struct *tsk = current;
	DECLARE_WAITQUEUE(wait, tsk);
	xsa_task = tsk;

	daemonize();
	reparent_to_init();
	strcpy(xsa_task->comm, MAJOR_NAME);
	xsa_task->tty = NULL;

	/* only want to receive SIGKILL */
	spin_lock_irq(&xsa_task->sigmask_lock);
	siginitsetinv(&xsa_task->blocked, sigmask(SIGKILL));
	recalc_sigpending(xsa_task);
	spin_unlock_irq(&xsa_task->sigmask_lock);

	complete(&task_sync);

	add_wait_queue(&req_wait, &wait);
	for (;;) {
		XStatus(*cur_req) (XSysAce * InstancePtr, u32 StartSector,
				   int NumSectors, u8 * BufferPtr);

		/* Block waiting for request */
		for (;;) {
			set_task_state(tsk, TASK_INTERRUPTIBLE);
			cur_req = req_fnc;
			if (cur_req != NULL) {
				req_fnc = NULL;
				break;	/* We've got a request. */
			}
			while (signal_pending(tsk)) {
				siginfo_t info;

				/* Only honor the signal if we're cleaning up */
				if (task_shutdown)
					goto exit;
				/*
				 * Someone else sent us a kill (probably
				 * the shutdown scripts "Sending all
				 * processes the KILL signal").  Just
				 * dequeue it and ignore it.
				 */
				spin_lock_irq(&current->sigmask_lock);
				(void)dequeue_signal(&current->blocked, &info);
				spin_unlock_irq(&current->sigmask_lock);
			}
			schedule();
		}
		set_task_state(tsk, TASK_RUNNING);

		/* We have a request. */
		sector = (CURRENT->sector +
			  xsa_hd[MINOR(CURRENT->rq_dev)].start_sect);

		while ((stat = XSysAce_Lock(&SysAce, 0)) == XST_DEVICE_BUSY)
			xsa_short_delay();
		if (stat != XST_SUCCESS) {
			printk(KERN_ERR "%s: Error %d when locking.\n",
			       DEVICE_NAME, stat);
			xsa_complete_request(0);	/* Request failed. */
		}

		while ((stat = cur_req(&SysAce, sector,
				       CURRENT->current_nr_sectors,
				       CURRENT->buffer)) == XST_DEVICE_BUSY)
			xsa_short_delay();
		/*
		 * If the stat is XST_SUCCESS, we have successfully
		 * gotten the request started on the hardware.  The
		 * completion (or error) interrupt will unlock the
		 * CompactFlash and complete the request, so we don't
		 * need to do anything except just loop around and wait
		 * for the next request.  If the status is not
		 * XST_SUCCESS, we need to finish the request with an
		 * error before waiting for the next request.
		 */
		if (stat != XST_SUCCESS) {
			printk(KERN_ERR "%s: Error %d when %s sector %lu.\n",
			       DEVICE_NAME, stat, req_str, sector);
			xsa_complete_request(0);	/* Request failed. */
		}
	}

      exit:
	remove_wait_queue(&req_wait, &wait);

	xsa_task = NULL;
	complete_and_exit(&task_sync, 0);
}

static void
xsysace_do_request(request_queue_t * q)
{
	/* We're already handling a request.  Don't accept another. */
	if (req_active)
		return;

	for (;;) {
		INIT_REQUEST;

		switch (CURRENT->cmd) {
		case READ:
			req_str = "reading";
			req_fnc = XSysAce_SectorRead;
			break;
		case WRITE:
			req_str = "writing";
			req_fnc = XSysAce_SectorWrite;
			break;
		default:
			printk(KERN_CRIT "%s: unknown request %d.\n",
			       DEVICE_NAME, CURRENT->cmd);
			end_request(0);	/* Bad request.  End it and onwards. */
			continue;	/* Go on to the next request. */
		}

		req_active = 1;
		wake_up(&req_wait);	/* Schedule task to take care of it */
		/*
		 * It is up to the task or interrupt handler to complete
		 * the request.  We return now so the io_request_lock
		 * will get cleared and requests can be queued.
		 */
		return;
	}
}

extern struct gendisk xsa_gendisk;
static int
xsa_revalidate(kdev_t kdev, int max_access)
{
	int target, max_p, start, i;
	long flags;

	target = DEVICE_NR(kdev);

	save_flags(flags);
	cli();
	if (revalidating || access_count > max_access) {
		restore_flags(flags);
		return -EBUSY;
	}
	revalidating = 1;
	restore_flags(flags);

	max_p = xsa_gendisk.max_p;
	start = target << PARTN_BITS;
	for (i = max_p - 1; i >= 0; i--) {
		int minor = start + i;
		invalidate_device(MKDEV(MAJOR_NR, minor), 1);
		xsa_gendisk.part[minor].start_sect = 0;
		xsa_gendisk.part[minor].nr_sects = 0;
	}

	grok_partitions(&xsa_gendisk, target, NR_HD << PARTN_BITS,
			(long) cylinders * (long) heads * (long) sectors);
	revalidating = 0;
	wake_up(&revalidate_wait);
	return 0;
}

/*
 * For right now, there isn't a way to tell the hardware to spin down
 * the drive.  For IBM Microdrives, which is what the ML300 ships with,
 * removing them without a spindown is probably not very good.  Thus, we
 * don't even want to have the System ACE driver handle removable media.
 * For now, placeholder code has been surrounded by ifdef XSA_REMOVABLE
 * (which is not defined) in a few places in this file.  Something that
 * is not handled in the placeholder code is identifying the new drive.
 */
#ifdef XSA_REMOVABLE
static int
xsysace_revalidate(kdev_t kdev)
{
	return xsa_revalidate(kdev, 0);
}

static int
xsysace_check_change(kdev_t kdev)
{
	/*
	 * If and when we do support removable media, we'll need to
	 * figure out how to tell if the media was changed.  For now,
	 * just always say that the media was changed, which is safe,
	 * but not optimal.
	 */
	return 1;
}
#endif

static int
xsysace_open(struct inode *inode, struct file *file)
{
	wait_event(revalidate_wait, !revalidating);
	access_count++;

	MOD_INC_USE_COUNT;
	return 0;
}
static int
xsysace_release(struct inode *inode, struct file *file)
{
	access_count--;

	MOD_DEC_USE_COUNT;
	return 0;
}
static int
xsysace_ioctl(struct inode *inode, struct file *file,
	      unsigned int cmd, unsigned long arg)
{
	if (!inode || !inode->i_rdev)
		return -EINVAL;

	switch (cmd) {
	case BLKGETSIZE:
		return put_user(xsa_hd[MINOR(inode->i_rdev)].nr_sects,
				(unsigned long *) arg);
	case BLKGETSIZE64:
		return put_user((u64) xsa_hd[MINOR(inode->i_rdev)].
				nr_sects, (u64 *) arg);
	case BLKRRPART:
		if (!capable(CAP_SYS_ADMIN))
			return -EACCES;

		return xsa_revalidate(inode->i_rdev, 1);

	case HDIO_GETGEO:
		{
			struct hd_geometry *loc, g;
			loc = (struct hd_geometry *) arg;
			if (!loc)
				return -EINVAL;
			g.heads = heads;
			g.sectors = sectors;
			g.cylinders = cylinders;
			g.start = xsa_hd[MINOR(inode->i_rdev)].start_sect;
			return copy_to_user(loc, &g, sizeof(g)) ? -EFAULT : 0;
		}
	default:
		/* Let the block layer handle all the others. */
		return blk_ioctl(inode->i_rdev, cmd, arg);
	}
}

static struct block_device_operations xsa_fops = {
	open:xsysace_open,
	release:xsysace_release,
	ioctl:xsysace_ioctl,
#ifdef XSA_REMOVABLE
	check_media_change:xsysace_check_change,
	revalidate:xsysace_revalidate
#endif
};
static struct gendisk xsa_gendisk = {	/* Generic Hard Disk */
	major_name:MAJOR_NAME,
	minor_shift:PARTN_BITS,
	max_p:NR_HD << PARTN_BITS,
	part:xsa_hd,
	sizes:xsa_sizes,
	fops:&xsa_fops
};

/* FIXME hardcoded IRQ for sysace at IRQ 4 */
#if 1
#define XSA_IRQ (4)
#else
#define XSA_IRQ (31 - XPAR_INTC_0_SYSACE_0_VEC_ID)
#endif

static void
cleanup(void)
{
	XSysAce_Config *cfg;
	int i;

	/* Make sure everything is flushed. */
	for (i = 0; i < (NR_HD << PARTN_BITS); i++)
		fsync_dev(MKDEV(xsa_major, i));

	proc_cleanup();

	if (registered) {
		blk_cleanup_queue(BLK_DEFAULT_QUEUE(xsa_major));
		del_gendisk(&xsa_gendisk);
		if (devfs_unregister_blkdev(xsa_major, DEVICE_NAME) != 0) {
			printk(KERN_ERR "%s: unable to release major %d\n",
			       DEVICE_NAME, xsa_major);
		}
	}

	/* Tidy up a little. */
	read_ahead[xsa_major] = 0;
	blk_size[xsa_major] = NULL;
	blksize_size[xsa_major] = NULL;
	hardsect_size[xsa_major] = NULL;
	max_sectors[xsa_major] = NULL;

	if (xsa_task) {
		task_shutdown = 1;
		send_sig(SIGKILL, xsa_task, 1);
		wait_for_completion(&task_sync);
	}

	if (reqirq) {
		XSysAce_DisableInterrupt(&SysAce);
		disable_irq(XSA_IRQ);
		free_irq(XSA_IRQ, NULL);
	}

	cfg = XSysAce_GetConfig(0);
	iounmap((void *) cfg->BaseAddress);
	cfg->BaseAddress = save_BaseAddress;

	// FIXME this is left over from the ppc version of this
        //       file. Sometime it has to be ported to the
        //       microblaze
	//	if (old_restart) 
	//		ppc_md.restart = old_restart; 
}

static int __init
xsysace_init(void)
{
	static const unsigned long remap_size
	    = XPAR_SYSACE_0_HIGHADDR - XPAR_SYSACE_0_BASEADDR + 1;
	XSysAce_Config *cfg;
	XSysAce_CFParameters ident;
	XStatus stat;
	long size;
	int i;

	printk("Initializing SystemAce driver\n");
	/* Find the config for our device. */
	cfg = XSysAce_GetConfig(0);
	if (!cfg)
		return -ENODEV;

	/* Change the addresses to be virtual; save the old ones to restore. */
	save_BaseAddress = cfg->BaseAddress;
	cfg->BaseAddress = (u32) ioremap(save_BaseAddress, remap_size);

	/* Tell the Xilinx code to bring this interface up. */
	if (XSysAce_Initialize(&SysAce, cfg->DeviceId) != XST_SUCCESS) {
		printk(KERN_ERR "%s: Could not initialize device.\n",
		       DEVICE_NAME);
		cleanup();
		return -ENODEV;
	}

	i = request_irq(XSA_IRQ, xsysace_interrupt, 0, DEVICE_NAME, NULL);
	if (i) {
		printk(KERN_ERR "%s: Could not allocate interrupt %d.\n",
		       DEVICE_NAME, XSA_IRQ);
		cleanup();
		return i;
	}
	reqirq = 1;
	XSysAce_SetEventHandler(&SysAce, EventHandler, (void *) NULL);
	XSysAce_EnableInterrupt(&SysAce);

	/* Time to identify the drive. */
	while (XSysAce_Lock(&SysAce, 0) == XST_DEVICE_BUSY)
		xsa_short_delay();
	while ((stat = XSysAce_IdentifyCF(&SysAce, &ident)) == XST_DEVICE_BUSY)
		xsa_short_delay();
	XSysAce_Unlock(&SysAce);
	if (stat != XST_SUCCESS) {
		printk(KERN_ERR "%s: Could not send identify command.\n",
		       DEVICE_NAME);
		cleanup();
		return -ENODEV;
	}

	/* Fill in what we learned. */
	heads = ident.NumHeads;
	sectors = ident.NumSectorsPerTrack;
	cylinders = ident.NumCylinders;
	size = (long) cylinders * (long) heads * (long) sectors;

	/* Start up our task. */
	init_completion(&task_sync);
	if ((i = kernel_thread(xsa_thread, 0, 0)) < 0) {
		cleanup();
		return i;
	}
	wait_for_completion(&task_sync);

	i = devfs_register_blkdev(xsa_major, DEVICE_NAME, &xsa_fops);
	if (i < 0) {
		printk(KERN_ERR "%s: unable to register device\n", DEVICE_NAME);
		cleanup();
		return i;
	}
	if (xsa_major == 0)
		xsa_major = i;
	registered = 1;
	xsa_gendisk.major = xsa_major;

	blk_init_queue(BLK_DEFAULT_QUEUE(xsa_major), xsysace_do_request);

	read_ahead[xsa_major] = 8;	/* 8 sector (4kB) read-ahead */
	add_gendisk(&xsa_gendisk);

	/* Start with zero-sized partitions, and correctly sized unit */
	memset(xsa_sizes, 0, sizeof(xsa_sizes));
	xsa_sizes[0] = size / 2;	/* convert sectors to kilobytes */
	blk_size[xsa_major] = xsa_sizes;
	memset(xsa_hd, 0, sizeof(xsa_hd));
	xsa_hd[0].nr_sects = size;
	for (i = 0; i < (NR_HD << PARTN_BITS); i++) {
		xsa_blocksizes[i] = 1024;
		xsa_hardsectsizes[i] = 512;
		/* XSysAce_Sector{Read,Write} will allow up to 256 sectors. */
		xsa_maxsect[i] = 256;
	}
	blksize_size[xsa_major] = xsa_blocksizes;
	hardsect_size[xsa_major] = xsa_hardsectsizes;
	max_sectors[xsa_major] = xsa_maxsect;

	xsa_gendisk.nr_real = NR_HD;

	register_disk(&xsa_gendisk, MKDEV(xsa_major, 0),
		      NR_HD << PARTN_BITS, &xsa_fops, size);

	printk(KERN_INFO
	       "%s at 0x%08X mapped to 0x%08X, irq=%d, %ldKB\n",
	       DEVICE_NAME, save_BaseAddress, cfg->BaseAddress, XSA_IRQ,
	       size / 2);

	/* Hook our reset function into system's restart code. */
        /* FIXME Still has to be ported to microblaze */
	//	old_restart = ppc_md.restart; 
	//	ppc_md.restart = xsysace_restart; 

	if (proc_init())
		printk(KERN_WARNING "%s: could not register /proc interface.\n",
		       DEVICE_NAME);

	return 0;
}

static void __exit
xsysace_exit(void)
{
	cleanup();
}

EXPORT_NO_SYMBOLS;

module_init(xsysace_init);
module_exit(xsysace_exit);
