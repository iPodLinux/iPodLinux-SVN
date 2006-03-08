/* Modified 2006-03-03 by Joshua Oreman to work with the rawpod interface. */

/*
 * mke2fs.c - Make a ext2fs filesystem.
 * 
 * Copyright (C) 1994, 1995, 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

/* Usage: mke2fs [options] device
 * 
 * The device may be a block device or a image of one, but this isn't
 * enforced (but it's not much fun on a character device :-). 
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "ext2.h"
#include "com_err.h"
#include "uuid.h"
#include "e2p.h"
#include "ext2fs.h"

#define _(x) x
#define E2FSPROGS_VERSION "1.35-rawpod"
#define E2FSPROGS_DATE "02-Mar-2006"

#define STRIDE_LENGTH 8

#ifndef __sparc__
#define ZAP_BOOTBLOCK
#endif

const char * program_name = "mke2fs";
VFS::Device *device;

/* Command line options */
int	cflag;
int	verbose;
int	quiet;
int	super_only;
int	force;
int	noaction;
int	journal_size;
int	journal_flags;
char	*bad_blocks_filename;
u32	fs_stride;

struct ext2_super_block param;
char *creator_os;
char *volume_label;
char *mount_dir;
char *journal_device;
int sync_kludge;	/* Set using the MKE2FS_SYNC env. option */

int sys_page_size = 4096;

static void usage(void)
{
	fprintf(stderr, _("Usage: %s [-c|-t|-l filename] [-b block-size] "
	"[-f fragment-size]\n\t[-i bytes-per-inode] [-j] [-J journal-options]"
	" [-N number-of-inodes]\n\t[-m reserved-blocks-percentage] "
	"[-o creator-os] [-g blocks-per-group]\n\t[-L volume-label] "
	"[-M last-mounted-directory] [-O feature[,...]]\n\t"
	"[-r fs-revision] [-R raid_opts] [-qvSV] device [blocks-count]\n"),
		program_name);
	exit(1);
}

/*
 * Given argv[0], return the program name.
 */
char *get_progname(char *argv_zero)
{
	char	*cp;

	cp = strrchr(argv_zero, '/');
	if (!cp )
		return argv_zero;
	else
		return cp+1;
}

void proceed_question(void)
{
	char buf[256];
	const char *short_yes = _("yY");

	fflush(stdout);
	fflush(stderr);
	fputs(_("Proceed anyway? (y,n) "), stdout);
	buf[0] = 0;
	fgets(buf, sizeof(buf), stdin);
	if (strchr(short_yes, buf[0]) == 0)
		exit(1);
}

void check_mount(const char *device, int force, const char *type)
{
	errcode_t	retval;
	int		mount_flags;

	retval = ext2fs_check_if_mounted(device, &mount_flags);
	if (retval) {
		com_err("ext2fs_check_if_mount", retval,
			_("while determining whether %s is mounted."),
			device);
		return;
	}
	if (!(mount_flags & EXT2_MF_MOUNTED))
		return;

	fprintf(stderr, _("%s is mounted; "), device);
	if (force) {
		fputs(_("mke2fs forced anyway.  Hope /etc/mtab is "
			"incorrect.\n"), stderr);
	} else {
		fprintf(stderr, _("will not make a %s here!\n"), type);
		exit(1);
	}
}

void parse_journal_opts(const char *opts)
{
	char	*buf, *token, *next, *p, *arg;
	int	len;
	int	journal_usage = 0;

	len = strlen(opts);
	buf = (char *)malloc(len+1);
	if (!buf) {
		fputs(_("Couldn't allocate memory to parse journal "
			"options!\n"), stderr);
		exit(1);
	}
	strcpy(buf, opts);
	for (token = buf; token && *token; token = next) {
		p = strchr(token, ',');
		next = 0;
		if (p) {
			*p = 0;
			next = p+1;
		} 
		arg = strchr(token, '=');
		if (arg) {
			*arg = 0;
			arg++;
		}
#if 0
		printf("Journal option=%s, argument=%s\n", token,
		       arg ? arg : "NONE");
#endif
		if (strcmp(token, "device") == 0) {
                    journal_device = 0;
			if (!journal_device) {
				journal_usage++;
				continue;
			}
		} else if (strcmp(token, "size") == 0) {
			if (!arg) {
				journal_usage++;
				continue;
			}
			journal_size = strtoul(arg, &p, 0);
			if (*p)
				journal_usage++;
		} else if (strcmp(token, "v1_superblock") == 0) {
			journal_flags |= EXT2_MKJOURNAL_V1_SUPER;
			continue;
		} else
			journal_usage++;
	}
	if (journal_usage) {
		fputs(_("\nBad journal options specified.\n\n"
			"Journal options are separated by commas, "
			"and may take an argument which\n"
			"\tis set off by an equals ('=') sign.\n\n"
			"Valid journal options are:\n"
			"\tsize=<journal size in megabytes>\n"
			"\tdevice=<journal device>\n\n"
			"The journal size must be between "
			"1024 and 102400 filesystem blocks.\n\n"), stderr);
		exit(1);
	}
}	

