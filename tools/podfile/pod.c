#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#include "pod.h"

#define BUFFERSIZE 2048

unsigned long blocksize;
char verbose = 0;

u_int32_t read32(FILE *fp)
{
	u_int32_t value;
	fread(&value, sizeof(u_int32_t), 1, fp);
	value = ntohl(value);
	return value;
}
void write32(FILE *fp, u_int32_t value)
{
	value = htonl(value);
	fwrite(&value, sizeof(u_int32_t), 1, fp);
}

u_int16_t read16(FILE *fp)
{
	u_int16_t value;
	fread(&value, sizeof(u_int16_t), 1, fp);
	value = ntohs(value);
	return value;
}
void write16(FILE *fp, u_int16_t value)
{
	value = htons(value);
	fwrite(&value, sizeof(u_int16_t), 1, fp);
}

char *read_string(FILE *fp)
{
	u_int16_t length;
	char *string;

	length = read16(fp);
	string = malloc(length + 1);
	fread(string, sizeof(char), length, fp);
	*(string + length) = '\0';

	return string;
}
void write_string(FILE *fp, char *string)
{
	u_int16_t length = strlen (string);
	write16(fp, length);
	fwrite(string, sizeof(char), length, fp);
}

void read_header(FILE *fp, Pod_header *header)
{
	fread(header->magic, sizeof(header->magic), 1, fp);
	header->rev = read16(fp);
	header->blocksize = read32(fp);
	header->file_count = read32(fp);
	header->filehdr_size = read32(fp);
}
void read_filehdr(FILE *fp, Ar_file *filehdr)
{
	filehdr->type = read16(fp);
	filehdr->offset = read32(fp);
	filehdr->length = read32(fp);
	filehdr->blocks = read32(fp);
	filehdr->filename = read_string(fp);
}

void create(int num, char **args)
{
	char buf[BUFFERSIZE];
	FILE *fp;
	u_int32_t type = 0;
	u_int32_t *offsets;
	u_int32_t headers_size = 0;
	int i;

	if ((fp = fopen(args[0], "wb")) == NULL) {
		perror(args[0]);
		exit(2);
	}
	offsets = (u_int32_t *)malloc(sizeof(u_int32_t) * (num - 2));
	fwrite(PODMAGIC, sizeof(char), 8, fp);
	write16(fp, REV);
	write32(fp, blocksize);
	write32(fp, num - 1);
	write32(fp, 0); // will insert filehdrs size later`

	for (i = 1; i < num; i++) {
		write16(fp, type);
		offsets[i - 1] = ftell(fp);
		write32(fp, 0);
		write32(fp, 0);
		write32(fp, 0);
		write_string(fp, args[i]);
		headers_size += FILEHDR_SIZE + strlen (args[i]);
	}
	for (i = 1; i < num; i++) {
		FILE *nfp;
		u_int32_t offset = ftell(fp);
		u_int32_t length, pos, clen;
		if ((nfp = fopen(args[i], "rb")) == NULL) {
			perror(args[i]);
			exit(4);
		}
		if (offset % blocksize != 0) {
			fseek(fp, blocksize - (offset % blocksize), SEEK_CUR);
			offset = ftell(fp);
		}
		fseek(nfp, 0, SEEK_END);
		length = ftell(nfp);
		rewind(nfp);
		pos = length;
		while (pos > 0) {
			clen = fread(buf, 1,BUFFERSIZE>pos?pos:BUFFERSIZE, nfp);
			pos -= clen;
			while (clen > 0)
				 clen -= fwrite(buf, 1, clen, fp);
		}
		fclose(nfp);

		clen = ftell(fp);
		fseek(fp, offsets[i - 1], SEEK_SET);
		write32(fp, offset);
		write32(fp, length);
		write32(fp, (length + blocksize - 1) / blocksize);
		fseek(fp, clen, SEEK_SET);
		if (verbose)
			printf("%s\n", args[i]);
	}
	fseek(fp, 16, SEEK_SET);
	write32(fp, headers_size);
	fclose(fp);
	free(offsets);
}

