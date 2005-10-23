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
#define REV 3
#define DEFAULT_BLOCKSIZE 4096

#define HEADER_SIZE 20
typedef struct _Pod_header {
	char magic[8];
	uint32_t rev;
	uint32_t blocksize;
	uint32_t file_count;
} Pod_header;

#define FILEHDR_SIZE 20 /* not including name itself, but including name length*/
typedef struct _Ar_file {
	uint32_t type;
	uint32_t offset;
	uint32_t length;
	uint32_t blocks;
	uint32_t namelen;
	char *filename; /* NOT NUL-terminated on disk, but it is in RAM */
} Ar_file;

#endif /* _POD_H_ */
