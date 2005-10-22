#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pod.h"

#define BUFFERSIZE 2048

long read_long(FILE *fp)
{
	long value;
	fread(&value, sizeof(long), 1, fp);
	value = ntohl(value);
	return value;
}
void write_long(FILE *fp, long value)
{
	value = htonl(value);
	fwrite(&value, sizeof(long), 1, fp);
}

short read_short(FILE *fp)
{
	short value;
	fread(&value, sizeof(short), 1, fp);
	value = ntohs(value);
	return value;
}
void write_short(FILE *fp, short value)
{
	value = htons(value);
	fwrite(&value, sizeof(short), 1, fp);
}

char *read_string(FILE *fp)
{
	long length, offset;
	char *string;

	length = 1;
	offset = ftell(fp);
	while (fgetc(fp) != '\0') length++;
	fseek(fp, offset, SEEK_SET);

	string = malloc(sizeof(char) * length);
	fread(string, sizeof(char), length, fp);

	return string;
}
void write_string(FILE *fp, char *string)
{
	char null = '\0';
	fwrite(string, sizeof(char), strlen(string), fp);
	fwrite(&null, sizeof(char), 1, fp);
}

void create(int num, char **args)
{
	char buf[BUFFERSIZE];
	FILE *fp;
	short type = 0;
	long *offsets;
	int i;

	if ((fp = fopen(args[0], "wb")) == NULL) {
		perror(args[0]);
		exit(2);
	}
	offsets = (long *)malloc(sizeof(long) * (num - 2));
	fwrite(PODMAGIC, sizeof(char), 5, fp);
	write_short(fp, REV);
	write_long(fp, num - 1);
	

	for (i = 1; i < num; i++) {
		write_short(fp, type);
		write_string(fp, args[i]);
		offsets[i - 1] = ftell(fp);
		write_long(fp, 0);
		write_long(fp, 0);
	}
	for (i = 1; i < num; i++) {
		FILE *nfp;
		long offset = ftell(fp);
		long length, pos, clen;
		if ((nfp = fopen(args[i], "rb")) == NULL) {
			perror(args[i]);
			exit(4);
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
		write_long(fp, offset);
		write_long(fp, length);
		fseek(fp, clen, SEEK_SET);
	}
	fclose(fp);
	free(offsets);
}

void extract_file(FILE *ofp, Ar_file *file)
{
	char buf[BUFFERSIZE];
	long length, pos;
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
	long l;
	Pod_header header;
	FILE *fp;

	if ((fp = fopen(filename, "rb")) == NULL) {
		perror(filename);
		exit(2);
	}
	fread(&header.magic, sizeof(char), 5, fp);
	if (memcmp(PODMAGIC, &header.magic, 5) != 0) {
		fprintf(stderr, "Not a pod file.\n");
		exit(3);
	}
	header.rev = read_short(fp);
	header.file_count = read_long(fp);

	for (l = header.file_count; l > 0; l--) {
		Ar_file file;
		file.type = read_short(fp);
		file.filename = read_string(fp);
		file.offset = read_long(fp);
		file.length = read_long(fp);
		extract_file(fp, &file);
		free(file.filename);
	}
	fclose(fp);
}

void usage(char *rstr)
{
	fprintf(stderr, "%s -x <archive>\n"
			"%s -c <archive> ...\n\n"
			"      -x   extract archive\n"
			"      -c   create archive from files\n",
			rstr, rstr);	
}

int main(int argc, char **argv)
{
	char ext = 0;
	char creat = 0;
	int ch;

	char *app = strrchr(argv[0], '/');
	if (!app++)
		app = argv[0];

	while ((ch = getopt(argc, argv, "xc")) != -1) {
		switch (ch) {
		case 'x':
			ext = 1;
			break;
		case 'c':
			creat = 1;
			break;
		default:
			usage(app);
			return 1;
		}
	}
	argc -= optind;
	argv += optind;

	if ((ext && creat) || (!ext && !creat) ||
			(ext && argc != 1) || (creat && argc < 2)) {
		usage(app);
		return 1;
	}
	if (ext)
		extract(argv[0]);

	if (creat)
		create(argc, argv);

	return 0;
}
