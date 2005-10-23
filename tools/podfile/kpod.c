/*
 * Copyright (c) 2005 Joshua Oreman and Courtney Cavin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/pagemap.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/buffer_head.h>
#include <linux/vfs.h>
#include <linux/string.h>
#include <linux/blkdev.h>
#include <linux/stat.h>
#include <linux/smp_lock.h>

#include <asm/semaphore.h>
#include <asm/uaccess.h>

#include "pod.h"

static struct super_operations podfs_ops;
static struct inode_operations podfs_dir_inode_operations;
static struct file_operations podfs_directory_operations;
static struct address_space_operations podfs_aops;

static struct timespec zerotime;

struct podfs_info 
{
    Pod_header *header;
};

static void podfs_put_super (struct super_block *s) 
{
    struct podfs_info *sbi = (struct podfs_info *)s->s_fs_info;
    if (sbi) {
	kfree (sbi->header);
	kfree (sbi);
    }
    s->s_fs_info = NULL;
}

static int podfs_remount (struct super_block *s, int *flags, char *data) 
{
    *flags |= MS_RDONLY;
    return 0;
}

static int podfs_fill_super (struct super_block *s, void *data, int silent) 
{
    struct buffer_head *bh;
    struct podfs_info *sbi;
    Pod_header *hdr;
    struct inode *root;
    
    s->s_flags |= MS_RDONLY;

    sbi = kmalloc (sizeof(struct podfs_info), GFP_KERNEL);
    if (!sbi)
	return -ENOMEM;
    memset (sbi, 0, sizeof(struct podfs_info));
    s->s_fs_info = sbi;

    /* No options. */
    
    bh = sb_bread (s, 0);
    if (!bh) {
	printk ("podfs: unable to read header\n");
	goto outnobh;
    }

    hdr = (Pod_header *)bh->b_data;
    if (memcmp (hdr->magic, PODMAGIC, 5) != 0) {
	if (!silent)
	    printk ("VFS: Can't find a podfs filesystem on dev "
		    "%s.\n", s->s_id);
	goto out;
    }

    /* Convert endianness */
    hdr->rev = be16_to_cpu (hdr->rev);
    hdr->blocksize = be32_to_cpu (hdr->blocksize);
    hdr->file_count = be32_to_cpu (hdr->file_count);
    hdr->filehdr_size = be32_to_cpu (hdr->filehdr_size);

    printk ("<06>podfs: r=%d, b=%d, fc=%d, fhs=%d\n", hdr->rev, hdr->blocksize, hdr->file_count, hdr->filehdr_size);

    if (hdr->rev != REV) {
	printk ("podfs: revision mismatch, I understand %d but podfile is %d.\n",
		REV, hdr->rev);
	goto out;
    }
    
    if (!hdr->blocksize) {
	printk ("podfs: blocksize set to zero\n");
	goto out;
    }

    sb_set_blocksize (s, hdr->blocksize);
    s->s_maxbytes = 0xffffffff;
    
    sbi->header = kmalloc (sizeof(Pod_header), GFP_KERNEL);
    memcpy (sbi->header, hdr, sizeof(Pod_header));

    s->s_op = &podfs_ops;
    root = new_inode (s);
    if (!root)
	goto out;

    root->i_mode = S_IFDIR | 0755;
    root->i_uid = 0;
    root->i_size = 1;
    root->i_blocks = 1;
    root->i_blksize = s->s_blocksize;
    root->i_gid = 0;
    root->i_mtime = root->i_atime = root->i_ctime = zerotime;
    root->i_nlink = 1;
    insert_inode_hash (root);
    root->i_op = &podfs_dir_inode_operations;
    root->i_fop = &podfs_directory_operations;
    
    s->s_magic = 0x728C0943;
    s->s_root = d_alloc_root (root);
    if (!s->s_root)
	goto outiput;
    
    brelse (bh);
    return 0;
    
 outiput:
    iput (root);
 out:
    brelse (bh);
 outnobh:
    kfree (sbi);
    s->s_fs_info = NULL;
    return -EINVAL;
}

static int
podfs_statfs (struct super_block *sb, struct kstatfs *buf) 
{
    struct podfs_info *sbi = (struct podfs_info *)sb->s_fs_info;

    buf->f_type = 0x728C0943;
    buf->f_bsize = sb->s_blocksize;
    buf->f_bfree = buf->f_bavail = 0;
    buf->f_blocks = sbi->header->file_count; // XXX replace with something better
    buf->f_files = sbi->header->file_count;
    buf->f_ffree = 0;
    buf->f_namelen = 65536;
    return 0;
}

