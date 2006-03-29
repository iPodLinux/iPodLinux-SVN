/*
 * make_fw.c - iPodLinux loader installer
 *
 * Copyright (C) 2006 Joshua Oreman
 * 
 * based on Danial Palffy's make_fw.c
 * Copyright (C) 2003 Daniel Palffy
 *
 * based on Bernard Leach's patch_fw.c
 * Copyright (C) 2002 Bernard Leach
 * big endian support added 2003 Steven Lucy
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <setjmp.h>
#include "make_fw2.h"

#ifdef __WIN32__
#include "getopt.c"
#else
#include <getopt.h>
#endif

#define TBL 0x4200
#define IMAGE_PADDING ((fw_version == 3)? 0x200 : 0)
#define FIRST_OFFSET (TBL + 0x200 + IMAGE_PADDING)

static int fw_version = 0;
static int image_version = 0;
static int verbose = 1;
static int generation = 0;
static int loadertype = 1;
static int loadall = 0;
static const char *output_file = 0;
static struct imginf *images = 0;
static int num_images = 0;
jmp_buf fw_error_out;
#define ERROR_EXIT(n) longjmp(fw_error_out,n)

static int big_endian = 0;

int stdio_open (fw_fileops *fo, const char *name, int writing) 
{
    fo->data = fopen (name, writing? "w+b" : "rb");
    if (!fo->data) return -1;
    return 0;
}
void stdio_close (fw_fileops *fo) { fclose ((FILE *)fo->data); }
int stdio_read (struct fops *fo, void *buf, int len) { return fread (buf, 1, len, (FILE *)fo->data); }
int stdio_write (struct fops *fo, const void *buf, int len) { return fwrite (buf, 1, len, (FILE *)fo->data); }
int stdio_lseek (struct fops *fo, long long off, int whence) { return fseek ((FILE *)fo->data, off, whence); }
long long stdio_tell (struct fops *fo) { return ftell ((FILE *)fo->data); }

fw_fileops fw_default_fileops = { stdio_open, stdio_close, stdio_read, stdio_write, stdio_lseek, stdio_tell, 0 };

static const char *apple_copyright = "{{~~  /-----\\   "
                                     "{{~~ /       \\  "
                                     "{{~~|         | "
                                     "{{~~| S T O P | "
                                     "{{~~|         | "
                                     "{{~~ \\       /  "
                                     "{{~~  \\-----/   "
                                     "Copyright(C) 2001 Apple Computer, Inc."
                                     "-----------------------------------------------------"
                                     "----------------------------------------------------";

static unsigned int
switch_32 (unsigned int l) 
{
    if (big_endian)
        return ((l & 0xff) << 24) | ((l & 0xff00) << 8) | ((l & 0xff0000) >> 8) | ((l & 0xff000000) >> 24);
    return l;
}

static unsigned short
switch_16 (unsigned short s) 
{
    if (big_endian)
        return ((s & 0xff) << 8) | ((s & 0xff00) >> 8);
    return s;
}

static void
switch_endian (fw_image_t *image)
{
    if (big_endian) {
	image->devOffset = switch_32(image->devOffset);
	image->len = switch_32(image->len);
	image->addr = switch_32(image->addr);
	image->entryOffset = switch_32(image->entryOffset);
	image->chksum = switch_32(image->chksum);
	image->vers = switch_32(image->vers);
	image->loadAddr = switch_32(image->loadAddr);
    }

    /* make the ID readable */
    char idt[2];
    idt[0] = image->id[0];
    idt[1] = image->id[1];
    image->id[0] = image->id[3];
    image->id[1] = image->id[2];
    image->id[2] = idt[1];
    image->id[3] = idt[0];
}

void
fw_clear() 
{
    fw_image_info *inf = images;
    int i;
    
    while (inf) {
        if (inf->hassubs) {
            for (i = 0; i < inf->nsubs; i++) {
                if (inf->subs[i]) {
                    if (inf->subs[i]->memblock) free (inf->subs[i]->memblock);
                    free (inf->subs[i]);
                }
            }
        }
        if (inf->memblock) free (inf->memblock);
        fw_image_info *last = inf;
        inf = inf->next;
        free (last);
    }

    images = NULL;
}

static void
print_image (fw_image_t *image, const char *head) 
{
    if (verbose >= 2) {
        printf ("%stype: `%c%c%c%c' id: `%c%c%c%c' len: 0x%08x addr: 0x%08x vers: 0x%08x\n",
                head, image->type[3], image->type[2], image->type[1], image->type[0],
                image->id[0], image->id[1], image->id[2], image->id[3],
                image->len, image->addr, image->vers);
        printf ("devOffset: 0x%08x entryOffset: 0x%08x loadAddr: 0x%08x chksum: 0x%08x\n",
                image->devOffset, image->entryOffset, image->loadAddr, image->chksum);
    } else if (verbose) {
        printf ("%s%c%c%c%c: %d bytes loaded from +%d to 0x%08x",
                head, image->id[0], image->id[1], image->id[2], image->id[3],
                image->len, image->devOffset, image->addr);
        if (image->entryOffset)
            printf (" executing from 0x%08x", image->addr + image->entryOffset);
        printf ("\n");
    }
}

