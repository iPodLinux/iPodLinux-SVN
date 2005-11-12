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
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/smp_lock.h>
#include <linux/pagemap.h>
#ifdef MODULE_2_6
#include <linux/buffer_head.h>
#endif
#include <linux/vfs.h>
#include <linux/string.h>
#include <linux/stat.h>

#include <asm/semaphore.h>
#include <asm/uaccess.h>

#include "pod.h"

#ifndef MODULE_2_6
#define s_fs_info u.generic_sbp
#endif

static struct super_operations podfs_ops;
static struct inode_operations podfs_dir_inode_operations;
static struct file_operations podfs_directory_operations;
static struct address_space_operations podfs_aops;

#ifdef MODULE_2_6
static struct timespec zerotime;
#endif

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

#ifdef MODULE_2_6
static int podfs_fill_super (struct super_block *s, void *data, int silent) 
#else
static struct super_block * podfs_read_super (struct super_block *s, void *data, int silent)
#endif
{
    struct buffer_head *bh;
    struct podfs_info *sbi;
    Pod_header *hdr;
    struct inode *root;
    
    s->s_flags |= MS_RDONLY;

    sbi = kmalloc (sizeof(struct podfs_info), GFP_KERNEL);
    if (!sbi)
	return 0;
    memset (sbi, 0, sizeof(struct podfs_info));
    s->s_fs_info = sbi;

    /* No options. */
    
    sb_set_blocksize (s, 4096);
    bh = sb_bread (s, 0);
    if (!bh) {
	printk ("podfs: unable to read header\n");
	goto outnobh;
    }

    hdr = (Pod_header *)bh->b_data;
    if (memcmp (hdr->magic, PODMAGIC, 5) != 0) {
	if (!silent)
#ifdef MODULE_2_6
	    printk ("VFS: Can't find a podfs filesystem on dev "
	            "%s.\n", s->s_id);
#else
		printk ("VFS: Can't find a podfs filesystem on dev "
		        "%s.\n", kdevname(s->s_dev));
#endif
	goto out;
    }

    /* Convert endianness */
    hdr->rev = be16_to_cpu (hdr->rev);
    hdr->blocksize = be32_to_cpu (hdr->blocksize);
    hdr->file_count = be32_to_cpu (hdr->file_count);
    hdr->filehdr_size = be32_to_cpu (hdr->filehdr_size);

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
#ifdef MODULE_2_6
    root->i_mtime = root->i_atime = root->i_ctime = zerotime;
#else
    root->i_mtime = root->i_atime = root->i_ctime = 0;
#endif
    root->i_nlink = 1;
    insert_inode_hash (root);
    root->i_op = &podfs_dir_inode_operations;
    root->i_fop = &podfs_directory_operations;
    
    s->s_magic = 0x728C0943;
    s->s_root = d_alloc_root (root);
    if (!s->s_root)
	goto outiput;
    
    brelse (bh);
#ifdef MODULE_2_6
    return 0;
#else
    return s;
#endif
    
 outiput:
    iput (root);
 out:
    brelse (bh);
 outnobh:
    kfree (sbi);
    s->s_fs_info = NULL;
#ifdef MODULE_2_6
    return -EINVAL;
#else
    return 0;
#endif
}

#ifdef MODULE_2_6
#define STATFS kstatfs
#else
#define STATFS statfs
#endif

static int
podfs_statfs (struct super_block *sb, struct STATFS *buf) 
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
podfs_copyfrom (struct super_block *sb, void *dest, unsigned int offset, unsigned int count) 
{
    struct buffer_head *bh;
    unsigned int avail, maxsize, res;
    char *ptr;

    bh = sb_bread (sb, offset / sb->s_blocksize);
    if (!bh)
	return -1;
    
    avail = sb->s_blocksize - (offset & (sb->s_blocksize - 1));
    maxsize = min_t (unsigned int, count, avail);
    ptr = bh->b_data + (offset & (sb->s_blocksize - 1));
    memcpy (dest, ptr, maxsize);
    brelse (bh);
    
    res = maxsize;
    while (res < count) {
	offset += maxsize;
	dest += maxsize;
	
	bh = sb_bread (sb, offset / sb->s_blocksize);
	if (!bh)
	    return -1;
	maxsize = min_t (unsigned int, count - res, sb->s_blocksize);
	memcpy (dest, bh->b_data, maxsize);
	brelse (bh);
	res += maxsize;
    };
    return res;
}

static void
podfs_fh_fix_endian (Ar_file *i) 
{
    i->type = be16_to_cpu (i->type);
    i->offset = be32_to_cpu (i->offset);
    i->length = be32_to_cpu (i->length);
    i->blocks = be32_to_cpu (i->blocks);
    i->namelen = be16_to_cpu (i->namelen);
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
	podfs_fh_fix_endian (&i);
	i.filename = kmalloc (i.namelen + 1, GFP_KERNEL);
	podfs_copyfrom (sb, i.filename, HEADER_SIZE + offset + FILEHDR_SIZE, i.namelen);
	i.filename[i.namelen] = 0;

	if (filldir (dirent, i.filename, i.namelen, offset, i.offset, (S_IFREG | 0755) >> 12)) {
	    break;
	}
	
	kfree (i.filename);
	offset += FILEHDR_SIZE + i.namelen;
	filp->f_pos = offset;
	copied++;
    }
    return 0;
}

#ifdef MODULE_2_6
static struct dentry * podfs_lookup (struct inode *dir, struct dentry *dentry, struct nameidata *nd)
#else
static struct dentry * podfs_lookup (struct inode *dir, struct dentry *dentry)
#endif
{
	unsigned int offset = 0;
    struct super_block *sb = dir->i_sb;
    struct podfs_info *sbi = (struct podfs_info *)sb->s_fs_info;

    while (offset < sbi->header->filehdr_size) {
	Ar_file i;
	struct inode *ino;

	podfs_copyfrom (sb, &i, HEADER_SIZE + offset, FILEHDR_SIZE);
	podfs_fh_fix_endian (&i);
	if (dentry->d_name.len != i.namelen) {
	    offset += FILEHDR_SIZE + i.namelen;
	    continue;
	}

	i.filename = kmalloc (i.namelen + 1, GFP_KERNEL);
	podfs_copyfrom (sb, i.filename, HEADER_SIZE + offset + FILEHDR_SIZE, i.namelen);
	i.filename[i.namelen] = 0;
	offset += FILEHDR_SIZE + i.namelen;

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
#ifdef MODULE_2_6
	ino->i_mtime = ino->i_atime = ino->i_ctime = zerotime;
#else
	ino->i_mtime = ino->i_atime = ino->i_ctime = 0;
#endif
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
    unsigned int offset, avail, readlen;
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
	readlen = min_t (unsigned int, avail, PAGE_SIZE);
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
#ifdef MODULE_2_6
    unlock_page (page);
#else
    UnlockPage (page);
#endif
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

#ifdef MODULE_2_6
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
#else
static DECLARE_FSTYPE_DEV(podfs_fs_type, "podfs", podfs_read_super);
#endif

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
#ifndef MODULE_2_6
EXPORT_NO_SYMBOLS;
#endif