/* Extraordinarily similar to romfs_copyfrom ;-) */
static int
podfs_copyfrom (struct super_block *sb, void *dest, unsigned long offset, unsigned long count) 
{
    struct buffer_head *bh;
    unsigned long avail, maxsize, res;

    printk ("<0> podfs_copyfrom: %p, %p, %d, %d\n", sb, dest, offset, count);
    printk ("<0> first block: %d\n", offset / sb->s_blocksize);
    bh = sb_bread (sb, offset / sb->s_blocksize);
    if (!bh)
	return -1;
    
    avail = sb->s_blocksize - (offset & (sb->s_blocksize - 1));
    maxsize = min_t (unsigned long, count, avail);
    printk ("<0> will copy %d bytes\n", maxsize);
    memcpy (dest, ((char *)bh->b_data) + (offset & (sb->s_blocksize - 1)), maxsize);
    brelse (bh);
    
    res = maxsize;
    printk ("<0> %d bytes left\n", count - res);
    while (res < count) {
	offset += maxsize;
	dest += maxsize;
	
	bh = sb_bread (sb, offset / sb->s_blocksize);
	if (!bh)
	    return -1;
	maxsize = min_t (unsigned long, count - res, sb->s_blocksize);
	memcpy (dest, bh->b_data, maxsize);
	printk ("<0> %d more bytes copied from blk %d to %p, %d left\n", maxsize,
		offset / sb->s_blocksize, dest, count - res - maxsize);
	brelse (bh);
	res += maxsize;
    };
    printk ("<0> %d bytes copied, returning\n", res);
    return res;
}

static void
podfs_fh_fix_endian (Ar_file *i) 
{
    printk ("<0>big endian values: t=%x o=%x l=%x b=%x nl=%x\n",
	    i->type, i->offset, i->length, i->blocks, i->namelen);
    
    i->type = be16_to_cpu (i->type);
    i->offset = be32_to_cpu (i->offset);
    i->length = be32_to_cpu (i->length);
    i->blocks = be32_to_cpu (i->blocks);
    i->namelen = be16_to_cpu (i->namelen);

    printk ("<0>lil endian values: t=%x o=%x l=%x b=%x nl=%x\n",
	    i->type, i->offset, i->length, i->blocks, i->namelen);
}

static int
podfs_readdir (struct file *filp, void *dirent, filldir_t filldir) 
{
    struct inode *inode = filp->f_dentry->d_inode;
    struct super_block *sb = inode->i_sb;
    struct podfs_info *sbi = (struct podfs_info *)sb->s_fs_info;
    unsigned int offset;
    int copied;
    Ar_file i;
    
    printk ("<0> podfs_readdir %p, %p, %p\n", filp, dirent, filldir);

    offset = filp->f_pos;
    if (offset >= sbi->header->filehdr_size)
	return 0;

    printk ("<0> f->offset = %d\n", offset);
    
    copied = 0;
    while (offset < sbi->header->filehdr_size) {
	podfs_copyfrom (sb, &i, HEADER_SIZE + offset, FILEHDR_SIZE);
	podfs_fh_fix_endian (&i);
	printk ("<0> t=%hd o=%d l=%d b=%d nl=%d\n", i.type, i.offset, i.length,
		i.blocks, i.namelen);
	i.filename = kmalloc (i.namelen + 1, GFP_KERNEL);
	podfs_copyfrom (sb, i.filename, HEADER_SIZE + offset + FILEHDR_SIZE, i.namelen);
	i.filename[i.namelen] = 0;
	printk ("<0> name = %s (len %d = %d)\n", i.filename, strlen (i.filename), i.namelen);

	if (filldir (dirent, i.filename, i.namelen, offset, i.offset, (S_IFREG | 0755) >> 12)) {
	    printk ("<0> filldir failed\n");
	    break;
	}
	
	kfree (i.filename);
	offset += FILEHDR_SIZE + i.namelen;
	filp->f_pos = offset;
	copied++;
	printk ("<0> new f->offset: %d,  %d dentries copied so far\n", offset,
		copied);
    }
    printk ("<0> done, exiting\n");
    return 0;
}