void
usage (int exitcode)
{
    printf ("make_fw is a program used to create a bootable iPod image.\n"
            "It can be used in any of the following modes; you must specify\n"
            "exactly one mode.\n\n"
            
            "This screen:\n"
            "    make_fw [-h]\n"
            "List images in a firmware dump:\n"
            "    make_fw -t DUMPFILENAME\n"
            "Extract an image from a firmware dump:\n"
            "    make_fw -x IMAGENAME DUMPFILENAME\n"
            "      The image will be extracted to IMAGENAME.fw, unless you\n"
            "      specify -o.\n"
            "Create a firmware dump:\n"
            "    make_fw -c IMAGENAME[=IMAGEFILE] [IMAGENAME[=IMAGEFILE] [...]]\n"
            "      IMAGEFILEs may be either files extracted with -x or raw\n"
            "      binaries. The output will be written to ipodlinux.fw, unless\n"
            "      you specify -o.\n\n"
            
            "Options:\n"
            "  -g <gen>   Specify the generation of iPod for which you're creating\n"
            "             the firmware. This is REQUIRED. Valid values include:\n"
            "                1g, 2g, 3g, 4g, 5g  (scroll, touch, dock, click, video)\n"
            "                mini, photo, color, nano\n"
            "             You may also specify a raw hw_ver number preceded by `x', or\n"
            "             a raw firmware version number preceded by `v', if you know\n"
            "             what you're doing.\n"
            "  -1         For options where it matters, consider the image as using loader1.\n"
            "  -2         Ditto, for loader2.\n"
            "  -3         Equivalent to `-g v3', for compatibility reasons. (Creates an image\n"
            "             compatible with iPods 4g/mini through 5g/video inclusive.)\n"
            "  -A <file>  With -c, put [a]ll images from the dump <file> into the output.\n"
            "  -a <file>  Like -A, but treat the osos image like -i does instead of keeping\n"
            "             its name the same.\n"
            "  -b <ldr>   For loader1, equivalent to `osos@=<ldr>'; for loader2, `osos=<ldr>'.\n"
            "  -d <id>    Ignore the image with ID <id> when using -a, -A, -p, -P, -x, or -t.\n"
            "             This may be specified multiple times.\n"
            "  -i <file>  For loader1, equivalent to `ososN=<file>', where N starts from 0\n"
            "             and increases with each -i or -l option. For loader2, equivalent\n"
            "             to `aple=<file>'.\n"
            "  -l <file>  For loader1, equivalent to `ososN=<file>'; see the documentation for\n"
            "             -i. For loader2, equivalent to `lnux=<file>'.\n"
            "  -m         Load all inputs before writing any outputs. You must specify this\n"
            "             option if input and output are the same file. Uses lots of memory.\n"
            "  -o <file>  Specifies where the output should go. If you don't specify this,\n"
            "             a sensible default is used.\n"
            "  -p <file>  Equivalent to -m -a <file> -o <file>; [p]atch the image in.\n"
            "  -P <file>  Equivalent to -m -A <file> -o <file>.\n"
            "  -r <rev>   Specify the revision number, in hex, to use for the top-level images.\n"
            "             Use only if you know what you're doing.\n"
            "  -v         Increase the verbosity (chattiness).\n"
            "  -q         Decrease the verbosity.\n\n"
            
            "The iPod's firmware partition is organized as a set of `images'; each has\n"
            "a four-character ID, like `osos' or `aupd' or `rsrc'. Whenever an IMAGENAME\n"
            "is specified above, it's looking for such an ID. If you put a number after\n"
            "an ID, it refers to a subimage (Linux or the Apple firmware) used\n"
            "by ipodloader1. If you put an @ sign after one, it refers to the loader\n"
            "stub image. (See the examples.)\n\n"

            "Examples:\n"
            "  To create a firmware dump using ipodloader1:\n"
            "    ./make_fw -1 -g <gen> -c -a ipod.fw -l linux.bin -b loader.bin\n"
            "  The same, but the `long way':\n"
            "    ./make_fw -g <gen> -c osos@=loader.bin osos0=ipod.fw:osos osos1=linux.bin\n"
            "  The \"long way\" with ipodloader2:\n"
            "    ./make_fw -g <gen> -c osos=loader.bin aple=ipod.fw:osos lnux=linux.bin\n"
            "    (Of course, with loader2 you don't necessarily have to put those\n"
            "     images in the firmware partition.)\n");
    exit (exitcode);
}

#define READING 0
#define WRITING 1
fw_fileops *
fw_fops_open (const char *filename, int mode) 
{
    fw_fileops *ret = malloc (sizeof(fw_fileops));
    memcpy (ret, &fw_default_fileops, sizeof(fw_fileops));
    if (ret->open (ret, filename, mode) < 0) {
        perror (filename);
        ERROR_EXIT (10);
    }
    return ret;
}

/** Checksum functions **/
static void
updatesum (const unsigned char *buf, int len, unsigned int *sum) 
{
    int i;
    for (i = 0; i < len; i++) {
        *sum += buf[i];
    }
}

