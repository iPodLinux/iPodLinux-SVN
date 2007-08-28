/*
 *  linux/drivers/char/de2fpga.c -- FPGA driver for the DragonEngine
 *
 *	Copyright (C) 2003 Georges Menie
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <asm/MC68VZ328.h>

#include <linux/de2fpga.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Georges Menie");
MODULE_DESCRIPTION("Spartan II FPGA driver for the DragonEngine");

#define MDL_MAJOR 61
#define MDL_NAME "de2fpga"

#undef PDEBUG
#ifdef MDL_DEBUG
#ifdef __KERNEL__
#define PDEBUG(fmt,args...) printk(KERN_ALERT MDL_NAME ": " fmt, ##args);
#else
#define PDEBUG(fmt,args...) fprintf(stderr, MDL_NAME ": " fmt, ##args);
#endif
#else
#define PDEBUG(fmt,args...)		/* void */
#endif

#undef IPDEBUG
#define IPDEBUG(fmt,args...)	/* void */

static int major = MDL_MAJOR;
MODULE_PARM(major, "i");

static struct device_tag {
	long addr;
	spinlock_t lock;
	int readerc;
	int writerc;
	int status;
	int state;
	int offs;
	int prog;
	char *file;
	char *part;
	char *date;
	char *time;
	char buf[DE2FPGA_MAX_TAGLEN];
} drvinfo = {
	0x08000000,					/* fpga registers start address */
};

static const unsigned char bs_head[] =
	{ 0, 9, 15, 240, 15, 240, 15, 240, 15, 240, 0, 0, 1 };

#define BIT(x) (1<<(x))

static inline void de2fpga_setup_hw(void)
{
	/* program the PROG pin (PB7) as output, and set high */
	PBSEL |= BIT(7);
	PBDIR |= BIT(7);
	PBDATA |= BIT(7);

	/* program the CS/WR pin (PF7) as output, and set high */
	PFSEL |= BIT(7);
	PFDIR |= BIT(7);
	PFDATA |= BIT(7);
	
	/* program the DONE pin (PM5) as input */
	PMSEL |= BIT(5);
	PMDIR &= ~BIT(5);

	/* Select PF2 pin function as CLKO output pin, disabled */
	PFSEL &= ~BIT(2);
	PLLCR |= PLLCR_CLKEN;
}

static inline void de2fpa_fpga_select(void)
{
	/* CS low */
	PFDATA &= ~BIT(7);
}

static inline void de2fpa_fpga_deselect(void)
{
	/* CS high */
	PFDATA |= BIT(7);
}

static inline int de2fpa_start_prog(void)
{
	int i, ret;

	de2fpa_fpga_select();

	/* pulse PROG low */
	PBDATA &= ~BIT(7);
	for (i = 0; i < 5; ++i);
	PBDATA |= BIT(7);

	/* Check status */
	for (i = 0; i < 20; ++i) {
		ret = PMDATA & BIT(5);
		if (ret == 0)
			break;
	}
	if (ret) {
		/* error, CS high */
		de2fpa_fpga_deselect();
		return 1;
	}

	return 0;
}

static inline void de2fpa_prog(unsigned char byte)
{
	/* reverse the bits order */
	byte = ((byte & 0xAA) >>  1) | ((byte & 0x55) <<  1);
	byte = ((byte & 0xCC) >>  2) | ((byte & 0x33) <<  2);
	byte = (byte >> 4) | (byte << 4);

	BYTE_REF(drvinfo.addr+1) = byte;
}

static inline int de2fpa_end_prog(void)
{
	int i, ret;

	/* Check status */
	for (i = 0; i < 20; ++i) {
		ret = PMDATA & BIT(5);
		if (ret != 0)
			break;
	}

	de2fpa_fpga_deselect();

	if (ret != 0) {
		/* Enable the CLKO output buffer */
		PLLCR &= ~PLLCR_CLKEN;
		return 0;
	}

	return 1;
}

static void de2fpga_reset_prog_data(void)
{
	drvinfo.file = drvinfo.part = drvinfo.buf;
	drvinfo.date = drvinfo.time = drvinfo.buf;
	drvinfo.buf[0] = '\0';
	drvinfo.status = DE2FPGA_STS_UNKNOWN;
}

static void de2fpga_init_state(void)
{
	drvinfo.state = 0;
	drvinfo.offs = 0;
	drvinfo.prog = 0;
}