void extract_file(FILE *ofp, Ar_file *file)
{
	char buf[BUFFERSIZE];
	u_int32_t length, pos;
	FILE *fp, *nfp;

	fp = fdopen(dup(fileno(ofp)), "rb");
	if ((nfp = fopen(file->filename, "wb")) == NULL) {
		perror(file->filename);
		return;
	}

	fseek(fp, file->offset, SEEK_SET);
	pos = file->length;
	while (pos > 0) {
		length = fread(buf, 1, pos < BUFFERSIZE ? pos : BUFFERSIZE, fp);
		pos -= length;
		while (length > 0)
			length -= fwrite(buf, 1, length, nfp);
	}
	fclose(nfp);
	fclose(fp);
}

void extract(char *filename)
{
	u_int32_t l;
	Pod_header header;
	FILE *fp;

	if ((fp = fopen(filename, "rb")) == NULL) {
		perror(filename);
		exit(2);
	}
	read_header(fp, &header);
	if (memcmp(PODMAGIC, &header.magic, 5) != 0) {
		fprintf(stderr, "Not a pod file.\n");
		exit(3);
	}
	if (header.rev != REV) {
		fprintf(stderr, "Podfile is revision %d. This extracter only"
				" extracts revision %d podfiles.\n", header.rev,
				REV);
		exit(4);
	}

	for (l = header.file_count; l > 0; l--) {
		Ar_file file;
		u_int32_t pos;
		read_filehdr(fp, &file);
		pos = ftell(fp);
		extract_file(fp, &file);
		if (verbose)
			printf("%s\n", file.filename);
		free(file.filename);
		fseek(fp, pos, SEEK_SET);
	}
	fclose(fp);
}

void list_files(char *filename)
{
	u_int32_t l;
	Pod_header header;
	FILE *fp;

	if ((fp = fopen(filename, "rb")) == NULL) {
		perror(filename);
		exit(2);
	}
	read_header(fp, &header);
	if (memcmp(PODMAGIC, &header.magic, 5) != 0) {
		fprintf(stderr, "Not a pod file.\n");
		exit(3);
	}
	if (header.rev != REV) {
		fprintf(stderr, "Podfile is revision %d. This extracter only"
				" extracts revision %d podfiles.\n", header.rev,
				REV);
		exit(4);
	}

	if (verbose)
		puts("  length/blocks@address \tfilename");
	for (l = header.file_count; l > 0; l--) {
		Ar_file file;
		u_int32_t pos;
		read_filehdr(fp, &file);
		pos = ftell(fp);
		if (verbose)
			printf("%8d/%-6d@%08x\t", file.length, file.blocks, file.offset);
		printf("%s\n", file.filename);
		free(file.filename);
		fseek(fp, pos, SEEK_SET);
	}
	if (verbose)
		printf("files: %d\nblocksize: %d\nrevision: %d\n",
				header.file_count,header.blocksize,header.rev);
	fclose(fp);
}

void usage(char *rstr)
{
	fprintf(stderr, "%s -x <archive>\n"
			"%s -l <archive>\n"
			"%s [-b <blocksize>] -c <archive> ...\n"
			"      -x   extract archive\n"
			"      -c   create archive from files\n"
			"      -l   list files in archive\n"
			"      -v   verbose\n"
			"      -b   specify blocksize to use when creating\n",
			rstr, rstr, rstr);	
}

int main(int argc, char **argv)
{
	char ext = 0;
	char creat = 0;
	char list = 0;
	int ch;

	char *app = strrchr(argv[0], '/');
	if (!app++)
		app = argv[0];
	blocksize = DEFAULT_BLOCKSIZE;

	while ((ch = getopt(argc, argv, "b:clvx")) != -1) {
		switch (ch) {
		case 'x':
			ext = 1;
			break;
		case 'c':
			creat = 1;
			break;
		case 'l':
			list = 1;
			break;
		case 'b':
			blocksize = atol(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage(app);
			return 1;
		}
	}
	argc -= optind;
	argv += optind;

	if ((ext && (creat || list)) || (list && (creat || ext)) ||
	    (!ext && !creat && !list) || (ext && argc != 1) ||
	    (creat && argc < 2) || (list && argc != 1)) {
		usage(app);
		return 1;
	}
	if (ext)
		extract(argv[0]);

	if (creat)
		create(argc, argv);

	if (list)
		list_files(argv[0]);

	return 0;
}