#define BUFFER_SIZE 512
static unsigned int
copysum (fw_fileops *s, fw_fileops *d, unsigned int len, unsigned int off) 
{
    unsigned char buf[BUFFER_SIZE];
    unsigned int sum = 0;
    unsigned int pos = 0;
    
    if (s->lseek (s, off, SEEK_SET) < 0) {
        fprintf (stderr, "seek failed: %s\n", strerror (errno));
        ERROR_EXIT (10);
    }
    
    while (pos < len) {
        int rdlen = (pos + BUFFER_SIZE < len)? BUFFER_SIZE : len - pos;
        int rret;
        if ((rret = s->read (s, buf, rdlen)) != rdlen) {
            if (rret >= 0) {
                fprintf (stderr, "short read on file\n");
                ERROR_EXIT (10);
            } else {
                fprintf (stderr, "read failed: %s\n", strerror (errno));
                ERROR_EXIT (10);
            }
        }

        updatesum (buf, rdlen, &sum);

        if (d) {
            if (d->write (d, buf, rdlen) != rdlen) {
                fprintf (stderr, "write failed: %s\n", strerror (errno));
                ERROR_EXIT (10);
            }
        }
        
        pos += BUFFER_SIZE;
    }

    return sum;
}

static unsigned int
writesum (const unsigned char *s, fw_fileops *d, unsigned int len) 
{
    unsigned int sum = 0;
    updatesum (s, len, &sum);
    if (d->write (d, s, len) != (int)len) {
        fprintf (stderr, "write failed: %s\n", strerror (errno));
        ERROR_EXIT (10);
    }
    return sum;
}

/* return the size of f */
static unsigned int
lengthof(fw_fileops *f)
{
    if (f->lseek(f, 0, SEEK_END) < 0) {
	fprintf(stderr, "seek failed: %s\n", strerror(errno));
	return -1;
    }
    return f->tell(f);
}

void
fw_set_options (int opts) 
{
    loadall = !!(opts & FW_LOAD_IMAGES_TO_MEMORY);
    switch (opts & (FW_LOADER1|FW_LOADER2)) {
    case FW_LOADER1:
        loadertype = 1;
        break;
    case FW_LOADER2:
        loadertype = 2;
        break;
    default:
        fprintf (stderr, "Warning: invalid loader type option passed to fw_set_options()\n");
        break;
    }

    verbose += !!(opts & FW_VERBOSE);
    verbose -= !!(opts & FW_QUIET);
}

void
fw_test_endian(void)
{
    char ch[4] = { '\0', '\1', '\2', '\3' };
    unsigned i = 0x00010203;

    if (*((unsigned *)ch) == i) 
	big_endian = 1;
    else
	big_endian = 0;
    return;
}

struct idlist
{
    const char *name;
    struct idlist *next;
} *to_extract = 0, *to_ignore = 0;

static void
add_id (struct idlist **head, const char *name) 
{
    struct idlist *current = *head;
    if (!current)
        current = *head = (struct idlist *)malloc (sizeof(struct idlist));
    else {
        while (current->next) current = current->next;
        current = current->next = (struct idlist *)malloc (sizeof(struct idlist));
    }
    
    current->name = name;
    current->next = 0;
}

void fw_clear_extract() { to_extract = 0; }
void fw_clear_ignore() { to_ignore = 0; }

void fw_add_extract (const char *name) { add_id (&to_extract, name); }
void fw_add_ignore (const char *name) { add_id (&to_ignore, name); }

static int
find_id (struct idlist *head, const char *name) 
{
    struct idlist *current = head;
    while (current) {
        if (!strcmp (current->name, name))
            return 1;
        current = current->next;
    }
    return 0;
}

/* load the boot entry from 
 * boot table at offset, 
 * entry number entry
 * file fw */
static int 
load_entry(fw_image_t *image, fw_fileops *fw, unsigned offset, int entry)
{
    if (fw->lseek(fw, offset + entry * sizeof(fw_image_t), SEEK_SET) < 0) {
	fprintf(stderr, "fseek failed: %s\n", strerror(errno));
	return -1;
    }
    if (fw->read(fw, image, sizeof(fw_image_t)) != sizeof(fw_image_t)) {
	fprintf(stderr, "fread failed: %s\n", strerror(errno));
	return -1;
    }
    switch_endian(image);
    return 0;
}

/* store the boot entry to
 * boot table at offset, 
 * entry number entry
 * file fw */
static int 
write_entry(fw_image_t *image, fw_fileops *fw, unsigned offset, int entry, int pad)
{
    if (fw->lseek(fw, offset + entry * sizeof(fw_image_t), SEEK_SET) < 0) {
	fprintf(stderr, "fseek failed: %s\n", strerror(errno));
	return -1;
    }
    if (pad) image->devOffset -= IMAGE_PADDING;
    switch_endian(image);
    if (fw->write(fw, image, sizeof(fw_image_t)) != sizeof(fw_image_t)) {
	fprintf(stderr, "fwrite failed: %s\n", strerror(errno));
	switch_endian(image);
	return -1;
    }
    switch_endian(image);
    if (pad) image->devOffset += IMAGE_PADDING;
    return 0;
}

/* Writes the image described by inf to the file out, offset
 * inf->header.devOffset and length inf->header.len; computes
 * the checksum and updates inf->header.checksum with it;
 * then writes the image header to the boot table at offset,
 * slot entry.
 */
