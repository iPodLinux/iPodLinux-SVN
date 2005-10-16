#ifndef _FLAT_H_
#define _FLAT_H_
#include <stdio.h>

#define FLTMAGIC "bFLT"

struct flat_hdr {
	char magic[4];
	unsigned long rev;
	unsigned long entry;
	unsigned long data_start;
	unsigned long data_end;
	unsigned long bss_end;
	unsigned long stack_size;
	unsigned long reloc_start;
	unsigned long reloc_count;
	unsigned long flags;       
	unsigned long filler[6];
};

int flat_read_header(FILE *, struct flat_hdr *);
int flat_read_file_header(char *filename, struct flat_hdr *);

#endif /* _FLAT_H_ */
