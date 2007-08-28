/*****************************************************************************/

/*
 *	exp.c -- simple expansion interface driver.
 *
 *	(C) Copyright 2001-2003, Greg Ungerer <gerg@snapgear.com>
 */

/*****************************************************************************/

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/init.h>
#include <asm/param.h>
#include <asm/uaccess.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>

/*****************************************************************************/

/*
 *	Define driver major number.
 */
#define	EXP_MAJOR	121

#define	EXP_ADDR	0xe0000000
#define	EXP_IRQ		67
#define	EXP_MSIZE	64

/*
 *	Internal driver state.
 */
int	exp_isopen;
int	exp_blocking;

/*
 *	Access to the hardware port.
 */
volatile unsigned char	*exp_datap;

/*****************************************************************************/

int exp_open(struct inode *inode, struct file *filp)
{
#ifdef DEBUG
	printk("exp_open()\n");
#endif
	exp_blocking = (filp->f_flags & O_NONBLOCK) ? 0 : 1;
	exp_isopen++;
	return(0);
}

/*****************************************************************************/

int exp_close(struct inode *inode, struct file *filp)
{
#ifdef DEBUG
	printk("exp_close()\n");
#endif
	exp_isopen = 0;
	return(0);
}

/*****************************************************************************/

ssize_t exp_read(struct file *fp, char *buf, size_t count, loff_t *ptr)
{
	volatile unsigned char	*dp;
	ssize_t			total;

#ifdef DEBUG
	printk("exp_read(buf=%x,count=%d)\n", (int) buf, count);
#endif

	if (fp->f_pos >= EXP_MSIZE)
		return(0);
	if (count > (EXP_MSIZE - fp->f_pos))
		count = EXP_MSIZE - fp->f_pos;

	for (total = 0, dp = &exp_datap[fp->f_pos]; (total < count); total++)
		put_user(*dp++, buf++);

	fp->f_pos += total;
	return(total);
}

/*****************************************************************************/

ssize_t exp_write(struct file *fp, const char *buf, size_t count, loff_t *ptr)
{
	volatile unsigned char	*dp;
	ssize_t			total;

#if 0
	printk("exp_write(buf=%x,count=%d)\n", (int) buf, count);
#endif

	if (fp->f_pos >= EXP_MSIZE)
		return(0);
	if (count > (EXP_MSIZE - fp->f_pos))
		count = EXP_MSIZE - fp->f_pos;

	for (total = 0, dp = &exp_datap[fp->f_pos]; (total < count); total++)
		get_user(*dp++, buf++);

	fp->f_pos += total;
	return(total);
}

/*****************************************************************************/

int exp_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	volatile unsigned short	*ap;
	unsigned int		val;
	int			rc = 0;

	switch (cmd) {
	case 0x5501:
		ap = (volatile unsigned short *) (MCF_MBAR + MCFSIM_PADAT);
		val = (*ap & 0xc000) >> 14;
		put_user(val, (unsigned int *) arg);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return(rc);
}

/*****************************************************************************/

void exp_isr(int irq, void *dev_id, struct pt_regs *regs)
{
	volatile unsigned long	*icrp;

#ifdef DEBUG
	printk("exp_isr(irq=%d)\n", irq);
#endif

	/* Acknowledge interrupt */
	icrp = (volatile unsigned long *) (MCF_MBAR + MCFSIM_ICR1);
	*icrp = (*icrp & 0x77777777) | 0x00800000;
}

/*****************************************************************************/

/*
 *	Exported file operations structure for driver...
 */

struct file_operations	exp_fops = {
	open: exp_open,
	release: exp_close,
	read: exp_read,
	write: exp_write,
	ioctl: exp_ioctl,
};

/*****************************************************************************/

int __init exp_init(void)
{
	volatile unsigned long	*icrp;
	int			rc;

	if ((rc = register_chrdev(EXP_MAJOR, "exp", &exp_fops)) < 0) {
		printk(KERN_WARNING "EXP: register_chrdev(major=%d) failed, "
			"rc=%d\n", EXP_MAJOR, rc);
		return(-EBUSY);
	}
	printk("EXP: Copyright (C) 2001-2003, Greg Ungerer <gerg@snapgear.com>\n");

	/* Port CS6 setup in boot loader */
	exp_datap = (volatile unsigned char *) EXP_ADDR;

	icrp = (volatile unsigned long *) (MCF_MBAR + MCFSIM_ICR1);
	*icrp = (*icrp & 0x77077777) | 0x00d00000;
	if (request_irq(EXP_IRQ, exp_isr, SA_INTERRUPT, "exp", NULL))
		printk(KERN_WARNING "EXP: request_irq(%d) failed\n", EXP_IRQ);
	return(0);
}

/*****************************************************************************/

module_init(exp_init);

/*****************************************************************************/
