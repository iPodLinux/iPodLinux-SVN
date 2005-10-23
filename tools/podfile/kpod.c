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
    
    s->s_blocksize = 4096;
    s->s_blocksize_bits = 12; // for now
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
    
    if (!hdr->blocksize) {
	printk ("podfs: blocksize set to zero\n");
	goto out;
    }

    s->s_blocksize = hdr->blocksize;
    s->s_blocksize_bits = 0;
    while (!(s->s_blocksize & (1 << s->s_blocksize_bits))) s->s_blocksize_bits++;

    sbi->header = kmalloc (sizeof(Pod_header), GFP_KERNEL);
    memcpy (sbi->header, hdr, sizeof(Pod_header));

    brelse (bh);
    bh = sb_bread (s, 0); // with the new block size
    

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
    
    bh = sb_bread (sb, offset / sb->s_blocksize);
    if (!bh)
	return -1;
    
    avail = sb->s_blocksize - (offset & (sb->s_blocksize - 1));
    maxsize = min_t (unsigned long, count, avail);
    memcpy (dest, ((char *)bh->b_data) + (offset & (sb->s_blocksize - 1)), maxsize);
    brelse (bh);
    
    res = maxsize;
    
    while (res < count) {
	offset += maxsize;
	dest += maxsize;
	
	bh = sb_bread (sb, offset / sb->s_blocksize);
	if (!bh)
	    return -1;
	maxsize = min_t (unsigned long, count - res, sb->s_blocksize);
	memcpy (dest, bh->b_data, maxsize);
	brelse (bh);
	res += maxsize;
    }
    return res;
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
    
    offset = filp->f_pos;
    if (offset >= sbi->header->filehdr_size)
	return 0;
    
    copied = 0;
    while (offset < sbi->header->filehdr_size) {
	podfs_copyfrom (sb, &i, HEADER_SIZE + offset, FILEHDR_SIZE);
	i.filename = kmalloc (i.namelen, GFP_KERNEL);
	podfs_copyfrom (sb, i.filename, HEADER_SIZE + offset + FILEHDR_SIZE, i.namelen);

	if (filldir (dirent, i.filename, i.namelen, offset, i.offset, (S_IFREG | 0755) >> 12))
	    break;
	
	kfree (i.filename);
	offset += FILEHDR_SIZE + i.namelen;
	filp->f_pos = offset;
	copied++;
    }
    return 0;
}

static struct dentry * podfs_lookup (struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
    unsigned int offset = 0;
    struct super_block *sb = dir->i_sb;
    struct podfs_info *sbi = (struct podfs_info *)sb->s_fs_info;

    while (offset < sbi->header->filehdr_size) {
	Ar_file i;
	struct inode *ino;

	podfs_copyfrom (sb, &i, HEADER_SIZE + offset, FILEHDR_SIZE);
	if (dentry->d_name.len != i.namelen)
	    continue;

	i.filename = kmalloc (i.namelen, GFP_KERNEL);
	podfs_copyfrom (sb, i.filename, HEADER_SIZE + offset + FILEHDR_SIZE, i.namelen);

	if (memcmp (dentry->d_name.name, i.filename, i.namelen) != 0)
	    continue;

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
    return register_filesystem (&podfs_fs_type);
}
static void __exit exit_podfs_fs (void) 
{
    unregister_filesystem (&podfs_fs_type);
}

module_init (init_podfs_fs)
module_exit (exit_podfs_fs)
MODULE_LICENSE ("GPL");
