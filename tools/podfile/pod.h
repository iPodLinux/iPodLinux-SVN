/*
 * MAGIC (5 chars)
 * REVISION (short)
 * BLOCKSIZE (long)
 * FILE COUNT (long)
 *	TYPE (short)
 *	FILENAME (NULL terminated)
 *	OFFSET (long)
 *	LENGTH (long)
 * DATA
 */

#ifndef _POD_H_
#define _POD_H_

#define PODMAGIC "PODar"
#define REV 2
#define DEFAULT_BLOCKSIZE 4096

typedef struct _Pod_header {
	char magic[5];
	unsigned short rev;
	unsigned long blocksize;
	unsigned long file_count;
} __attribute__ ((packed)) Pod_header;

typedef struct _Ar_file {
	short type;
	char *filename;
	long offset;
	long length;
	void *data;
} __attribute__ ((packed)) Ar_file;

#endif /* _POD_H_ */
