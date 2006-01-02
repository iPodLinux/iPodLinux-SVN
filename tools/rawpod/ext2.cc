/* fs/ext2.cc -*- C++ -*- Copyright (c) 2005 Joshua Oreman.
 * XenOS, of which this file is a part, is licensed under the
 * GNU General Public License. See the file COPYING in the
 * source distribution for details.
 */

#include "vfs.h"
#include "ext2.h"
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <time.h>
using std::cerr;
using VFS::ErrorFile;
using VFS::ErrorDir;

#ifndef MIN
#define MIN(x,y) (((x)<(y))?(x):(y))
#endif

u32 getTime() 
{
    return time (0);
}

Ext2FS::Ext2FS (VFS::Device *d) 
    : VFS::Filesystem (d), _ok (0), _writable (1), _sb (0), _group_desc (0)
{}

Ext2FS::~Ext2FS()
{
    if (_sb) delete _sb;
    if (_group_desc) delete[] _group_desc;
}

int Ext2FS::init() 
{
    _sb = new ext2_super_block;
    _device->lseek (0x400, SEEK_SET);
    if (_device->read (_sb, sizeof(*_sb)) < 0) {
        cerr << "EXT2-fs IO error reading superblock\n";
        return -EIO;
    }
    sb_to_cpu();
    if (_sb->s_magic != EXT2_SUPER_MAGIC) {
        cerr << "EXT2-fs error: bad magic in superblock, probably not an EXT2 filesystem\n";
        return -EINVAL;
    }
    if (_sb->s_feature_ro_compat & ~(EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER | EXT2_FEATURE_RO_COMPAT_LARGE_FILE)) {
        cerr << "EXT2-fs has R/O-compat features set that I don't know about, mounting read-only\n";
        _writable = 0;
    }
    if (_sb->s_feature_incompat & ~EXT2_FEATURE_INCOMPAT_FILETYPE) {
        cerr << "EXT2-fs has INCOMPAT features set that I don't know about - sorry, I can't mount it\n";
        return -EINVAL;
    }
    _blocks = _sb->s_blocks_count;
    _inodes = _sb->s_inodes_count;
    _blocksize_bits = _sb->s_log_block_size + 10;
    _blocksize = 1 << _blocksize_bits;
    _blk_per_group = _sb->s_blocks_per_group;
    _ino_per_group = _sb->s_inodes_per_group;
    if (!_sb->s_inode_size) _sb->s_inode_size = sizeof(ext2_inode);
    _ino_per_blk = _blocksize / _sb->s_inode_size;
    _groups = (_blocks + (_blk_per_group - 1)) / _blk_per_group;

    _group_desc = new ext2_group_desc[_groups];
    if (readblocks (_group_desc, 1 + _sb->s_first_data_block, _groups * sizeof(ext2_group_desc)) < 0) {
        cerr << "EXT2-fs IO error reading group descriptors\n";
        return -EIO;
    }

    _ok = 1;
    return 0;
}

int Ext2FS::readblocks (void *buf, u32 block, int len) 
{
    int ret;
    
    _device->lseek (block << _blocksize_bits, SEEK_SET);
    ret = _device->read (buf, len);
    
    if (ret < 0) {
        switch (_sb->s_errors) {
        case EXT2_ERRORS_CONTINUE:
            cerr << "EXT2-fs warning: IO error reading block " << block << " for "
                      << len << " bytes; continuing.\n";
            break;
        case EXT2_ERRORS_RO:
            cerr << "EXT2-fs error: IO error reading block " << block << " for "
                      << len << " bytes. Remounting read-only.\n";
            _writable = 0;
            break;
        case EXT2_ERRORS_PANIC:
            cerr << "EXT2-fs error: IO error reading block " << block << " for"
                      << len << " bytes.\n";
            printf ("EXT2-fs error mode set to PANIC, quitting\n");
            exit (255);
        }
    }
    return ret;
}
int Ext2FS::writeblocks (void *buf, u32 block, int len) 
{
    int ret;
    
    if (!_writable)
        return -EROFS;

    _device->lseek (block << _blocksize_bits, SEEK_SET);
    ret = _device->write (buf, len);
    
    if (ret < 0) {
        switch (_sb->s_errors) {
        case EXT2_ERRORS_CONTINUE:
            cerr << "EXT2-fs warning: IO error writing block " << block << " for "
                      << len << " bytes; continuing.\n";
            break;
        case EXT2_ERRORS_RO:
            cerr << "EXT2-fs error: IO error writing block " << block << " for "
                      << len << " bytes. Remounting read-only.\n";
            _writable = 0;
            break;
        case EXT2_ERRORS_PANIC:
            cerr << "EXT2-fs error: IO error writing block " << block << " for"
                      << len << " bytes.\n";
            printf ("EXT2-fs error mode set to PANIC, quitting\n");
            exit (255);
        }
    }
    return ret;
}

#ifdef linux
#include <arpa/inet.h>

u32 swab32 (u32 x) 
{
    return (((x & 0xff000000) >> 24) | ((x & 0x00ff0000) >> 8) |
            ((x & 0x0000ff00) << 8) | ((x & 0x000000ff) << 24));
}

u16 swab16 (u16 x) 
{
    return (((x & 0xff00) >> 8) | ((x & 0x00ff) << 8));
}

#define C2L32(x) x = swab32(htonl(x))
#define C2L16(x) x = swab16(htons(x))
#define L2C32(x) x = ntohl(swab32(x))
#define L2C16(x) x = ntohs(swab16(x))
#else
#define C2L32(x)
#define C2L16(x)
#define L2C32(x)
#define L2C16(x)
#endif

void Ext2FS::sb_to_little() 
{
    C2L32 (_sb->s_inodes_count);
    C2L32 (_sb->s_blocks_count);
    C2L32 (_sb->s_r_blocks_count);
    C2L32 (_sb->s_free_blocks_count);
    C2L32 (_sb->s_free_inodes_count);
    C2L32 (_sb->s_first_data_block);
    C2L32 (_sb->s_log_block_size);
    C2L32 (_sb->s_log_frag_size);
    C2L32 (_sb->s_blocks_per_group);
    C2L32 (_sb->s_frags_per_group);
    C2L32 (_sb->s_inodes_per_group);
    C2L32 (_sb->s_mtime);
    C2L32 (_sb->s_wtime);
    C2L16 (_sb->s_mnt_count);
    C2L16 (_sb->s_max_mnt_count);
    C2L16 (_sb->s_magic);
    C2L16 (_sb->s_state);
    C2L16 (_sb->s_errors);
    C2L16 (_sb->s_minor_rev_level);
    C2L32 (_sb->s_lastcheck);
    C2L32 (_sb->s_checkinterval);
    C2L32 (_sb->s_creator_os);
    C2L32 (_sb->s_rev_level);
    C2L16 (_sb->s_def_resuid);
    C2L16 (_sb->s_def_resgid);
    C2L32 (_sb->s_first_ino);
    C2L16 (_sb->s_inode_size);
    C2L16 (_sb->s_block_group_nr);
    C2L32 (_sb->s_feature_compat);
    C2L32 (_sb->s_feature_incompat);
    C2L32 (_sb->s_feature_ro_compat);
    C2L32 (_sb->s_algorithm_usage_bitmap);
    C2L32 (_sb->s_journal_inum);
    C2L32 (_sb->s_journal_dev);
    C2L32 (_sb->s_last_orphan);
    C2L16 (_sb->s_reserved_word_pad);
    C2L32 (_sb->s_default_mount_opts);
    C2L32 (_sb->s_first_meta_bg);
}

