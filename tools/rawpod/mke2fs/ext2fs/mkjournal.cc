/*
 * mkjournal.c --- make a journal for a filesystem
 *
 * Copyright (C) 2000 Theodore Ts'o.
 * 
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h> /* for ntohl and friends */

#include "ext2.h"
#include "e2p.h"
#include "ext2fs.h"
#include "jfs_user.h"

/*
 * This function automatically sets up the journal superblock and
 * returns it as an allocated block.
 */
errcode_t ext2fs_create_journal_superblock(ext2_filsys fs,
					   __u32 size, int flags,
					   char  **ret_jsb)
{
	errcode_t		retval;
	journal_superblock_t	*jsb;

	if (size < 1024)
		return EXT2_ET_JOURNAL_TOO_SMALL;

	if ((retval = ext2fs_get_mem(fs->blocksize, &jsb)))
		return retval;

	memset (jsb, 0, fs->blocksize);

	jsb->s_header.h_magic = htonl(JFS_MAGIC_NUMBER);
	if (flags & EXT2_MKJOURNAL_V1_SUPER)
		jsb->s_header.h_blocktype = htonl(JFS_SUPERBLOCK_V1);
	else
		jsb->s_header.h_blocktype = htonl(JFS_SUPERBLOCK_V2);
	jsb->s_blocksize = htonl(fs->blocksize);
	jsb->s_maxlen = htonl(size);
	jsb->s_nr_users = htonl(1);
	jsb->s_first = htonl(1);
	jsb->s_sequence = htonl(1);
	memcpy(jsb->s_uuid, fs->super->s_uuid, sizeof(fs->super->s_uuid));
	/*
	 * If we're creating an external journal device, we need to
	 * adjust these fields.
	 */
	if (fs->super->s_feature_incompat &
	    EXT3_FEATURE_INCOMPAT_JOURNAL_DEV) {
		jsb->s_nr_users = 0;
		if (fs->blocksize == 1024)
			jsb->s_first = htonl(3);
		else
			jsb->s_first = htonl(2);
	}

	*ret_jsb = (char *) jsb;
	return 0;
}

/*
 * This function writes a journal using POSIX routines.  It is used
 * for creating external journals and creating journals on live
 * filesystems.
 */
static errcode_t write_journal_file(ext2_filsys fs, char *filename,
				    blk_t size, int flags)
{
        return ENOSYS;
}

/*
 * Helper function for creating the journal using direct I/O routines
 */
struct mkjournal_struct {
	int		num_blocks;
	int		newblocks;
	char		*buf;
	errcode_t	err;
};

static int mkjournal_proc(ext2_filsys	fs,
			   blk_t	*blocknr,
			   e2_blkcnt_t	blockcnt,
			   blk_t	ref_block EXT2FS_ATTR((unused)),
			   int		ref_offset EXT2FS_ATTR((unused)),
			   void		*priv_data)
{
	struct mkjournal_struct *es = (struct mkjournal_struct *) priv_data;
	blk_t	new_blk;
	static blk_t	last_blk = 0;
	errcode_t	retval;
	
	if (*blocknr) {
		last_blk = *blocknr;
		return 0;
	}
	retval = ext2fs_new_block(fs, last_blk, 0, &new_blk);
	if (retval) {
		es->err = retval;
		return BLOCK_ABORT;
	}
	if (blockcnt > 0)
		es->num_blocks--;

	es->newblocks++;
	retval = io_channel_write_blk(fs->io, new_blk, 1, es->buf);

	if (blockcnt == 0)
		memset(es->buf, 0, fs->blocksize);

	if (retval) {
		es->err = retval;
		return BLOCK_ABORT;
	}
	*blocknr = new_blk;
	last_blk = new_blk;
	ext2fs_block_alloc_stats(fs, new_blk, +1);

	if (es->num_blocks == 0)
		return (BLOCK_CHANGED | BLOCK_ABORT);
	else
		return BLOCK_CHANGED;
	
}

