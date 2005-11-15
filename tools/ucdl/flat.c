#include <stdio.h>
#include "flat.h"

int flat_read_header(FILE *fp, struct flat_hdr *header)
{
	rewind(fp);

	fread(header->magic, sizeof(char), 4, fp);
	if (memcmp(&header->magic, FLTMAGIC, 4) != 0)
		return 2;
	fread(&header->rev, sizeof(unsigned int), 1, fp);
	fread(&header->entry, sizeof(unsigned int), 1, fp);
	fread(&header->data_start, sizeof(unsigned int), 1, fp);
	fread(&header->data_end, sizeof(unsigned int), 1, fp);
	fread(&header->bss_end, sizeof(unsigned int), 1, fp);
	fread(&header->stack_size, sizeof(unsigned int), 1, fp);
	fread(&header->reloc_start, sizeof(unsigned int), 1, fp);
	fread(&header->reloc_count, sizeof(unsigned int), 1, fp);
	fread(&header->flags, sizeof(unsigned int), 1, fp);

	header->rev = ntohl (header->rev);
	header->entry = ntohl (header->entry);
	header->data_start = ntohl (header->data_start);
	header->data_end = ntohl (header->data_end);
	header->bss_end = ntohl (header->bss_end);
	header->stack_size = ntohl (header->stack_size);
	header->reloc_start = ntohl (header->reloc_start);
	header->reloc_count = ntohl (header->reloc_count);
	header->flags = ntohl (header->flags);

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