void Ext2FS::sb_to_cpu()
{
    L2C32 (_sb->s_inodes_count);
    L2C32 (_sb->s_blocks_count);
    L2C32 (_sb->s_r_blocks_count);
    L2C32 (_sb->s_free_blocks_count);
    L2C32 (_sb->s_free_inodes_count);
    L2C32 (_sb->s_first_data_block);
    L2C32 (_sb->s_log_block_size);
    L2C32 (_sb->s_log_frag_size);
    L2C32 (_sb->s_blocks_per_group);
    L2C32 (_sb->s_frags_per_group);
    L2C32 (_sb->s_inodes_per_group);
    L2C32 (_sb->s_mtime);
    L2C32 (_sb->s_wtime);
    L2C16 (_sb->s_mnt_count);
    L2C16 (_sb->s_max_mnt_count);
    L2C16 (_sb->s_magic);
    L2C16 (_sb->s_state);
    L2C16 (_sb->s_errors);
    L2C16 (_sb->s_minor_rev_level);
    L2C32 (_sb->s_lastcheck);
    L2C32 (_sb->s_checkinterval);
    L2C32 (_sb->s_creator_os);
    L2C32 (_sb->s_rev_level);
    L2C16 (_sb->s_def_resuid);
    L2C16 (_sb->s_def_resgid);
    L2C32 (_sb->s_first_ino);
    L2C16 (_sb->s_inode_size);
    L2C16 (_sb->s_block_group_nr);
    L2C32 (_sb->s_feature_compat);
    L2C32 (_sb->s_feature_incompat);
    L2C32 (_sb->s_feature_ro_compat);
    L2C32 (_sb->s_algorithm_usage_bitmap);
    L2C32 (_sb->s_journal_inum);
    L2C32 (_sb->s_journal_dev);
    L2C32 (_sb->s_last_orphan);
    L2C16 (_sb->s_reserved_word_pad);
    L2C32 (_sb->s_default_mount_opts);
    L2C32 (_sb->s_first_meta_bg);
}
    
void Ext2FS::gdesc_to_little() 
{
    int i;
    ext2_group_desc *d;
    for (i = 0, d = _group_desc; i < _groups; i++, d++) {
        C2L32 (d->bg_block_bitmap);
        C2L32 (d->bg_inode_bitmap);
        C2L32 (d->bg_inode_table);
        C2L16 (d->bg_free_blocks_count);
        C2L16 (d->bg_free_inodes_count);
        C2L16 (d->bg_used_dirs_count);
    }
}
void Ext2FS::gdesc_to_cpu()
{
    int i;
    ext2_group_desc *d;
    for (i = 0, d = _group_desc; i < _groups; i++, d++) {
        L2C32 (d->bg_block_bitmap);
        L2C32 (d->bg_inode_bitmap);
        L2C32 (d->bg_inode_table);
        L2C16 (d->bg_free_blocks_count);
        L2C16 (d->bg_free_inodes_count);
        L2C16 (d->bg_used_dirs_count);
    }
}

void Ext2FS::inode_to_little (ext2_inode *i) 
{
    C2L16 (i->i_mode);
    C2L16 (i->i_uid);
    C2L32 (i->i_size);
    C2L32 (i->i_atime);
    C2L32 (i->i_ctime);
    C2L32 (i->i_mtime);
    C2L32 (i->i_dtime);
    C2L16 (i->i_gid);
    C2L16 (i->i_links_count);
    C2L32 (i->i_blocks);
    C2L32 (i->i_flags);
    C2L32 (i->i_generation);
    C2L32 (i->i_file_acl);
    C2L32 (i->i_dir_acl);
    C2L32 (i->i_faddr);
    C2L16 (i->i_uid_high);
    C2L16 (i->i_gid_high);

    for (int n = 0; n < EXT2_N_BLOCKS; n++)
        C2L32 (i->i_block[n]);
}
void Ext2FS::inode_to_cpu (ext2_inode *i) 
{
    L2C16 (i->i_mode);
    L2C16 (i->i_uid);
    L2C32 (i->i_size);
    L2C32 (i->i_atime);
    L2C32 (i->i_ctime);
    L2C32 (i->i_mtime);
    L2C32 (i->i_dtime);
    L2C16 (i->i_gid);
    L2C16 (i->i_links_count);
    L2C32 (i->i_blocks);
    L2C32 (i->i_flags);
    L2C32 (i->i_generation);
    L2C32 (i->i_file_acl);
    L2C32 (i->i_dir_acl);
    L2C32 (i->i_faddr);
    L2C16 (i->i_uid_high);
    L2C16 (i->i_gid_high);

    for (int n = 0; n < EXT2_N_BLOCKS; n++) {
        L2C32 (i->i_block[n]);
    }
}

ext2_inode *Ext2FS::geti (u32 ino) 
{
    if (ino == 0) return 0;

    ino--; // e2fs inodes are numbered from 1 up but stored from 0 up.
    
    int group = ino / _ino_per_group;
    int ino_off = ino % _ino_per_group;
    ext2_inode *ret = new ext2_inode;
    
    _device->lseek ((_group_desc[group].bg_inode_table << _blocksize_bits) + (ino_off * _sb->s_inode_size), SEEK_SET);
    _device->read (ret, _sb->s_inode_size);
    return ret;
}

// Used mainly for reading directories - you wouldn't want to read
// most files all at once.
int Ext2FS::readiblocks (u8 *buf, u32 *iblock, int n)
{
    int i, ret;
    int blk_per_blk = _blocksize >> 2;

    for (i = 0; i < blk_per_blk && n > 0; i++) {
        if (iblock[i]) {
            if ((ret = readblocks (buf, iblock[i], MIN (n, _blocksize))) < 0) {
                return ret;
            }
        }
        buf += _blocksize;
        n -= _blocksize;
    }
}
int Ext2FS::readiiblocks (u8 *buf, u32 *diblock, int n)
{
    int i, ret;
    int blk_per_blk = _blocksize >> 2;
    u32 *iblock = new u32[blk_per_blk];

    for (i = 0; i < blk_per_blk && n > 0; i++) {
        if (diblock[i]) {
            if ((ret = readblocks (iblock, diblock[i], _blocksize)) < 0) {
                delete[] iblock;
                return ret;
            }
            if ((ret = readiblocks (buf, iblock, n)) < 0) {
                delete[] iblock;
                return ret;
            }
        }
        buf += _blocksize << (_blocksize_bits - 2);
        n -= _blocksize << (_blocksize_bits - 2);
    }

    delete[] iblock;
}
int Ext2FS::readiiiblocks (u8 *buf, u32 *tiblock, int n) 
{
    int i, ret;
    int blk_per_blk = _blocksize >> 2;
    u32 *diblock = new u32[blk_per_blk];
    
    for (i = 0; i < blk_per_blk && n > 0; i++) {
        if (tiblock[i]) {
            if ((ret = readblocks (diblock, tiblock[i], _blocksize)) < 0) {
                delete[] diblock;
                return ret;
            }
            if ((ret = readiiblocks (buf, diblock, n)) < 0) {
                delete[] diblock;
                return ret;
            }
        }
        buf += _blocksize << ((_blocksize_bits << 1) - 4);
        n -= _blocksize << ((_blocksize_bits << 1) - 4);
    }

    delete[] diblock;
}

