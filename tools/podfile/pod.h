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
#include <arpa/inet.h>

#define PODMAGIC "PODar"
#define REV 3
#define DEFAULT_BLOCKSIZE 4096

#include <stdint.h>
#include <sys/types.h>

#define HEADER_SIZE 16
typedef struct _Pod_header {
	char magic[6];
	u_int16_t rev;
	u_int32_t blocksize;
	u_int32_t file_count;
} Pod_header;

#define FILEHDR_SIZE 16 /* not including name itself, but including name length*/
typedef struct _Ar_file {
	u_int16_t type;
	u_int32_t offset;
	u_int32_t length;
	u_int32_t blocks;
	u_int16_t namelen;
	char *filename; /* NOT NUL-terminated on disk, but it is in RAM */
} Ar_file;

#endif /* _POD_H_ */