/*
 * This function creates a journal using direct I/O routines.
 */
static errcode_t write_journal_inode(ext2_filsys fs, ext2_ino_t journal_ino,
				     blk_t size, int flags)
{
	char			*buf;
	errcode_t		retval;
	struct ext2_inode	inode;
	struct mkjournal_struct	es;

	if ((retval = ext2fs_create_journal_superblock(fs, size, flags, &buf)))
		return retval;
	
	if ((retval = ext2fs_read_bitmaps(fs)))
		return retval;

	if ((retval = ext2fs_read_inode(fs, journal_ino, &inode)))
		return retval;

	if (inode.i_blocks > 0)
		return EEXIST;

	es.num_blocks = size;
	es.newblocks = 0;
	es.buf = buf;
	es.err = 0;

	retval = ext2fs_block_iterate2(fs, journal_ino, BLOCK_FLAG_APPEND,
				       0, mkjournal_proc, &es);
	if (es.err) {
		retval = es.err;
		goto errout;
	}

	if ((retval = ext2fs_read_inode(fs, journal_ino, &inode)))
		goto errout;

 	inode.i_size += fs->blocksize * size;
	inode.i_blocks += (fs->blocksize / 512) * es.newblocks;
	inode.i_mtime = inode.i_ctime = time(0);
	inode.i_links_count = 1;
	inode.i_mode = LINUX_S_IFREG | 0600;

	if ((retval = ext2fs_write_inode(fs, journal_ino, &inode)))
		goto errout;
	retval = 0;

	memcpy(fs->super->s_jnl_blocks, inode.i_block, EXT2_N_BLOCKS*4);
	fs->super->s_jnl_blocks[16] = inode.i_size;
	fs->super->s_jnl_backup_type = EXT3_JNL_BACKUP_BLOCKS;
	ext2fs_mark_super_dirty(fs);

errout:
	ext2fs_free_mem(&buf);
	return retval;
}

/*
 * This function adds a journal inode to a filesystem, using either
 * POSIX routines if the filesystem is mounted, or using direct I/O
 * functions if it is not.
 */
errcode_t ext2fs_add_journal_inode(ext2_filsys fs, blk_t size, int flags)
{
	errcode_t		retval;
	ext2_ino_t		journal_ino;
	struct stat		st;
	char			jfile[1024];
	int			fd, mount_flags, f;

	if ((retval = ext2fs_check_mount_point(fs->device_name, &mount_flags,
					       jfile, sizeof(jfile)-10)))
		return retval;

        journal_ino = EXT2_JOURNAL_INO;
        if ((retval = write_journal_inode(fs, journal_ino,
                                          size, flags)))
            return retval;
	
	fs->super->s_journal_inum = journal_ino;
	fs->super->s_journal_dev = 0;
	memset(fs->super->s_journal_uuid, 0,
	       sizeof(fs->super->s_journal_uuid));
	fs->super->s_feature_compat |= EXT3_FEATURE_COMPAT_HAS_JOURNAL;

	ext2fs_mark_super_dirty(fs);
	return 0;
errout:
	return retval;
}

#ifdef DEBUG
main(int argc, char **argv)
{
	errcode_t	retval;
	char		*device_name;
	ext2_filsys 	fs;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s filesystem\n", argv[0]);
		exit(1);
	}
	device_name = argv[1];
	
	retval = ext2fs_open (device_name, EXT2_FLAG_RW, 0, 0,
			      unix_io_manager, &fs);
	if (retval) {
		com_err(argv[0], retval, "while opening %s", device_name);
		exit(1);
	}

	retval = ext2fs_add_journal_inode(fs, 1024);
	if (retval) {
		com_err(argv[0], retval, "while adding journal to %s",
			device_name);
		exit(1);
	}
	retval = ext2fs_flush(fs);
	if (retval) {
		printf("Warning, had trouble writing out superblocks.\n");
	}
	ext2fs_close(fs);
	exit(0);
	
}
#endif