int Ext2FS::readfile (void *b, ext2_inode *inode) 
{
    int i, ret;
    int blk_per_blk = _blocksize >> 2;
    u32 *iblock = new u32[blk_per_blk];
    u8 *buf = (u8 *)b; // so we can add with byte offsets
    int n = inode->i_size;
    
    // Direct blocks
    for (i = 0; i < EXT2_NDIR_BLOCKS && n > 0; i++) {
        if (inode->i_block[i]) {
            if ((ret = readblocks (buf, inode->i_block[i], MIN (n, _blocksize))) < 0) {
                delete[] iblock;
                return ret;
            }
        }
        buf += _blocksize;
        n -= _blocksize;
    }

    // Indirect
    if (inode->i_block[EXT2_IND_BLOCK] && n > 0) {
        if ((ret = readblocks (iblock, inode->i_block[EXT2_IND_BLOCK], _blocksize)) < 0) {
            delete[] iblock;
            return ret;
        }
        if ((ret = readiblocks (buf, iblock, n)) < 0) {
            delete[] iblock;
            return ret;
        }
    }
    buf += _blocksize << (_blocksize_bits - 2);
    n -= _blocksize << (_blocksize_bits - 2);

    // Double-indirect
    if (inode->i_block[EXT2_DIND_BLOCK] && n > 0) {
        if ((ret = readblocks (iblock, inode->i_block[EXT2_DIND_BLOCK], _blocksize)) < 0) {
            delete[] iblock;
            return ret;
        }
        if ((ret = readiiblocks (buf, iblock, n)) < 0) {
            delete[] iblock;
            return ret;
        }
    }
    buf += _blocksize << ((_blocksize_bits << 1) - 4);
    n -= _blocksize << ((_blocksize_bits << 1) - 4);

    // Triple-indirect
    if (inode->i_block[EXT2_TIND_BLOCK] && n > 0) {
        if ((ret = readblocks (iblock, inode->i_block[EXT2_TIND_BLOCK], _blocksize)) < 0) {
            delete[] iblock;
            return ret;
        }
        if ((ret = readiiiblocks (buf, iblock, n)) < 0) {
            delete[] iblock;
            return ret;
        }
    }

    delete[] iblock;
    return 0;
}

s32 Ext2FS::sublookup (u32 root, const char *path, int resolve_final_symlink) 
{
    static int depth;
    u32 ino = root, dirino = 0;
    ext2_inode *inode;
    char *component = new char[257];
    const char *p = path;

    depth++;
    if (depth > 15) {
        delete[] component;
        depth--;
        return -ELOOP;
    }

    while (p) {
        while (*p == '/') p++;
        if (!*p) break;

        inode = geti (ino);

        int len;
        if (strchr (p, '/'))
            len = strchr (p, '/') - p;
        else
            len = strlen (p);

        if (len > 255) {
            delete[] component;
            delete inode;
            depth--;
            return -ENAMETOOLONG;
        }

        strncpy (component, p, len);
        component[len] = 0;
        
        int dirsize = inode->i_size;
        u8 *dirdata = new u8[dirsize], *dirp = dirdata;
        ext2_dir_entry_2 *de;
        
        readfile (dirdata, inode);
        while (dirp < dirdata + dirsize) {
            de = (ext2_dir_entry_2 *)dirp;
            if (de->name_len == len) {
                if (!memcmp (de->name, component, de->name_len)) {
                    if (de->inode) {
                        delete inode;
                        dirino = ino;
                        ino = de->inode;
                        delete[] dirdata;
                        goto good;
                    }
                }
            }
            if (!de->rec_len) break;
            dirp += de->rec_len;
        }
        delete inode;
        delete[] dirdata;
        delete[] component;
        depth--;
        return -ENOENT;

    good:
        if (strchr (p, '/') || resolve_final_symlink) {
            inode = geti (ino);
            if ((inode->i_mode & 0770000) == 0120000) { // follow link
                char *linktarget = new char[inode->i_size + 1];
                if (inode->i_blocks == 0) { // fast symlink
                    memcpy (linktarget, inode->i_block, inode->i_size);
                } else {
                    readfile (linktarget, inode);
                }
                linktarget[inode->i_size] = 0;
                
                // Figure out the complete rest-of-the-path
                char *rest;
                if (strchr (p, '/')) {
                    rest = new char[inode->i_size + strlen (strchr (p, '/')) + 1];
                    strcpy (rest, linktarget);
                    strcat (rest, strchr (p, '/'));
                } else {
                    rest = new char[inode->i_size + 1];
                    strcpy (rest, linktarget);
                }

                // Cleanup...
                delete[] linktarget;
                delete[] component;
                delete inode;

                // And do it!
                s32 ret = sublookup (dirino, rest, resolve_final_symlink);

                delete[] rest;
                depth--;
                return ret;
            } else if (strchr (p, '/') && (inode->i_mode & 0770000) != 0040000) { // not a dir
                delete inode;
                delete[] component;
                depth--;
                return -ENOTDIR;
            }
            delete inode;
        }

        p = strchr (p, '/'); // if no more components left, will exit the loop.
    }

    delete[] component;
    depth--;
    return ino;
}

s32 Ext2FS::lookup (const char *path, int resolve_final_symlink) 
{
    return sublookup (EXT2_ROOT_INO, path, resolve_final_symlink);
}

int Ext2FS::probe (VFS::Device *dev) 
{
    ext2_super_block *sb = new ext2_super_block;
    sb->s_magic = 0;
    dev->lseek (1024, SEEK_SET);
    dev->read (sb, sizeof(*sb));

    u32 magic = sb->s_magic;
    delete sb;
    
    if (magic != EXT2_SUPER_MAGIC)
        return 0;
    return 1;
}

// To use, *(u32 *)newdir = ino, *(u32 *)(newdir + 12) = dirino,
// *(u16 *)(newdir + 28) = blocksize - 24, write newdir to the dir.
static u8 newdir[] = {
    0x00, 0x00, 0x00, 0x00,     /* this inode */
    0x0C, 0x00,                 /* rec_len */
    0x01,                       /* name_len */
    EXT2_FT_DIR,                /* file_type */
    '.',                        /* name */
    0x00, 0x00, 0x00,           /* padding */

    0x00, 0x00, 0x00, 0x00,     /* inode of parent dir */
    0x0C, 0x00,                 /* rec_len */
    0x02,                       /* name_len */
    EXT2_FT_DIR,                /* file_type */
    '.', '.',                   /* name */
    0x00, 0x00,                 /* padding */
    
    0x00, 0x00, 0x00, 0x00,     /* zero */
    0x00, 0x00,                 /* blocksize - 24 */
    0x00,
    EXT2_FT_UNKNOWN,
    0x00, 0x00, 0x00, 0x00
    // ...
};

