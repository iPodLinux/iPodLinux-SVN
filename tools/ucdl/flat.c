#include <stdio.h>
#include "flat.h"

int flat_read_header(FILE *fp, struct flat_hdr *header)
{
	rewind(fp);

	fread(header->magic, sizeof(char), 4, fp);
	if (memcmp(&header->magic, FLTMAGIC, 4) != 0)
		return 2;
	fread(&header->rev, sizeof(unsigned long), 1, fp);
	fread(&header->entry, sizeof(unsigned long), 1, fp);
	fread(&header->data_start, sizeof(unsigned long), 1, fp);
	fread(&header->data_end, sizeof(unsigned long), 1, fp);
	fread(&header->bss_end, sizeof(unsigned long), 1, fp);
	fread(&header->stack_size, sizeof(unsigned long), 1, fp);
	fread(&header->reloc_start, sizeof(unsigned long), 1, fp);
	fread(&header->reloc_count, sizeof(unsigned long), 1, fp);
	fread(&header->flags, sizeof(unsigned long), 1, fp);

	return 0;
}

int flat_read_file_header(char *filename, struct flat_hdr *header)
{
	FILE *fp;
	int ret = 0;

	if ((fp = fopen(filename, "r")) == NULL) {
		return 1;
	}
	ret = flat_read_header(fp, header);
	fclose(fp);

	return ret;
}

