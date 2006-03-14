/*-*-C++-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
 *  linux/include/linux/ext2_fs.h
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/include/linux/minix_fs.h
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/* XenOS ext2 class added. Joshua Oreman, 12/18/2005. */

#ifndef _EXT2_H_
#define _EXT2_H_

#include "vfs.h"

extern int CreateExt2Filesystem (VFS::Device *dev);

/*
 * The second extended filesystem constants/structures
 */

/*
 * The second extended file system version
 */
#define EXT2FS_DATE		"95/08/09"
#define EXT2FS_VERSION		"0.5b"

/*
 * Special inode numbers
 */
#define	EXT2_BAD_INO		 1	/* Bad blocks inode */
#define EXT2_ROOT_INO		 2	/* Root inode */
#define EXT2_BOOT_LOADER_INO	 5	/* Boot loader inode */
#define EXT2_UNDEL_DIR_INO	 6	/* Undelete directory inode */
#define EXT2_RESIZE_INO		 7	/* Reserved group descriptors inode */
#define EXT2_JOURNAL_INO	 8	/* Journal inode */

/* First non-reserved inode for old ext2 filesystems */
#define EXT2_GOOD_OLD_FIRST_INO	11

/*
 * The second extended file system magic number
 */
#define EXT2_SUPER_MAGIC	0xEF53

/*
 * Maximal count of links to a file
 */
#define EXT2_LINK_MAX		32000

/*
 * Macro-instructions used to manage several block sizes
 */
#define EXT2_MIN_BLOCK_SIZE		1024
#define	EXT2_MAX_BLOCK_SIZE		4096
#define EXT2_MIN_BLOCK_LOG_SIZE		  10
#define EXT2_BLOCK_SIZE(s)	(EXT2_MIN_BLOCK_SIZE << (s)->s_log_block_size)
#define EXT2_BLOCK_SIZE_BITS(s)	((s)->s_log_block_size + 10)
#define EXT2_INODE_SIZE(s)	(((s)->s_rev_level == EXT2_GOOD_OLD_REV) ? \
				 EXT2_GOOD_OLD_INODE_SIZE : (s)->s_inode_size)
#define EXT2_FIRST_INO(s)	(((s)->s_rev_level == EXT2_GOOD_OLD_REV) ? \
				 EXT2_GOOD_OLD_FIRST_INO : (s)->s_first_ino)
#define EXT2_ADDR_PER_BLOCK(s)	(EXT2_BLOCK_SIZE(s) / sizeof(u32))

/*
 * Macro-instructions used to manage fragments
 */
#define EXT2_MIN_FRAG_SIZE		1024
#define	EXT2_MAX_FRAG_SIZE		4096
#define EXT2_MIN_FRAG_LOG_SIZE		  10
#define EXT2_FRAG_SIZE(s)		(EXT2_MIN_FRAG_SIZE << (s)->s_log_frag_size)
#define EXT2_FRAGS_PER_BLOCK(s)		(EXT2_BLOCK_SIZE(s) / EXT2_FRAG_SIZE(s))

/*
 * ACL structures
 */
struct ext2_acl_header	/* Header of Access Control Lists */
{
	u32	aclh_size;
	u32	aclh_file_count;
	u32	aclh_acle_count;
	u32	aclh_first_acle;
};

struct ext2_acl_entry	/* Access Control List Entry */
{
	u32	acle_size;
	u16	acle_perms;	/* Access permissions */
	u16	acle_type;	/* Type of entry */
	u16	acle_tag;	/* User or group identity */
	u16	acle_pad1;
	u32	acle_next;	/* Pointer on next entry for the */
					/* same inode or on next free entry */
};

/*
 * Structure of a blocks group descriptor
 */
struct ext2_group_desc
{
	u32	bg_block_bitmap;		/* Blocks bitmap block */
	u32	bg_inode_bitmap;		/* Inodes bitmap block */
	u32	bg_inode_table;		/* Inodes table block */
	u16	bg_free_blocks_count;	/* Free blocks count */
	u16	bg_free_inodes_count;	/* Free inodes count */
	u16	bg_used_dirs_count;	/* Directories count */
	u16	bg_pad;
	u32	bg_reserved[3];
};

/*
 * Data structures used by the directory indexing feature
 *
 * Note: all of the multibyte integer fields are little endian.
 */