static int de2fpga_load_bitstream(int byte)
{
	static int tagsize;
	static int bitsize;
	static char *tag;

	switch (drvinfo.state) {
	case 0:					/* read header */
		if (drvinfo.offs >= sizeof bs_head
			|| byte != bs_head[drvinfo.offs])
			return DE2FPGA_STS_HDR_ERROR;
		if (drvinfo.offs == sizeof bs_head - 1) {
			drvinfo.state = 1;
			tag = drvinfo.buf;
		}
		break;
	case 1:					/* read tag letter */
		if (byte >= 'a' && byte <= 'd') {
			drvinfo.state = 2;
			switch (byte) {
			case 'a':
				drvinfo.file = tag;
				break;
			case 'b':
				drvinfo.part = tag;
				break;
			case 'c':
				drvinfo.date = tag;
				break;
			case 'd':
				drvinfo.time = tag;
				break;
			}
		} else if (byte == 'e') {
			drvinfo.state = 5;
		} else {
			return DE2FPGA_STS_TGL_ERROR;
		}
		break;
	case 2:					/* read tag size MSB */
		drvinfo.state = 3;
		tagsize = byte << 8;
		break;
	case 3:					/* read tag size LSB */
		drvinfo.state = 4;
		tagsize += byte;
		break;
	case 4:					/* read tag content */
		if (tag - drvinfo.buf >= DE2FPGA_MAX_TAGLEN)
			return DE2FPGA_STS_TGC_ERROR;
		*tag++ = byte;
		--tagsize;
		if (tagsize == 0) {
			drvinfo.state = 1;
		}
		break;
	case 5:					/* read bitstream size MSB */
		drvinfo.state = 6;
		bitsize = byte << 24;
		break;
	case 6:
		drvinfo.state = 7;
		bitsize += byte << 16;
		break;
	case 7:
		drvinfo.state = 8;
		bitsize += byte << 8;
		break;
	case 8:					/* read bitstream size LSB */
		drvinfo.state = 9;
		bitsize += byte;
		if (bitsize == 0)
			return DE2FPGA_STS_NSZ_ERROR;
		break;
	case 9:					/* start program bitstream */
		drvinfo.state = 10;
		drvinfo.status = DE2FPGA_STS_UNKNOWN;
		if (de2fpa_start_prog())
			return DE2FPGA_STS_PRG_ERROR;
		drvinfo.prog = 1;
		/* Fall through */
	case 10:				/* program bitstream */
		de2fpa_prog(byte);
		--bitsize;
		if (bitsize == 0) {
			drvinfo.state = -1;
			if (de2fpa_end_prog())
				return DE2FPGA_STS_EPR_ERROR;
			drvinfo.prog = 0;
			drvinfo.status = DE2FPGA_STS_OK;
		}
		break;
	default:
		return DE2FPGA_STS_FIL_ERROR;
	}
	++drvinfo.offs;
	return 0;
}

static ssize_t de2fpga_write(struct file *filp, const char *buf,
							 size_t count, loff_t * offp)
{
	int i, e;
	unsigned char c;

	/* Can't seek on this device */
	if (offp != &filp->f_pos)
		return -ESPIPE;

	for (i = 0; i < count; ++i) {
		if (get_user(c, buf++)) {
			PDEBUG("get_user error\n");
			if (drvinfo.prog) {
				de2fpga_reset_prog_data();
				de2fpga_init_state();
				drvinfo.status = DE2FPGA_STS_DRV_ERROR;
			}
			return -EFAULT;
		}
		if ((e = de2fpga_load_bitstream(c)) != 0) {
			PDEBUG("load_bitstream error %d @ %d\n", e, drvinfo.offs);
			if (drvinfo.prog) {
				de2fpga_reset_prog_data();
				de2fpga_init_state();
			}
			drvinfo.status = e;
			return -EFAULT;
		}
	}

	*offp += count;

	return count;
}

static int de2fpga_mmap(struct file *filp, struct vm_area_struct *vma)
{
	/* this is uClinux (no MMU) specific code */
	vma->vm_flags |= VM_RESERVED;
	vma->vm_start = drvinfo.addr;

	return 0;
}

