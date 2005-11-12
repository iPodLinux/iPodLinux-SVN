#ifndef _FLAT_H_
#define _FLAT_H_
#include <stdio.h>

#define FLTMAGIC "bFLT"

struct flat_hdr {
	char magic[4];
	unsigned int rev;
	unsigned int entry;
	unsigned int data_start;
	unsigned int data_end;
	unsigned int bss_end;
	unsigned int stack_size;
	unsigned int reloc_start;
	unsigned int reloc_count;
	unsigned int flags;       
	unsigned int filler[6];
};

int flat_read_header(FILE *, struct flat_hdr *);
int flat_read_file_header(char *filename, struct flat_hdr *);

#endif /* _FLAT_H_ */
