/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2001, 2002 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@cambridge.redhat.com>
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 *
 * $Id: super-v24.c,v 1.1 2003/08/28 11:55:10 davidm Exp $
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/jffs2.h>
#include <linux/pagemap.h>
#include <linux/mtd/mtd.h>
#include <linux/interrupt.h>
#include "nodelist.h"

#ifndef MTD_BLOCK_MAJOR
#define MTD_BLOCK_MAJOR 31
#endif

void jffs2_put_super (struct super_block *);

static struct super_operations jffs2_super_operations =
{
	read_inode:	jffs2_read_inode,
	put_super:	jffs2_put_super,
	write_super:	jffs2_write_super,
	statfs:		jffs2_statfs,
	remount_fs:	jffs2_remount_fs,
	clear_inode:	jffs2_clear_inode
};


static struct super_block *jffs2_read_super(struct super_block *sb, void *data, int silent)
{
	struct jffs2_sb_info *c;
	int ret;

	D1(printk(KERN_DEBUG "jffs2: read_super for device %s\n", kdevname(sb->s_dev)));

	if (major(sb->s_dev) != MTD_BLOCK_MAJOR) {
		if (!silent)
			printk(KERN_DEBUG "jffs2: attempt to mount non-MTD device %s\n", kdevname(sb->s_dev));
		return NULL;
	}

	c = JFFS2_SB_INFO(sb);
	memset(c, 0, sizeof(*c));

	sb->s_op = &jffs2_super_operations;

	c->mtd = get_mtd_device(NULL, minor(sb->s_dev));
	if (!c->mtd) {
		D1(printk(KERN_DEBUG "jffs2: MTD device #%u doesn't appear to exist\n", minor(sb->s_dev)));
		return NULL;
	}

	ret = jffs2_do_fill_super(sb, data, silent);
	if (ret) {
		put_mtd_device(c->mtd);
		return NULL;
	}

	return sb;
}

void jffs2_put_super (struct super_block *sb)
{
	struct jffs2_sb_info *c = JFFS2_SB_INFO(sb);

	D2(printk(KERN_DEBUG "jffs2: jffs2_put_super()\n"));


	if (!(sb->s_flags & MS_RDONLY))
		jffs2_stop_garbage_collect_thread(c);
	down(&c->alloc_sem);
	jffs2_flush_wbuf(c, 1);
	up(&c->alloc_sem);
	jffs2_free_ino_caches(c);
	jffs2_free_raw_node_refs(c);
	kfree(c->blocks);
	if (c->mtd->sync)
		c->mtd->sync(c->mtd);
	put_mtd_device(c->mtd);
	
	D1(printk(KERN_DEBUG "jffs2_put_super returning\n"));
}

static DECLARE_FSTYPE_DEV(jffs2_fs_type, "jffs2", jffs2_read_super);

static int __init init_jffs2_fs(void)
{
	int ret;

	printk(KERN_NOTICE "JFFS2 version 2.1. (C) 2001, 2002 Red Hat, Inc., designed by Axis Communications AB.\n");

#ifdef JFFS2_OUT_OF_KERNEL
	/* sanity checks. Could we do these at compile time? */
	if (sizeof(struct jffs2_sb_info) > sizeof (((struct super_block *)NULL)->u)) {
		printk(KERN_ERR "JFFS2 error: struct jffs2_sb_info (%d bytes) doesn't fit in the super_block union (%d bytes)\n", 
		       sizeof(struct jffs2_sb_info), sizeof (((struct super_block *)NULL)->u));
		return -EIO;
	}

	if (sizeof(struct jffs2_inode_info) > sizeof (((struct inode *)NULL)->u)) {
		printk(KERN_ERR "JFFS2 error: struct jffs2_inode_info (%d bytes) doesn't fit in the inode union (%d bytes)\n", 
		       sizeof(struct jffs2_inode_info), sizeof (((struct inode *)NULL)->u));
		return -EIO;
	}
#endif
	ret = jffs2_zlib_init();
	if (ret) {
		printk(KERN_ERR "JFFS2 error: Failed to initialise zlib workspaces\n");
		goto out;
	}
	ret = jffs2_create_slab_caches();
	if (ret) {
		printk(KERN_ERR "JFFS2 error: Failed to initialise slab caches\n");
		goto out_zlib;
	}
	ret = register_filesystem(&jffs2_fs_type);
	if (ret) {
		printk(KERN_ERR "JFFS2 error: Failed to register filesystem\n");
		goto out_slab;
	}
	return 0;

 out_slab:
	jffs2_destroy_slab_caches();
 out_zlib:
	jffs2_zlib_exit();
 out:

	return ret;
}

static void __exit exit_jffs2_fs(void)
{
	jffs2_destroy_slab_caches();
	jffs2_zlib_exit();
	unregister_filesystem(&jffs2_fs_type);
}

module_init(init_jffs2_fs);
module_exit(exit_jffs2_fs);

MODULE_DESCRIPTION("The Journalling Flash File System, v2");
MODULE_AUTHOR("Red Hat, Inc.");
MODULE_LICENSE("GPL"); // Actually dual-licensed, but it doesn't matter for 
		       // the sake of this tag. It's Free Software.