static int
write_image (fw_image_info *inf, fw_fileops *out, unsigned offset, int entry, int pad) 
{
    if (out->lseek (out, inf->header.devOffset, SEEK_SET) < 0) {
        fprintf (stderr, "fseek failed: %s\n", strerror (errno));
        return -1;
    }

    if (loadall && inf->memblock) {
        inf->header.chksum = writesum (inf->memblock, out, inf->header.len);
    } else {
        inf->header.chksum = copysum (inf->file, out, inf->header.len, inf->fileoff);
    }

    return write_entry (&inf->header, out, offset, entry, pad);
}

/* Finds the image with the specified ID and returns it,
 * or makes a new one.
 * Only the first four characters of `id' are considered.
 * Pass subimg = '@' to find a parent image, 'N' to make
 * the next child of that id, or '0'-'4' to find/make that child,
 * or 0 to find/make a non-sub image.
 */
static fw_image_info *
find_or_make_image (const char *id, char subimg) 
{
    /* Find the parent first. */
    fw_image_info *current = images;
    int found = 0;
    
    while (current) {
        if (!strncmp (id, current->name, 4)) {
            found = 1;
            break;
        }

        if (!current->next) break;
        current = current->next;
    }

    /* Make the parent if we didn't find it. */
    if (!found) {
        if (current) { /* adding onto an existing list */
            current = current->next = (fw_image_info *)calloc (1, sizeof(fw_image_info));
        } else { /* starting off the list */
            current = images = (fw_image_info *)calloc (1, sizeof(fw_image_info));
        }
        current->name = strdup (id);
        memcpy (current->header.id, id, 4);
        current->hassubs = (subimg != 0);
        if (current->hassubs) { current->name[4] = '@'; current->name[5] = 0; }
    }

    /* Find the appropriate child. */
    switch (subimg) {
    case 0: /* We just want a parent, so we're done! */
        if (current->hassubs) {
            fprintf (stderr, "warning: overriding %s with non-parental version %s\n", current->name, id);
            current->name = strdup (id);
            current->hassubs = 0;
        }
        return current;
    case '@': /* Similar. */
        if (!current->hassubs) {
            fprintf (stderr, "warning: overriding %s with parental version %s\n", current->name, id);
            current->name = strdup (id);
            current->hassubs = 1;
        }
        return current;
    case 'N': /* Find the next sub. */
        if (current->nsubs >= 5) {
            fprintf (stderr, "error: too many sub-images for %s\n", current->name);
            ERROR_EXIT (11);
        }
        subimg = current->nsubs + '0';
        /* FALLTHRU */
    case '0': case '1': case '2': case '3': case '4':
        if (!current->hassubs) {
            fprintf (stderr, "error: can't add sub-image %s to a non-parental parent %s\n", id, current->name);
            ERROR_EXIT (11);
        }
        int subnr = subimg - '0';
        if (current->nsubs <= subnr)
            current->nsubs = subnr + 1;

        if (!current->subs[subnr]) {
            current->subs[subnr] = (fw_image_info *)calloc (1, sizeof(fw_image_info));
            current->subs[subnr]->name = strdup (id);
        }
        return current->subs[subnr];
    default:
        fprintf (stderr, "error: unrecognized subimage type `%c'\n", subimg);
        ERROR_EXIT (11);
    }
    return 0;
}

/* Adds `image' to the list of images to write, with id `id' (which may
 * include things like osos1, osos@, ososN) from file `file' at offset
 * image->devOffset and length image->len.
 *
 * Every field of `image' must be properly filled out, except the checksum.
 */
void
fw_add_image (fw_image_t *image, const char *name, const char *file) 
{
    if (strlen (name) < 4 || strlen (name) > 5) {
        fprintf (stderr, "`%s' is an invalid ID (must be 4 or 5 characters)\n", name);
        ERROR_EXIT (11);
    }
    if (strlen (name) == 5 && !strpbrk (name + 4, "01234N@")) {
        fprintf (stderr, "`%s' is an invalid ID for a subimage (last char must be 0-4, N, or @)\n", name);
        ERROR_EXIT (11);
    }

    fw_image_info *inf = find_or_make_image (name, name[4]);
    if (inf->file) {
        fprintf (stderr, "warning: overriding image %s; loading from %s +%d instead of old file +%d\n",
                 inf->name, file, image->devOffset, inf->fileoff);
        inf->file->close (inf->file);
        if (inf->memblock) free (inf->memblock);
    }

    inf->file = fw_fops_open (file, READING);
    inf->fileoff = image->devOffset;
    memcpy (&inf->header, image, sizeof(fw_image_t));
    strncpy (inf->header.id, name, 4);

    if (loadall) {
        inf->memblock = malloc (inf->header.len);
        if (!inf->memblock) {
            fprintf (stderr, "error: out of memory allocating %d bytes for loading of %s; try not using -m\n",
                     inf->header.len, inf->name);
            ERROR_EXIT (255);
        }
        inf->file->lseek (inf->file, inf->fileoff, SEEK_SET);
        if (inf->file->read (inf->file, inf->memblock, inf->header.len) != (int)inf->header.len) {
            fprintf (stderr, "error reading %d bytes from %s\n", inf->header.len, file);
            ERROR_EXIT (10);
        }
    }

    num_images++;
}

/* Finds a loaded image and returns it, or NULL if it doesn't exist. */
fw_image_info *
fw_find_image (const char *name) 
{
    fw_image_info *cur = images;
    while (cur) {
        if (!strcmp (cur->name, name)) return cur;
        if (cur->hassubs) {
            int i;
            for (i = 0; i < cur->nsubs; i++) {
                if (cur->subs[i] && !strcmp (cur->name, cur->subs[i]->name)) return cur->subs[i];
            }
        }
        cur = cur->next;
    }
}