/*
 * Note: dx_root_info is laid out so that if it should somehow get
 * overlaid by a dirent the two low bits of the hash version will be
 * zero.  Therefore, the hash version mod 4 should never be 0.
 * Sincerely, the paranoia department.
 */
struct ext2_dx_root_info {
	u32 reserved_zero;
	u8 hash_version; /* 0 now, 1 at release */
	u8 info_length; /* 8 */
	u8 indirect_levels;
	u8 unused_flags;
};

#define EXT2_HASH_LEGACY	0
#define EXT2_HASH_HALF_MD4	1
#define EXT2_HASH_TEA		2

#define EXT2_HASH_FLAG_INCOMPAT	0x1

struct ext2_dx_entry {
	u32 hash;
	u32 block;
};

struct ext2_dx_countlimit {
	u16 limit;
	u16 count;
};

/*
 * Macro-instructions used to manage group descriptors
 */
#define EXT2_BLOCKS_PER_GROUP(s)	((s)->s_blocks_per_group)
#define EXT2_INODES_PER_GROUP(s)	((s)->s_inodes_per_group)
#define EXT2_INODES_PER_BLOCK(s)	(EXT2_BLOCK_SIZE(s)/EXT2_INODE_SIZE(s))
/* limits imposed by 16-bit value gd_free_{blocks,inode}_count */
#define EXT2_MAX_BLOCKS_PER_GROUP(s)	((1 << 16) - 8)
#define EXT2_MAX_INODES_PER_GROUP(s)	((1 << 16) - EXT2_INODES_PER_BLOCK(s))
#define EXT2_DESC_PER_BLOCK(s)		(EXT2_BLOCK_SIZE(s) / sizeof (struct ext2_group_desc))

/*
 * Constants relative to the data blocks
 */
#define	EXT2_NDIR_BLOCKS		12
#define	EXT2_IND_BLOCK			EXT2_NDIR_BLOCKS
#define	EXT2_DIND_BLOCK			(EXT2_IND_BLOCK + 1)
#define	EXT2_TIND_BLOCK			(EXT2_DIND_BLOCK + 1)
#define	EXT2_N_BLOCKS			(EXT2_TIND_BLOCK + 1)

/*
 * Inode flags
 */
#define EXT2_SECRM_FL			0x00000001 /* Secure deletion */
#define EXT2_UNRM_FL			0x00000002 /* Undelete */
#define EXT2_COMPR_FL			0x00000004 /* Compress file */
#define EXT2_SYNC_FL			0x00000008 /* Synchronous updates */
#define EXT2_IMMUTABLE_FL		0x00000010 /* Immutable file */
#define EXT2_APPEND_FL			0x00000020 /* writes to file may only append */
#define EXT2_NODUMP_FL			0x00000040 /* do not dump file */
#define EXT2_NOATIME_FL			0x00000080 /* do not update atime */
/* Reserved for compression usage... */
#define EXT2_DIRTY_FL			0x00000100
#define EXT2_COMPRBLK_FL		0x00000200 /* One or more compressed clusters */
#define EXT2_NOCOMP_FL			0x00000400 /* Don't compress */
#define EXT2_ECOMPR_FL			0x00000800 /* Compression error */
/* End compression flags --- maybe not all used */	
#define EXT2_BTREE_FL			0x00001000 /* btree format dir */
#define EXT2_INDEX_FL			0x00001000 /* hash-indexed directory */
#define EXT2_IMAGIC_FL			0x00002000 /* AFS directory */
#define EXT3_JOURNAL_DATA_FL		0x00004000 /* Reserved for ext3 */
#define EXT2_NOTAIL_FL			0x00008000 /* file tail should not be merged */
#define EXT2_DIRSYNC_FL			0x00010000 /* dirsync behaviour (directories only) */
#define EXT2_TOPDIR_FL			0x00020000 /* Top of directory hierarchies*/
#define EXT2_RESERVED_FL		0x80000000 /* reserved for ext2 lib */

#define EXT2_FL_USER_VISIBLE		0x0003DFFF /* User visible flags */
#define EXT2_FL_USER_MODIFIABLE		0x000380FF /* User modifiable flags */

/*
 * Structure of an inode on the disk
 */
