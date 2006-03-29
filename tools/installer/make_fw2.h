#ifndef _MAKE_FW2_H_
#define _MAKE_FW2_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct image 
{
    char type[4];
    char id[4];
    char pad1[4];
#define isparent pad1[3]
    unsigned int devOffset;
    unsigned int len;
    unsigned int addr;
    unsigned int entryOffset;
    unsigned int chksum;
    unsigned int vers;
    unsigned int loadAddr;
} fw_image_t;

typedef struct fops 
{
    int (*open)(struct fops *fo, const char *name, int writing);
    void (*close)(struct fops *fo);
    int (*read)(struct fops *fo, void *buf, int len);
    int (*write)(struct fops *fo, const void *buf, int len);
    int (*lseek)(struct fops *fo, long long off, int whence);
    long long (*tell)(struct fops *fo);
    void *data;
} fw_fileops;

extern fw_fileops fw_default_fileops;

#define READING 0
#define WRITING 1
extern fw_fileops *fw_fops_open (const char *filename, int mode);


extern int stdio_open (fw_fileops *fo, const char *name, int writing);
extern void stdio_close (fw_fileops *fo);
extern int stdio_read (fw_fileops *fo, void *buf, int len);
extern int stdio_write (fw_fileops *fo, const void *buf, int len);
extern int stdio_lseek (fw_fileops *fo, long long off, int whence);
extern long long stdio_tell (fw_fileops *fo);

#define FW_LOAD_IMAGES_TO_MEMORY     0x0001
#define FW_LOADER1                   0x0000
#define FW_LOADER2                   0x0002
#define FW_VERBOSE                   0x0004
#define FW_QUIET                     0x0008
extern void fw_set_options (int opts);
extern void fw_set_generation (int gen);

extern void fw_clear();
extern void fw_test_endian();
extern void fw_clear_extract();
extern void fw_clear_ignore();
extern void fw_add_extract (const char *id);
extern void fw_add_ignore (const char *id);

void fw_add_image (fw_image_t *image, const char *id, const char *file);
void fw_iterate_images (const char *filename, void *data, void (*fn)(fw_image_t *image,
                                                                     const char *id,
                                                                     const char *file,
                                                                     void *data));
void fw_load_all (const char *filename, const char *osos_replace);
void fw_load_dumped (const char *filename, const char *osos_replace, const char *newid);
void fw_load_binary (const char *filename, const char *id);
void fw_load_unknown (const char *id, const char *filename);
int fw_create_dump (const char *outfile);

extern jmp_buf fw_error_out;

#ifdef __cplusplus
}
#endif

#endif