/* Does something with each image in `filename'. */
void
fw_iterate_images (const char *filename, void *data, void (*fn)(fw_image_t *, const char *, const char *, void *)) 
{
    fw_fileops *in = fw_fops_open (filename, READING);
    int old_version = fw_version;
    unsigned short ver;
    char buf[6] = "????";
    fw_image_t image;

    in->lseek (in, 0x10A, SEEK_SET); /* seek to the version */
    in->read (in, &ver, 2);
    fw_version = switch_16 (ver);

    if (verbose >= 2) printf ("Version: %d\n", fw_version);
    if (fw_version < 2 || fw_version > 3) {
        fprintf (stderr, "%s: invalid version (0x%04x); are you sure that's a dump file?\n", filename, fw_version);
        ERROR_EXIT (10);
    }

    int idx;
    for (idx = 0; idx < 10; idx++) {

        if (load_entry (&image, in, TBL, idx) == -1)
            break;
        image.devOffset += IMAGE_PADDING;
        if (!isalnum (image.id[0]) || !isalnum (image.id[1]) || !isalnum (image.id[2]) ||
            !isalnum (image.id[3]))
            break;
        if (image.isparent) {
            int subidx;
            fw_image_t subimg;
            
            strncpy (buf, image.id, 4);
            buf[4] = '@';
            buf[5] = 0;

            fw_image_t bootimg = image;
            bootimg.devOffset += bootimg.entryOffset;
            bootimg.len -= bootimg.entryOffset;
            bootimg.entryOffset = 0;

            if (!find_id (to_ignore, buf))
                (*fn)(&bootimg, buf, filename, data);

            for (subidx = 0; subidx < 5; subidx++) {
                if (load_entry (&subimg, in, image.devOffset + image.entryOffset + 0x100, subidx) == -1)
                    break;
                if (!isalnum (subimg.id[0]) || !isalnum (subimg.id[1]) || !isalnum (subimg.id[2]) ||
                    !isalnum (subimg.id[3]))
                    break;

                buf[4] = subidx + '0';

                if (!find_id (to_ignore, buf))
                    (*fn)(&subimg, buf, filename, data);
            }
        } else {
            strncpy (buf, image.id, 4);
            buf[4] = 0;

            if (!find_id (to_ignore, buf))
                (*fn)(&image, buf, filename, data);
        }
    }

    if (!idx) {
        fprintf (stderr, "%s: warning: no valid images\n", filename);
    }

    fw_version = old_version;
    in->close (in);
}

/* Callback for load_all. */
static void
load_one (fw_image_t *image, const char *id, const char *filename, void *data) 
{
    const char *osos_replace = (const char *)data;

    fw_add_image (image, (!strcmp (id, "osos"))? osos_replace : id, filename);

    if (verbose >= 2) print_image (image, "Loaded image header from firmware file: ");
}

/* Loads all images from `filename', renaming the osos image
 * to `osos_replace' (just pass "osos" if you don't want renaming).
 */
void
fw_load_all (const char *filename, const char *osos_replace) 
{
    fw_iterate_images (filename, (void *)osos_replace, load_one);
}

/* Loads a loader-dumped image from `filename'. The image's ID is taken
 * to be the ID in `filename', or `osos_replace' if that id is "osos".
 * The ID is changed to `newid' if `newid' is non-NULL.
 */
void
fw_load_dumped (const char *filename, const char *osos_replace, const char *newid)
{
    fw_fileops *in = fw_fops_open (filename, READING);
    char magic[9] = "????????";
    fw_image_t image;
    
    in->lseek (in, 64, SEEK_SET); /* seek to the magic */
    in->read (in, magic, 8); /* read magic */
    if (strcmp (magic, "make_fw2") != 0) {
        fprintf (stderr, "%s: warning: bad magic, ignoring\n", filename);
        in->close (in);
        return;
    }

    if (load_entry (&image, in, 0, 0) == -1) {
        fprintf (stderr, "%s: warning: error loading image header\n", filename);
        return;
    }

    if (!isalnum (image.id[0]) || !isalnum (image.id[1]) || !isalnum (image.id[2]) ||
        !isalnum (image.id[3])) {
        fprintf (stderr, "%s: warning: invalid image ID `%c%c%c%c'\n", filename,
                 image.id[0], image.id[1], image.id[2], image.id[3]);
        return;
    }

    if (!strcmp (image.id, "osos")) {
        newid = osos_replace;
        strncpy (image.id, newid, 4);
    }

    if (verbose >= 2) print_image (&image, "Loaded previously dumped image: ");

    char id[5] = "????";
    strncpy (id, image.id, 4);
    fw_add_image (&image, newid? newid : id, filename);
}

/* Loads a raw binary from `filename' for turning into an image.
 * You must appropriately specify the ID.
 * The image will be loaded to 0x10000000 with its entry point at its beginning.
 */