struct ext2_inode {
	u16	i_mode;		/* File mode */
	u16	i_uid;		/* Low 16 bits of Owner Uid */
	u32	i_size;		/* Size in bytes */
	u32	i_atime;	/* Access time */
	u32	i_ctime;	/* Creation time */
	u32	i_mtime;	/* Modification time */
	u32	i_dtime;	/* Deletion Time */
	u16	i_gid;		/* Low 16 bits of Group Id */
	u16	i_links_count;	/* Links count */
	u32	i_blocks;	/* Blocks count */
	u32	i_flags;	/* File flags */
	union {
		struct {
			u32  l_i_reserved1;
		} linux1;
		struct {
			u32  h_i_translator;
		} hurd1;
		struct {
			u32  m_i_reserved1;
		} masix1;
	} osd1;				/* OS dependent 1 */
	u32	i_block[EXT2_N_BLOCKS];/* Pointers to blocks */
	u32	i_generation;	/* File version (for NFS) */
	u32	i_file_acl;	/* File ACL */
	u32	i_dir_acl;	/* Directory ACL */
	u32	i_faddr;	/* Fragment address */
	union {
		struct {
			u8	l_i_frag;	/* Fragment number */
			u8	l_i_fsize;	/* Fragment size */
			u16	i_pad1;
			u16	l_i_uid_high;	/* these 2 fields    */
			u16	l_i_gid_high;	/* were reserved2[0] */
			u32	l_i_reserved2;
		} linux2;
		struct {
			u8	h_i_frag;	/* Fragment number */
			u8	h_i_fsize;	/* Fragment size */
			u16	h_i_mode_high;
			u16	h_i_uid_high;
			u16	h_i_gid_high;
			u32	h_i_author;
		} hurd2;
		struct {
			u8	m_i_frag;	/* Fragment number */
			u8	m_i_fsize;	/* Fragment size */
			u16	m_pad1;
			u32	m_i_reserved2[2];
		} masix2;
	} osd2;				/* OS dependent 2 */
};

#define i_size_high   i_dir_acl

#define i_reserved1	osd1.linux1.l_i_reserved1
#define i_frag		osd2.linux2.l_i_frag
#define i_fsize		osd2.linux2.l_i_fsize
#define i_uid_low	i_uid
#define i_gid_low	i_gid
#define i_uid_high	osd2.linux2.l_i_uid_high
#define i_gid_high	osd2.linux2.l_i_gid_high
#define i_reserved2	osd2.linux2.l_i_reserved2

/*
 * File system states
 */
#define	EXT2_VALID_FS			0x0001	/* Unmounted cleanly */
#define	EXT2_ERROR_FS			0x0002	/* Errors detected */

/*
 * Mount flags
 */
#define EXT2_MOUNT_CHECK		0x0001	/* Do mount-time checks */
#define EXT2_MOUNT_OLDALLOC		0x0002  /* Don't use the new Orlov allocator */
#define EXT2_MOUNT_GRPID		0x0004	/* Create files with directory's group */
#define EXT2_MOUNT_DEBUG		0x0008	/* Some debugging messages */
#define EXT2_MOUNT_ERRORS_CONT		0x0010	/* Continue on errors */
#define EXT2_MOUNT_ERRORS_RO		0x0020	/* Remount fs ro on errors */
#define EXT2_MOUNT_ERRORS_PANIC		0x0040	/* Panic on errors */
#define EXT2_MOUNT_MINIX_DF		0x0080	/* Mimics the Minix statfs */
#define EXT2_MOUNT_NOBH			0x0100	/* No buffer_heads */
#define EXT2_MOUNT_NO_UID32		0x0200  /* Disable 32-bit UIDs */
#define EXT2_MOUNT_XATTR_USER		0x4000	/* Extended user attributes */
#define EXT2_MOUNT_POSIX_ACL		0x8000	/* POSIX Access Control Lists */

/*
 * Maximal mount counts between two filesystem checks
 */
#define EXT2_DFL_MAX_MNT_COUNT		20	/* Allow 20 mounts */
#define EXT2_DFL_CHECKINTERVAL		0	/* Don't use interval check */

/*
 * Behaviour when detecting errors
 */
#define EXT2_ERRORS_CONTINUE		1	/* Continue execution */
#define EXT2_ERRORS_RO			2	/* Remount fs read-only */
#define EXT2_ERRORS_PANIC		3	/* Panic */
#define EXT2_ERRORS_DEFAULT		EXT2_ERRORS_CONTINUE

/*
 * Structure of the super block
 */