int Ext2FS::mkdir (const char *path) 
{
    s32 ino = lookup (path);
    if (ino >= 0)
        return -EEXIST;
    if (!_writable)
        return -EROFS;

    ext2_inode *inode;
    
    // Check to make sure the parent directory exists.
    char *dir = new char[strlen (path) + 1];
    
    strcpy (dir, path);
    if (!strrchr (dir, '/')) {
        strcpy (dir, "/");
    } else {
        *(strrchr (dir, '/') + 1) = 0; // leave trailing slash
    }

    s32 dirino = lookup (dir);
    delete[] dir;
    if (dirino < 0) {
        return dirino;
    }

    ino = creat (path, EXT2_FT_DIR);
    if (ino < 0)
        return ino;
    
    Ext2File *fp = new Ext2File (this, ino, 1);

    fp->_inode->i_mode &= 07777;
    fp->_inode->i_mode |= S_IFDIR;
    fp->_inode->i_mode |= 00111; // exec/search perms

    // Write a full block of zeroes.
    u8 *zeroes = new u8[_blocksize];
    memset (zeroes, 0, _blocksize);
    fp->write (zeroes, _blocksize);
    delete[] zeroes;

    // Write the new-directory data.
    *(u32 *)(newdir +  0) = fp->_ino;
    *(u32 *)(newdir + 12) = dirino;
    *(u16 *)(newdir + 28) = _blocksize - 24;
    fp->lseek (0, SEEK_SET);
    fp->write (newdir, 36);
    
    // Fix the link count. The new directory's should be 2; that is, add 1.
    fp->inc_nlink();
    
    // We're done with the stuff for the new dir.
    fp->close();
    delete fp;
    
    // Bump the block group's used_dirs_count.
    int group = (ino - 1) / _ino_per_group;
    _group_desc[group].bg_used_dirs_count++;

    // Save it.
    gdesc_to_little();
    writeblocks (_group_desc, 1 + _sb->s_first_data_block, _groups * sizeof(ext2_group_desc));
    gdesc_to_cpu();

    return 0;
}

int Ext2FS::rmdir (const char *path) 
{
    if (!_writable)
        return -EROFS;

    s32 ino = lookup (path);
    if (ino < 0) return ino;

    // Decrement the block group's used_dirs_count.
    int group = (ino - 1) / _ino_per_group;
    _group_desc[group].bg_used_dirs_count--;

    // Save it.
    gdesc_to_little();
    writeblocks (_group_desc, 1 + _sb->s_first_data_block, _groups * sizeof(ext2_group_desc));
    gdesc_to_cpu();

    // Do the unlink. XXX this doesn't check if the dir is empty.
    return unlink (path);
}

int Ext2FS::unlink (const char *path) 
{
    if (!_writable)
        return -EROFS;

    s32 ino = lookup (path);
    if (ino < 0)
        return ino;

    char *dir = new char[strlen (path) + 1];
    const char *file;
    int nlink_dec = 0;
    
    strcpy (dir, path);
    if (!strrchr (dir, '/')) {
        strcpy (dir, "/");
        file = path;
    } else {
        *(strrchr (dir, '/') + 1) = 0; // leave trailing slash
        file = strrchr (path, '/') + 1;
    }
    
    s32 dirino = lookup (dir);
    delete[] dir;
    if (dirino < 0) {
        return dirino;
    }
    
    ext2_inode *dirinode = geti (dirino);
    int mode = dirinode->i_mode;
    delete dirinode;
    if (!S_ISDIR (mode))
        return -ENOTDIR;

    _unlink (dirino, file);
    
    Ext2File *fp = new Ext2File (this, ino, 1);
    int clrbit = 0;
    
    fp->dec_nlink();
    if (fp->_inode->i_links_count <= 0) {
        // Delete file
        fp->_inode->i_dtime = getTime();
        fp->kill_file (0);
        clrbit = 1;
    }
    fp->close();
    delete fp;
    
    if (clrbit) {
        // Clear the bit in the inode bitmap.
        int group = (ino - 1) / _ino_per_group;
        int bit = (ino - 1) % _ino_per_group;
        u8 *inode_bitmap = new u8[_blocksize];
        
        readblocks (inode_bitmap, _group_desc[group].bg_inode_bitmap, _blocksize);
        inode_bitmap[bit >> 3] &= ~(1 << (bit & 7));
        writeblocks (inode_bitmap, _group_desc[group].bg_inode_bitmap, _blocksize);

        _group_desc[group].bg_free_inodes_count++;

        gdesc_to_little();
        writeblocks (_group_desc, 1 + _sb->s_first_data_block,
                     _groups * sizeof(ext2_group_desc));
        gdesc_to_cpu();

        _sb->s_free_inodes_count++;
        
        sb_to_little();
        _device->lseek (0x400, SEEK_SET);
        _device->write (_sb, sizeof(ext2_super_block));
        sb_to_cpu();
    }
}

int Ext2FS::_unlink (u32 dirino, const char *name) 
{
    Ext2File *dirp = new Ext2File (this, dirino, 1);
    ext2_dir_entry_2 *de = new ext2_dir_entry_2;
    int ofs = 0, oldofs = -1, thislen;
    while(1) {
        if (dirp->read (de, sizeof(ext2_dir_entry_2)) <= 0) {
            return -ENOENT;
        }
        if (de->name_len == strlen (name)) {
            if (!memcmp (de->name, name, de->name_len)) {
                thislen = de->rec_len;
                break;
            }
        }
        oldofs = ofs;
        ofs += de->rec_len;
        dirp->lseek (ofs, SEEK_SET);
    }

    if (ofs == -1) return -EIO;
    
    // We extend the size of the previous entry to cover this one.
    dirp->lseek (oldofs, SEEK_SET);
    dirp->read (de, sizeof(ext2_dir_entry_2));
    de->rec_len += thislen;
    dirp->lseek (oldofs, SEEK_SET);
    dirp->write (de, 8); // write only the metadata, including the length - no need to
                         // rewrite the name
    dirp->close();
    delete dirp;
}

int Ext2FS::rename (const char *oldf, const char *newf) 
{
    int err;
    if ((err = link (oldf, newf)) < 0)
        return err;
    if ((err = unlink (oldf)) < 0)
        return err;
}

int Ext2FS::link (const char *dest, const char *src) 
{
    int err;

    s32 dino = lookup (dest);
    if (dino < 0) return dino;
    
    char *dir = new char[strlen (src) + 1];
    const char *file;
    
    strcpy (dir, src);
    if (!strrchr (dir, '/')) {
        strcpy (dir, "/");
        file = src;
    } else {
        *(strrchr (dir, '/') + 1) = 0; // leave trailing slash
        file = strrchr (src, '/') + 1;
    }
    
    s32 dirino = lookup (dir);
    delete[] dir;
    if (dirino < 0) {
        return dirino;
    }
    
    ext2_inode *dirinode = geti (dirino);
    int mode = dirinode->i_mode;
    delete dirinode;
    if (!S_ISDIR (mode))
        return -ENOTDIR;
    
    // Make the link
    if ((err = _link (dirino, dino, file)) < 0)
        return err;
    
    // Bump linkcount
    Ext2File *fp = new Ext2File (this, dino, 1);
    fp->inc_nlink();
    fp->close();
    delete fp;
    
    return 0;
}