void
fw_load_binary (const char *filename, const char *id) 
{
    fw_fileops *in = fw_fops_open (filename, READING);
    fw_image_t image;
    image.len = lengthof (in);
    image.devOffset = 0;
    image.addr = 0x10000000;
    image.entryOffset = 0;
    image.vers = image_version;
    image.loadAddr = 0xffffffff;
    memcpy (image.type, "!ATA", 4);
    memcpy (image.id, id, 4);
    memcpy (image.pad1, "\0\0\0\0", 4);
    if (verbose >= 2) print_image (&image, "Created image from raw binary file: ");
    fw_add_image (&image, id, filename);
    in->close (in);
}

/* The DWIMmy function. Checks magic in `filename', and executes
 * either load_binary or load_dumped as appropriate.
 * The ID of the loaded file is set to `id'. If `id' is NULL,
 * an attempt will be made to determine whether `filename' is a
 * dump, loader, or kernel, and the ID will be set appropriately.
 * If `filename' is NULL, a reasonable default will be used.
 */
void
fw_load_unknown (const char *id, const char *filename) 
{
    char buf[128];
    int idforce = !!id;
    enum { Binary, Dump } type = Binary;

    if (id && !id[0]) id = 0;
    if (filename && !filename[0]) filename = 0;
    if (!id && !filename) {
        fprintf (stderr, "You must specify at least one of ID and filename.\n");
        ERROR_EXIT (10);
    }

    if (!filename) {
        sprintf (buf, "%s.fw", id);
        filename = buf;
    }

    fw_fileops *in = fw_fops_open (filename, READING);
    char magic[9] = "????????";
    
    in->lseek (in, 64, SEEK_SET); /* seek to the magic */
    in->read (in, magic, 8); /* read magic */
    if (!strcmp (magic, "make_fw2")) {
        type = Dump;
        if (!id) id = (loadertype == 2)? "aple" : "ososN";
        if (verbose >= 2) printf ("%s: seems to be a DUMP\n", filename);
    } else if (id) {
        type = Binary;
    } else {
        /* Read the first instruction to try to figure out what it is. */
        in->lseek (in, 0, SEEK_SET);
        in->read (in, magic, 4);
        if (!memcmp (magic, "\xff\x04\xa0\xe3", 4)) { /* mov r0, #0xff000000    @ the loader */
            type = Binary;
            id = (loadertype == 2)? "osos" : "osos@";
            if (verbose >= 2) printf ("%s: seems to be a LOADER\n", filename);
        } else if (!memcmp (magic, "\xfe\x1f\x00\xea", 4)) { /* b +0x7ffc (to 0x8000)    @ the kernel */
            type = Binary;
            id = (loadertype == 2)? "lnux" : "ososN";
            if (verbose >= 2) printf ("%s: seems to be a KERNEL\n", filename);
        } else {
            fprintf (stderr, "%s: can't figure out whether it's a loader or kernel.", filename);
            fprintf (stderr, "%s: please specify an ID, or use -l or -b\n", filename);
            return;
        }
    }

    if (type == Dump) {
        if (idforce)
            fw_load_dumped (filename, "osos", id);
        else
            fw_load_dumped (filename, id, 0);
    } else {
        fw_load_binary (filename, id);
    }
}


/* Callback for list_images. */
static void
list_one_image (fw_image_t *image, const char *id, const char *filename, void *data) 
{
    char buf[64];
    static int imgnr = 0;
    static int subnr = 0;

    (void)filename, (void)data;

    if (strlen (id) == 5) {
        if (id[4] == '@') {
            imgnr++;
            subnr = 0;
            sprintf (buf, "% 2d) [M] ", imgnr);
        } else {
            subnr++;
            sprintf (buf, "    % 2d) ", subnr);
        }
    } else {
        imgnr++;
        subnr = 0;
        sprintf (buf, "% 2d) ", imgnr);
    }
    print_image (image, buf);
}

/* List all images in `file'. */
static int
list_images (const char *file) 
{
    fw_iterate_images (file, 0, list_one_image);
    return 0;
}


/* Callback for extract_images. */
static void
mark_one_image_for_extraction (fw_image_t *image, const char *id, const char *filename, void *data) 
{
    (void)data;

    if (!find_id (to_extract, id) && to_extract) {
        if (verbose >= 2) printf ("Not loading %s for extraction - not requested\n", id);
        return;
    }
    if (verbose >= 2) printf ("Loading %s for extraction.\n", id);
    
    load_one (image, id, filename, (void *)"osos");
}

/* Extract one image (and its subs) */
static void
extract_one_image (fw_image_info *inf) 
{
    char buf[512], ext[16];
    
    /* Figure out what to call it. */
    if (output_file) {
        strcpy (buf, output_file);
        if (num_images > 1) {
            if (strchr (buf, '.')) {
                strncpy (ext, strrchr (buf, '.'), 16);
                ext[15] = 0;
                *strrchr (buf, '.') = 0;
            } else {
                ext[0] = 0;
            }
            sprintf (buf + strlen (buf), ".%s%s", inf->name, ext);
        }
    } else {
        sprintf (buf, "%s.fw", inf->name);
    }
    
    if (verbose) {
        printf ("Extracting image %s to %s... ", inf->name, buf);
        fflush (stdout);
    }
    
    /* Open our file. */
    fw_fileops *out = fw_fops_open (buf, WRITING);
    inf->header.devOffset = 0x200;
    
    /* Write the magic and info. */
    time_t tim = time (0);
    
    out->lseek (out, 64, SEEK_SET);
    out->write (out, "make_fw2", 8);
    out->lseek (out, 80, SEEK_SET);
    sprintf (buf, "This is a firmware dump file of the image `%s' created by make_fw on %s.\n"
             "This image is to be loaded to 0x%08x, and execution should start at 0x%08x.\n"
             "If you want to work on this for reverse-engineering reasons, please chop off\n"
             "the first 512 bytes before loading it into your disassembler.\n\n",
             inf->name, ctime (&tim), inf->header.addr, inf->header.addr + inf->header.entryOffset);
    out->write (out, buf, strlen (buf));
#define MSG "Image starts here. ->"
    out->lseek (out, 512 - strlen (MSG), SEEK_SET);
    out->write (out, MSG, strlen (MSG));
#undef MSG

    /* Write the image and header. */
    write_image (inf, out, 0, 0, 0);
    
    /* Close it. */
    out->close (out);
    
    if (verbose) printf ("done\n");
}