/*
 * Determine the number of journal blocks to use, either via
 * user-specified # of megabytes, or via some intelligently selected
 * defaults.
 * 
 * Find a reasonable journal file size (in blocks) given the number of blocks
 * in the filesystem.  For very small filesystems, it is not reasonable to
 * have a journal that fills more than half of the filesystem.
 */
int figure_journal_size(int size, ext2_filsys fs)
{
	blk_t j_blocks;

	if (fs->super->s_blocks_count < 2048) {
		fputs(_("\nFilesystem too small for a journal\n"), stderr);
		return 0;
	}
	
	if (size >= 0) {
		j_blocks = size * 1024 / (fs->blocksize	/ 1024);
		if (j_blocks < 1024 || j_blocks > 102400) {
			fprintf(stderr, _("\nThe requested journal "
				"size is %d blocks; it must be\n"
				"between 1024 and 102400 blocks.  "
				"Aborting.\n"),
				j_blocks);
			exit(1);
		}
		if (j_blocks > fs->super->s_free_blocks_count) {
			fputs(_("\nJournal size too big for filesystem.\n"),
			      stderr);
			exit(1);
		}
		return j_blocks;
	}

	if (fs->super->s_blocks_count < 32768)
		j_blocks = 1024;
	else if (fs->super->s_blocks_count < 262144)
		j_blocks = 4096;
	else
		j_blocks = 8192;

	return j_blocks;
}

void print_check_message(ext2_filsys fs)
{
	printf(_("This filesystem will be automatically "
		 "checked every %d mounts or\n"
		 "%g days, whichever comes first.  "
		 "Use tune2fs -c or -i to override.\n"),
	       fs->super->s_max_mnt_count,
	       (double)fs->super->s_checkinterval / (3600 * 24));
}

static int int_log2(int arg)
{
	int	l = 0;

	arg >>= 1;
	while (arg) {
		l++;
		arg >>= 1;
	}
	return l;
}

static int int_log10(unsigned int arg)
{
	int	l;

	for (l=0; arg ; l++)
		arg = arg / 10;
	return l;
}

/*
 * This function sets the default parameters for a filesystem
 *
 * The type is specified by the user.  The size is the maximum size
 * (in megabytes) for which a set of parameters applies, with a size
 * of zero meaning that it is the default parameter for the type.
 * Note that order is important in the table below.
 */
#define DEF_MAX_BLOCKSIZE -1
static char default_str[] = "default";
struct mke2fs_defaults {
	const char	*type;
	int		size;
	int		blocksize;
	int		inode_ratio;
} settings[] = {
	{ default_str, 0, 4096, 8192 },
	{ default_str, 512, 1024, 4096 },
	{ default_str, 3, 1024, 8192 },
	{ "journal", 0, 4096, 8192 },
	{ "news", 0, 4096, 4096 },
	{ "largefile", 0, DEF_MAX_BLOCKSIZE, 1024 * 1024 },
	{ "largefile4", 0, DEF_MAX_BLOCKSIZE, 4096 * 1024 },
	{ 0, 0, 0, 0},
};

static void set_fs_defaults(const char *fs_type,
			    struct ext2_super_block *super,
			    int blocksize, int sector_size,
			    int *inode_ratio)
{
	int	megs;
	int	ratio = 0;
	struct mke2fs_defaults *p;
	int	use_bsize = 1024;

	megs = super->s_blocks_count * (EXT2_BLOCK_SIZE(super) / 1024) / 1024;
	if (inode_ratio)
		ratio = *inode_ratio;
	if (!fs_type)
		fs_type = default_str;
	for (p = settings; p->type; p++) {
		if ((strcmp(p->type, fs_type) != 0) &&
		    (strcmp(p->type, default_str) != 0))
			continue;
		if ((p->size != 0) && (megs > p->size))
			continue;
		if (ratio == 0)
			*inode_ratio = p->inode_ratio < blocksize ?
				blocksize : p->inode_ratio;
		use_bsize = p->blocksize;
	}
	if (blocksize <= 0) {
		if (use_bsize == DEF_MAX_BLOCKSIZE)
			use_bsize = sys_page_size;
		if (sector_size && use_bsize < sector_size)
			use_bsize = sector_size;
		if ((blocksize < 0) && (use_bsize < (-blocksize)))
			use_bsize = -blocksize;
		blocksize = use_bsize;
		super->s_blocks_count /= blocksize / 1024;
	}
	super->s_log_frag_size = super->s_log_block_size =
		int_log2(blocksize >> EXT2_MIN_BLOCK_LOG_SIZE);
}


/*
 * Helper function for read_bb_file and test_disk
 */
static void invalid_block(ext2_filsys fs EXT2FS_ATTR((unused)), blk_t blk)
{
	fprintf(stderr, _("Bad block %u out of range; ignored.\n"), blk);
	return;
}

/*
 * Reads the bad blocks list from a file
 */