int Ext2FS::symlink (const char *target, const char *link) 
{
    s32 ino = lookup (link);
    if (ino > 0)
        return -EEXIST;
    
    ino = creat (link, EXT2_FT_SYMLINK);
    if (ino < 0)
        return ino;
    
    Ext2File *fp = new Ext2File (this, ino, 1);
    
    if (strlen (target) < EXT2_N_BLOCKS*4) {
        // fast link
        memcpy (fp->_inode->i_block, target, strlen (target));
        fp->_inode->i_size = fp->_size = strlen (target);
    } else {
        fp->write (target, strlen (target));
    }

    fp->_inode->i_mode = S_IFLNK | 0777; // lrwxrwxrwx - default symlink perms
    
    fp->close();
    delete fp;
    return 0;
}

int Ext2FS::readlink (const char *path, char *buf, int len) 
{
    s32 ino = lookup (path);
    if (ino < 0)
        return ino;

    ext2_inode *inode = geti (ino);
    if ((inode->i_mode & 0770000) != 0120000) {
        delete inode;
        return -EINVAL;
    }
    if (!inode->i_blocks) { // fast symlink
        if (len < inode->i_size) {
            memcpy (buf, inode->i_block, len-1);
            buf[len-1] = 0;
        } else {
            memcpy (buf, inode->i_block, inode->i_size);
            buf[inode->i_size] = 0;
        }
        delete inode;
        return 0;
    }
    delete inode;

    Ext2File *fp = new Ext2File (this, ino, 0);
    fp->read (buf, len);
    delete fp;
    return 0;
}



Ext2File::Ext2File (Ext2FS *ext2, u32 ino, int writable) 
    : VFS::BlockFile (ext2->_blocksize, ext2->_blocksize_bits),
      _ext2 (ext2), _ino (ino), _iblock (0), _diblock (0), _tiblock (0), _rw (writable), _open (1)
{
    int blk_per_blk = _ext2->_blocksize >> 2;
    
    _inode = _ext2->geti (_ino);
    _size = _inode->i_size;

    if (_inode->i_block[EXT2_IND_BLOCK]) {
        _iblock = new u32[blk_per_blk];
        if (_ext2->readblocks (_iblock, _inode->i_block[EXT2_IND_BLOCK], _blocksize) < 0) {
            delete[] _iblock;
            _iblock = 0;
        }
    }
    if (_inode->i_block[EXT2_DIND_BLOCK]) {
        _diblock = new u32[blk_per_blk];
        if (_ext2->readblocks (_diblock, _inode->i_block[EXT2_DIND_BLOCK], _blocksize)< 0) {
            delete[] _diblock;
            _diblock = 0;
        }
    }
    if (_inode->i_block[EXT2_TIND_BLOCK]) {
        _tiblock = new u32[blk_per_blk];
        if (_ext2->readblocks (_tiblock, _inode->i_block[EXT2_TIND_BLOCK], _blocksize) < 0) {
            delete[] _tiblock;
            _tiblock = 0;
        }
    }
}

int Ext2File::close() 
{
    if (_rw) {
        int i = _ino - 1; // e2fs inodes are numbered from 1 up but stored from 0 up.
        _inode->i_size = _size;
        _ext2->_device->lseek ((_ext2->_group_desc[i / _ext2->_ino_per_group].bg_inode_table << _blocksize_bits) +
                               (i % _ext2->_ino_per_group) * _ext2->_sb->s_inode_size, SEEK_SET);
        _ext2->_device->write (_inode, _ext2->_sb->s_inode_size);
    }
    _open = 0;
}

static int findfreebit (u8 *bitmap, int size) 
{
    for (int i = 0; i < size; i++) {
        if (bitmap[i] != 0xff) { // 0xff == all used
            for (int j = 0; j < 8; j++) {
                if (!(bitmap[i] & (1 << j))) {
                    bitmap[i] |= (1 << j);
                    return (i << 3) + j;
                }
            }
        }
    }
    return -1;
}


void Ext2File::killblock (s32 blk) 
{
    static u8 this_block_bitmap[4096]; // 4096=max blocksize
    static int this_block_group = -1;
    int group = (blk - 1) / _ext2->_blk_per_group;
    int bit = (blk - 1) % _ext2->_blk_per_group;

    if (blk == 0) return;

    if (blk < 0) { // "flush"
        _ext2->writeblocks (this_block_bitmap, _ext2->_group_desc[this_block_group].bg_block_bitmap,
                            _blocksize);

        _ext2->sb_to_little();
        _ext2->_device->lseek (0x400, SEEK_SET);
        _ext2->_device->write (_ext2->_sb, sizeof(ext2_super_block));
        _ext2->sb_to_cpu();

        group = -1;
        return;
    }

    if (this_block_group != group) {
        if (this_block_group >= 0) {
            _ext2->writeblocks (this_block_bitmap,
                                _ext2->_group_desc[this_block_group].bg_block_bitmap,
                                _blocksize);
            _ext2->gdesc_to_little();
            _ext2->writeblocks (_ext2->_group_desc, 1 + _ext2->_sb->s_first_data_block,
                                _ext2->_groups * sizeof(ext2_group_desc));
            _ext2->gdesc_to_cpu();
        }
        this_block_group = group;
        _ext2->readblocks (this_block_bitmap, _ext2->_group_desc[group].bg_block_bitmap, _blocksize);
    }

    this_block_bitmap[bit >> 3] &= ~(1 << (bit & 7));
    _ext2->_group_desc[this_block_group].bg_free_blocks_count++;
    _ext2->_sb->s_free_blocks_count++;
}

void Ext2File::killiblock (u32 *iblock, u32 iblknr, int clri)
{
    int i, blk_per_blk = _blocksize >> 2;
    
    for (i = 0; i < blk_per_blk; i++) {
        killblock (iblock[i]);
    }

    killblock (iblknr);

    if (clri) {
        memset (iblock, 0, blk_per_blk * 4);
        _ext2->writeblocks (iblock, iblknr, _blocksize);
    }
}

void Ext2File::killiiblock (u32 *diblock, u32 diblknr, int clri) 
{
    int i, blk_per_blk = _blocksize >> 2;
    u32 *iblock = new u32[blk_per_blk];
    
    for (i = 0; i < blk_per_blk; i++) {
        if (diblock[i]) {
            _ext2->readblocks (iblock, diblock[i], _blocksize);
            killiblock (iblock, diblock[i], clri);
        }
    }

    killblock (diblknr);

    if (clri) {
        memset (diblock, 0, blk_per_blk * 4);
        _ext2->writeblocks (diblock, diblknr, _blocksize);
    }

    delete[] iblock;
}

