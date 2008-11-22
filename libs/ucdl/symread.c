#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "flat.h"

int symread(const char *symfile)
{
	FILE *fp;
	struct flat_hdr header;
	long n_syms;

	if ((fp = fopen(symfile, "r")) == NULL) {
		return 0;
	}

	if (flat_read_header(fp, &header) != 0) {
		return 0;
	}

	fseek(fp, header.reloc_start + 4*header.reloc_count, SEEK_SET);

	fread(&n_syms, sizeof(long), 1, fp);
	n_syms = ntohl(n_syms);
	if (!n_syms) {
		fprintf(stderr, "No symbols.\n");
		return 1;
	}

	while (n_syms-- > 0) {
		unsigned int pos, addr = 0;
		char *sym;
		int len = 0;
		char whence;

		fread(&addr, sizeof(unsigned int), 1, fp);
		addr = ntohl(addr);
		pos = ftell(fp);
		while ((fgetc(fp)) != '\0')
			len++;
		fseek(fp, pos, SEEK_SET);
		sym = malloc(sizeof(char) * (len + 1));
		fread(sym, sizeof(char), len + 1, fp);
		fread(&whence, sizeof(char), 1, fp);
		printf("%08x\t%s\n", addr, sym);
		free(sym);
	}
	fclose(fp);

	return 0;
}

int main(int argc, char **argv)
{
	char *file = argv[1];
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <bFLT file>\n", argv[0]);
		return 1;
	}
	return symread(file);
}