struct ext2_super_block {
	u32	s_inodes_count;		/* Inodes count */
	u32	s_blocks_count;		/* Blocks count */
	u32	s_r_blocks_count;	/* Reserved blocks count */
	u32	s_free_blocks_count;	/* Free blocks count */
	u32	s_free_inodes_count;	/* Free inodes count */
	u32	s_first_data_block;	/* First Data Block */
	u32	s_log_block_size;	/* Block size */
	u32	s_log_frag_size;	/* Fragment size */
	u32	s_blocks_per_group;	/* # Blocks per group */
	u32	s_frags_per_group;	/* # Fragments per group */
	u32	s_inodes_per_group;	/* # Inodes per group */
	u32	s_mtime;		/* Mount time */
	u32	s_wtime;		/* Write time */
	u16	s_mnt_count;		/* Mount count */
	u16	s_max_mnt_count;	/* Maximal mount count */
	u16	s_magic;		/* Magic signature */
	u16	s_state;		/* File system state */
	u16	s_errors;		/* Behaviour when detecting errors */
	u16	s_minor_rev_level; 	/* minor revision level */
	u32	s_lastcheck;		/* time of last check */
	u32	s_checkinterval;	/* max. time between checks */
	u32	s_creator_os;		/* OS */
	u32	s_rev_level;		/* Revision level */
	u16	s_def_resuid;		/* Default uid for reserved blocks */
	u16	s_def_resgid;		/* Default gid for reserved blocks */
	/*
	 * These fields are for EXT2_DYNAMIC_REV superblocks only.
	 *
	 * Note: the difference between the compatible feature set and
	 * the incompatible feature set is that if there is a bit set
	 * in the incompatible feature set that the kernel doesn't
	 * know about, it should refuse to mount the filesystem.
	 * 
	 * e2fsck's requirements are more strict; if it doesn't know
	 * about a feature in either the compatible or incompatible
	 * feature set, it must abort and not try to meddle with
	 * things it doesn't understand...
	 */
	u32	s_first_ino; 		/* First non-reserved inode */
	u16   s_inode_size; 		/* size of inode structure */
	u16	s_block_group_nr; 	/* block group # of this superblock */
	u32	s_feature_compat; 	/* compatible feature set */
	u32	s_feature_incompat; 	/* incompatible feature set */
	u32	s_feature_ro_compat; 	/* readonly-compatible feature set */
	u8	s_uuid[16];		/* 128-bit uuid for volume */
	char	s_volume_name[16]; 	/* volume name */
	char	s_last_mounted[64]; 	/* directory where last mounted */
	u32	s_algorithm_usage_bitmap; /* For compression */
	/*
	 * Performance hints.  Directory preallocation should only
	 * happen if the EXT2_COMPAT_PREALLOC flag is on.
	 */
	u8	s_prealloc_blocks;	/* Nr of blocks to try to preallocate*/
	u8	s_prealloc_dir_blocks;	/* Nr to preallocate for dirs */
	u16	s_padding1;
	/*
	 * Journaling support valid if EXT3_FEATURE_COMPAT_HAS_JOURNAL set.
	 */
	u8	s_journal_uuid[16];	/* uuid of journal superblock */
	u32	s_journal_inum;		/* inode number of journal file */
	u32	s_journal_dev;		/* device number of journal file */
	u32	s_last_orphan;		/* start of list of inodes to delete */
	u32	s_hash_seed[4];		/* HTREE hash seed */
	u8	s_def_hash_version;	/* Default hash version to use */
	u8	s_jnl_backup_type;
	u16	s_reserved_word_pad;
	u32	s_default_mount_opts;
 	u32	s_first_meta_bg; 	/* First metablock block group */
	u32	s_mkfs_time;		/* When the filesystem was created */
	u32	s_jnl_blocks[17]; 	/* Backup of the journal inode */
	u32	s_reserved[172];	/* Padding to the end of the block */
};

/*
 * Codes for operating systems
 */
#define EXT2_OS_LINUX		0
#define EXT2_OS_HURD		1
#define EXT2_OS_MASIX		2
#define EXT2_OS_FREEBSD		3
#define EXT2_OS_LITES		4

/*
 * Revision levels
 */
#define EXT2_GOOD_OLD_REV	0	/* The good old (original) format */
#define EXT2_DYNAMIC_REV	1 	/* V2 format w/ dynamic inode sizes */

#define EXT2_CURRENT_REV	EXT2_GOOD_OLD_REV
#define EXT2_MAX_SUPP_REV	EXT2_DYNAMIC_REV

#define EXT2_GOOD_OLD_INODE_SIZE 128

/*
 * Journal inode backup types
 */
#define EXT3_JNL_BACKUP_BLOCKS	1