void Ext2File::killiiiblock (u32 *tiblock, u32 tiblknr, int clri) 
{
    int i, blk_per_blk = _blocksize >> 2;
    u32 *diblock = new u32[blk_per_blk];
    
    for (i = 0; i < blk_per_blk; i++) {
        if (tiblock[i]) {
            _ext2->readblocks (diblock, tiblock[i], _blocksize);
            killiiblock (diblock, tiblock[i], clri);
        }
    }

    killblock (tiblknr);

    if (clri) {
        memset (tiblock, 0, blk_per_blk * 4);
        _ext2->writeblocks (tiblock, tiblknr, _blocksize);
    }

    delete[] diblock;
}

void Ext2File::kill_file (int clri)
{
    int i;
    if (_inode->i_blocks) { // beware the fast symlink
        for (i = 0; i < EXT2_NDIR_BLOCKS; i++) {
            killblock (_inode->i_block[i]);
        }
        
        int blk_per_blk = _blocksize >> 2;
        
        if (_iblock)  killiblock (_iblock, _inode->i_block[EXT2_IND_BLOCK], clri);
        if (_diblock) killiiblock (_diblock, _inode->i_block[EXT2_DIND_BLOCK], clri);
        if (_tiblock) killiiiblock (_tiblock, _inode->i_block[EXT2_TIND_BLOCK], clri);
        
        killblock (-1);
        _inode->i_blocks = 0;
        
        if (clri) memset (_inode->i_block, 0, EXT2_N_BLOCKS * 4);
    }
}

int Ext2File::writeblock (void *buf, u32 block) 
{
    if (!_open) return -EBADF;
    if (!_rw) return -EROFS;

    s32 pblk = translateblock (block);
    if (pblk < 0) return pblk;

    if (!pblk) {
        // Appending to a file, or filling in a sparse part. Alloc
        // a new block.
        int group = (_ino - 1) / _ext2->_ino_per_group;
        int blk_per_blk = _blocksize >> 2;
        int newblk;
        
        pblk = balloc (group);
        
        // Update the inode
        if (block < EXT2_NDIR_BLOCKS) {
            _inode->i_block[block] = pblk;
        } else {
            block -= EXT2_NDIR_BLOCKS;
            if (block < blk_per_blk) {
                if (!_iblock) {
                    if ((newblk = balloc (group)) < 0)
                        return newblk;
                    _iblock = new u32[blk_per_blk];
                    memset (_iblock, 0, _blocksize);
                    _inode->i_block[EXT2_IND_BLOCK] = newblk;
                }
                _iblock[block] = pblk;
                _ext2->writeblocks (_iblock, _inode->i_block[EXT2_IND_BLOCK], _blocksize);
            }
            else {
                block -= blk_per_blk;
                if (block < (blk_per_blk << (_blocksize_bits - 2))) {
                    if (!_diblock) {
                        if ((newblk = balloc (group)) < 0)
                            return newblk;
                        _diblock = new u32[blk_per_blk];
                        memset (_diblock, 0, _blocksize);
                        _inode->i_block[EXT2_DIND_BLOCK] = newblk;
                    }

                    if (!_diblock[block >> (_blocksize_bits - 2)]) {
                        if ((newblk = balloc (group)) < 0)
                            return newblk;
                        _diblock[block >> (_blocksize_bits - 2)] = newblk;
                        _ext2->writeblocks (_diblock, _inode->i_block[EXT2_DIND_BLOCK], _blocksize);
                    }

                    u32 *iblock = new u32[blk_per_blk];
                    if (_ext2->readblocks (iblock, _diblock[block >> (_blocksize_bits - 2)], _blocksize) < 0) {
                        delete[] iblock;
                        return -EIO;
                    }
                    iblock[block & (blk_per_blk - 1)] = pblk;
                    _ext2->writeblocks (iblock, _diblock[block >> (_blocksize_bits - 2)], _blocksize);
                    delete[] iblock;
                }
                else {
                    block -= (blk_per_blk << (_blocksize_bits - 2));
                    if (block < (blk_per_blk << ((_blocksize_bits << 1) - 4))) {
                        if (!_tiblock) {
                            if ((newblk = balloc (group)) < 0)
                                return newblk;
                            _tiblock = new u32[blk_per_blk];
                            memset (_tiblock, 0, _blocksize);
                            _inode->i_block[EXT2_TIND_BLOCK] = newblk;
                        }
                        
                        if (!_tiblock[block >> ((_blocksize_bits << 1) - 4)]) {
                            if ((newblk = balloc (group)) < 0)
                                return newblk;
                            _tiblock[block >> ((_blocksize_bits << 1) - 4)] = newblk;
                            _ext2->writeblocks (_tiblock, _inode->i_block[EXT2_TIND_BLOCK], _blocksize);
                        }

                        u32 *diblock = new u32[blk_per_blk];
                        if (_ext2->readblocks (diblock, _tiblock[block >> ((_blocksize_bits << 1) - 4)], _blocksize) < 0) {
                            delete[] diblock;
                            return -EIO;
                        }

                        if (!diblock[(block >> (_blocksize_bits - 2)) & (blk_per_blk - 1)]) {
                            if ((newblk = balloc (group)) < 0) {
                                delete[] diblock;
                                return newblk;
                            }
                            diblock[(block >> (_blocksize_bits - 2)) & (blk_per_blk - 1)] = newblk;
                            _ext2->writeblocks (diblock, _tiblock[block >> ((_blocksize_bits << 1) - 4)], _blocksize);
                        }

                        u32 *iblock = new u32[blk_per_blk];
                        if (_ext2->readblocks (iblock, diblock[(block >> (_blocksize_bits - 2)) & (blk_per_blk - 1)], _blocksize) < 0) {
                            delete[] iblock, diblock;
                            return -EIO;
                        }
                        iblock[block & (blk_per_blk - 1)] = pblk;
                        _ext2->writeblocks (iblock, diblock[(block >> (_blocksize_bits - 2)) & (blk_per_blk - 1)], _blocksize);
                        delete[] iblock, diblock;
                    } else {
                        return -E2BIG;
                    }
                }
            }
        }
    }

    int ret = _ext2->writeblocks (buf, pblk, _blocksize);
    if (ret < 0) return ret;
    return 0;
}

int Ext2File::readblock (void *buf, u32 block) 
{
    if (!_open) return -EBADF;

    s32 pblk = translateblock (block);
    if (pblk < 0) return pblk;

    if (pblk == 0) {
        u32 *b = (u32 *)buf;
        for (int i = 0; i < _blocksize>>2; i++) {
            b[i] = 0;
        }
        return 0;
    }
    
    int ret = _ext2->readblocks (buf, pblk, _blocksize);
    if (ret < 0) return ret;

    return 0;
}