static void read_bb_file(ext2_filsys fs, badblocks_list *bb_list,
			 const char *bad_blocks_file)
{
	FILE		*f;
	errcode_t	retval;

	f = fopen(bad_blocks_file, "r");
	if (!f) {
		com_err("read_bad_blocks_file", 0,
			_("while trying to open %s"), bad_blocks_file);
		exit(1);
	}
	retval = ext2fs_read_bb_FILE(fs, f, bb_list, invalid_block);
	fclose (f);
	if (retval) {
		com_err("ext2fs_read_bb_FILE", retval,
			_("while reading in list of bad blocks from file"));
		exit(1);
	}
}

static void test_disk(ext2_filsys fs, badblocks_list *bb_list)
{
}

static void handle_bad_blocks(ext2_filsys fs, badblocks_list bb_list)
{
	dgrp_t			i;
	blk_t			j;
	unsigned 		must_be_good;
	blk_t			blk;
	badblocks_iterate	bb_iter;
	errcode_t		retval;
	blk_t			group_block;
	int			group;
	int			group_bad;

	if (!bb_list)
		return;
	
	/*
	 * The primary superblock and group descriptors *must* be
	 * good; if not, abort.
	 */
	must_be_good = fs->super->s_first_data_block + 1 + fs->desc_blocks;
	for (i = fs->super->s_first_data_block; i <= must_be_good; i++) {
		if (ext2fs_badblocks_list_test(bb_list, i)) {
			fprintf(stderr, _("Block %d in primary "
				"superblock/group descriptor area bad.\n"), i);
			fprintf(stderr, _("Blocks %d through %d must be good "
				"in order to build a filesystem.\n"),
				fs->super->s_first_data_block, must_be_good);
			fputs(_("Aborting....\n"), stderr);
			exit(1);
		}
	}

	/*
	 * See if any of the bad blocks are showing up in the backup
	 * superblocks and/or group descriptors.  If so, issue a
	 * warning and adjust the block counts appropriately.
	 */
	group_block = fs->super->s_first_data_block +
		fs->super->s_blocks_per_group;
	
	for (i = 1; i < fs->group_desc_count; i++) {
		group_bad = 0;
		for (j=0; j < fs->desc_blocks+1; j++) {
			if (ext2fs_badblocks_list_test(bb_list,
						       group_block + j)) {
				if (!group_bad) 
					fprintf(stderr,
_("Warning: the backup superblock/group descriptors at block %d contain\n"
"	bad blocks.\n\n"),
						group_block);
				group_bad++;
				group = ext2fs_group_of_blk(fs, group_block+j);
				fs->group_desc[group].bg_free_blocks_count++;
				fs->super->s_free_blocks_count++;
			}
		}
		group_block += fs->super->s_blocks_per_group;
	}
	
	/*
	 * Mark all the bad blocks as used...
	 */
	retval = ext2fs_badblocks_list_iterate_begin(bb_list, &bb_iter);
	if (retval) {
		com_err("ext2fs_badblocks_list_iterate_begin", retval,
			_("while marking bad blocks as used"));
		exit(1);
	}
	while (ext2fs_badblocks_list_iterate(bb_iter, &blk)) 
		ext2fs_mark_block_bitmap(fs->block_map, blk);
	ext2fs_badblocks_list_iterate_end(bb_iter);
}

/*
 * These functions implement a generalized progress meter.
 */
struct progress_struct {
	char		format[20];
	char		backup[80];
	u32		max;
	int		skip_progress;
};

static void progress_init(struct progress_struct *progress,
			  const char *label,__u32 max)
{
	int	i;

	memset(progress, 0, sizeof(struct progress_struct));
	if (quiet)
		return;

	/*
	 * Figure out how many digits we need
	 */
	i = int_log10(max);
	sprintf(progress->format, "%%%dd/%%%dld", i, i);
	memset(progress->backup, '\b', sizeof(progress->backup)-1);
	progress->backup[sizeof(progress->backup)-1] = 0;
	if ((2*i)+1 < (int) sizeof(progress->backup))
		progress->backup[(2*i)+1] = 0;
	progress->max = max;

	progress->skip_progress = 0;
	if (getenv("MKE2FS_SKIP_PROGRESS"))
		progress->skip_progress++;

	fputs(label, stdout);
	fflush(stdout);
}

static void progress_update(struct progress_struct *progress, __u32 val)
{
	if ((progress->format[0] == 0) || progress->skip_progress)
		return;
	printf(progress->format, val, progress->max);
	fputs(progress->backup, stdout);
}

static void progress_close(struct progress_struct *progress)
{
	if (progress->format[0] == 0)
		return;
	fputs(_("done                            \n"), stdout);
}


/*
 * Helper function which zeros out _num_ blocks starting at _blk_.  In
 * case of an error, the details of the error is returned via _ret_blk_
 * and _ret_count_ if they are non-NULL pointers.  Returns 0 on
 * success, and an error code on an error.
 *
 * As a special case, if the first argument is NULL, then it will
 * attempt to free the static zeroizing buffer.  (This is to keep
 * programs that check for memory leaks happy.)
 */