/* Extract all requested images, or all images. */
static int
extract_images (const char *file) 
{
    fw_iterate_images (file, 0, mark_one_image_for_extraction);
    
    fw_image_info *inf = images;
    while (inf) {
        extract_one_image (inf);

        /* Check subimages */
        if (inf->hassubs) {
            int sub;
            for (sub = 0; sub < inf->nsubs; sub++) {
                if (inf->subs[sub]) {
                    extract_one_image (inf->subs[sub]);
                }
            }
        }

        inf = inf->next;
    }
    
    return 0;
}


int
fw_create_dump (const char *outfile) 
{
    fw_fileops *out = fw_fops_open (outfile, WRITING);
    unsigned int offset = FIRST_OFFSET;
    int table_slot = 0;

    /* Write the header stuff. */
    unsigned int val;
    if (out->write(out, apple_copyright, 0x100) != 0x100) {
	fprintf(stderr, "fwrite failed: %s\n", strerror(errno));
	return 1;
    }
    val = switch_32(0x5b68695d); /* '[hi]' */
    if (out->write(out, &val, 4) != 4) {
	fprintf(stderr, "fwrite failed: %s\n", strerror(errno));
	return 1;
    }
    val = switch_32(0x00004000); /* magic */
    if (out->write(out, &val, 4) != 4) {
	fprintf(stderr, "fwrite failed: %s\n", strerror(errno));
	return 1;
    }

    if (fw_version == 3) {
        val = switch_32(0x0003010c); /* version/magic */
    } else {
        val = switch_32(0x0002010c); /* version/magic */
    }
    if (out->write(out, &val, 4) != 4) {
	fprintf(stderr, "fwrite failed: %s\n", strerror(errno));
	return 1;
    }

    /* Write the images. */
    fw_image_info *inf = images;
    while (inf) {
        unsigned int totlen = inf->header.len, sublen = 0;
        inf->header.devOffset = (offset + 0x1ff) & ~0x1ff;
        if (inf->hassubs) {
            /* Figure out the total length, put the parent at the end */
            int i;
            for (i = 0; i < inf->nsubs; i++) {
                if (inf->subs[i]) {
                    sublen = (sublen + inf->subs[i]->header.len + 0x1ff) & ~0x1ff;
                }
            }
            totlen += sublen;
            inf->header.devOffset += sublen;
        }

        /* Actually write the image. */
        write_image (inf, out, TBL, table_slot, 1);
        print_image (&inf->header, "Writing image: ");

        if (inf->hassubs) {
            /* Write each of the subs. */
            int suboff = offset;
            int subslot = 0;
            int sub;
            for (sub = 0; sub < inf->nsubs; sub++) {
                if (inf->subs[sub]) {
                    inf->subs[sub]->header.devOffset = ((suboff + 0x1ff) & ~0x1ff);
                    suboff = inf->subs[sub]->header.devOffset + inf->subs[sub]->header.len;
                    write_image (inf->subs[sub], out, inf->header.devOffset + 0x100, subslot++, 0);
                    print_image (&inf->subs[sub]->header, "  Writing sub-image: ");
                    if (inf->subs[sub]->header.vers > inf->header.vers)
                        inf->header.vers = inf->subs[sub]->header.vers;
                }
            }

            /* Update the table to properly reflect the total length and checksum. */
            inf->header.devOffset -= sublen;
            inf->header.entryOffset = sublen;
            inf->header.len = totlen;
            inf->header.chksum = copysum (out, 0, totlen, inf->header.devOffset);
            inf->header.isparent = 1;
            write_entry (&inf->header, out, TBL, table_slot, 1);
            print_image (&inf->header, "Master image: ");
        }

        table_slot++;
        offset = (offset + totlen + 0x1ff) & ~0x1ff;
        inf = inf->next;
    }
    
    return 0;
}


void
fw_set_generation (int gen) 
{
    generation = gen;
    fw_version = 2 + (gen >= 4);
}


#ifdef EMBED_MAKEFW
#define main make_fw_main
#endif