/*
 * Feature set definitions
 */
#define EXT2_FEATURE_COMPAT_DIR_PREALLOC	0x0001
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES	0x0002
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL		0x0004
#define EXT2_FEATURE_COMPAT_EXT_ATTR		0x0008
#define EXT2_FEATURE_COMPAT_RESIZE_INODE	0x0010
#define EXT2_FEATURE_COMPAT_DIR_INDEX		0x0020
#define EXT2_FEATURE_COMPAT_ANY			0xffffffff

#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER	0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE	0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR	0x0004
#define EXT2_FEATURE_RO_COMPAT_ANY		0xffffffff

#define EXT2_FEATURE_INCOMPAT_COMPRESSION	0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE		0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER		0x0004
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV	0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG		0x0010
#define EXT2_FEATURE_INCOMPAT_ANY		0xffffffff

/*
 * Default values for user and/or group using reserved blocks
 */
#define	EXT2_DEF_RESUID		0
#define	EXT2_DEF_RESGID		0

/*
 * Default mount options
 */
#define EXT2_DEFM_DEBUG		0x0001
#define EXT2_DEFM_BSDGROUPS	0x0002
#define EXT2_DEFM_XATTR_USER	0x0004
#define EXT2_DEFM_ACL		0x0008
#define EXT2_DEFM_UID16		0x0010
    /* Not used by ext2, but reserved for use by ext3 */
#define EXT3_DEFM_JMODE		0x0060 
#define EXT3_DEFM_JMODE_DATA	0x0020
#define EXT3_DEFM_JMODE_ORDERED	0x0040
#define EXT3_DEFM_JMODE_WBACK	0x0060

/*
 * Structure of a directory entry
 */
#define EXT2_NAME_LEN 255

struct ext2_dir_entry {
	u32	inode;			/* Inode number */
	u16	rec_len;		/* Directory entry length */
	u16	name_len;		/* Name length */
	char	name[EXT2_NAME_LEN];	/* File name */
};

/*
 * The new version of the directory entry.  Since EXT2 structures are
 * stored in intel byte order, and the name_len field could never be
 * bigger than 255 chars, it's safe to reclaim the extra byte for the
 * file_type field.
 */
struct ext2_dir_entry_2 {
	u32	inode;			/* Inode number */
	u16	rec_len;		/* Directory entry length */
	u8	name_len;		/* Name length */
	u8	file_type;
	char	name[EXT2_NAME_LEN];	/* File name */
};

/*
 * Ext2 directory file types.  Only the low 3 bits are used.  The
 * other bits are reserved for now.
 */
enum {
	EXT2_FT_UNKNOWN,
	EXT2_FT_REG_FILE,
	EXT2_FT_DIR,
	EXT2_FT_CHRDEV,
	EXT2_FT_BLKDEV,
	EXT2_FT_FIFO,
	EXT2_FT_SOCK,
	EXT2_FT_SYMLINK,
	EXT2_FT_MAX
};

/*
 * EXT2_DIR_PAD defines the directory entries boundaries
 *
 * NOTE: It must be a multiple of 4
 */
#define EXT2_DIR_PAD		 	4
#define EXT2_DIR_ROUND 			(EXT2_DIR_PAD - 1)
#define EXT2_DIR_REC_LEN(name_len)	(((name_len) + 8 + EXT2_DIR_ROUND) & \
					 ~EXT2_DIR_ROUND)

#ifdef __cplusplus
class Ext2File;
class Ext2FS : public VFS::Filesystem
{
    friend class Ext2File;

public:
                  Ext2FS (VFS::Device *d);
                 ~Ext2FS();
    int           init();
    
    static int   probe (VFS::Device *dev);
    
    virtual VFS::File *open (const char *path, int flags);
    virtual VFS::Dir  *opendir (const char *path);
    
    virtual int   mkdir (const char *path);
    virtual int   rmdir (const char *path);
    virtual int   unlink (const char *path);
    
    virtual int   rename (const char *oldpath, const char *newpath);
    virtual int   link (const char *oldpath, const char *newpath);
    virtual int   symlink (const char *dest, const char *path);
    virtual int   readlink (const char *path, char *buf, int len);
    virtual int   lstat (const char *path, struct my_stat *st);
    virtual int   chmod (const char *path, int mode);
    virtual int   chown (const char *path, int uid, int gid);