s32 Ext2File::translateblock (u32 lblock)
{
    s32 pblock;
    int blk_per_blk = _blocksize >> 2;
    
    if (lblock < EXT2_NDIR_BLOCKS) {
        pblock = _inode->i_block[lblock];
    }
    else {
        lblock -= EXT2_NDIR_BLOCKS;
        if (lblock < blk_per_blk) {
            if (_iblock)
                pblock = _iblock[lblock];
            else
                pblock = 0;
        }
        else {
            lblock -= blk_per_blk;
            if (lblock < (blk_per_blk << (_blocksize_bits - 2))) {
                if (!_diblock)
                    pblock = 0;
                else if (!_diblock[lblock >> (_blocksize_bits - 2)])
                    pblock = 0;
                else {
                    u32 *iblock = new u32[blk_per_blk];
                    if (_ext2->readblocks (iblock, _diblock[lblock >> (_blocksize_bits - 2)], _blocksize) < 0) {
                        pblock = -EIO;
                    } else {
                        pblock = iblock[lblock & (blk_per_blk - 1)];
                    }
                    delete[] iblock;
                }
            }
            else {
                lblock -= (blk_per_blk << (_blocksize_bits - 2));
                if (lblock < (blk_per_blk << ((_blocksize_bits << 1) - 4))) {
                    if (!_tiblock)
                        pblock = 0;
                    else if (!_tiblock[lblock >> ((_blocksize_bits << 1) - 4)])
                        pblock = 0;
                    else {
                        u32 *diblock = new u32[blk_per_blk];
                        if (_ext2->readblocks (diblock, _tiblock[lblock >> ((_blocksize_bits << 1) - 4)], _blocksize) < 0) {
                            pblock = -EIO;
                        }
                        else if (!diblock[(lblock >> (_blocksize_bits - 2)) & (blk_per_blk - 1)])
                            pblock = 0;
                        else {
                            u32 *iblock = new u32[blk_per_blk];
                            if (_ext2->readblocks (iblock, diblock[(lblock >> (_blocksize_bits - 2)) & (blk_per_blk - 1)], _blocksize) < 0) {
                                pblock = -EIO;
                            } else {
                                pblock = iblock[lblock & (blk_per_blk - 1)];
                            }
                            delete[] iblock;
                        }
                        delete[] diblock;
                    }
                }
            }
        }
    }
    
    return pblock;
}

s32 Ext2File::balloc (int group) 
{
    u8 *blkmap = new u8[_blocksize];
    int freeblk = -1;
    int pblk;
        
    // First try to find a block in the bitmap of this group
    if (group >= 0) {
        if (_ext2->_group_desc[group].bg_free_blocks_count < _ext2->_blk_per_group) {
            _ext2->readblocks (blkmap, _ext2->_group_desc[group].bg_block_bitmap, _blocksize);
            freeblk = findfreebit (blkmap, _blocksize);
        }
    }
        
    // If not, try all groups.
    if (freeblk == -1) {
        for (int grp = 0; grp < _ext2->_groups; grp++) {
            if (_ext2->_group_desc[grp].bg_free_blocks_count < _ext2->_blk_per_group) {
                _ext2->readblocks (blkmap, _ext2->_group_desc[grp].bg_block_bitmap, _blocksize);
                freeblk = findfreebit (blkmap, _blocksize);
                if (freeblk) {
                    group = grp;
                    break;
                }
            }
        }
    }
    
    // If not, try all groups irrespective of whether they *say* they have free
    // blocks or not.
    if (freeblk == -1) {
        for (int grp = 0; grp < _ext2->_groups; grp++) {
            _ext2->readblocks (blkmap, _ext2->_group_desc[grp].bg_block_bitmap, _blocksize);
            freeblk = findfreebit (blkmap, _blocksize);
            if (freeblk) {
                group = grp;
                break;
            }
        }
    }
    
    // If not, no space left.
    if (freeblk == -1) {
        delete[] blkmap;
        return -ENOSPC;
    }
    
    // Update the info; bit already set in the bitmap. SYNCHRONOUS updates.
    _ext2->_group_desc[group].bg_free_blocks_count--;
    _ext2->_sb->s_free_blocks_count--;
    
    _ext2->gdesc_to_little();
    _ext2->sb_to_little();
    
    _ext2->_device->lseek (0x400, SEEK_SET);
    _ext2->_device->write (_ext2->_sb, sizeof(ext2_super_block));
    _ext2->writeblocks (_ext2->_group_desc, 1 + _ext2->_sb->s_first_data_block, _ext2->_groups * sizeof(ext2_group_desc));
    _ext2->writeblocks (blkmap, _ext2->_group_desc[group].bg_block_bitmap, _blocksize);
    
    _ext2->sb_to_cpu();
    _ext2->gdesc_to_cpu();
    
    pblk = freeblk + (group * _ext2->_blk_per_group) + 1;
    

    _inode->i_blocks += _blocksize / 512;

    delete[] blkmap;

    // Zero the new block.
    u8 *z = new u8[_blocksize];
    memset (z, 0, _blocksize);
    _ext2->writeblocks (z, pblk, _blocksize);
    delete[] z;

    return pblk;
}

int Ext2FS::_link (u32 dirino, u32 ino, const char *name, int type)
{
    if (!_writable)
        return -EROFS;

    if (strlen (name) > 255)
        return -ENAMETOOLONG;

    Ext2File *dirp = new Ext2File (this, dirino, 1);
    ext2_dir_entry_2 *de = new ext2_dir_entry_2;
    int ofs = 0, oldofs = -1;
    do {
        if (dirp->read (de, sizeof(ext2_dir_entry_2)) <= 0) {
            ofs = oldofs;
            break;
        }

        if (de->inode) {
            oldofs = ofs;
            ofs += de->rec_len;
            dirp->lseek (ofs, SEEK_SET);
        }
    } while (de->inode != 0);

    if (ofs == -1) return -EINVAL;

    dirp->lseek (ofs, SEEK_SET);
    dirp->read (de, sizeof(ext2_dir_entry_2));

    int totlen = de->rec_len;

    // Check if the space we have is big enough
    if (((8 + strlen (name) + 3) & ~3) + ((8 + de->name_len + 3) & ~3) > totlen) {
        // Leave this one as is, alloc another block.
        ofs = (ofs + _blocksize - 1) & ~(_blocksize - 1);
        totlen = _blocksize;
        // Write a solid block of zeroes, so the size is still a multiple of the blocksize.
        char *z = new char[_blocksize];
        memset (z, 0, _blocksize);
        dirp->lseek (ofs, SEEK_SET);
        dirp->write (z, _blocksize);
        delete[] z;
    } else if (de->inode) {
        // Size of a dirent = 8 bytes + name len. Round up to a 4-byte boundary.
        de->rec_len = (8 + de->name_len + 3) & ~3;
        dirp->lseek (ofs, SEEK_SET);
        dirp->write (de, (de->rec_len > sizeof(*de))? sizeof(*de) : de->rec_len);
        ofs += de->rec_len;
        totlen -= de->rec_len;
    }

    de->inode = ino;
    de->rec_len = totlen;
    de->name_len = strlen (name);
    de->file_type = type;
    strcpy (de->name, name);

    if (type == EXT2_FT_DIR)
        dirp->inc_nlink();
    
    dirp->lseek (ofs, SEEK_SET);
    dirp->write (de, (de->rec_len > sizeof(*de))? sizeof(*de) : de->rec_len);

    delete de;

    dirp->close();
    delete dirp;

    return 0;
}