int
main (int argc, char **argv) 
{
    int c, jr;
    const char *input_file = "ipod.fw";
    enum { None = 0, List, Extract, Create } mode = None;
    
    if ((jr = setjmp (fw_error_out)) != 0) {
        fprintf (stderr, "A fatal error has occurred; exiting.\n");
        return jr;
    }

    fw_test_endian();
    
    opterr = 0;
    while ((c = getopt (argc, argv, "123A:a:b:cd:g:hi:l:mo:p:P:qr:tvx")) != EOF) {
        switch (c) {
        case '?':
            fprintf (stderr, "invalid option -%c specified\n", optopt);
            usage (1);
            break;
        case ':':
            fprintf (stderr, "option -%c needs an argument\n", optopt);
            usage (2);
            break;
        case 'h':
            usage (0);
            break;
            
        case '1':
            loadertype = 1;
            break;
        case '2':
            loadertype = 2;
            break;
            
        case '3':
            fw_version = 3;
            break;
            
        case 'c':
            if (mode != None) { fprintf (stderr, "more than one mode specified\n"); usage (3); }
            mode = Create;
            break;
        case 'x':
            if (mode != None) { fprintf (stderr, "more than one mode specified\n"); usage (3); }
            mode = Extract;
            break;
        case 't':
            if (mode != None) { fprintf (stderr, "more than one mode specified\n"); usage (3); }
            mode = List;
            break;
            
        case 'g':
            if (optarg[0] == 'v' && isdigit (optarg[1]))
                fw_version = optarg[1] - '0';
            else if (optarg[0] == 'x' && isxdigit (optarg[1]))
                generation = (isalpha(optarg[1])? toupper(optarg[1]) - 'A' + 10 : optarg[1] - '0');
            else {
                if (!strcasecmp (optarg, "1g") || !strcasecmp (optarg, "scroll"))
                    generation = 1;
                else if (!strcasecmp (optarg, "2g") || !strcasecmp (optarg, "touch"))
                    generation = 2;
                else if (!strcasecmp (optarg, "3g") || !strcasecmp (optarg, "dock"))
                    generation = 3;
                else if (!strcasecmp (optarg, "mini"))
                    generation = 4;
                else if (!strcasecmp (optarg, "4g") || !strcasecmp (optarg, "click"))
                    generation = 5;
                else if (!strcasecmp (optarg, "photo") || !strcasecmp (optarg, "color"))
                    generation = 6;
                else if (!strcasecmp (optarg, "mini2g"))
                    generation = 7;
                else if (!strcasecmp (optarg, "5g") || !strcasecmp (optarg, "video"))
                    generation = 0xB;
                else if (!strcasecmp (optarg, "nano"))
                    generation = 0xC;
                else {
                    fprintf (stderr, "invalid generation `%s'\n", optarg);
                    usage (4);
                }
            }
            break;
        case 'r':
            image_version = strtol (optarg, 0, 16);
            break;

        case 'm':
            loadall++;
            break;
        case 'q':
            verbose--;
            break;
        case 'v':
            verbose++;
            break;
        case 'o':
            output_file = optarg;
            break;
            
        case 'd':
            add_id (&to_ignore, optarg);
            break;

        case 'P':
            loadall++;
            output_file = optarg;
            /* FALLTHRU */
        case 'A':
            fw_load_all (optarg, "osos");
            break;
        case 'p':
            loadall++;
            output_file = optarg;
            /* FALLTHRU */
        case 'a':
            fw_load_all (optarg, (loadertype == 2)? "aple" : "ososN");
            break;
        case 'b':
            fw_load_binary (optarg, (loadertype == 2)? "osos" : "osos@");
            break;
        case 'i':
            fw_load_dumped (optarg, (loadertype == 2)? "aple" : "ososN", 0);
            break;
        case 'l':
            fw_load_binary (optarg, (loadertype == 2)? "lnux" : "ososN");
            break;
        default:
            fprintf (stderr, "unknown option (?) -%c\n", c);
            usage (5);
            break;
        }
    }

    argc -= optind;
    argv += optind;
    
    if (mode == None) mode = Create;

    while (argc--) {
        switch (mode) {
        case Create: /* args are IMAGE=FILE pairs */
            if (strchr (*argv, '=')) {
                char *key = *argv, *value = strchr (*argv, '=');
                *value++ = 0;
                fw_load_unknown (key, value);
            } else if (strpbrk (*argv, "._:-") || strlen (*argv) > 5 || strlen (*argv) < 4) { /* probably a filename */
                fw_load_unknown (0, *argv);
            } else {
                fw_load_unknown (*argv, 0);
            }
            break;

        case Extract: /* args are all IMAGE, except for the optional FILE at the end */
            if (!argc && (strpbrk (*argv, "._:-") || strlen (*argv) > 5)) { /* file at end */
                input_file = *argv;
            } else {
                add_id (&to_extract, *argv);
            }
            break;
            
        case List: /* list takes an optional FILE argument */
            input_file = *argv;
            break;

        case None: break; /* shut up, gcc */
        }

        argv++;
    }

    if (mode == Create && !num_images) {
        fprintf (stderr, "No images specified. Please specify some.\n");
        usage (7);
    }

    if (mode == List) {
        return list_images (input_file);
    } else if (mode == Extract) {
        return extract_images (input_file);
    } else {
        if (!fw_version && !generation) {
            fprintf (stderr, "You must specify a generation with the -g option.\n");
            exit (7);
        }
        if (fw_version < 2) {
            fw_version = 2 + (generation >= 4);
        }
        if (!output_file) output_file = "ipodlinux.fw";
        return fw_create_dump (output_file);
    }
}