static errcode_t zero_blocks(ext2_filsys fs, blk_t blk, int num,
			     struct progress_struct *progress,
			     blk_t *ret_blk, int *ret_count)
{
	int		j, count, next_update, next_update_incr;
	static char	*buf;
	errcode_t	retval;

	/* If fs is null, clean up the static buffer and return */
	if (!fs) {
		if (buf) {
			free(buf);
			buf = 0;
		}
		return 0;
	}
	/* Allocate the zeroizing buffer if necessary */
	if (!buf) {
		buf = (char *)malloc(fs->blocksize * STRIDE_LENGTH);
		if (!buf) {
			com_err("malloc", ENOMEM,
				_("while allocating zeroizing buffer"));
			exit(1);
		}
		memset(buf, 0, fs->blocksize * STRIDE_LENGTH);
	}
	/* OK, do the write loop */
	next_update = 0;
	next_update_incr = num / 100;
	if (next_update_incr < 1)
		next_update_incr = 1;
	for (j=0; j < num; j += STRIDE_LENGTH, blk += STRIDE_LENGTH) {
		count = num - j;
		if (count > STRIDE_LENGTH)
			count = STRIDE_LENGTH;
		retval = io_channel_write_blk(fs->io, blk, count, buf);
		if (retval) {
			if (ret_count)
				*ret_count = count;
			if (ret_blk)
				*ret_blk = blk;
			return retval;
		}
		if (progress && j > next_update) {
			next_update += num / 100;
			progress_update(progress, blk);
		}
	}
	return 0;
}	

static void write_inode_tables(ext2_filsys fs)
{
	errcode_t	retval;
	blk_t		blk;
	dgrp_t		i;
	int		num;
	struct progress_struct progress;

	if (quiet)
		memset(&progress, 0, sizeof(progress));
	else
		progress_init(&progress, _("Writing inode tables: "),
			      fs->group_desc_count);

	for (i = 0; i < fs->group_desc_count; i++) {
		progress_update(&progress, i);
		
		blk = fs->group_desc[i].bg_inode_table;
		num = fs->inode_blocks_per_group;

		retval = zero_blocks(fs, blk, num, 0, &blk, &num);
		if (retval) {
			fprintf(stderr, _("\nCould not write %d blocks "
				"in inode table starting at %d: %s\n"),
				num, blk, error_message(retval));
			exit(1);
		}
	}
	zero_blocks(0, 0, 0, 0, 0, 0);
	progress_close(&progress);
}

static void create_root_dir(ext2_filsys fs)
{
	errcode_t	retval;
	struct ext2_inode	inode;

	retval = ext2fs_mkdir(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, 0);
	if (retval) {
		com_err("ext2fs_mkdir", retval, _("while creating root dir"));
		exit(1);
	}
}

static void create_lost_and_found(ext2_filsys fs)
{
	errcode_t		retval;
	ext2_ino_t		ino;
	const char		*name = "lost+found";
	int			i;
	int			lpf_size = 0;

	fs->umask = 077;
	retval = ext2fs_mkdir(fs, EXT2_ROOT_INO, 0, name);
	if (retval) {
		com_err("ext2fs_mkdir", retval,
			_("while creating /lost+found"));
		exit(1);
	}

	retval = ext2fs_lookup(fs, EXT2_ROOT_INO, name, strlen(name), 0, &ino);
	if (retval) {
		com_err("ext2_lookup", retval,
			_("while looking up /lost+found"));
		exit(1);
	}
	
	for (i=1; i < EXT2_NDIR_BLOCKS; i++) {
		if ((lpf_size += fs->blocksize) >= 16*1024)
			break;
		retval = ext2fs_expand_dir(fs, ino);
		if (retval) {
			com_err("ext2fs_expand_dir", retval,
				_("while expanding /lost+found"));
			exit(1);
		}
	}
}

static void create_bad_block_inode(ext2_filsys fs, badblocks_list bb_list)
{
	errcode_t	retval;
	
	ext2fs_mark_inode_bitmap(fs->inode_map, EXT2_BAD_INO);
	fs->group_desc[0].bg_free_inodes_count--;
	fs->super->s_free_inodes_count--;
	retval = ext2fs_update_bb_inode(fs, bb_list);
	if (retval) {
		com_err("ext2fs_update_bb_inode", retval,
			_("while setting bad block inode"));
		exit(1);
	}

}

static void reserve_inodes(ext2_filsys fs)
{
	ext2_ino_t	i;
	int		group;

	for (i = EXT2_ROOT_INO + 1; i < EXT2_FIRST_INODE(fs->super); i++) {
		ext2fs_mark_inode_bitmap(fs->inode_map, i);
		group = ext2fs_group_of_ino(fs, i);
		fs->group_desc[group].bg_free_inodes_count--;
		fs->super->s_free_inodes_count--;
	}
	ext2fs_mark_ib_dirty(fs);
}

#define BSD_DISKMAGIC   (0x82564557UL)  /* The disk magic number */
#define BSD_MAGICDISK   (0x57455682UL)  /* The disk magic number reversed */
#define BSD_LABEL_OFFSET        64