s32 Ext2FS::creat (const char *path, int type) 
{
    s32 ino = lookup (path);
    ext2_inode *inode;

    if (ino >= 0) return -EEXIST;
    
    // Check to make sure the parent directory exists.
    char *dir = new char[strlen (path) + 1];
    const char *file;
    
    strcpy (dir, path);
    if (!strrchr (dir, '/')) {
        strcpy (dir, "/");
        file = path;
    } else {
        *(strrchr (dir, '/') + 1) = 0; // leave trailing slash
        file = strrchr (path, '/') + 1;
    }
    
    s32 dirino = lookup (dir);
    delete[] dir;
    if (dirino < 0) {
        return dirino;
    }
    
    ext2_inode *dirinode = geti (dirino);
    int mode = dirinode->i_mode;
    delete dirinode;
    if (!S_ISDIR (mode))
        return -ENOTDIR;
    
    // Create a new inode.
    inode = new ext2_inode;
    memset (inode, 0, sizeof(*inode));
    inode->i_mode = 0644;
    inode->i_uid = inode->i_gid = 0;
    inode->i_size = 0;
    inode->i_atime = inode->i_ctime = inode->i_mtime = getTime();
    inode->i_dtime = 0;
    inode->i_links_count = 1;
    inode->i_blocks = 0;
    inode->i_flags = 0;

    // Set appropriate mode based on type
    switch (type) {
    case EXT2_FT_REG_FILE: inode->i_mode |= S_IFREG; break;
    case EXT2_FT_DIR: inode->i_mode |= S_IFDIR; break;
    case EXT2_FT_CHRDEV: inode->i_mode |= S_IFCHR; break;
    case EXT2_FT_BLKDEV: inode->i_mode |= S_IFBLK; break;
    case EXT2_FT_FIFO: inode->i_mode |= S_IFIFO; break;
    case EXT2_FT_SOCK: inode->i_mode |= S_IFSOCK; break;
    case EXT2_FT_SYMLINK: inode->i_mode |= S_IFLNK; break;
    default: break;
    }
    
    // Find a place to put it - allocate a new inode.
    u8 *inomap = new u8[_blocksize];
    int group = 0;
    int freeino = -1;
    
    // First, try to find a bit in the bitmap of some group that says it
    // has free inodes.
    for (int grp = 0; grp < _groups; grp++) {
        if (_group_desc[grp].bg_free_inodes_count < _ino_per_group) {
            readblocks (inomap, _group_desc[grp].bg_inode_bitmap, _blocksize);
            freeino = findfreebit (inomap, _blocksize);
            if (freeino) {
                group = grp;
                break;
            }
        }
    }
    
    // If not, try all groups irrespective of whether they say they have free
    // inodes or not.
    if (freeino == -1) {
        for (int grp = 0; grp < _groups; grp++) {
            readblocks (inomap, _group_desc[grp].bg_inode_bitmap, _blocksize);
            freeino = findfreebit (inomap, _blocksize);
            if (freeino) {
                group = grp;
                break;
            }
        }
    }
    
    // If not, no inodes left.
    if (freeino == -1) {
        delete[] inomap;
        delete inode;
        return -ENOSPC;
    }
    
    ino = (group * _ino_per_group) + freeino + 1; // remember - no inode 0!
    
    // Update the info; bit already set in the bitmap. SYNCHRONOUS updates.
    _group_desc[group].bg_free_inodes_count--;
    _sb->s_free_inodes_count--;
    
    inode_to_little (inode);
    gdesc_to_little();
    sb_to_little();
    
    // superblock...
    _device->lseek (0x400, SEEK_SET);
    _device->write (_sb, sizeof(ext2_super_block));
    // inode...
    _device->lseek ((_group_desc[group].bg_inode_table << _blocksize_bits) +
                    freeino * _sb->s_inode_size, SEEK_SET);
    _device->write (inode, _sb->s_inode_size);
    // group descriptors...
    writeblocks (_group_desc, 1 + _sb->s_first_data_block, _groups * sizeof(ext2_group_desc));
    // inode bitmap...
    writeblocks (inomap, _group_desc[group].bg_inode_bitmap, _blocksize);
    
    sb_to_cpu();
    gdesc_to_cpu();
    
    delete inode;
    delete[] inomap;
    
    // Add to the parent directory.
    _link (dirino, ino, file, type);

    return ino;
}

VFS::File *Ext2FS::open (const char *path, int flags)
{
    s32 ino = lookup (path, 1);
    if (((flags & O_WRONLY) || (flags & O_RDWR)) && !_writable)
        return new ErrorFile (EROFS);

    if (ino < 0 && ((ino != -ENOENT) || !(flags & O_CREAT) || !_writable))
        return new ErrorFile (ino);
  
    if (ino <= 0 && (flags & O_CREAT)) {
        int err;
        if ((err = creat (path, EXT2_FT_REG_FILE)) < 0) {
            return new ErrorFile (err);
        }
        ino = lookup (path);
    }

    if (ino < 0)
        return new ErrorFile (ino);

    ext2_inode *inode = geti (ino);
    switch (inode->i_mode & S_IFMT) {
    case S_IFREG: break;
    case S_IFDIR: if ((flags & O_WRONLY) || (flags & O_RDWR)) return new ErrorFile (EISDIR); break;
    case S_IFSOCK: case S_IFIFO: case S_IFBLK: case S_IFCHR: return new ErrorFile (ENXIO);
    case S_IFLNK: return new ErrorFile (ENOENT);
    }
    delete inode;

    return new Ext2File (this, ino, ((flags & O_WRONLY) || (flags & O_RDWR)) && _writable);
}

int Ext2FS::lstat (const char *path, struct my_stat *st) 
{
    s32 ino = lookup (path, 0);
    if (ino < 0)
        return -ENOENT;

    int err = 0;
    Ext2File *fp = new Ext2File (this, ino, 0);
    err = fp->stat (st);
    delete fp;
    return err;
}

VFS::Dir *Ext2FS::opendir (const char *path) 
{
    s32 ino = lookup (path, 1);
    if (ino < 0)
        return new ErrorDir (ENOENT);

    ext2_inode *inode = geti (ino);
    if ((inode->i_mode & S_IFMT) != S_IFDIR)
        return new ErrorDir (ENOTDIR);

    return new Ext2Dir (this, ino);
}

Ext2Dir::Ext2Dir (Ext2FS *ext2, u32 ino)
    : VFS::Dir(), Ext2File (ext2, ino, 0), _off (0)
{}

int Ext2Dir::close() 
{
    Ext2File::close();
}

int Ext2Dir::readdir (struct VFS::dirent *d) 
{
    ext2_dir_entry_2 *de = new ext2_dir_entry_2;
    int ret;
    int good = 0;
    
    do {
        lseek (_off, SEEK_SET);
        ret = read (de, sizeof(ext2_dir_entry_2));
        _off += de->rec_len;

        if (de->inode) good = 1;
    } while (!good && (ret > 0) && de->rec_len);
    
    d->d_ino = de->inode;
    memcpy (d->d_name, de->name, de->name_len);
    d->d_name[de->name_len] = 0;

    return ret;
}