    void setWritable (int flag) { _writable = flag; }
    
protected:
    s32           lookup (const char *path, int resolve_final_symlink = 0);
    s32           sublookup (u32 root_ino, const char *path, int resolve_final_symlink);
    ext2_inode   *geti (u32 ino);
    // Saves the inode and frees its memory.
    void          puti (u32 ino, ext2_inode *inode);
    // Makes the entry in the dir. Does not change link count.
    int          _link (u32 dirino, u32 ino, const char *name, int type = EXT2_FT_REG_FILE);
    // Removes the entry in the dir. Does not change link count.
    int          _unlink (u32 dirino, const char *name);
    // Returns the new inode.
    s32           creat (const char *path, int type, int mode = 0644);

    int           readfile (void *buf, ext2_inode *inode);
    int           readiblocks (u8 *buf, u32 *iblock, int n);
    int           readiiblocks (u8 *buf, u32 *diblock, int n);
    int           readiiiblocks (u8 *buf, u32 *tiblock, int n);

    int           readblocks (void *buf, u32 block, int len); // len in bytes
    int           writeblocks (void *buf, u32 block, int len);

    /* Endian conversions */
    void          sb_to_little();
    void          sb_to_cpu();
    void          gdesc_to_little();
    void          gdesc_to_cpu();
    void          inode_to_little (ext2_inode *i);
    void          inode_to_cpu (ext2_inode *i);

private:
    int _ok;
    int _writable;
    int _blocksize, _blocksize_bits;
    int _groups; u32 _inodes; u64 _blocks;
    int _blk_per_group, _ino_per_group, _ino_per_blk;
    ext2_super_block *_sb;
    ext2_group_desc  *_group_desc; /* array of all of 'em */
};

class Ext2File : public VFS::BlockFile
{
    friend class Ext2FS;

public:
    Ext2File (Ext2FS *ext2, u32 ino, int writable);
    ~Ext2File() {
        if (_open) close();
        delete _inode;
        if (_iblock) delete[] _iblock;
        if (_diblock) delete[] _diblock;
        if (_tiblock) delete[] _tiblock;
    }

    virtual int chown (int uid, int gid = -1) {
        if (!_rw) return -EROFS;
        if (uid >= 0) _inode->i_uid = uid;
        if (gid >= 0) _inode->i_gid = gid;
        return 0;
    }
    virtual int chmod (int mode) {
        if (!_rw) return -EROFS;
        _inode->i_mode = (_inode->i_mode & ~07777) | (mode & 07777);
        return 0;
    }
    virtual int truncate() {
        if (!_rw) return -EROFS;
        kill_file (1);
        _pos = _size = 0;
        return 0;
    }
    virtual int stat (struct my_stat *st) {
        st->st_dev = 0;
        st->st_ino = _ino;
        st->st_mode = _inode->i_mode;
        st->st_nlink = _inode->i_links_count;
        st->st_uid = _inode->i_uid;
        st->st_gid = _inode->i_gid;
        st->st_rdev = 0;
        st->st_size = _inode->i_size;
        st->st_blksize = 512;
        st->st_blocks = _inode->i_blocks;
        st->st_atime = _inode->i_atime;
        st->st_ctime = _inode->i_ctime;
        st->st_mtime = _inode->i_mtime;
        return 0;
    }

    virtual int close();

    void inc_nlink() { _inode->i_links_count++; }
    void dec_nlink() { _inode->i_links_count--; }

protected:
    virtual int readblock (void *buf, u32 block);
    virtual int writeblock (void *buf, u32 block);

    s32 translateblock (u32 lblock);
    s32 balloc (int group = -1);

    void kill_file (int clri = 0); // deallocate all blocks
    void killblock (s32 blk);
    void killiblock (u32 *iblock, u32 iblknr, int clri);
    void killiiblock (u32 *diblock, u32 diblknr, int clri);
    void killiiiblock (u32 *tiblock, u32 tiblknr, int clri);
                       
private:
    Ext2FS     *_ext2;
    u32         _ino;
    ext2_inode *_inode;
    u32        *_iblock, *_diblock, *_tiblock;
    int         _rw, _open;
};

class Ext2Dir : public Ext2File, public VFS::Dir
{
public:
    Ext2Dir (Ext2FS *ext2, u32 ino);
    ~Ext2Dir() {}
    
    virtual int close();
    virtual int readdir (struct VFS::dirent *buf);

private:
    u32 _off;
};
#endif

#endif	/* _LINUX_EXT2_FS_H */
