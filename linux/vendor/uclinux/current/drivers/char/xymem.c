/****************************************************************************/

/*
 *	xymem.c  -- memory driver for DSP X/Y memory.
 *
 *	(C) Copyright 2001, Greg Ungerer (gerg@snapgear.com)
 *	(C) Copyright 2001, Lineo (www.lineo.com)
 */

/****************************************************************************/

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/init.h>
#include <linux/devfs_fs_kernel.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/pgalloc.h>

/****************************************************************************/

static char	*version = "v1.0.0";

MODULE_DESCRIPTION("XYMEM -- X/Y memory driver for SH3-DSP");
MODULE_AUTHOR("Greg Ungerer (gerg@snapgear.com)");

#ifndef XYMEM_MAJOR
#define	XYMEM_MAJOR	127
#endif

/****************************************************************************/

/*
 *	Physical addresses of xymem regions, and their virtual memory
 *	mapping addresses. The regions are setup as:
 *
 *	[0] = entire xymem region
 *	[1] = x-memory region
 *	[2] = y-memory region
 */
#define	XYMEM_REGIONS	3

struct xyregion {
	unsigned long	phys;
	unsigned int	len;
	void		*virt;
};

struct xyregion	xymem_regions[XYMEM_REGIONS] = {
	{ 0xa5000000, (16*1024*1024), NULL },
	{ 0xa5007000, (8*1024), NULL },
	{ 0xa5017000, (8*1024), NULL },
};

/****************************************************************************/

int xymem_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long	size;
	int		m;

#if 0
	printk("xymem_mmap(file=%x,vma=%x)\n", (int)file, (int)vma);
#endif

	m = MINOR(file->f_dentry->d_inode->i_rdev);
	if ((m < 0) || (m >= XYMEM_REGIONS))
		return(-EINVAL);

	size = vma->vm_end - vma->vm_start;
	if (size > xymem_regions[m].len)
		size = xymem_regions[m].len;

	vma->vm_flags |= VM_RESERVED | VM_IO;
	vma->vm_page_prot.pgprot &= ~_PAGE_CACHABLE;
	if (remap_page_range(vma->vm_start, xymem_regions[m].phys,
	    size, vma->vm_page_prot))
		return(EAGAIN);

	return(0);
}

/****************************************************************************/

ssize_t xymem_read(struct file *fp, char *buf, size_t count, loff_t *offp)
{
	unsigned long	ppos, len;
	int		m;

	m = MINOR(fp->f_dentry->d_inode->i_rdev);
	if ((m < 0) || (m >= XYMEM_REGIONS))
		return(-ENODEV);
	if (xymem_regions[m].virt == NULL)
		return(-ENODEV);
	ppos = *offp;
	if (ppos >= xymem_regions[m].len)
		return(0);

	len = xymem_regions[m].len - ppos;
	if (count < len)
		len = count;
	copy_to_user(buf, (xymem_regions[m].virt + ppos), len);
	*offp += len;

	return(len);
}

/****************************************************************************/

ssize_t xymem_write(struct file *fp, const char *buf, size_t count, loff_t *offp)
{
	unsigned long	ppos, len;
	int		m;

	m = MINOR(fp->f_dentry->d_inode->i_rdev);
	if ((m < 0) || (m >= XYMEM_REGIONS))
		return(-ENODEV);
	if (xymem_regions[m].virt == NULL)
		return(-ENODEV);
	ppos = *offp;
	if (ppos >= xymem_regions[m].len)
		return(0);

	len = xymem_regions[m].len - ppos;
	if (count < len)
		len = count;
	copy_from_user((xymem_regions[m].virt + ppos), buf, len);
	*offp += len;

	return(len);
}

/****************************************************************************/

struct file_operations	xymem_ops = {
	owner:	THIS_MODULE,
	read:	xymem_read,
	write:	xymem_write,
	mmap:	xymem_mmap,
};

/****************************************************************************/

int __init xymem_init(void)
{
	void	*vp;
	int	i;

	printk("XYMEM driver: version %s\n"
		"(C) Copyright 2001, Greg Ungerer (gerg@snapgear.com)\n"
		"(C) Copyright 2001, Lineo (www.lineo.com)\n", version);

	if ((i = devfs_register_chrdev(XYMEM_MAJOR, "xymem", &xymem_ops)))
		printk("XYMEM: failed to register device, error=%d??\n", i);

	for (i = 0; (i < XYMEM_REGIONS); i++) {
		vp = ioremap(xymem_regions[i].phys, xymem_regions[i].len);
		if ((xymem_regions[i].virt = vp) == NULL) {
			printk("XYMEM: failed to ioremap(%x)\n",
				(int) xymem_regions[i].phys);
		}
	}

	return(0);
}

/****************************************************************************/

void __exit xymem_fini(void)
{
	int	i;

	printk("XYMEM: unloading driver\n");
	devfs_unregister_chrdev(XYMEM_MAJOR, "xymem");
	for (i = 0; (i < XYMEM_REGIONS); i++) {
		if (xymem_regions[i].virt)
			iounmap(xymem_regions[i].virt);
	}
}

/****************************************************************************/

module_init(xymem_init);
module_exit(xymem_fini);

/****************************************************************************/
