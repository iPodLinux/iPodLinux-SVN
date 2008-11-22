#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "flat.h"

long axtol(char *hex) {
	int i, m, count;
	long value = 0;
	int digit[16];
	char *ptr = hex;

	for (i = 0; i < 16; i++) {
		if (*ptr == '\0')
			break;
		if (*ptr > 0x29 && *ptr < 0x40)
			digit[i] = *ptr & 0x0f;
		else if ((*ptr >= 'a' && *ptr <= 'f') ||
			 (*ptr >= 'A' && *ptr <= 'F'))
			digit[i] = (*ptr & 0x0f) + 9;
		else
			break;
		ptr++;
	}
	count = i;
	m = i - 1;

	for (i = 0; i < count; i++) {
		value = value | (digit[i] << (m << 2));
		m--;
	}
	return value;
}

int main(int argc, char **argv)
{
	long n_syms = 0;
	FILE *fp, *symp;
	char *buf;
	struct flat_hdr header;
	char line[4096];

	if (argc > 2) {
		fp = fopen (argv[2], "r+");
		symp = fopen (argv[1], "r");

		if (!fp) {
			perror (argv[2]);
			return 1;
		}
		if (!symp) {
			perror (argv[1]);
			return 1;
		}
	} else if (argc > 1) {
		fp = fopen (argv[1], "r+");
		symp = stdin;
		if (isatty (0))
			fprintf (stderr, "Reading symbols from standard input.\n");
		if (!fp) {
			perror (argv[1]);
			return 1;
		}
	} else {
		fprintf (stderr, "Usage: symadd [symfile] fltfile\n"
			"  If [symfile] is omitted, defaults to standard input.\n");
		return 255;
	}

	if (flat_read_header(fp, &header) != 0) {
		fprintf(stderr, "Bad magic. %s is not a bFLT exec.\n", argv[2]);
		return 2;
	}
	fseek(fp, header.reloc_start + 4*header.reloc_count + 4, SEEK_SET);

	while (fgets(line, 4095, symp)) {
		char offsets[9];
		char type, t;
		unsigned int offset;
		char sym[1024]; /* overflow unlikely, but possible */
		sscanf(line, "%s %c %s", &offsets, &type, &sym);
                t = toupper (type);
		if (t == 'T' || t == 'W') t = 0;
		if (t == 'D' || t == 'V' || t == 'R' || t == 'G') t = 1;
		if (t == 'B' || t == 'S') t = 2;
                if (t == 'C') {
                    fprintf (stderr, "warning: common symbol %s remains, dropping\n", sym);
                }
                if (islower (type)) t |= 4;
		if (t >= 8)    continue;
		offsets[8] = '\0';
		offset = axtol(offsets);
		offset = htonl(offset);
		fwrite(&offset, sizeof(unsigned int), 1, fp);
		fwrite(&sym, sizeof(char), strlen(sym), fp);
		fwrite("\0", sizeof(char), 1, fp);
		fwrite(&t, sizeof(char), 1, fp);
		n_syms++;
	}
	fclose(symp);

	fseek(fp, header.reloc_start + 4*header.reloc_count, SEEK_SET);
	n_syms = htonl(n_syms);
	fwrite(&n_syms, sizeof(unsigned int), 1, fp);

	fclose(fp);

	return 0;
}