static int de2fpga_ioctl(struct inode *inode, struct file *filp,
						 unsigned int cmd, unsigned long arg)
{
	int i;
	char *p_in;
	char *p_out;
	struct de2fpga_info param;

	switch (cmd) {

	case DE2FPGA_RESET:
		de2fpga_setup_hw();
		de2fpga_reset_prog_data();
		de2fpga_init_state();
		/* fall through */
	case DE2FPGA_PARAMS_GET:
		if (verify_area(VERIFY_WRITE, (char *) arg,
						sizeof(struct de2fpga_info)))
			return -EBADRQC;

		param.status = drvinfo.status;
		param.file = drvinfo.file - drvinfo.buf;
		param.part = drvinfo.part - drvinfo.buf;
		param.date = drvinfo.date - drvinfo.buf;
		param.time = drvinfo.time - drvinfo.buf;
		memcpy(param.buf, drvinfo.buf, DE2FPGA_MAX_TAGLEN);

		p_in = (char *) &param;
		p_out = (char *) arg;
		for (i = 0; i < sizeof(struct de2fpga_info); i++)
			put_user(p_in[i], p_out + i);

		return 0;

	default:
		break;
	}
	return -ENOIOCTLCMD;
}

static int de2fpga_open(struct inode *inode, struct file *filp)
{
	/*
	 * loosy reader/writer protection
	 *
	 * unrelated process may open the device RDONLY multiple times
	 * and RDWR/WRONLY if there is no readers and no writers.
	 *
	 * forked process will be able to share an opened device,
	 * it is supposed the programmer knows what he is doing.
	 *
	 * Note: readerc is not decremented, so it is *not* a
	 * true reader count.
	 */
	spin_lock(&drvinfo.lock);
	if ((filp->f_flags & O_ACCMODE) == O_RDONLY) {
		if (drvinfo.writerc) {
			spin_unlock(&drvinfo.lock);
			return -EBUSY;
		}
		++drvinfo.readerc;
	}
	else {
		if (drvinfo.readerc || drvinfo.writerc) {
			spin_unlock(&drvinfo.lock);
			return -EBUSY;
		}
		++drvinfo.writerc;
	}
	spin_unlock(&drvinfo.lock);

	return 0;
}

static int de2fpga_release(struct inode *inode, struct file *filp)
{
	if (drvinfo.prog) {
		de2fpa_fpga_deselect();
		de2fpga_reset_prog_data();
		drvinfo.status = DE2FPGA_STS_REL_ERROR;
	}
	de2fpga_init_state();
	drvinfo.readerc = 0;
	drvinfo.writerc = 0;
	
	return 0;
}

struct file_operations de2fpga_fops = {
	owner:THIS_MODULE,
	write:de2fpga_write,
	mmap:de2fpga_mmap,
	ioctl:de2fpga_ioctl,
	open:de2fpga_open,
	release:de2fpga_release,
};

static int de2fpga_proc_read(char *buf, char **start, off_t offset,
							 int count, int *eof, void *data)
{
	int len = 0;

	len += sprintf(buf + len, "FPGA status: ");
	if (drvinfo.status == DE2FPGA_STS_OK) {
		len += sprintf(buf + len, "ok\n");
		len += sprintf(buf + len, "file: %s\n", drvinfo.file);
		len += sprintf(buf + len, "part: %s\n", drvinfo.part);
		len += sprintf(buf + len, "date: %s\n", drvinfo.date);
		len += sprintf(buf + len, "time: %s\n", drvinfo.time);
	}
	else if (drvinfo.status == DE2FPGA_STS_UNKNOWN) {
		len += sprintf(buf + len, "unknown\n");
	}
	else {
		len += sprintf(buf + len, "error %d\n", drvinfo.status);
	}

	*eof = 1;
	return len;
}

static int __init de2fpga_init(void)
{
	int result;

	result = register_chrdev(major, MDL_NAME, &de2fpga_fops);
	if (result < 0) {
		printk(KERN_WARNING "%s: can't get major %d\n", MDL_NAME, major);
		return result;
	}
	if (major == 0)
		major = result;

	create_proc_read_entry(MDL_NAME, 0, NULL, de2fpga_proc_read, NULL);

	printk(KERN_INFO "%s: Spartan II fpga driver installed (c,%d,0).\n", MDL_NAME,
		   major);

	de2fpga_setup_hw();
	de2fpga_reset_prog_data();
	de2fpga_init_state();
	spin_lock_init(&drvinfo.lock);

	return 0;
}

static void __exit de2fpga_cleanup(void)
{
	remove_proc_entry(MDL_NAME, NULL);
	unregister_chrdev(major, MDL_NAME);
}

module_init(de2fpga_init);
module_exit(de2fpga_cleanup);