static void zap_sector(ext2_filsys fs, int sect, int nsect)
{
	char *buf;
	int retval;
	unsigned int *magic;

	buf = (char *)malloc(512*nsect);
	if (!buf) {
		printf(_("Out of memory erasing sectors %d-%d\n"),
		       sect, sect + nsect - 1);
		exit(1);
	}

	if (sect == 0) {
		/* Check for a BSD disklabel, and don't erase it if so */
		retval = io_channel_read_blk(fs->io, 0, -512, buf);
		if (retval)
			fprintf(stderr,
				_("Warning: could not read block 0: %s\n"),
				error_message(retval));
		else {
			magic = (unsigned int *) (buf + BSD_LABEL_OFFSET);
			if ((*magic == BSD_DISKMAGIC) ||
			    (*magic == BSD_MAGICDISK))
				return;
		}
	}

	memset(buf, 0, 512*nsect);
	io_channel_set_blksize(fs->io, 512);
	retval = io_channel_write_blk(fs->io, sect, -512*nsect, buf);
	io_channel_set_blksize(fs->io, fs->blocksize);
	free(buf);
	if (retval)
		fprintf(stderr, _("Warning: could not erase sector %d: %s\n"),
			sect, error_message(retval));
}

static void create_journal_dev(ext2_filsys fs)
{
	struct progress_struct progress;
	errcode_t		retval;
	char			*buf;
	blk_t			blk;
	int			count;

	retval = ext2fs_create_journal_superblock(fs,
				  fs->super->s_blocks_count, 0, &buf);
	if (retval) {
		com_err("create_journal_dev", retval,
			_("while initializing journal superblock"));
		exit(1);
	}
	if (quiet)
		memset(&progress, 0, sizeof(progress));
	else
		progress_init(&progress, _("Zeroing journal device: "),
			      fs->super->s_blocks_count);

	retval = zero_blocks(fs, 0, fs->super->s_blocks_count,
			     &progress, &blk, &count);
	if (retval) {
		com_err("create_journal_dev", retval,
			_("while zeroing journal device (block %u, count %d)"),
			blk, count);
		exit(1);
	}
	zero_blocks(0, 0, 0, 0, 0, 0);

	retval = io_channel_write_blk(fs->io,
				      fs->super->s_first_data_block+1,
				      1, buf);
	if (retval) {
		com_err("create_journal_dev", retval,
			_("while writing journal superblock"));
		exit(1);
	}
	progress_close(&progress);
}

static void show_stats(ext2_filsys fs)
{
	struct ext2_super_block *s = fs->super;
	char 			buf[80];
	blk_t			group_block;
	dgrp_t			i;
	int			need, col_left;
	
	if (param.s_blocks_count != s->s_blocks_count)
		fprintf(stderr, _("warning: %d blocks unused.\n\n"),
		       param.s_blocks_count - s->s_blocks_count);

	memset(buf, 0, sizeof(buf));
	strncpy(buf, s->s_volume_name, sizeof(s->s_volume_name));
	printf(_("Filesystem label=%s\n"), buf);
	fputs(_("OS type: "), stdout);
	switch (fs->super->s_creator_os) {
	    case EXT2_OS_LINUX: fputs("Linux", stdout); break;
	    case EXT2_OS_HURD:  fputs("GNU/Hurd", stdout);   break;
	    case EXT2_OS_MASIX: fputs ("Masix", stdout); break;
	    default:		fputs(_("(unknown os)"), stdout);
        }
	printf("\n");
	printf(_("Block size=%u (log=%u)\n"), fs->blocksize,
		s->s_log_block_size);
	printf(_("Fragment size=%u (log=%u)\n"), fs->fragsize,
		s->s_log_frag_size);
	printf(_("%u inodes, %u blocks\n"), s->s_inodes_count,
	       s->s_blocks_count);
	printf(_("%u blocks (%2.2f%%) reserved for the super user\n"),
		s->s_r_blocks_count,
	       100.0 * s->s_r_blocks_count / s->s_blocks_count);
	printf(_("First data block=%u\n"), s->s_first_data_block);
	if (fs->group_desc_count > 1)
		printf(_("%u block groups\n"), fs->group_desc_count);
	else
		printf(_("%u block group\n"), fs->group_desc_count);
	printf(_("%u blocks per group, %u fragments per group\n"),
	       s->s_blocks_per_group, s->s_frags_per_group);
	printf(_("%u inodes per group\n"), s->s_inodes_per_group);

	if (fs->group_desc_count == 1) {
		printf("\n");
		return;
	}
	
	printf(_("Superblock backups stored on blocks: "));
	group_block = s->s_first_data_block;
	col_left = 0;
	for (i = 1; i < fs->group_desc_count; i++) {
		group_block += s->s_blocks_per_group;
		if (!ext2fs_bg_has_super(fs, i))
			continue;
		if (i != 1)
			printf(", ");
		need = int_log10(group_block) + 2;
		if (need > col_left) {
			printf("\n\t");
			col_left = 72;
		}
		col_left -= need;
		printf("%u", group_block);
	}
	printf("\n\n");
}

/*
 * Set the S_CREATOR_OS field.  Return true if OS is known,
 * otherwise, 0.
 */