static struct dentry * podfs_lookup (struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
    unsigned int offset = 0;
    struct super_block *sb = dir->i_sb;
    struct podfs_info *sbi = (struct podfs_info *)sb->s_fs_info;

    printk ("<0> podfs_lookup %p, %p, %p\n", dir, dentry, nd);

    while (offset < sbi->header->filehdr_size) {
	Ar_file i;
	struct inode *ino;

	printk ("<0> offset = %d, fhsz = %d\n", offset, sbi->header->filehdr_size);

	podfs_copyfrom (sb, &i, HEADER_SIZE + offset, FILEHDR_SIZE);
	podfs_fh_fix_endian (&i);
	printk ("<0> t=%hd o=%d l=%d b=%d nl=%d\n", i.type, i.offset, i.length,
		i.blocks, i.namelen);
	offset += FILEHDR_SIZE + i.namelen;
	if (dentry->d_name.len != i.namelen)
	    continue;

	i.filename = kmalloc (i.namelen + 1, GFP_KERNEL);
	podfs_copyfrom (sb, i.filename, HEADER_SIZE + offset + FILEHDR_SIZE, i.namelen);
	i.filename[i.namelen] = 0;
	printk ("<0> name = %s (len %d = %d)\n", i.filename, strlen (i.filename), i.namelen);

	if (memcmp (dentry->d_name.name, i.filename, i.namelen) != 0) {
	    kfree (i.filename);
	    continue;
	}

	ino = new_inode (sb);
	ino->i_mode = S_IFREG | 0755;
	ino->i_uid = 0;
	ino->i_size = i.length;
	ino->i_blocks = (i.length - 1) / 512 + 1;
	ino->i_blksize = sb->s_blocksize;
	ino->i_gid = 0;
	ino->i_mtime = ino->i_atime = ino->i_ctime = zerotime;
	ino->i_ino = i.offset;
	ino->i_nlink = 1;
	insert_inode_hash (ino);
	ino->i_fop = &generic_ro_fops;
	ino->i_data.a_ops = &podfs_aops;
	d_add (dentry, ino);
	printk ("<0> match! ino=%p\n", ino);
	return NULL;
    }
    d_add (dentry, NULL);
    return NULL;
}

/* Copied almost exactly from romfs_readpage. */
static int podfs_readpage (struct file *file, struct page *page) 
{
    struct inode *inode = page->mapping->host;
    unsigned long offset, avail, readlen;
    void *buf;
    int result = -EIO;
    
    page_cache_get (page);
    lock_kernel();
    buf = kmap (page);
    if (!buf)
	goto err_out;
    
    offset = page->index << PAGE_CACHE_SHIFT;
    if (offset < inode->i_size) {
	avail = inode->i_size - offset;
	readlen = min_t (unsigned long, avail, PAGE_SIZE);
	if (podfs_copyfrom (inode->i_sb, buf, inode->i_ino + offset, readlen) == readlen) {
	    if (readlen < PAGE_SIZE) {
		memset (buf + readlen, 0, PAGE_SIZE - readlen);
	    }
	    SetPageUptodate (page);
	    result = 0;
	}
    }

    if (result) {
	memset (buf, 0, PAGE_SIZE);
	SetPageError (page);
    }
    flush_dcache_page (page);
    unlock_page (page);
    kunmap (page);
    
 err_out:
    page_cache_release (page);
    unlock_kernel();

    return result;
}


/* Operations: */
static struct address_space_operations podfs_aops = {
    .readpage = podfs_readpage,
};

static struct file_operations podfs_directory_operations = {
    .llseek  = generic_file_llseek,
    .read    = generic_read_dir,
    .readdir = podfs_readdir,
};

static struct inode_operations podfs_dir_inode_operations = {
    .lookup  = podfs_lookup,
};

static struct super_operations podfs_ops = {
    .put_super  = podfs_put_super,
    .remount_fs = podfs_remount,
    .statfs     = podfs_statfs,
};

static struct super_block *podfs_get_sb (struct file_system_type *fs_type,
					 int flags, const char *dev_name,
					 void *data) 
{
    return get_sb_bdev (fs_type, flags, dev_name, data, podfs_fill_super);
}

static struct file_system_type podfs_fs_type = {
    .owner    = THIS_MODULE,
    .name     = "podfs",
    .get_sb   = podfs_get_sb,
    .kill_sb  = kill_block_super,
    .fs_flags = FS_REQUIRES_DEV,
};

static int __init init_podfs_fs (void) 
{
    int ret = register_filesystem (&podfs_fs_type);
    if (ret)
	printk ("podfs: couldn't register filesystem (%d)\n", ret);
    else
	printk ("podfs: initialized\n");
    return ret;
}
static void __exit exit_podfs_fs (void) 
{
    unregister_filesystem (&podfs_fs_type);
    printk ("podfs: unloaded\n");
}

module_init (init_podfs_fs)
module_exit (exit_podfs_fs)
MODULE_LICENSE ("GPL");
