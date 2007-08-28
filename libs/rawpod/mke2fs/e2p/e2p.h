#include <stdio.h>

#include "dirent.h"
#include "ext2.h"

#define E2P_FEATURE_COMPAT	0
#define E2P_FEATURE_INCOMPAT	1
#define E2P_FEATURE_RO_INCOMPAT	2


/* `options' for print_flags() */

#define PFOPT_LONG  1 /* Must be 1 for compatibility with `int long_format'. */


int iterate_on_dir (const char * dir_name,
		    int (*func) (const char *, struct dirent *, void *),
		    void * priv);
void list_super(struct ext2_super_block * s);
void list_super2(struct ext2_super_block * s, FILE *f);
void print_fs_errors (FILE * f, unsigned short errors);
void print_flags (FILE * f, unsigned long flags, unsigned options);
void print_fs_state (FILE * f, unsigned short state);

const char *e2p_feature2string(int compat, unsigned int mask);
int e2p_string2feature(char *string, int *compat, unsigned int *mask);
int e2p_edit_feature(const char *str, u32 *compat_array, u32 *ok_array);

int e2p_is_null_uuid(void *uu);
void e2p_uuid_to_str(void *uu, char *out);
const char *e2p_uuid2str(void *uu);

const char *e2p_hash2string(int num);
int e2p_string2hash(char *string);

const char *e2p_mntopt2string(unsigned int mask);
int e2p_string2mntopt(char *string, unsigned int *mask);
int e2p_edit_mntopts(const char *str, u32 *mntopts, u32 ok);