static int set_os(struct ext2_super_block *sb, char *os)
{
	if (isdigit (*os))
		sb->s_creator_os = atoi (os);
	else if (strcasecmp(os, "linux") == 0)
		sb->s_creator_os = EXT2_OS_LINUX;
	else if (strcasecmp(os, "GNU") == 0 || strcasecmp(os, "hurd") == 0)
		sb->s_creator_os = EXT2_OS_HURD;
	else if (strcasecmp(os, "masix") == 0)
		sb->s_creator_os = EXT2_OS_MASIX;
	else
		return 0;
	return 1;
}

#define PATH_SET "PATH=/sbin"

static void parse_raid_opts(const char *opts)
{
	char	*buf, *token, *next, *p, *arg;
	int	len;
	int	raid_usage = 0;

	len = strlen(opts);
	buf = (char *)malloc(len+1);
	if (!buf) {
		fprintf(stderr, _("Couldn't allocate memory to parse "
			"raid options!\n"));
		exit(1);
	}
	strcpy(buf, opts);
	for (token = buf; token && *token; token = next) {
		p = strchr(token, ',');
		next = 0;
		if (p) {
			*p = 0;
			next = p+1;
		} 
		arg = strchr(token, '=');
		if (arg) {
			*arg = 0;
			arg++;
		}
		if (strcmp(token, "stride") == 0) {
			if (!arg) {
				raid_usage++;
				continue;
			}
			fs_stride = strtoul(arg, &p, 0);
			if (*p || (fs_stride == 0)) {
				fprintf(stderr,
					_("Invalid stride parameter.\n"));
				raid_usage++;
				continue;
			}
		} else
			raid_usage++;
	}
	if (raid_usage) {
		fprintf(stderr, _("\nBad raid options specified.\n\n"
			"Raid options are separated by commas, "
			"and may take an argument which\n"
			"\tis set off by an equals ('=') sign.\n\n"
			"Valid raid options are:\n"
			"\tstride=<stride length in blocks>\n\n"));
		exit(1);
	}
}	

static __u32 ok_features[3] = {
	EXT3_FEATURE_COMPAT_HAS_JOURNAL |
		EXT2_FEATURE_COMPAT_DIR_INDEX,	/* Compat */
	EXT2_FEATURE_INCOMPAT_FILETYPE|		/* Incompat */
		EXT3_FEATURE_INCOMPAT_JOURNAL_DEV|
		EXT2_FEATURE_INCOMPAT_META_BG,
	EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER	/* R/O compat */
};

static int
valid_offset (VFS::Device *dev, loff_t offset)
{
  char ch;

  if (dev->lseek (offset, SEEK_SET) != offset)
    return 0;
  if (dev->read (&ch, 1) < 1)
    return 0;
  return 1;
}

static int
count_blocks (VFS::Device *dev, int block_size)
{
  loff_t high, low;

  /* first try SEEK_END, which should work on most devices nowadays */
  if ((low = dev->lseek(0, SEEK_END)) <= 0) {
      low = 0;
      for (high = 1; valid_offset (dev, high); high *= 2)
	  low = high;
      while (low < high - 1) {
	  const loff_t mid = (low + high) / 2;
	  if (valid_offset (dev, mid))
	      low = mid;
	  else
	      high = mid;
      }
      ++low;
  }

  return low / block_size;
}


static void PRS()
{
	int		b, c;
	int		size;
	char *		tmp;
	int		blocksize = 0;
	int		inode_ratio = 0;
	int		inode_size = 0;
	int		reserved_ratio = 5;
	int		sector_size = 0;
	int		show_version_only = 0;
	ext2_ino_t	num_inodes = 0;
	errcode_t	retval;
	char *		raid_opts = 0;
	const char *	fs_type = 0;
	int		default_features = 1;
	blk_t		dev_size;
	long		sysval;

	/* Determine the system page size if possible */
        sys_page_size = 4096;
	
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	initialize_ext2_error_table();
	memset(&param, 0, sizeof(struct ext2_super_block));
	param.s_rev_level = 1;  /* Create revision 1 filesystems now */
	param.s_feature_incompat |= EXT2_FEATURE_INCOMPAT_FILETYPE;
	param.s_feature_ro_compat |= EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER;

        reserved_ratio = 5;

	if (!quiet || show_version_only)
		fprintf (stderr, "mke2fs %s (%s)\n", E2FSPROGS_VERSION, 
			 E2FSPROGS_DATE);

	if (show_version_only) {
		fprintf(stderr, _("\tUsing %s\n"), 
			error_message(EXT2_ET_BASE));
		exit(0);
	}

	if (raid_opts)
		parse_raid_opts(raid_opts);

	if (blocksize > sys_page_size) {
		if (!force) {
			com_err(program_name, 0,
				_("%d-byte blocks too big for system (max %d)"),
				blocksize, sys_page_size);
			proceed_question();
		}
		fprintf(stderr, _("Warning: %d-byte blocks too big for system "
				  "(max %d), forced to continue\n"),
			blocksize, sys_page_size);
	}
	if ((blocksize > 4096) &&
	    (param.s_feature_compat & EXT3_FEATURE_COMPAT_HAS_JOURNAL))
		fprintf(stderr, "\nWarning: some 2.4 kernels do not support "
			"blocksizes greater than 4096 \n\tusing ext3."
			"  Use -b 4096 if this is an issue for you.\n\n");

	if (param.s_feature_incompat & EXT3_FEATURE_INCOMPAT_JOURNAL_DEV) {
		if (!fs_type)
			fs_type = "journal";
		reserved_ratio = 0;
		param.s_feature_incompat = EXT3_FEATURE_INCOMPAT_JOURNAL_DEV;
		param.s_feature_compat = 0;
		param.s_feature_ro_compat = 0;
 	}
	if (param.s_rev_level == EXT2_GOOD_OLD_REV &&
	    (param.s_feature_compat || param.s_feature_ro_compat ||
	     param.s_feature_incompat))
		param.s_rev_level = 1;  /* Create a revision 1 filesystem */

	param.s_log_frag_size = param.s_log_block_size;

	if (noaction && param.s_blocks_count) {
		dev_size = param.s_blocks_count;
		retval = 0;
	} else {
            dev_size = count_blocks (device, EXT2_BLOCK_SIZE(&param));
            retval = 0;
        }

	if (retval && (retval != EXT2_ET_UNIMPLEMENTED)) {
		com_err(program_name, retval,
			_("while trying to determine filesystem size"));
		exit(1);
	}
	if (!param.s_blocks_count) {
		if (retval == EXT2_ET_UNIMPLEMENTED) {
			com_err(program_name, 0,
				_("Couldn't determine device size; you "
				"must specify\nthe size of the "
				"filesystem\n"));
			exit(1);
		} else {
			if (dev_size == 0) {
				com_err(program_name, 0,
				_("Device size reported to be zero.  "
				  "Invalid partition specified, or\n\t"
				  "partition table wasn't reread "
				  "after running fdisk, due to\n\t"
				  "a modified partition being busy "
				  "and in use.  You may need to reboot\n\t"
				  "to re-read your partition table.\n"
				  ));
				exit(1);
			}
			param.s_blocks_count = dev_size;
			if (sys_page_size > EXT2_BLOCK_SIZE(&param))
				param.s_blocks_count &= ~((sys_page_size /
							   EXT2_BLOCK_SIZE(&param))-1);
		}
		
	} else if (!force && (param.s_blocks_count > dev_size)) {
		com_err(program_name, 0,
			_("Filesystem larger than apparent device size."));
		proceed_question();
	}

	/*
	 * If the user asked for HAS_JOURNAL, then make sure a journal
	 * gets created.
	 */
	if ((param.s_feature_compat & EXT3_FEATURE_COMPAT_HAS_JOURNAL) &&
	    !journal_size)
		journal_size = -1;

	/* Get the hardware sector size, if available */
        sector_size = 512;

	set_fs_defaults(fs_type, &param, blocksize, sector_size, &inode_ratio);
	blocksize = EXT2_BLOCK_SIZE(&param);
	
	if (param.s_blocks_per_group) {
		if (param.s_blocks_per_group < 256 ||
		    param.s_blocks_per_group > 8 * (unsigned) blocksize) {
			com_err(program_name, 0,
				_("blocks per group count out of range"));
			exit(1);
		}
	}

	if (inode_size) {
		if (inode_size < EXT2_GOOD_OLD_INODE_SIZE ||
		    inode_size > EXT2_BLOCK_SIZE(&param) ||
		    inode_size & (inode_size - 1)) {
			com_err(program_name, 0,
				_("bad inode size %d (min %d/max %d)"),
				inode_size, EXT2_GOOD_OLD_INODE_SIZE,
				blocksize);
			exit(1);
		}
		if (inode_size != EXT2_GOOD_OLD_INODE_SIZE)
			fprintf(stderr, _("Warning: %d-byte inodes not usable "
				"on most systems\n"),
				inode_size);
		param.s_inode_size = inode_size;
	}

	/*
	 * Calculate number of inodes based on the inode ratio
	 */
	param.s_inodes_count = num_inodes ? num_inodes : 
		((__u64) param.s_blocks_count * blocksize)
			/ inode_ratio;

	/*
	 * Calculate number of blocks to reserve
	 */
	param.s_r_blocks_count = (param.s_blocks_count * reserved_ratio) / 100;

}
					
int CreateExt2Filesystem (VFS::Device *dev)
{
	errcode_t	retval = 0;
	ext2_filsys	fs;
	badblocks_list	bb_list = 0;
	int		journal_blocks;
	unsigned int	i;
	int		val;
	io_manager	io_ptr;

        device = dev;
	PRS();

	io_ptr = unix_io_manager;

	/*
	 * Initialize the superblock....
	 */
	retval = ext2fs_initialize(device, 0, &param,
				   io_ptr, &fs);
	if (retval) {
		com_err("<rawpod device>", retval, _("while setting up superblock"));
		exit(1);
	}

	/*
	 * Wipe out the old on-disk superblock
	 */
	if (!noaction)
		zap_sector(fs, 2, 6);

	/*
	 * Generate a UUID for it...
	 */
	uuid_generate(fs->super->s_uuid);

	/*
	 * Initialize the directory index variables
	 */
	fs->super->s_def_hash_version = EXT2_HASH_TEA;
	uuid_generate((unsigned char *) fs->super->s_hash_seed);

	/*
	 * Add "jitter" to the superblock's check interval so that we
	 * don't check all the filesystems at the same time.  We use a
	 * kludgy hack of using the UUID to derive a random jitter value.
	 */
	for (i = 0, val = 0 ; i < sizeof(fs->super->s_uuid); i++)
		val += fs->super->s_uuid[i];
	fs->super->s_max_mnt_count += val % EXT2_DFL_MAX_MNT_COUNT;

	/*
	 * Override the creator OS, if applicable
	 */
	if (creator_os && !set_os(fs->super, creator_os)) {
		com_err (program_name, 0, _("unknown os - %s"), creator_os);
		exit(1);
	}

	/*
	 * For the Hurd, we will turn off filetype since it doesn't
	 * support it.
	 */
	if (fs->super->s_creator_os == EXT2_OS_HURD)
		fs->super->s_feature_incompat &=
			~EXT2_FEATURE_INCOMPAT_FILETYPE;

	/*
	 * Set the volume label...
	 */
	if (volume_label) {
		memset(fs->super->s_volume_name, 0,
		       sizeof(fs->super->s_volume_name));
		strncpy(fs->super->s_volume_name, volume_label,
			sizeof(fs->super->s_volume_name));
	}

	/*
	 * Set the last mount directory
	 */
	if (mount_dir) {
		memset(fs->super->s_last_mounted, 0,
		       sizeof(fs->super->s_last_mounted));
		strncpy(fs->super->s_last_mounted, mount_dir,
			sizeof(fs->super->s_last_mounted));
	}
	
	if (!quiet || noaction)
		show_stats(fs);

	if (noaction)
		exit(0);

	if (fs->super->s_feature_incompat &
	    EXT3_FEATURE_INCOMPAT_JOURNAL_DEV) {
		create_journal_dev(fs);
		exit(ext2fs_close(fs) ? 1 : 0);
	}

	if (bad_blocks_filename)
		read_bb_file(fs, &bb_list, bad_blocks_filename);
	if (cflag)
		test_disk(fs, &bb_list);

	handle_bad_blocks(fs, bb_list);
	fs->stride = fs_stride;
	retval = ext2fs_allocate_tables(fs);
	if (retval) {
		com_err(program_name, retval,
			_("while trying to allocate filesystem tables"));
		exit(1);
	}
	if (super_only) {
		fs->super->s_state |= EXT2_ERROR_FS;
		fs->flags &= ~(EXT2_FLAG_IB_DIRTY|EXT2_FLAG_BB_DIRTY);
	} else {
		/* rsv must be a power of two (64kB is MD RAID sb alignment) */
		unsigned int rsv = 65536 / fs->blocksize;
		unsigned long blocks = fs->super->s_blocks_count;
		unsigned long start;
		blk_t ret_blk;

#ifdef ZAP_BOOTBLOCK
		zap_sector(fs, 0, 2);
#endif

		/*
		 * Wipe out any old MD RAID (or other) metadata at the end
		 * of the device.  This will also verify that the device is
		 * as large as we think.  Be careful with very small devices.
		 */
		start = (blocks & ~(rsv - 1));
		if (start > rsv)
			start -= rsv;
		if (start > 0)
			retval = zero_blocks(fs, start, blocks - start,
					     NULL, &ret_blk, NULL);

		if (retval) {
			com_err(program_name, retval,
				_("while zeroing block %u at end of filesystem"),
				ret_blk);
		}
		write_inode_tables(fs);
		create_root_dir(fs);
		create_lost_and_found(fs);
		reserve_inodes(fs);
		create_bad_block_inode(fs, bb_list);
	}

	if (journal_size) {
		journal_blocks = figure_journal_size(journal_size, fs);

		if (!journal_blocks) {
			fs->super->s_feature_compat &=
				~EXT3_FEATURE_COMPAT_HAS_JOURNAL;
			goto no_journal;
		}
		if (!quiet) {
			printf(_("Creating journal (%d blocks): "),
			       journal_blocks);
			fflush(stdout);
		}
		retval = ext2fs_add_journal_inode(fs, journal_blocks,
						  journal_flags);
		if (retval) {
			com_err (program_name, retval,
				 _("\n\twhile trying to create journal"));
			exit(1);
		}
		if (!quiet)
			printf(_("done\n"));
	}
no_journal:

	if (!quiet)
		printf(_("Writing superblocks and "
		       "filesystem accounting information: "));
	retval = ext2fs_flush(fs);
	if (retval) {
		fprintf(stderr,
			_("\nWarning, had trouble writing out superblocks."));
	}
	if (!quiet) {
		printf(_("done\n\n"));
                print_check_message(fs);
	}
	val = ext2fs_close(fs);
	return (retval || val) ? 1 : 0;
}
